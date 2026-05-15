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

#include "mlink_dsp.h"
#include "utils.h"
#include "HAL/debug.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "Pod/Misc/fix_fft.h"
#include "Pod/Misc/fxpt_atan2.h"

int16_t im[MBT_CFG_ML_SAMPLE_BUFFER_SIZE_SAMPLES];



int32_t mldsp_getAngleDiff(uint16_t a1, uint16_t a2)
{
    // Minimum difference between angles
    int32_t diff = a1 - a2;
    diff = diff < 0 ? -diff : diff; // abs(difF)
    diff %= 0xFFFF; // % 2*PI
    diff = diff > 0x8000 ? 0xFFFF - diff : diff; // 0..PI
    return diff;
}

int32_t mldsp_getPhaseDiff(uint16_t pRef, uint16_t multiplier, uint16_t pCheck)
{
    // Multiply phase05 x4
    uint32_t tmp = pRef * multiplier;
    tmp %= 0xFFFF; // % 2*PI
    pRef = (uint16_t)tmp;

    return mldsp_getAngleDiff(pRef, pCheck);
}

void mldsp_normalize(uint16_t* data, uint8_t len)
{
    uint16_t _max = 0;
    uint16_t sf;

    for(uint8_t i = 0; i < len; ++i)
    {
        _max = data[i] > _max ? data[i] : _max;        
    }

    if(_max > 100)
    {
        sf = 100 - (_max - 100);
    } else {
        sf = 100 + (100 - _max);
    }

    for(uint8_t i = 0; i < len; ++i)
    {
        data[i] = (data[i] * sf / 100);
    }
}

// 64 samples

#if 0
// 2 amplitudes version (sample rate: 64 Hz, buffer len: 64)
void mldsp_feedSamples(HAL_analogSample_t* buffer, uint16_t len)
{
    static uint32_t lastAvg = 0;
    static volatile uint64_t avg = 0;

    memset(&im[0], 0, sizeof(im));

    for(uint8_t i = 0; i < len; ++i)
        avg = (uint64_t)(avg + buffer[i]);
    avg /= (len);


    for(uint8_t i = 0; i < len; ++i)
    {        
        // 14 to 16-bit
        buffer[i] <<= 2;
        // Scale samples
        buffer[i] = (uint16_t)((int16_t)buffer[i] - 0x8000);        
    }

    uint32_t avgDiff = (uint32_t)((int32_t)((int32_t)avg - (int32_t)lastAvg) < 0 ? (lastAvg - avg) : (avg - lastAvg));

    //MBT_LOG("AVG DIFF = %u\n", avgDiff);
    
    lastAvg = (uint32_t)avg;

    fix_fft((uint16_t*)&buffer[0],  &im[0], 6, 0);

    int8_t g = -1;   
    uint16_t lm = 1;
    for(uint8_t i = 0; i < MBT_CFG_ML_SAMPLE_BUFFER_SIZE_SAMPLES / 2; ++i)
    {
        buffer[i] = (uint16_t)Utils_getApproximatedDistance2D((int16_t)buffer[i], (int16_t)im[i]);
        if(i == 0) continue;
        if(buffer[i] >= lm)
        {
            lm = buffer[i];
            g = i;
        }
    }
/*
    MBT_LOG("v=[\n");
*/
/*
    for(uint8_t i = 0; i < MBT_CFG_ML_SAMPLE_BUFFER_SIZE_SAMPLES / 2; ++i)
    {        
        MBT_LOG("%u Hz: %u\t%s\n", (10 * (64*i)/MBT_CFG_ML_SAMPLE_BUFFER_SIZE_SAMPLES), buffer[i], (g == i && buffer[i] > 1000 ? "<======" : ""));
        //MBT_LOG("%u\n", buffer[i]);
    }
    */
/*
    MBT_LOG("]\n");

    MBT_LOG("\n\n");
    */

    uint16_t _max = 0;

    uint16_t a[2];
    
    a[0] = (buffer[1]); // 1 Hz
    a[1] = (buffer[3]); // 2 Hz

    _max = a[0];
    
    a[0] = (buffer[1] * 100 / _max); // 1 Hz
    a[1] = (buffer[3] * 100 / _max); // 2 Hz

    //MBT_LOG("%d%%, %d%%\n", a[0], a[1]);
    

    if(avgDiff < 100 &&      // Avoid false positives when lifting it up from the table (big changes in avg light)
        buffer[1] > 100 &&  // Minimum threshold
        (g == 1 || g == 3) && // Dominant frequency is either of these two
        a[1] > 80) // The secondary frequency is at least 40% of the primary
    {
        //MBT_LOG("SYMBOL ===> 0x%x\n", sym);
        mldec_onProlog();
        mldec_onByte(0xff);
        MBT_LOG("*** DETECTED-\n");
        //Pod_vibSetVibration(128, 100);
    } else {
        //MBT_LOG("-NOT DETECTED-\n");
        //mldec_onByte(0);
    }
}
#endif

