// Synth public header — based on DuinoTune by Blake Livingston
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

#ifndef _SYNTH_H_
#define _SYNTH_H_

#include "common.h"
#include "mbt_config.h"
#include "Audio/gsm.h"
#include "Compression/lzssdec.h"

// Code definitions for song events
#define SYNTH_TRK_EVT_NOTE_ON             (0)
#define SYNTH_TRK_EVT_NOTE_OFF            (1)
#define SYNTH_TRK_EVT_ROW_ADV             (3)
#define SYNTH_TRK_EVT_SET_VOL             (4)
#define SYNTH_TRK_EVT_SET_INST            (5)
#define SYNTH_TRK_EVT_SET_GLIDE_SPEED     (6)
#define SYNTH_TRK_EVT_SET_PORTAMENTO      (7)
#define SYNTH_TRK_EVT_NOTE_ON_FULL_VOL    (8)

// PWM instruments
#define SYNTH_TRK_EVT_PWM_SET_LEVEL_ABS   (9)
#define SYNTH_TRK_EVT_PWM_FADEIN          (10) // 0x0A
#define SYNTH_TRK_EVT_PWM_FADETO_ABS      (11) // 0x0B
#define SYNTH_TRK_EVT_CMD_PLATPCM         (12) // 0x0C

// Player events
#define SYNTH_EVT_MSK_EOL                 (1) // End of loop
#define SYNTH_EVT_MSK_ALLLOOPSDONE        (2) // All loops finished

// INSTRUMENT ENVELOPE definition
typedef struct synth_envelope_t {
	int8_t num_points;                    // Number of envelope points
	uint8_t starting_level;               // Fixed point 9.7 starting volume;	
	const int16_t* env_slopes;            // Fixed point slopes 9.7	
	const uint8_t* point_ticks;           // Number of ticks to run each slope.	
	uint8_t sustain_tick;                 // Tick to halt envelope until note off.
} synth_envelope_t;

// INSTRUMENT definition
typedef struct synth_instrument_t {
	uint8_t voice_type;                   // TRI, PWM, NOISE, PWM_C or CMD
	uint8_t duty;                         // Duty cycle (or PWM channel in PWM_C instruments)
	uint8_t bitcrush;                     // Uses bit crush?
	const synth_envelope_t* envelope;     // Envelope
} synth_instrument_t;

// VOICE (track) definition
typedef struct synth_voice_t {
	enum {
            SYNTH_IT_PWM,                     // Square wave
            SYNTH_IT_TRI,                     // Triangle wave
            SYNTH_IT_NOISE,                   // Noise
            SYNTH_IT_PWM_C,                   // Non-music PWM commands for driving LEDs or motor (fade in, out, set level, ...)
            SYNTH_IT_CMD,                     // Misc commands
            SYNTH_IT_PCM,                     // PCM channel
	} voice_type;

	
	uint16_t    hz;                       // Integer hz.
	uint8_t     f_hz;                     // Fractional hz for portamento accuracy. 0.4 fp.	
	uint8_t     volume;                   // Volume.
	int8_t      _s_volume;                // Fixed 9.7 envelope volume	
	int16_t     _env_volume;              // Index of the current envelope's progress	
	uint8_t     _env_idx;                 // Number of ticks remaining on the current envelope node.	
	uint8_t     _env_ticks_left_for_node; // How many ticks left for current envelope node
	uint8_t     _env_ticks;               // Total env_ticks	
	uint8_t     sustaining;               // Have we hit the sustain_pt?
	uint8_t     gliding;                  // Is this voice gliding?
	uint16_t    porta_rate;               // Portamento rate
	uint16_t    porta_pitch;              // Portamento pitch

	const synth_envelope_t* _envelope;    // Envelope definition
	
	bool        enabled;                  // Voice enabled
	uint16_t    _period;                  // 
	uint16_t    _err;                     // error term.
	uint16_t    bcrunch;                  // bit crunching (triangle wave)        

        // Abstract interface. Actual pointed functions depend on current instrument
	void(*_getSample)(struct synth_voice_t*);
	void(*_setVolume)(struct synth_voice_t*, uint8_t);
	void(*_setDuty)(struct synth_voice_t*, uint8_t duty);
	void(*_setPitch)(struct synth_voice_t*, uint16_t pitch);
	void(*_setEnable)(struct synth_voice_t*, bool enable);

        // Per-instrument state
	union
        {
            struct
            {
                uint8_t     duty;
                uint16_t    duty_period;
            } pwm;
            struct
            {
                uint8_t     duty;
                uint16_t    rise_period;			
                int16_t     fp_vol;		// 9.7 bit fixed point.
                int16_t     level;
                int16_t     rise_slp;
                int16_t     fall_slp;
            } tri;
            struct
            {
                PCMStreamState_t *p_codecStream;
                // TODO: Loop points
                uint8_t flags;
            } pcm;
	} _params;
} synth_voice_t;

