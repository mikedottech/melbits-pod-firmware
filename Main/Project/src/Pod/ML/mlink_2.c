// SPDX-FileCopyrightText: 2017-2020 Melbot Studios, S.L.
// SPDX-License-Identifier: LicenseRef-Melbot-Portfolio
//
// Original author: Miguel Angel Exposito (2017-2020)
//
// This source code is the property of Melbot Studios, S.L. and is published
// publicly for portfolio and educational review purposes with the express
// written authorization of Melbot Studios, S.L.
//
// All rights remain with Melbot Studios, S.L. Redistribution, modification,
// commercial use, or derivative works are not permitted without prior
// written consent of the copyright holder. See LICENSE for full terms.

#if 0
#include "mlink_dsp.h"
#include "utils.h"
#include "HAL/debug.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>

#define ST_HEADER 0
#define ST_BIT0   1

static uint8_t __state = 0;
static uint8_t __lastState = 0;

static uint16_t __last;
static uint16_t __lastSymbol = 0;

static uint16_t __cnt;


static uint16_t vals[3];
static uint8_t __idx;

const uint32_t kCloseFactor = 400;

bool mldsp_isClose(HAL_analogSample_t smp, HAL_analogSample_t ref)
{
    int32_t diff = smp - ref;
    if(diff < 0) diff = -diff;
    return (diff < kCloseFactor);
}

bool mldsp_hasValue(HAL_analogSample_t v, HAL_analogSample_t *pArray, uint16_t len)
{
    for(uint32_t i = 0; i < len; ++i)
    {
        if (mldsp_isClose(v, pArray[i]))
            return true;
    }
    return false;
}

void mldsp_addVal(HAL_analogSample_t v, HAL_analogSample_t *pArray)
{
    if(__idx >= 3) return;
    pArray[__idx++] = v;
}

void mldsp_sortVals(HAL_analogSample_t *pArray, uint16_t len)
{
    static HAL_analogSample_t __mid = 0, __min = 0, __max = 0;

    uint16_t _min = 0xFFFF, _max = 0, _mid = 0;
    for(uint32_t i = 0; i < len; ++i)
    {
        if(pArray[i] == 0)
        {
            pArray[0] = __min;
            pArray[1] = __mid;
            pArray[2] = __max;
            MBT_LOG("OLD_VALS\n");
            return;
        }
        if(pArray[i] < _min) _min = pArray[i];
        if(pArray[i] > _max) _max = pArray[i];
    }
    for(uint32_t i = 0; i < len; ++i)
    {
        if(pArray[i] != _min && pArray[i] != _max)
        {
            _mid = pArray[i];
        }
    }

    __min = pArray[0] = _min;
    __mid = pArray[1] = _mid;
    __max = pArray[2] = _max;
}

void fsm_update(uint8_t symbol);

uint8_t __rx;
bool __error = false;

const int32_t kMaxDiff = 400;

