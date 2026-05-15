// Polyphonic fixed-point synth — based on DuinoTune by Blake Livingston
// (https://github.com/blakelivingston/DuinoTune), heavily modified for the
// Melbits POD firmware (2017-2020) by Miguel Angel Exposito while
// employed by Melbot Studios, S.L.
//
// The original DuinoTune MIT License is reproduced below and governs both
// the upstream code and the modifications in this file.

/*
The MIT License (MIT)

Copyright (c) 2016 Blake Livingston (blake.a.livingston@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "common.h"
#include "synth.h"


#include "Audio/gsm_private.h"
#include "HAL/HAL.h"
#include "Pod/Power.h"
#include "HAL/Debug.h"
#include "Pod/Storage.h"

#ifndef MAX
#   define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif
#ifndef MIN
#   define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

// These should be part of synth state struct
struct synth_voice_t     voices[MBT_CFG_SYNTH_N_VOICES];  
volatile uint32_t       sample_cnt = 0;
static int16_t          *sample_buffer;
static uint8_t          sample_buf_clock = MBT_CFG_AUDIO_BUFFER_SIZE_SAMPLES - 1;
static uint8_t          sample_update_idx = 0;

static PCMStreamState_t s_uniquePCMStream;

extern void synth_onTrackEvent(uint8_t channel, uint8_t event, uint8_t param);

const uint8_t* s_pTmp = NULL; //&sound_0[0];

// Synth state
static synth_state_t s_synth_state;

int32_t lzss_getByte()
{
    // TODO: Check for overrun
    return s_synth_state.song_def->pattern_data[s_synth_state.lzssStreamPos++];
}

void lzss_seek(uint16_t offset)
{
    s_synth_state.lzssStreamPos = offset;
}

void synth_init(void)
{
    // Default all voices to PWM
    for (uint8_t i = 0; i < MBT_CFG_SYNTH_N_VOICES; ++i)
        synth_initVoicePWM(i);
    s_synth_state.playing = 0;    
    s_synth_state.outputVolumeFactor = 0; // Max vol
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
static void _getSamplePWM(synth_voice_t* v)
{
    for (uint8_t i = 0; i < MBT_CFG_AUDIO_BUFFER_SIZE_SAMPLES /*/ 2*/; ++i)
    {
        int8_t outp = v->_s_volume;
        v->_err += v->hz;
        if (v->_err >= MBT_CFG_SYNTH_SAMPLE_RATE_HZ)
        {
            v->_err -= MBT_CFG_SYNTH_SAMPLE_RATE_HZ;
        }
        if (v->_err >= v->_params.pwm.duty_period)
        {
            outp = -outp;
        }

        sample_buffer[i + sample_update_idx] += outp;
    }
}

// Adds to the PCM buffer with samples from NOISE channel
static void _getSampleNOISE(synth_voice_t* v)
{
    int8_t val = v->_params.pwm.duty_period;
    uint16_t hz4 = v->hz << 4;
    for (uint8_t i = 0; i < MBT_CFG_AUDIO_BUFFER_SIZE_SAMPLES /*/ 2*/; ++i) {
        v->_err += hz4;
        if (v->_err >= MBT_CFG_SYNTH_SAMPLE_RATE_HZ) {
            v->_err -= MBT_CFG_SYNTH_SAMPLE_RATE_HZ;
            val = (prnd() * v->_s_volume) >> 7; // Should be 8, technically, but not loud enough...
        }
        sample_buffer[sample_update_idx + i] += val;
    }
    v->_params.pwm.duty_period = val;
}

