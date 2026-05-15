#include "synth.h"

#ifndef MAX
#   define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif
#ifndef MIN
#   define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

// These should be part of synth state struct
struct      TTVOICE voices[N_VOICES];  
volatile    uint32_t sample_cnt = 0;
int16_t     sample_buffer[SAMPLE_BUFFER];
uint8_t     sample_buf_clock = SAMPLE_BUFFER - 1;
uint8_t     sample_update_idx = 0;

// Synth state
song_info_t song_info;

extern void __setChannelPWM(uint8_t ch, uint8_t val);

void SynthInit(void)
{
    // Default all voices to PWM
    for (uint8_t i = 0; i < N_VOICES; ++i)
        SynthInitVoicePWM(i);
    song_info.playing = 0;
}

static inline uint8_t prnd(void) {
    // Cheap pseudo random noise with a linear feedback shift register.
    // https://en.wikipedia.org/wiki/Linear-feedback_shift_register
    static volatile uint16_t state = 0xACA1u;
    uint8_t lsb = state & 1;
    state = state >> 1;
    state ^= (-lsb) & 0xB400u;
    return state & 0xff;
}

// Adds to the PCM buffer with samples from PWM channel
static void _getSamplePWM(TTVOICE* v)
{
    for (uint8_t i = 0; i < SAMPLE_BUFFER /*/ 2*/; ++i)
    {
        int8_t outp = v->_s_volume;
        v->_err += v->hz;
        if (v->_err >= SAMPLE_RATE)
        {
            v->_err -= SAMPLE_RATE;
        }
        if (v->_err >= v->_params.pwm.duty_period)
        {
            outp = -outp;
        }

        sample_buffer[i + sample_update_idx] += outp;
    }
}

// Adds to the PCM buffer with samples from NOISE channel
static void _getSampleNOISE(TTVOICE* v)
{
    int8_t val = v->_params.pwm.duty_period;
    uint16_t hz4 = v->hz << 4;
    for (uint8_t i = 0; i < SAMPLE_BUFFER /*/ 2*/; ++i) {
        v->_err += hz4;
        if (v->_err >= SAMPLE_RATE) {
            v->_err -= SAMPLE_RATE;
            val = (prnd() * v->_s_volume) >> 7; // Should be 8, technically, but not loud enough...
        }
        sample_buffer[sample_update_idx + i] += val;
    }
    v->_params.pwm.duty_period = val;
}

// Adds to the PCM buffer with samples from TRI channel
void _getSampleTRI(struct TTVOICE* v)
{
    uint8_t lp		= SAMPLE_BUFFER /*/ 2*/;
    uint8_t i		= sample_update_idx;
    uint16_t err	= v->_err;
    int16_t lev		= v->_params.tri.level;
    int16_t fp_vol	= v->_params.tri.fp_vol;
    uint16_t rise_p	= v->_params.tri.rise_period;
    uint16_t rslp	= v->_params.tri.rise_slp;
    uint16_t fslp	= v->_params.tri.fall_slp;
    uint16_t bcrunch	= v->bcrunch;

    do
    {
        if (err >= SAMPLE_RATE)
        {
            err -= SAMPLE_RATE;
            lev = fp_vol;
        }
        if (err < rise_p)
        {
            lev += rslp;
        } else {
            lev += fslp;
        }
        err += v->hz;
        sample_buffer[i++] += (lev >> 7) & bcrunch;
    } while (--lp != 0);
    v->_params.tri.level = lev;
    v->_err = err;
}

void _setDutyPWM(struct TTVOICE* v, uint8_t duty)
{
    if (duty < 0x80)
        duty = 0x80 + (0x79 - duty);
    v->_params.pwm.duty = duty;
    v->_params.pwm.duty_period = (((uint32_t)duty * (uint32_t)SAMPLE_RATE) >> 8);
}

void _setPitchPWM(struct TTVOICE* v, uint16_t pitch)
{
    v->hz = pitch;
    v->_period = SAMPLE_RATE / pitch;
    _setDutyPWM(v, v->_params.pwm.duty);
}        

void _setDutyNOISE(struct TTVOICE* v, uint8_t duty)
{
}

void _setPitchNOISE(struct TTVOICE* v, uint16_t pitch)
{
	v->hz = pitch;
}