void mldsp_feedSamples(HAL_analogSample_t* buffer, uint16_t len)
{
 
    __idx = 0;
    __cnt = 0;
    memset(&vals[0], 0, sizeof(vals));
    vals[0] = 0xFFFF;

    HAL_analogSample_t tmp = __last;
    uint16_t __cnt2 = __cnt;

    uint32_t nSamplesMin = 0;
    uint32_t avgMin = 0;

    uint32_t nSamplesMax = 0;
    uint32_t avgMax = 0;

    for(uint8_t i = 0; i < len; ++i)
    {
        int32_t diff = buffer[i] - __last;
        diff = (diff < 0) ? -diff : diff;
        //MBT_LOG("%d\t[%d]\n", buffer[i], (int32_t)(buffer[i] - __last));
        __last = buffer[i];
        if(diff < kMaxDiff)
        {
            if(++__cnt >= 3)
            {
                if(buffer[i] < vals[0]) 
                {
                    avgMin += buffer[i];
                    nSamplesMin++;
                    vals[0] = buffer[i];
                }
                
                if(buffer[i] > vals[2])
                {
                    avgMax += buffer[i];
                    nSamplesMax++;
                    vals[2] = buffer[i];
                }

              //if(!mldsp_hasValue(buffer[i], &vals[0], 3))
                //mldsp_addVal(buffer[i], &vals[0]);
                __cnt = 0;
                /*
                __cnt = 0;
                if(buffer[i] < __min)
                    __min = buffer[i];
                if(buffer[i] > __max)
                    __max = buffer[i];
                if(__min != 0xFFFF && __max != 0)
                {
                    if(!mldsp_isClose(buffer[i], __max) &&
                    !mldsp_isClose(buffer[i], __min))
                    {
                        __mid = buffer[i];
                    }
                }*/
            }
        } else {
            __cnt = 0;
        }
    }

    uint32_t nSamples = 0;
    uint32_t avg = 0;

    for(uint8_t i = 0; i < len; ++i)
    {
        int32_t diff = buffer[i] - tmp;
        diff = (diff < 0) ? -diff : diff;
        //MBT_LOG("%d\t[%d]\n", buffer[i], (int32_t)(buffer[i] - __last));
        tmp = buffer[i];
        if(diff < kMaxDiff)
        {
            if(++__cnt2 >= 3)
            {  
                __cnt2 = 0;
                if(!mldsp_isClose(buffer[i], vals[0]) &&
                    !mldsp_isClose(buffer[i], vals[2]))
                    {
                        avg += buffer[i];
                        nSamples++;                        
                    }
            }
        } else {
            __cnt2 = 0;
        }
    }

    if(nSamples > 0)
    {
        vals[1] = avg / nSamples;
    }

    static uint16_t lastVals[3] = {0};

    if(vals[0] != 0 && vals[1] != 0 && vals[2] != 0)
    {
        memcpy(&lastVals[0], &vals[0], sizeof(lastVals));
    } else {
        MBT_LOG("USING CACHED VALUES\n");
        memcpy(&vals[0], &lastVals[0], sizeof(vals));
    }

    __cnt2 = __cnt;
    //mldsp_sortVals(&vals[0], 3);

    // Decode protocol
    // Header: ^-^_ ..... -> 12 steps
    for(uint8_t i = 0; i < len; ++i)
    {
        int32_t diff = buffer[i] - tmp;
        diff = (diff < 0) ? -diff : diff;
        //MBT_LOG("%d\t[%d]\n", buffer[i], (int32_t)(buffer[i] - __last));
        tmp = buffer[i];
        if(diff < kMaxDiff)
        {
            if(++__cnt2 >= 3)
            {  
                __cnt2 = 0;

                HAL_analogSample_t newSym = 0;
                if(mldsp_isClose(buffer[i], vals[0]))
                    newSym = 0;
                else if(mldsp_isClose(buffer[i], vals[1]))
                    newSym = 1;
                else if(mldsp_isClose(buffer[i], vals[2]))
                    newSym = 2;
                else
                    continue;

                if(newSym != __lastSymbol)
                {
                    __lastSymbol = newSym;
                    if(newSym == 0)
                    {
                        MBT_LOG("_");
                        fsm_update(0);
                        //MBT_LOG("1\n");
                    } else if(newSym == 1)
                    {
                        MBT_LOG("-");
                        fsm_update(1);
                        //MBT_LOG("0\n");
                    } else if(newSym == 2)
                    {
                        MBT_LOG("^");
                        fsm_update(2);
                        //MBT_LOG("1\n");
                    }
                }
            }
        } else {
            __cnt2 = 0;
        }       
    }

    MBT_LOG("\n");

    MBT_LOG("MIN: %d, MID: %d, MAX: %d\n", vals[0], vals[1], vals[2]);
    __cnt2 = __cnt;

    if(__error)
    {
        __error = false;
        for(uint8_t i = 0; i < len; ++i)
        {
            int32_t diff = buffer[i] - tmp;
            diff = (diff < 0) ? -diff : diff;
            MBT_LOG("%d\t[%d]\n", buffer[i], (int32_t)(buffer[i] - tmp));
            tmp = buffer[i];
            if(diff < 100)
            {
                if(++__cnt2 >= 3)
                {  
                    __cnt2 = 0;
                    if(mldsp_isClose(buffer[i], vals[0]))
                        MBT_LOG("0\n");
                    else if(mldsp_isClose(buffer[i], vals[1]))
                        MBT_LOG("1\n");
                    else if(mldsp_isClose(buffer[i], vals[2]))
                        MBT_LOG("2\n");
                }
            }
        }
    }
}

bool toggle = true;

uint8_t lastSymbols[4];

void shiftNewSymbol(uint8_t s)
{
    lastSymbols[0] = lastSymbols[1];
    lastSymbols[1] = lastSymbols[2];
    lastSymbols[2] = lastSymbols[3];
    lastSymbols[3] = s;
}

void fsm_update(uint8_t symbol)
{
    shiftNewSymbol(symbol);
    //MBT_LOG("state = %d\n", (int32_t)__state);
    switch(__state)
    {
    case 0:
        if(lastSymbols[0] == 2 && lastSymbols[1] == 1 && lastSymbols[2] == 2 && lastSymbols[3] == 0)
        {   
            mldec_onProlog();
            if(__rx != 0)
                mldec_onByte(__rx);
            __rx = 0;
            
            MBT_LOG("@");
            __state = 4;
        }
    break;

    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
        {
            uint8_t bit = (symbol == 1 ? 0 : 1);
            __rx |= (bit << (__state - 4));     
//            if(toggle != bit)
//            {
//                MBT_LOG("\nTOGGLE ERROR\n");
//                __error = true;
//            }
            toggle = !toggle;
        }                        
        if(++__state > 11) 
        {
            MBT_LOG("|\n");
            MBT_LOG("RX = 0x%x\n", __rx);
           
            __state = 0;
            //if(__rx != 0x55) __error = true;
        }
        
    break;
    }

}
void mldsp_reset(void)
{
    MBT_LOG("DSPRESET\n");
    __state = 0;
    __rx = 0;

    __lastState = 0;
    __last = 0;
    __lastSymbol = 0;
    __cnt = 0;

    memset(&vals[0], 0, sizeof(vals));
    __idx = 0;
    memset(&lastSymbols[0], 0, sizeof(lastSymbols));
}
#endif