// SONG (MOD) definition
typedef struct {
	const uint8_t* /* const **/ pattern_data;       // Points to array of encoded pattern data
	const uint16_t*         pattern_lengths;        // Points to array of pattern lengths
        //const uint16_t*         pattern_offsets;      // Points to array containing the offset of each pattern within the compressed stream
	uint8_t                 num_patterns;           // Total number of patterns in song
	const uint8_t*          pattern_order;          // Array of indices determining the order of patterns in sequence
	uint16_t                bpm;                    // Beats Per Minute
	uint8_t                 rows_per_beat;          // Rows Per Beat
	uint8_t                 ticks_per_row;          // Ticks Per Row
	const synth_instrument_t**    instruments;      // Points to an array of pointers to instruments
} synth_songDefinition_t;


typedef enum
{
    SYNTH_MIXER_VOL_100 = 0,
    SYNTH_MIXER_VOL_50 = 2,
    SYNTH_MIXER_VOL_25 = 4,
    SYNTH_MIXER_VOL_MUTE = 8
} synth_globalVolume_t;

// SYNTH state
typedef struct {
	uint16_t samples_per_tick;                      // Samples Per Tick
	uint16_t tick;                                  // Current tick
	uint16_t next_tick;                             // Next tick
	uint16_t tick_smp_count;                        // Sample count for current tick
	const synth_songDefinition_t* song_def;         // Points to song definition
	uint8_t cur_pattern;                            // Current pattern
	uint8_t order_idx;                              // Order index
	uint16_t pat_idx;                               // Pattern index
	bool playing;                                   // Is playing?
	uint8_t repeats;                                // How many times to repeat
        LZSSDecoderState_t lzss_decoder;                // LZSS stream decoder
        uint8_t lzssBuffer[LZSS_BUFSIZE];               // Scratchpad buffer for LZSS decompression
        uint16_t lzssStreamPos;                         // Points to the current position in the compressed buffer
        synth_globalVolume_t outputVolumeFactor;        // The output value is right shifted by this many bytes
        synth_voice_t     *pcmVoice;
} synth_state_t;

// Initializes the synth
void synth_init(void);

// Voices are indexed from 0 up to SYNTH_N_VOICES

// Sets the voice to PWM audio output
void synth_initVoicePWM(uint8_t voice);

// Sets the voice to triangle/saw audio output
void synth_initVoiceTRI(uint8_t voice);

// Sets the voice to noise output
void synth_initVoiceNOISE(uint8_t voice);

// The following voice modifiers can be called at any time.

// Sets the volume on a voice.
void synth_setVoiceVolume(uint8_t voice, uint8_t volume);

// Sets the Duty cycle on a voice. For PWM this implies the ratio of high/low, as
// a ratio of duty/255. For triangle waves, values between 0 - 255
// transition between TRI and SAW waves.
void synth_setVoiceDuty(uint8_t voice, uint8_t duty);

// Sets the pitch of a voice in Hz
void synth_setVoicePitch(uint8_t voice, uint16_t pitch);

// Enables/disables a voice. This must be set to true to hear anything for a voice.
void synth_setVoiceEnabled(uint8_t voice, bool enable);

// Sets the bit-wise quantization for a voice. E.g,  crunch=3,
// would quantize the voice to 8 - 3 = 5-bits.
// crunch=0 disables this.
// Sounds most distinctive on TRI waves. Compare crunch=4 to the TRI channel on an NES.
void synth_setVoiceBitCrunch(uint8_t voice, uint8_t crunch);


// The following only function during song playback

// Sets the envelope to use during song playback on this channel.
void synth_setVoiceEnvelope(uint8_t voice, const synth_envelope_t* envelope);

// Assigns instrument to voice
void synth_setVoiceInstrument(uint8_t voice, const synth_instrument_t* instrument);

// Sets the portamento rate on this voice. p_rate is a 12.4 fixed point frequency
// multiplier applied per song tick.
void synth_setVoicePortaRate(uint8_t voice, uint16_t p_rate);

// Enables or disables pitch portamento on this voice.
void synth_setVoicePorta(uint8_t voice, bool enable);

// Plays back a song, in the background defined by song_def. Loops forever.
void synth_playSong(const synth_songDefinition_t* song_def);

// Plays back a song, in the background defined by song_def. Loops [repeats] times. 1 to play once. 0 plays forever.
void synth_playSongLoops(const synth_songDefinition_t* song_def, uint8_t repeats);

bool synth_playPCM(uint16_t fileID);

// Stops the song
void synth_stopSong();
void synth_tick(uint32_t dt);
bool synth_isPlaying();

void synth_selectGlobalVolume(synth_globalVolume_t vol);

#endif