void _setDutyTRI(struct TTVOICE* v, uint8_t duty)
{
    if (duty < 0x80)
        duty = 0x80 + (0x79 - duty);
    
    v->_params.tri.duty = duty;
    uint16_t rise_len = (((uint32_t)duty * v->_period) >> 8);
    rise_len = MAX(1, rise_len);
    rise_len = MIN(v->_period - 1, rise_len);
    v->_params.tri.rise_period  = (((uint32_t)duty * (uint32_t)SAMPLE_RATE) >> 8);
    v->_params.tri.rise_slp     = -(v->_params.tri.fp_vol << 1) / rise_len;
    v->_params.tri.fall_slp     = (((int32_t)v->_params.tri.fp_vol << 1) / (v->_period - rise_len));
}

void _setVolume(struct TTVOICE* v, uint8_t volume) {
    v->volume = volume;
    v->_s_volume = ((v->_envelope) ? (((v->_env_volume >> 7) * (volume >> 1)) >> 8) : (volume >> 1));
}

void _setVolumeTRI(struct TTVOICE* v, uint8_t volume) {
    _setVolume(v, volume);
    v->_params.tri.fp_vol = -(v->_s_volume << 7);
    _setDutyTRI(v, v->_params.tri.duty);
}

void _setPitchTRI(struct TTVOICE* v, uint16_t pitch) {
    v->hz = pitch;
    v->_period = SAMPLE_RATE / pitch;
    _setDutyTRI(v, v->_params.pwm.duty);
}

void _setEnable(struct TTVOICE* v, bool enable) {
    if (v->_envelope && !v->gliding)
    {
        if (enable)
        {
            v->enabled = enable;
            v->_env_volume = v->_envelope->starting_level << 7;
            v->_env_idx = 0;
            v->_env_ticks = 0;
            v->_env_ticks_left_for_node = v->_envelope->point_ticks[0];
            v->sustaining = 0;
        } else {
            // note disables do note effect envelopes.
            // If the env volume hits zero the channel will be disabled.
            v->sustaining = 0;
            v->_env_ticks++;
            if (v->_env_ticks < v->_envelope->sustain_tick)
                v->_env_ticks = v->_envelope->sustain_tick + 1;
        }
    } else {
        v->enabled = enable;
    }
}

void initVoiceCommon(struct TTVOICE* v) {
    v->enabled = 0;
    v->_err = 0;
    v->_setDuty(v, 0x80);
    v->_setVolume(v, 0xb0);
    v->_setPitch(v, 440);
    v->_setEnable(v, 0);
    v->_envelope = 0;
    v->gliding = 0;
    v->f_hz = 0;
    v->bcrunch = 0xffff;
}

void SynthInitVoicePWM(uint8_t voice) {
    struct TTVOICE* v = &voices[voice];
    v->voice_type = TT_PWM;
    v->_getSample = &_getSamplePWM;
    v->_setEnable = &_setEnable;
    v->_setDuty = &_setDutyPWM;
    v->_setPitch = &_setPitchPWM;
    v->_setVolume = &_setVolume;
    initVoiceCommon(v);
}

void SynthInitVoiceTRI(uint8_t voice) {
    struct TTVOICE* v = &voices[voice];
    v->voice_type = TT_TRI;
    v->_getSample = &_getSampleTRI;
    v->_setEnable = &_setEnable;
    v->_setDuty = &_setDutyTRI;
    v->_setPitch = &_setPitchTRI;
    v->_setVolume = &_setVolumeTRI;
    initVoiceCommon(v);
}

void SynthInitVoiceNOISE(uint8_t voice) {
    struct TTVOICE* v = &voices[voice];
    v->voice_type = TT_NOISE;
    v->_getSample = &_getSampleNOISE;
    v->_setEnable = &_setEnable;
    v->_setDuty = &_setDutyNOISE; //Nop
    v->_setPitch = &_setPitchNOISE;
    initVoiceCommon(v);
}

void SynthSetVoiceVolume(uint8_t voice, uint8_t volume) {
    struct TTVOICE* v = &voices[voice];        
    v->_setVolume(v, volume);
}
void SynthSetVoiceDuty(uint8_t voice, uint8_t duty) {
    struct TTVOICE* v = &voices[voice];
    v->_setDuty(v, duty);
}

void SynthSetVoicePitch(uint8_t voice, uint16_t pitch) {
    struct TTVOICE* v = &voices[voice];
    if (!v->gliding)
        v->_setPitch(v, pitch);
    else
        // If we're gliding, don't set the pitch now, set the destination.
        // The tick handler will call _setPitch.
        v->porta_pitch = pitch;
}

