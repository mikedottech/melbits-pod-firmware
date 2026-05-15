
// MAGIC LINK PROTOCOL DECODER

#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

#include "MLDecoder.h"

void __attribute__((weak)) mldec_onProlog() {}
void __attribute__((weak)) mldec_onByte(uint8_t data) {}
void __attribute__((weak)) mldec_onError(uint32_t errorType) {}

#define EPSILON                     /*(1000)*/ (7) //(1200) //(400) // TODO: Make it depend on the detected bit period?
#define BIT_PERIOD_COUNT            /*(4166)*/ (66)  // Number of timer counts for a bit period (app frame)
#define BIT_PERIOD_SNAP_COUNT_NEXT  /*(2749)*/ (44) // Number of timer counts for an edge to be snapped to the next time boundary (66%)

typedef struct
{
    uint32_t h1, l1;        // (Temporary): For epilog recognition
    uint32_t h2, l2;        // (Temporary): For epligo recognition
    uint16_t inBuffer;      // Input buffer
    uint8_t state;          // Decoder state
    uint8_t parity;         // Parity bit
    mldec_WaveCycle_t buffer[2];  // Buffer for recognizing the prolog
    uint8_t ins;            // Index where the next element is to be inserted in the buffer
    uint8_t cap;            // Buffer capacity (can be 0, 1 or 2)
} mldec_State_t;

static mldec_State_t s_mldec = {0};

enum
{
    MD_PROLOG = 0,
    MD_BIT0, MD_BIT1, MD_BIT2, MD_BIT3, MD_BIT4, MD_BIT5, MD_BIT6, MD_BIT7,
    MD_PARITY,
    MD_EPILOG
};

static bool _mldec_push(uint32_t l, uint32_t h)
{
    s_mldec.ins = (s_mldec.ins + 1) & 0x1;
    s_mldec.buffer[s_mldec.ins].h = h;
    s_mldec.buffer[s_mldec.ins].l = l;    
    if(s_mldec.cap < 2) ++ s_mldec.cap;
}

static mldec_WaveCycle_t _mldecpeek(uint32_t offset)
{
    mldec_WaveCycle_t ret = s_mldec.buffer[s_mldec.ins];
    uint8_t idx = (s_mldec.ins - offset) & 0x1;
    ret.h = s_mldec.buffer[idx].h;
    ret.l = s_mldec.buffer[idx].l;
    return ret;
}

static void _mldec_flush()
{
    s_mldec.cap = 0;
}

static bool _mldecin_range(uint32_t val, uint32_t period)
{
    return (abs(val - period) <= EPSILON);
}

void mldec_reset(void)
{
    memset(&s_mldec, 0, sizeof(mldec_State_t));
}

static void _mldec_snap(uint32_t *v)
{
    uint32_t flr = *v / BIT_PERIOD_COUNT;
    uint32_t tmp = flr;    
    flr *= BIT_PERIOD_COUNT;
    if(*v - flr >= BIT_PERIOD_SNAP_COUNT_NEXT)    // 66%
    {
        *v = flr + BIT_PERIOD_COUNT;
        ++tmp;
    }
    else *v = flr;

    if(*v < BIT_PERIOD_COUNT) *v = BIT_PERIOD_COUNT;        // Min = At least 1 frame
    if(*v > BIT_PERIOD_COUNT*3) *v = BIT_PERIOD_COUNT*3;    // Max = 3 fames
}

void mldec_decode(mldec_WaveCycle_t c)
{
    bool quit;

    if(s_mldec.state == MD_PROLOG || s_mldec.state == MD_EPILOG)
    {
        // Snap prolog and epilog
        _mldec_snap(&c.l);
        _mldec_snap(&c.h);
        _mldec_push(c.l, c.h);
    }

    do
    {
        quit = true;
        switch(s_mldec.state)
        {
            case MD_PROLOG:
            {   
                // At least 2 cycles in buffer required
                if(s_mldec.cap < 2) break;

                s_mldec.h1 = 0;

                mldec_WaveCycle_t c2 = _mldecpeek(0);
                mldec_WaveCycle_t c1 = _mldecpeek(1);

                if( _mldecin_range(c2.l, c1.l >> 1) &&
                    _mldecin_range(c2.h, c1.h >> 1) &&
                    _mldecin_range(c1.h, c1.l) &&
                    _mldecin_range(c2.h, c2.l))
                {
                    // PROLOG RECOGNIZED
                    ++s_mldec.state;
                    _mldec_flush();
                    //send_string("s_mldec [PROLOG OK]\n");
                    mldec_onProlog();
                } else {
                    // BAD PROLOG                    
                    // Nothing to do
                    //send_string("s_mldec [BAD PROLOG] (diffH = %d, diffL = %d)\n", ((c1.h >> 1) - c2.h), ((c1.l >> 1) - c2.l));
                    mldec_onError(MLDEC_ERROR_PROLOG);
                }
            }
            break;
            case MD_BIT0: case MD_BIT1: case MD_BIT2: case MD_BIT3: case MD_BIT4:
            case MD_BIT5: case MD_BIT6: case MD_BIT7: case MD_PARITY:
            {                
                uint8_t bit;
                bit = (c.h > c.l);

                //send_string("s_mldec [BIT %d] -> %d\n", s_mldec.state, bit);

                if(s_mldec.state != MD_PARITY)
                {
                    s_mldec.inBuffer |= bit << (s_mldec.state - 1);
                } else {
                    s_mldec.parity = bit;                    
                }
                ++s_mldec.state;
            }
            break;
            case MD_EPILOG:
            {
                if(s_mldec.h1 == 0)
                {
                    s_mldec.h1 = c.h;
                    s_mldec.l1 = c.l;
                } else {
                    s_mldec.h2 = c.h;
                    s_mldec.l2 = c.l;
                
                    if( _mldecin_range(s_mldec.l1, s_mldec.l2) &&
                        _mldecin_range(s_mldec.h1, s_mldec.h2))
                    {
                        // EPILOG RECOGNIZED
                        // Check parity                    
                        uint8_t temp = s_mldec.inBuffer;
                        uint8_t p = 0;
                        for(uint8_t i = 0; i < 8; ++i)
                        {
                            if(temp & 0x1)
                            {
                                ++p;
                            }
                            temp >>= 1;
                        }
                        
                        p = !!!(p & 0x1);

                        if(p == s_mldec.parity)
                        {
                            // Notify received bit or error
                            //send_string("s_mldec [EPILOG OK!]\n");
                            //send_string("===================== s_mldec BYTE: 0x%x =======================\n", s_mldec.inBuffer);
                            mldec_onByte((uint8_t)s_mldec.inBuffer);
                            _mldec_flush();
                        } else {
                            //send_string("s_mldec [BAD PARITY!]\n");
                            mldec_onError(MLDEC_ERROR_PARITY);
                        }                    
                    } else {
                        // BAD EPILOG
                        // Nothing to do
                        //send_string("s_mldec [BAD EPILOG]\n");
                        mldec_onError(MLDEC_ERROR_EPILOG);
                    }   
                    //_mldecdecoder_reset();
                    s_mldec.state = MD_PROLOG;
                    s_mldec.inBuffer = 0;
                    quit = false; // Check prolog again
                }            
            }
            break;
        }
    } while(!quit);
}