// Adds to the PCM buffer with samples from TRI channel
void _getSampleTRI(struct synth_voice_t* v)
{
    uint8_t lp		= MBT_CFG_AUDIO_BUFFER_SIZE_SAMPLES /*/ 2*/;
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
        if (err >= MBT_CFG_SYNTH_SAMPLE_RATE_HZ)
        {
            err -= MBT_CFG_SYNTH_SAMPLE_RATE_HZ;
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

static void _pcmGoToLoopStart(PCMStreamState_t *s)
{
    s->decode_pos = s->loop_start % 160; /*200*/    
    s->src_pos = s->src + ((s->loop_start / 160) * sizeof(gsm_frame));
    s->src_end = s->src + s->src_size;
    s->samples_pos = s->loop_start;
    s->flags = 1;
}

static void _pcmResetDecoder(PCMStreamState_t *s)
{    
    s->codec_state.nrp = 40;
    s->cur_sample = 0;
    s->last_sample = 0;
    s->samples_pos = 0;
    //s->loop_start = 0;
    //s->loop_end = 0;
    _pcmGoToLoopStart(s);
    s->flags |= GSM_FLAG_SOS;
}

static bool loadPCMFile(PCMStreamState_t *s, uint16_t fileId)
{
    const Pod_storageFD_t *fd = Pod_storageGetFD(fileId);
    if(fd)
    {
        const uint8_t *ptr = Pod_storageGetPtr(fd);
        static const uint8_t expected[] =
            {
                0xAD,   // Magic
                0x00,   // Version
                0x00    // Format
            };
        if(memcmp(ptr, &expected[0], sizeof(expected))) return false;
        ptr += sizeof(expected);
                
        s->src_size = fd->size;

        if(*ptr++ == 0x01)  // Looping info
        {
            s->loop_start = *((uint16_t*)ptr); ptr += sizeof(uint16_t);
            s->loop_end = *((uint16_t*)ptr); ptr += sizeof(uint16_t);
        } else {
            s->loop_start = 0;
            s->loop_end = 160 * (s->src_size / sizeof(gsm_frame));
        }

        s->src = ptr;

        _pcmResetDecoder(s);
        return true;
    }
    return false;    
}

static bool _pcmSetSourceAbsFileID(synth_voice_t* v, uint16_t id)
{
    PCMStreamState_t *s = v->_params.pcm.p_codecStream;
        

    bool ret = loadPCMFile(s, id);
    if(ret)
        s_synth_state.pcmVoice = v;
    return ret;
}

static bool _pcmSetSource(synth_voice_t* v, uint8_t id)
{
    if(id & 0x80) // User slot
    {
        id &= 0x7F;
        bool result = _pcmSetSourceAbsFileID(v, POD_STORAGE_FID_FEEDBACK_PCM_BASE + id);
        if(!result)
        {
            PCMStreamState_t *s = v->_params.pcm.p_codecStream;
            s->src = NULL;
            s->src_size = 0;
        }
        return result;
    }
    return false;
}

// Adds to the PCM buffer with samples from PCM channel
static void _getSamplePCM(synth_voice_t* v)
{
    PCMStreamState_t *s = v->_params.pcm.p_codecStream;

    for (uint8_t i = 0; i < MBT_CFG_AUDIO_BUFFER_SIZE_SAMPLES /*/ 2*/; ++i)
    {
        // End of stream
        if(s->src_pos >= s->src_end) s->flags |= GSM_FLAG_EOS;
        // End of loop
        if(s->samples_pos >= s->loop_end) s->flags |= GSM_FLAG_EOL;
        // End of frame
        if(s->decode_pos >= 160) s->flags |= GSM_FLAG_EOF;

        if(s->flags)
        {
            if(s->flags & (GSM_FLAG_EOL | GSM_FLAG_EOS | GSM_FLAG_SOS))
            {
                // Start of stream or looping instrument (IF BIT CRUNCH == 1 IN THE INSTRUMENT DEFINITION, IT'S A LOOPING INSTRUMENT)
                // bcrunch = 0xFF00 if bcrunch in definition is 0
                // bcrunch = 0xFF80 if bcrunch in definition is 1
                if((s->flags & GSM_FLAG_SOS) || v->bcrunch == 0xFF80)
                {                    
                    // Loop
                    _pcmGoToLoopStart(s);
                } else {   
                    // End of loop or stream and not looping instrument. Disable stream
                    v->enabled = 0;
                    if(s_synth_state.pcmVoice == v) 
                        s_synth_state.pcmVoice = NULL;
                    return;
                }
            } else if(s->flags & GSM_FLAG_EOF)
            {
                s->decode_pos = 0;
            }

            gsm_decode(&(s->codec_state), (gsm_byte*)s->src_pos, s->out_samples);            
            s->src_pos += sizeof(gsm_frame);
            s->flags = 0;
        }


        // Linear interpolation (upscales from 8KHz to 16 KHz)
        int16_t fullSample = s->out_samples[s->decode_pos];
        int32_t smp = ((fullSample + s->last_sample) >> 1); // Divice by 2
        
        // Apply volume
        // 8.8 fp multiplication        
        
        {
            // smp = 16.0 , volume = 0.8
            smp *= v->volume; // result = 16.8
            smp >>= (16); // smp = 8.0
        }

        sample_buffer[i + sample_update_idx] += (int8_t)smp;
        
        s->last_sample = fullSample;
        v->_err += v->hz;
        
        // 261 is the pitch of C-4 which is considered the neutral tone
        while (v->_err >= 261*2)
        {
            v->_err -= 261*2;
            ++s->decode_pos;
            ++s->samples_pos;
        }
    }
}

void _setDutyPCM(struct synth_voice_t* v, uint8_t duty)
{
    // We use the duty parameter to store the sound ID    
    _pcmSetSource(v, duty);
}

void _setPitchPCM(struct synth_voice_t* v, uint16_t pitch)
{
    v->hz = pitch; 
    
    // Only reset decoder if the new pitch is set by a note on, not by a glide effect
    if(!v->gliding)
        _pcmResetDecoder(v->_params.pcm.p_codecStream);
}  

void _setDutyPWM(struct synth_voice_t* v, uint8_t duty)
{
    if (duty < 0x80)
        duty = 0x80 + (0x79 - duty);
    v->_params.pwm.duty = duty;
    v->_params.pwm.duty_period = (((uint32_t)duty * (uint32_t)MBT_CFG_SYNTH_SAMPLE_RATE_HZ) >> 8);
}

void _setPitchPWM(struct synth_voice_t* v, uint16_t pitch)
{
    v->hz = pitch;
    v->_period = MBT_CFG_SYNTH_SAMPLE_RATE_HZ / pitch;
    _setDutyPWM(v, v->_params.pwm.duty);
}        

void _setDutyNOISE(struct synth_voice_t* v, uint8_t duty)
{
}

void _setPitchNOISE(struct synth_voice_t* v, uint16_t pitch)
{
    v->hz = pitch;
}

void _setDutyTRI(struct synth_voice_t* v, uint8_t duty)
{
    if (duty < 0x80)
        duty = 0x80 + (0x79 - duty);
    
    v->_params.tri.duty = duty;
    uint16_t rise_len = (((uint32_t)duty * v->_period) >> 8);
    rise_len = MAX(1, rise_len);
    rise_len = MIN(v->_period - 1, rise_len);
    v->_params.tri.rise_period  = (((uint32_t)duty * (uint32_t)MBT_CFG_SYNTH_SAMPLE_RATE_HZ) >> 8);
    v->_params.tri.rise_slp     = -(v->_params.tri.fp_vol << 1) / rise_len;
    v->_params.tri.fall_slp     = (((int32_t)v->_params.tri.fp_vol << 1) / (v->_period - rise_len));
}

void _setVolume(struct synth_voice_t* v, uint8_t volume) {
    v->volume = volume;
    v->_s_volume = ((v->_envelope) ? (((v->_env_volume >> 7) * (volume >> 1)) >> 8) : (volume >> 1));
}

void _setVolumeTRI(struct synth_voice_t* v, uint8_t volume) {
    _setVolume(v, volume);
    v->_params.tri.fp_vol = -(v->_s_volume << 7);
    _setDutyTRI(v, v->_params.tri.duty);
}

void _setPitchTRI(struct synth_voice_t* v, uint16_t pitch) {
    v->hz = pitch;
    v->_period = MBT_CFG_SYNTH_SAMPLE_RATE_HZ / pitch;
    _setDutyTRI(v, v->_params.pwm.duty);
}

void _setEnable(struct synth_voice_t* v, bool enable) {
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

void _setEnablePCM(struct synth_voice_t* v, bool enable)
{
    _setEnable(v, enable);
/*
    if(v->_envelope) _setEnable(v, enable);
    else 
        if(enable) v->enabled = true;
        else v->_env_idx = 1;   // Delayed off: wait for the sample to finish
*/
}

void initVoiceCommon(struct synth_voice_t* v) {
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

void synth_initVoicePWM(uint8_t voice) {
    struct synth_voice_t* v = &voices[voice];
    v->voice_type = SYNTH_IT_PWM;
    v->_getSample = &_getSamplePWM;
    v->_setEnable = &_setEnable;
    v->_setDuty = &_setDutyPWM;
    v->_setPitch = &_setPitchPWM;
    v->_setVolume = &_setVolume;
    initVoiceCommon(v);
}

void synth_initVoiceTRI(uint8_t voice) {
    struct synth_voice_t* v = &voices[voice];
    v->voice_type = SYNTH_IT_TRI;
    v->_getSample = &_getSampleTRI;
    v->_setEnable = &_setEnable;
    v->_setDuty = &_setDutyTRI;
    v->_setPitch = &_setPitchTRI;
    v->_setVolume = &_setVolumeTRI;
    initVoiceCommon(v);
}

void synth_initVoiceNOISE(uint8_t voice) {
    struct synth_voice_t* v = &voices[voice];
    v->voice_type = SYNTH_IT_NOISE;
    v->_getSample = &_getSampleNOISE;
    v->_setEnable = &_setEnable;
    v->_setDuty = &_setDutyNOISE; //Nop
    v->_setPitch = &_setPitchNOISE;
    initVoiceCommon(v);
}

void synth_initVoicePCM(uint8_t voice) {
    struct synth_voice_t* v = &voices[voice];
    v->voice_type = SYNTH_IT_PCM;
    v->_getSample = &_getSamplePCM;
    //v->_setEnable = &_setEnablePCM;
    v->_setEnable = &_setEnable;
    v->_setDuty = &_setDutyPCM;
    v->_setPitch = &_setPitchPCM;
    v->_setVolume = &_setVolume;
    v->_params.pcm.p_codecStream = &s_uniquePCMStream;
    v->_params.pcm.flags = 0;

    v->enabled = 0;
    v->_err = 0;    
    //v->_setVolume(v, 0xb0);    
    v->_setVolume(v, 0xff);    
    v->_setEnable(v, 0);
    v->_envelope = 0;
    v->gliding = 0;
    v->f_hz = 0;
    v->bcrunch = 0xffff;
}

void synth_setVoiceVolume(uint8_t voice, uint8_t volume) {
    struct synth_voice_t* v = &voices[voice];        
    v->_setVolume(v, volume);
}
void synth_setVoiceDuty(uint8_t voice, uint8_t duty) {
    struct synth_voice_t* v = &voices[voice];
    v->_setDuty(v, duty);
}

void synth_setVoicePitch(uint8_t voice, uint16_t pitch) {
    struct synth_voice_t* v = &voices[voice];
    if (!v->gliding)
        v->_setPitch(v, pitch);
    else
        // If we're gliding, don't set the pitch now, set the destination.
        // The tick handler will call _setPitch.
        v->porta_pitch = pitch;
}

//extern void setVibration(uint8_t level);

void synth_setVoiceEnabled(uint8_t voice, bool enable) {
    struct synth_voice_t* v = &voices[voice];
    v->_setEnable(v, enable);
}

void synth_setVoiceBitCrunch(uint8_t voice, uint8_t crunch) {
    struct synth_voice_t* v = &voices[voice];
    v->bcrunch = 0xffff << (8 - crunch);
}
void synth_setVoicePortaRate(uint8_t voice, uint16_t p_rate) {
    struct synth_voice_t* v = &voices[voice];
    v->porta_rate = p_rate;
}

void synth_setVoicePorta(uint8_t voice, bool enable) {
    struct synth_voice_t* v = &voices[voice];
    v->gliding = enable;
}

// Call this with null to disable envelope.
void synth_setVoiceEnvelope(uint8_t voice, const struct synth_envelope_t* envelope) {
    struct synth_voice_t* v = &voices[voice];
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

void synth_setVoiceInstrument(uint8_t voice, const struct synth_instrument_t* instrument)
{
    switch (instrument->voice_type)
    {
        case SYNTH_IT_PWM:
            synth_initVoicePWM(voice);
            break;
        case SYNTH_IT_TRI:
            synth_initVoiceTRI(voice);
            break;
        case SYNTH_IT_NOISE:
            synth_initVoiceNOISE(voice);
            break;
        case SYNTH_IT_PCM:
            synth_initVoicePCM(voice);
            break;
    }
    if (instrument->envelope != 0)
    {
        synth_setVoiceEnvelope(voice, instrument->envelope);
    }
    
    synth_setVoiceDuty(voice, instrument->duty);
    synth_setVoiceBitCrunch(voice, instrument->bitcrush);
}

static void _synth_AudioOutBegin()
{
    uint32_t err = HAL_audioOutBegin(!!(s_synth_state.outputVolumeFactor != SYNTH_MIXER_VOL_MUTE));
    if(err != 0)
    {
        MBT_LOG("PANIC");
    }
}

void synth_playSongLoops(const synth_songDefinition_t* song, uint8_t repeats)
{    
    synth_stopSong();
    
    if(!song) return;

    // Yeah, fixed point.
    // ticks_sec is in 12.4 fixed point
    uint16_t ticks_sec = ((song->bpm << 4) / 60) * song->rows_per_beat * song->ticks_per_row;
    s_synth_state.song_def = song;
    s_synth_state.pat_idx = 0;
    s_synth_state.order_idx = 0;
    s_synth_state.cur_pattern = song->pattern_order[0];
    s_synth_state.tick = 0;
    s_synth_state.next_tick = 0;
    s_synth_state.tick_smp_count = 0;
    s_synth_state.samples_per_tick = ((uint32_t)MBT_CFG_SYNTH_SAMPLE_RATE_HZ << 4) / ticks_sec;
    
    s_synth_state.repeats = repeats;

    s_synth_state.lzss_decoder.buffer = &s_synth_state.lzssBuffer[0];
    s_synth_state.lzss_decoder.getByte = &lzss_getByte;
    s_synth_state.lzss_decoder.seek = &lzss_seek;
    s_synth_state.lzssStreamPos = 0;

    s_synth_state.pcmVoice = NULL;

    LZSSDecodeInit(&s_synth_state.lzss_decoder);
    //bool wasPlaying = s_synth_state.playing;
    
    s_synth_state.playing = 1;

    //HAL_audioOutEnd();
    //HAL_audioAbort();
    _synth_AudioOutBegin();
}

void synth_playSong(const synth_songDefinition_t* song) {    
    synth_playSongLoops(song, 1);
}

bool synth_playPCM(uint16_t fileID)
{
    synth_stopSong();
    struct synth_voice_t* v = &voices[0];    
    synth_initVoicePCM(0);
    bool ret = _pcmSetSourceAbsFileID(v, fileID);
    if(ret)
    {
        synth_setVoiceEnabled(0, 1);
        _setPitchPCM(v, 261);
        _synth_AudioOutBegin();
    }
    return ret;
}

static void synth_endSong(void)
{
    s_synth_state.playing = 0;    
    for (int i = 0; i < MBT_CFG_SYNTH_N_VOICES; ++i)
    {
        if(&voices[i] == s_synth_state.pcmVoice) continue;
        synth_setVoiceEnabled(i, 0);
        synth_setVoiceVolume(i, 0);
        synth_initVoicePWM(i);
    }

    // Turn off all PWM-driven signals
    for(int i = 0; i < 8; ++i)
        synth_onTrackEvent(i, SYNTH_TRK_EVT_PWM_SET_LEVEL_ABS, 0);
}

void synth_stopSong()
{
    synth_endSong();
    if(s_synth_state.pcmVoice)
        _setEnable(s_synth_state.pcmVoice, false);
}

// 14.2 fixed point note hz from c4-b4
static const uint16_t pitch_table[] = { 1046, 1108, 1174, 1244, 1318, 1396, 1479, 1567, 1661, 1760, 1864, 1975 };

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

static void do_song_tick(void)
{
    LZSSDecoderState_t *pDS = &s_synth_state.lzss_decoder;

    if (s_synth_state.tick == 0)
    {
        char done_row = 0;
        // Porta must be re-enabled every row.
        
        for (int i = 0; i < MBT_CFG_SYNTH_N_VOICES; ++i)
            synth_setVoicePorta(i, 0);

        do
        {
            uint8_t code = LZSSDecodeStep(pDS); ++s_synth_state.pat_idx;

            // Upper nibble is the voice id.
            uint8_t voice = code >> 4;
            // Lower nibble is the instruction.
            code = code & 0xf;

            switch (code) {
                case SYNTH_TRK_EVT_PWM_FADEIN:
                case SYNTH_TRK_EVT_PWM_FADETO_ABS:
                case SYNTH_TRK_EVT_PWM_SET_LEVEL_ABS:
                case SYNTH_TRK_EVT_CMD_PLATPCM:
                {
                    uint8_t param = LZSSDecodeStep(pDS); ++s_synth_state.pat_idx;
                    synth_onTrackEvent(s_synth_state.song_def->instruments[voice]->duty, code, param);
                }
                break;
                case SYNTH_TRK_EVT_NOTE_ON_FULL_VOL:
                    synth_setVoiceVolume(voice, 0xff);
                case SYNTH_TRK_EVT_NOTE_ON:
                {
                    uint8_t note = LZSSDecodeStep(pDS); ++s_synth_state.pat_idx;

                    // The top bit determines whether a volume follows the note-on.
                    uint16_t pitch = get_pitch(note & 0x7f);
                    synth_setVoicePitch(voice, pitch);
                    synth_setVoiceEnabled(voice, 1);
                    // if bit7 is set, trundle on into the volume code
                    // Break otherwise.
                    if ((note & 0x80) == 0) {
                        //SynthSetVoiceVolume(voice, 0xff);
                        break;
                    }
                }
                case SYNTH_TRK_EVT_SET_VOL:
                {
                    //uint8_t volume = (cur_pat[s_synth_state.pat_idx++]);
                    uint8_t volume = LZSSDecodeStep(pDS); ++s_synth_state.pat_idx;
                    synth_setVoiceVolume(voice, volume);
                }
                break;
                case SYNTH_TRK_EVT_NOTE_OFF:
                    synth_setVoiceEnabled(voice, 0);
                break;
                case SYNTH_TRK_EVT_ROW_ADV:
                {
                    // We use voice for the number of rows to wait.
                    // For more than 16.. wait twice.
                    s_synth_state.next_tick = (1 + voice) * s_synth_state.song_def->ticks_per_row;
                    done_row = 1;
                }
                break;
                case SYNTH_TRK_EVT_SET_INST:
                {
                    //uint8_t inst_id = (cur_pat[s_synth_state.pat_idx++]);
                    uint8_t inst_id = LZSSDecodeStep(pDS); ++s_synth_state.pat_idx;
                    synth_setVoiceInstrument(voice, s_synth_state.song_def->instruments[inst_id]);
                }
                break;
                case SYNTH_TRK_EVT_SET_GLIDE_SPEED:
                {
                    //uint8_t gl_low = (cur_pat[s_synth_state.pat_idx++]);
                    //uint8_t gl_high = (cur_pat[s_synth_state.pat_idx++]);
                    uint8_t gl_low = LZSSDecodeStep(pDS); ++s_synth_state.pat_idx;
                    uint8_t gl_high = LZSSDecodeStep(pDS); ++s_synth_state.pat_idx;
                    synth_setVoicePortaRate(voice, gl_low | (gl_high << 8));
                }
                break;
                case SYNTH_TRK_EVT_SET_PORTAMENTO:
                {
                    synth_setVoicePorta(voice, 1);
                }
                break;
            }
        } while (!done_row);
    }

    if (s_synth_state.pat_idx >= (s_synth_state.song_def->pattern_lengths[s_synth_state.cur_pattern]))
    {
        s_synth_state.pat_idx = 0;
        ++s_synth_state.order_idx;
        if (s_synth_state.order_idx >= s_synth_state.song_def->num_patterns)
        {            
            //p_eventCallback(SYNTH_EVT_EOL);
            s_synth_state.order_idx = 0;
            if (s_synth_state.repeats > 0)
            {
                s_synth_state.repeats--;
                if (s_synth_state.repeats == 0)
                {
                    //p_eventCallback(SYNTH_EVT_ALLLOOPSDONE);
                    //synth_stopSong();
                    synth_endSong();
                }
            }
        }

        uint8_t tmp = s_synth_state.cur_pattern;
        s_synth_state.cur_pattern = s_synth_state.song_def->pattern_order[s_synth_state.order_idx];

        // If it's not the next pattern we need to seek in the compressed stream
        if(s_synth_state.cur_pattern != tmp + 1)
        {            
            uint16_t off = 0;
            for(uint16_t i = 0; i < s_synth_state.cur_pattern - 1; ++i)
            {
                off += s_synth_state.song_def->pattern_lengths[i];
            }
            LZSSDecodeSeekTo(pDS, off);
        }
    }

    if (++s_synth_state.tick >= s_synth_state.next_tick)
    {
        s_synth_state.tick = 0;
    }

    // Now lets do our envelopes and effects!
    for (uint8_t i = 0; i < MBT_CFG_SYNTH_N_VOICES; ++i)
    {
        struct synth_voice_t* v = &voices[i];
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

uint16_t HAL_onAudioRequestSamples(uint16_t* buffer, uint16_t len)
{
    sample_buffer = (int16_t*)buffer;
    {            
        memset(&sample_buffer[0], 0x00, len << 1);            
        for (uint8_t i = 0; i < MBT_CFG_SYNTH_N_VOICES; ++i) {
            if (voices[i].enabled)
            {
                voices[i]._getSample(&voices[i]);                    
            }
        }
    
        for (uint8_t i = 0; i < len; ++i) {
            int16_t tmp = sample_buffer[sample_update_idx + i] >> (s_synth_state.outputVolumeFactor);
            tmp = MIN(tmp, 0x7f);
            tmp = MAX(tmp, -0x7f);
            *((uint16_t*)&(sample_buffer[sample_update_idx + i])) = tmp + 0x80;
        }
    
        s_synth_state.tick_smp_count += len;
    }

    return synth_isPlaying() ? MBT_CFG_AUDIO_BUFFER_SIZE_SAMPLES : 0;
}

bool synth_isPlaying()
{
    bool cond = (bool)s_synth_state.playing;

    if(s_synth_state.pcmVoice)
    {
        cond = cond || s_synth_state.pcmVoice->enabled;
    }

    return cond;
}


void synth_selectGlobalVolume(synth_globalVolume_t vol)
{
    s_synth_state.outputVolumeFactor = vol;
    if(synth_isPlaying())
    {
        HAL_setAudioPAState((vol != SYNTH_MIXER_VOL_MUTE));
    }
}

void synth_tick(uint32_t dt)
{
HAL_CRITICAL_SECTION_ENTER();
    if (s_synth_state.playing) {
        while(s_synth_state.tick_smp_count > s_synth_state.samples_per_tick)
        {                    
            s_synth_state.tick_smp_count -= s_synth_state.samples_per_tick;
            do_song_tick();
        }
        Pod_powerSetActivity(PWR_ACT_CLIP);
    } else {
        Pod_powerClearActivity(PWR_ACT_CLIP);
    }
HAL_CRITICAL_SECTION_LEAVE();
}