//extern void setVibration(uint8_t level);

void SynthSetVoiceEnabled(uint8_t voice, bool enable) {
    struct TTVOICE* v = &voices[voice];
    v->_setEnable(v, enable);
}

void SynthSetVoiceBitCrunch(uint8_t voice, uint8_t crunch) {
    struct TTVOICE* v = &voices[voice];
    v->bcrunch = 0xffff << (8 - crunch);
}
void SynthSetVoicePortaRate(uint8_t voice, uint16_t p_rate) {
    struct TTVOICE* v = &voices[voice];
    v->porta_rate = p_rate;
}

void SynthSetVoicePorta(uint8_t voice, bool enable) {
    struct TTVOICE* v = &voices[voice];
    v->gliding = enable;
}

// Call this with null to disable envelope.
void SynthSetVoiceEnvelope(uint8_t voice, const struct TTENVELOPE* envelope) {
    struct TTVOICE* v = &voices[voice];
    if (v->_envelope != envelope && envelope)
    {
        v->_envelope = envelope;
        v->_env_volume = v->_envelope->starting_level;
        v->_env_idx = 0;
        v->_env_ticks = 0;
        v->_env_ticks_left_for_node = v->_envelope->point_ticks[0];
        v->sustaining = 0;
    }
    if (!envelope)
    {
        v->_envelope = envelope;
    }
}

void SynthSetVoiceInstrument(uint8_t voice, const struct TTINSTRUMENT* instrument)
{
    switch (instrument->voice_type)
    {
        case TT_PWM:
            SynthInitVoicePWM(voice);
            break;
        case TT_TRI:
            SynthInitVoiceTRI(voice);
            break;
        case TT_NOISE:
            SynthInitVoiceNOISE(voice);
            break;
    }
    if (instrument->envelope != 0)
    {
        SynthSetVoiceEnvelope(voice, instrument->envelope);
    }
    SynthSetVoiceDuty(voice, instrument->duty);
    SynthSetVoiceBitCrunch(voice, instrument->bitcrush);
}

void SynthPlaySongLoops(song_definition_t* song, uint8_t repeats) {
    // Yeah, fixed point.
    // ticks_sec is in 12.4 fixed point
    uint16_t ticks_sec = ((song->bpm << 4) / 60) * song->rows_per_beat * song->ticks_per_row;
    song_info.song_def = song;
    song_info.pat_idx = 0;
    song_info.order_idx = 0;
    song_info.cur_pattern = song->pattern_order[0];
    song_info.tick = 0;
    song_info.next_tick = 0;
    song_info.tick_smp_count = 0;
    song_info.samples_per_tick = ((uint32_t)SAMPLE_RATE << 4) / ticks_sec;
    song_info.playing = 1;
    song_info.repeats = repeats;
}

void SynthPlaySong(song_definition_t* song) {
    SynthPlaySongLoops(song, 0);
}

void SynthStopSong()
{
    song_info.playing = 0;
    for (int i = 0; i < N_VOICES; ++i)
    {
        SynthSetVoiceEnabled(i, 0);
        SynthSetVoiceVolume(i, 0);
    }
}

// 14.2 fixed point note hz from c4-b4
const uint16_t pitch_table[] = { 1046, 1108, 1174, 1244, 1318, 1396, 1479, 1567, 1661, 1760, 1864, 1975 };

uint16_t get_pitch(uint8_t note_code)
{
    // if this is too slow switch to 4.4 octave.note coding.
    // otherwise midi compatibility is nice.
    int8_t octave = (note_code / 12) - 7;
    uint8_t note = note_code % 12;
    uint16_t note_hz = (pitch_table[note]);
    if (octave >= 0)
    {
        return note_hz << octave;
    } else {
        return note_hz >> -octave;
    }
}