// 4 amplitudes version
// (sample rate: 32 Hz, buffer len: 64)
#if 0
void mldsp_feedSamples(HAL_analogSample_t* buffer, uint16_t len)
{
    static uint32_t lastAvg = 0;
    static volatile uint64_t avg = 0;

    memset(&im[0], 0, sizeof(im));

    for(uint8_t i = 0; i < len; ++i)
        avg = (uint64_t)(avg + buffer[i]);
    avg /= (len/2);


    for(uint8_t i = 0; i < len; ++i)
    {        
        // 14 to 16-bit
        buffer[i] <<= 2;
        // Scale samples
        buffer[i] = (uint16_t)((int16_t)buffer[i] - 0x8000);        
    }


    uint32_t avgDiff = (uint32_t)((int32_t)((int32_t)avg - (int32_t)lastAvg) < 0 ? (lastAvg - avg) : (avg - lastAvg));

    MBT_LOG("AVG DIFF = %u\n", avgDiff);
    
    lastAvg = (uint32_t)avg;

    fix_fft((uint16_t*)&buffer[0],  &im[0], 6, 0);

    int8_t g = -1;   
    uint16_t lm = 1;
    for(uint8_t i = 0; i < MBT_CFG_ML_SAMPLE_BUFFER_SIZE_SAMPLES / 2; ++i)
    {
        buffer[i] = (uint16_t)Utils_getApproximatedDistance2D((int16_t)buffer[i], (int16_t)im[i]);
        if(i == 0) continue;
        if(buffer[i] >= lm)
        {
            lm = buffer[i];
            g = i;
        }
    }

    
    MBT_LOG("v=[\n");

    for(uint8_t i = 0; i < MBT_CFG_ML_SAMPLE_BUFFER_SIZE_SAMPLES / 2; ++i)
    {        
        MBT_LOG("%u Hz: %u\t%s\n", (10 * (32*i)/MBT_CFG_ML_SAMPLE_BUFFER_SIZE_SAMPLES), buffer[i], (g == i && buffer[i] > 1000 ? "<======" : ""));
        //MBT_LOG("%u\n", buffer[i]);
    }

    MBT_LOG("]\n");

    MBT_LOG("\n\n");

    uint16_t _max = 0;

    uint16_t a[4];
    a[0] = (buffer[1]); // 0.5 Hz
    a[1] = (buffer[3]); // 1.5 Hz
    a[2] = (buffer[5]); // 2.5 Hz
    a[3] = (buffer[7]); // 3.5 Hz

    for(uint8_t i = 0; i < 4; ++i)
    {
        if(_max < a[i]) _max = a[i];
    }
    
    a[0] = (buffer[1] * 100 / _max); // 0.5 Hz
    a[1] = (buffer[3] * 100 / _max); // 1.5 Hz
    a[2] = (buffer[5] * 100 / _max); // 2.5 Hz
    a[3] = (buffer[7] * 100 / _max); // 3.5 Hz

    //mldsp_normalize(&a[0], 4);

    //MBT_LOG("avg1: %u, avg2: %u, avg1 %s avg2\n", avg1, avg2, avg1 < avg2 ? "<" : ">=");

    MBT_LOG("%d%%, %d%%, %d%%, %d%%\n", a[0], a[1], a[2], a[3]);
    

    uint8_t sym = 0;
    sym |= a[0] > 50 ? 0x1 : 0x0;
    sym |= a[1] > 50 ? 0x2 : 0x0;
    sym |= a[2] > 50 ? 0x4 : 0x0;
    sym |= a[3] > 30 ? 0x8 : 0x0;


    if(buffer[g] > 100 && (g == 1 || g == 3 || g == 5 || g == 7) && avgDiff < 1000)
    {
        MBT_LOG("SYMBOL ===> 0x%x\n", sym);

        mldec_onProlog();
        mldec_onByte(sym);
    } else {
        MBT_LOG("-NOT DETECTED-\n");
        //mldec_onByte(0);
    }
}
#endif


// Amplitude + Phase version
// (sample rate: 128 Hz, buffer len: 64)
#if 1
//int32_t lastAvg;
typedef enum
{
    FS_IDLE,
    FS_DETECTED1,
    FS_DETECTED2
} FFTState_t;

FFTState_t state;
void mldsp_reset(void)
{
    //lastAvg = 0;
    state = FS_IDLE;
    //memset(&s_mldsp, 0, sizeof(s_mldsp));
}
void mldsp_feedSamples(HAL_analogSample_t* buffer, uint16_t len)
{
    memset(&im[0], 0, sizeof(im));
    //uint32_t avg = 0;
    for(uint8_t i = 0; i < len; ++i)
    {
        //avg += buffer[i];
        // 14 to 16-bit
        buffer[i] <<= 2;
        // Scale samples
        buffer[i] = (uint16_t)((int16_t)buffer[i] - 0x8000);
    }

    fix_fft((uint16_t*)&buffer[0],  &im[0], 6, 0);

    int8_t g = -1;
    uint16_t lm = 1;
    for(uint8_t i = 0; i < MBT_CFG_ML_SAMPLE_BUFFER_SIZE_SAMPLES / 2; ++i)
    {
        buffer[i] = (uint16_t)Utils_getApproximatedDistance2D((int16_t)buffer[i], (int16_t)im[i]);
        if(i == 0) continue;
        if(buffer[i] >= lm)
        {
            lm = buffer[i];
            g = i;
        }
    }

    if(g == 1 && buffer[1] > 100 && /*buffer[1] > buffer[0] &&*/ (buffer[1] - buffer[2]) > 2000)
    {
        if(state == FS_IDLE)
            state = FS_DETECTED1;
        else if(state >= FS_DETECTED1)
        {
            state = FS_DETECTED2;
            //MBT_LOG("************** OK\n");
            mldec_onProlog();    
            mldec_onByte(0xff);
        }
        
    } else {
        state = FS_IDLE;
    }
}
#endif

