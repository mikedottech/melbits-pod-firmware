#ifndef _SYNTH_H_
#define _SYNTH_H_

#include <inttypes.h>
#include <stdbool.h>

#define N_VOICES		(11)
#define SAMPLE_BUFFER	(64)
#define SAMPLE_RATE	/*(24000UL)*/ (32000UL) /*(16000UL)*/ /*(8000UL)*/

// Code definitions for song events
#define NOTE_ON 0
#define NOTE_OFF 1
#define ROW_ADV 3
#define SET_VOL 4
#define SET_INST 5
#define SET_GLIDE_SPEED 6
#define PORTAMENTO 7
#define NOTE_ON_FULL_VOL 8

// PWM instruments
#define PWM_SET_LEVEL_ABS   9
#define PWM_FADEIN          10
#define PWM_FADETO_ABS      11
#define CMD_PLAYPCM         12


#define OUTPUT_SCALE_SHIFT 3

// INSTRUMENT ENVELOPE definition
typedef struct TTENVELOPE {
	int8_t num_points;          // Number of envelope points
	uint8_t starting_level;     // Fixed point 9.7 starting volume;	
	int16_t* env_slopes;        // Fixed point slopes 9.7	
	uint8_t* point_ticks;       // Number of ticks to run each slope.	
	uint8_t sustain_tick;       // Tick to halt envelope until note off.
} TTENVELOPE;

// INSTRUMENT definition
typedef struct TTINSTRUMENT {
	uint8_t voice_type;         // TRI, PWM, NOISE, PWM_C or CMD
	uint8_t duty;               // Duty cycle (or PWM channel in PWM_C instruments)
	uint8_t bitcrush;           // Uses bit crush?
	TTENVELOPE* envelope;       // Envelope
} TTINSTRUMENT;

// VOICE (track) definition
typedef struct TTVOICE {
	enum {
            TT_PWM,                 // Square wave
            TT_TRI,                 // Triangle wave
            TT_NOISE,               // Noise
            TT_PWM_C,               // Non-music PWM commands for driving LEDs or motor (fade in, out, set level, ...)
            TT_CMD                  // Misc commands
	} voice_type;

	
	uint16_t    hz;             // Integer hz.
	uint8_t     f_hz;           // Fractional hz for portamento accuracy. 0.4 fp.	
	uint8_t     volume;         // Volume.
	int8_t      _s_volume;      // Fixed 9.7 envelope volume	
	int16_t     _env_volume;    // Index of the current envelope's progress	
	uint8_t     _env_idx;       // Number of ticks remaining on the current envelope node.	
	uint8_t     _env_ticks_left_for_node;	// How many ticks left for current envelope node
	uint8_t     _env_ticks;     // Total env_ticks	
	uint8_t     sustaining;     // Have we hit the sustain_pt?
	uint8_t     gliding;        // Is this voice gliding?
	uint16_t    porta_rate;     // Portamento rate
	uint16_t    porta_pitch;    // Portamento pitch

	const TTENVELOPE* _envelope; // Envelope definition
	
	bool        enabled;		// Voice enabled
	uint16_t    _period;		// 
	uint16_t    _err;		// error term.
	uint16_t bcrunch;               // bit crunching (triangle wave)

        // Abstract interface. Actual pointed functions depend on current instrument
	void(*_getSample)(struct TTVOICE*);
	void(*_setVolume)(struct TTVOICE*, uint8_t);
	void(*_setDuty)(struct TTVOICE*, uint8_t duty);
	void(*_setPitch)(struct TTVOICE*, uint16_t pitch);
	void(*_setEnable)(struct TTVOICE*, bool enable);

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
                int16_t     fp_vol;			// 9.7 bit fixed point.
                int16_t     level;
                int16_t     rise_slp;
                int16_t     fall_slp;
            } tri;
	} _params;
} TTVOICE;

// SONG (MOD) definition
typedef struct {
	const uint8_t* const *  pattern_data;           // Points to array of encoded pattern data
	const uint16_t*         pattern_lengths;        // Points to array of pattern lengths
	uint8_t                 num_patterns;           // Total number of patterns in song
	const uint8_t*          pattern_order;          // Array of indices determining the order of patterns in sequence
	uint16_t                bpm;                    // Beats Per Minute
	uint8_t                 rows_per_beat;          // Rows Per Beat
	uint8_t                 ticks_per_row;          // Ticks Per Row
	const TTINSTRUMENT**    instruments;            // Points to an array of pointers to instruments
} song_definition_t;

// SYNTH state
typedef struct {
	uint16_t samples_per_tick;                      // Samples Per Tick
	uint16_t tick;                                  // Current tick
	uint16_t next_tick;                             // Next tick
	uint16_t tick_smp_count;                        // Sample count for current tick
	song_definition_t* song_def;                    // Points to song definition
	uint8_t cur_pattern;                            // Current pattern
	uint8_t order_idx;                              // Order index
	uint16_t pat_idx;                               // Pattern index
	bool playing;                                   // Is playing?
	uint8_t repeats;                                // How many times to repeat
} song_info_t;

// Initializes the synth
void SynthInit(void);

// Voices are indexed from 0 up to N_VOICES

// Sets the voice to PWM audio output
void SynthInitVoicePWM(uint8_t voice);

// Sets the voice to triangle/saw audio output
void SynthInitVoiceTRI(uint8_t voice);

// Sets the voice to noise output
void SynthInitVoiceNOISE(uint8_t voice);

// The following voice modifiers can be called at any time.

// Sets the volume on a voice.
void SynthSetVoiceVolume(uint8_t voice, uint8_t volume);

// Sets the Duty cycle on a voice. For PWM this implies the ratio of high/low, as
// a ratio of duty/255. For triangle waves, values between 0 - 255
// transition between TRI and SAW waves.
void SynthSetVoiceDuty(uint8_t voice, uint8_t duty);

// Sets the pitch of a voice in Hz
void SynthSetVoicePitch(uint8_t voice, uint16_t pitch);

// Enables/disables a voice. This must be set to true to hear anything for a voice.
void SynthSetVoiceEnabled(uint8_t voice, bool enable);

// Sets the bit-wise quantization for a voice. E.g,  crunch=3,
// would quantize the voice to 8 - 3 = 5-bits.
// crunch=0 disables this.
// Sounds most distinctive on TRI waves. Compare crunch=4 to the TRI channel on an NES.
void SynthSetVoiceBitCrunch(uint8_t voice, uint8_t crunch);


// The following only function during song playback

// Sets the envelope to use during song playback on this channel.
void SynthSetVoiceEnvelope(uint8_t voice, const TTENVELOPE* envelope);

// Assigns instrument to voice
void SynthSetVoiceInstrument(uint8_t voice, const TTINSTRUMENT* instrument);

// Sets the portamento rate on this voice. p_rate is a 12.4 fixed point frequency
// multiplier applied per song tick.
void SynthSetVoicePortaRate(uint8_t voice, uint16_t p_rate);

// Enables or disables pitch portamento on this voice.
void SynthSetVoicePorta(uint8_t voice, bool enable);

// Plays back a song, in the background defined by song_def. Loops forever.
void SynthPlaySong(song_definition_t* song_def);

// Plays back a song, in the background defined by song_def. Loops [repeats] times. 1 to play once. 0 plays forever.
void SynthPlaySongLoops(song_definition_t* song_def, uint8_t repeats);

// Stops the song
void SynthStopSong();

// Fill up the given buffer with samples
uint8_t SynthUpdate(uint16_t* buf, uint16_t samples);

#endif