void do_song_tick(void)
{
    if (song_info.tick == 0)
    {
        char done_row = 0;
        // Porta must be re-enabled every row.
        
        for (int i = 0; i < N_VOICES; ++i)
            SynthSetVoicePorta(i, 0);

        char* cur_pat = (char*)(song_info.song_def->pattern_data[song_info.cur_pattern]);
        do
        {
            uint8_t code = (cur_pat[song_info.pat_idx++]);
            // Upper nibble is the voice id.
            uint8_t voice = code >> 4;
            // Lower nibble is the instruction.
            code = code & 0xf;

            switch (code) {
                case PWM_FADEIN:
                {
                    uint8_t level = (cur_pat[song_info.pat_idx++]);

                    if(level & 0x80) level = 0xFF;
                    else level <<= 1;

                    //printf("[SYNTH]: PWM_FADEIN (%s): 0x%02x\n", (code & 0x80 ? "U": "A"), level);
                    __setChannelPWM(song_info.song_def->instruments[voice]->duty, level);
                }
                break;                        
                case PWM_FADETO_ABS:
                {
                    uint8_t level = (cur_pat[song_info.pat_idx++]);

                    if(level & 0x80) level = 0xFF;
                    else level <<= 1;

                    //printf("[SYNTH]: PWM_FADEOUT: 0x%02x\n", level);
                    __setChannelPWM(song_info.song_def->instruments[voice]->duty, level);
                }
                break;                
                case PWM_SET_LEVEL_ABS:
                {
                    uint8_t level = (cur_pat[song_info.pat_idx++]);                            

                    if(level & 0x80) level = 0xFF;
                    else level <<= 1;

                    //printf("[SYNTH]: PWM_SET_LEVEL_ABS: 0x%02x\n", level);
                    __setChannelPWM(song_info.song_def->instruments[voice]->duty, level);
                }
                break;
                case CMD_PLAYPCM:
                {
                    uint8_t id = (cur_pat[song_info.pat_idx++]);
                    //printf("[SYNTH]: CMD_PLAYPCM: 0x%02x\n", id);
                    //__setChannelPWM(song_info.song_def->instruments[voice]->duty, level);
                }
                break;
                case NOTE_ON_FULL_VOL:
                    SynthSetVoiceVolume(voice, 0xff);
                case NOTE_ON:
                {
                    uint8_t note = (cur_pat[song_info.pat_idx++]);
                    // The top bit determines whether a volume follows the note-on.
                    uint16_t pitch = get_pitch(note & 0x7f);
                    SynthSetVoicePitch(voice, pitch);
                    SynthSetVoiceEnabled(voice, 1);
                    // if bit7 is set, trundle on into the volume code
                    // Break otherwise.
                    if ((note & 0x80) == 0) {
                        //SynthSetVoiceVolume(voice, 0xff);
                        break;
                    }
                }
                case SET_VOL:
                {
                    uint8_t volume = (cur_pat[song_info.pat_idx++]);
                    SynthSetVoiceVolume(voice, volume);
                }
                break;
                case NOTE_OFF:
                    SynthSetVoiceEnabled(voice, 0);
                break;
                case ROW_ADV:
                {
                    // We use voice for the number of rows to wait.
                    // For more than 16.. wait twice.
                    song_info.next_tick = (1 + voice) * song_info.song_def->ticks_per_row;
                    done_row = 1;
                }
                break;
                case SET_INST:
                {
                    uint8_t inst_id = (cur_pat[song_info.pat_idx++]);
                    SynthSetVoiceInstrument(voice, song_info.song_def->instruments[inst_id]);
                }
                break;
                case SET_GLIDE_SPEED:
                {
                    uint8_t gl_low = (cur_pat[song_info.pat_idx++]);
                    uint8_t gl_high = (cur_pat[song_info.pat_idx++]);
                    SynthSetVoicePortaRate(voice, gl_low | (gl_high << 8));
                }
                break;
                case PORTAMENTO:
                {
                    SynthSetVoicePorta(voice, 1);
                }
                break;
            }
        } while (!done_row);
    }

    if (song_info.pat_idx >= (song_info.song_def->pattern_lengths[song_info.cur_pattern]))
    {
        song_info.pat_idx = 0;
        ++song_info.order_idx;
        if (song_info.order_idx >= song_info.song_def->num_patterns)
        {
            song_info.order_idx = 0;
            if (song_info.repeats > 0)
            {
                song_info.repeats--;
                if (song_info.repeats == 0)
                    SynthStopSong();
            }
        }
        song_info.cur_pattern = song_info.song_def->pattern_order[song_info.order_idx];
    }

    if (++song_info.tick >= song_info.next_tick)
    {
        song_info.tick = 0;
    }

    // Now lets do our envelopes and effects!
    for (uint8_t i = 0; i < N_VOICES; ++i)
    {
        struct TTVOICE* v = &voices[i];
        if (v->gliding && v->hz != v->porta_pitch)
        {
            // add our fractional component back in.
            uint32_t fp_hz = (v->hz << 4) + v->f_hz;
            if (v->hz < v->porta_pitch)
            {
                // gliding up.
                uint32_t p_mul = 0x10000 + v->porta_rate;
                fp_hz = (fp_hz * p_mul) >> 16;
                v->hz = MIN(fp_hz >> 4, v->porta_pitch);
            } else {
                // Glide down.
                uint32_t p_mul = 0x10000 - v->porta_rate;
                fp_hz = (fp_hz * p_mul) >> 16;
                v->hz = MAX(fp_hz >> 4, v->porta_pitch);
            }
            v->f_hz = fp_hz & 0xf;
            v->_setPitch(v, v->hz);
        }
        if (v->enabled && voices[i]._envelope && v->_env_idx < v->_envelope->num_points)
        {
            if (v->_env_ticks == v->_envelope->sustain_tick)
            {
                v->sustaining = 1;
            }
            if (v->sustaining)
                continue; // Move along. We're sustaining!
            v->_env_ticks++;
            v->_env_volume += v->_envelope->env_slopes[v->_env_idx];
            v->_env_volume = MAX(0, v->_env_volume);
            v->_env_volume = MIN(0xff << 7, v->_env_volume);
            v->_setVolume(v, v->volume);
            if (--v->_env_ticks_left_for_node == 0)
            {
                v->_env_idx++;
                if (v->_env_idx == v->_envelope->num_points)
                    continue;
                v->_env_ticks_left_for_node = v->_envelope->point_ticks[v->_env_idx];
            }
        } else {
            if (v->enabled && voices[i]._envelope)
            {
                if (v->_s_volume < 5)
                    v->enabled = 0;
            }
        }
    }
}

#define SYNTH_TASK 1
#define SONG_TASK 2
volatile uint8_t task_bits = 0;

uint8_t SynthUpdate(uint16_t* buf, uint16_t samples)
{
    static bool pattern_processing = 0;

    uint16_t k = 0;

    while(k < samples)
    {
        uint8_t ret = sample_buffer[sample_buf_clock++];  // Write output PCM sample
        buf[k++] = ret;
        ++sample_cnt;
        ++song_info.tick_smp_count;
    
        static bool sample_processing = 0;

        if (sample_buf_clock == SAMPLE_BUFFER)
            sample_buf_clock = 0;
    
        uint8_t fill_sample_buffer = 0;
        if (sample_buf_clock == 0) {
            fill_sample_buffer = 1;
        }
        /*
        if (sample_buf_clock == 0) {
            fill_sample_buffer = 1;
            sample_update_idx = SAMPLE_BUFFER / 2;
        } else if (sample_buf_clock == SAMPLE_BUFFER / 2) {
            fill_sample_buffer = 1;
            sample_update_idx = 0;
        }*/

        if ((!sample_processing) && fill_sample_buffer && !(task_bits & SYNTH_TASK)) {
            sample_processing = 1;
            task_bits |= SYNTH_TASK;
            //sei();
            //memset(&sample_buffer[sample_update_idx], 0, SAMPLE_BUFFER);
            for (uint8_t i = 0; i < N_VOICES; ++i) {
                if (voices[i].enabled)
                    voices[i]._getSample(&voices[i]);
            }
            for (uint8_t i = 0; i < SAMPLE_BUFFER /*/ 2*/; ++i) {
                int16_t tmp = sample_buffer[sample_update_idx + i] >> (OUTPUT_SCALE_SHIFT - 1);
                tmp = MIN(tmp, 0x7f);
                tmp = MAX(tmp, -0x7f);
                sample_buffer[sample_update_idx + i] = tmp + 0x80;
            }
            task_bits &= ~SYNTH_TASK;
            sample_processing = 0;
            //return ret;
        }
    
        if(!pattern_processing) {
            pattern_processing = 1; 
            if (song_info.playing && song_info.tick_smp_count
                > song_info.samples_per_tick && !(task_bits & SONG_TASK) && !(task_bits
                & SYNTH_TASK)) {
                //task_bits &= ~SYNTH_TASK;
                // Re enable sample interrupts while we 'do' the song tick.
                // Note sets have division and stuff, so it might take a little while.
                // If it takes longer than the tick itself, then you have problems.
                task_bits |= SONG_TASK;
                //sei();
                song_info.tick_smp_count = 0;
                do_song_tick();
                task_bits &= ~SONG_TASK;
            }
            pattern_processing = 0;
        }  
    }
    return 0;
}
