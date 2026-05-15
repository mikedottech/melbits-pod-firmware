#include "mlink_dsp.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include <nrf_log.h>
#include <math.h>


static uint32_t divround(uint32_t a, uint32_t b)
{
    uint32_t fl = a / b;
    uint32_t m = a % b;
    if(m >= (b >> 1)) ++fl;
    return fl;
}

void __attribute__((weak)) mldsp_onWaveCycle(mldec_WaveCycle_t cycle) {
    //NRF_LOG_DEBUG("[] Cycle: l: %d [%d], h: %d [%d]\n", cycle.l, divround(cycle.l, 66), cycle.h, divround(cycle.h, 66));
}

typedef enum
{
    //MDS_IDLE = 0,
    MDS_EXPECTINGDOWN = 0,
    MDS_DOWN,
    MDS_EXPECTINGUP,
    MDS_UP
} mldsp_State;

typedef struct
{
    int32_t runningAverage;
    uint32_t timeStamp;    
    uint32_t risingEdgeTimeStamp;
    HALAnalogSample_t lastMin, lastMax;
    HALAnalogSample_t lastSample;
    mldsp_State state;
    bool nextExpectedEdgeIsUp;
} mldsp_State_t;

static mldsp_State_t s_mldsp = {0};

void mldsp_reset(void)
{
    memset(&s_mldsp, 0, sizeof(s_mldsp));
}

#define MAXMIN_WINDOW_HALFSIZE_SAMPLES (20)
#define SAMPLE_INTERVAL_MS (8)
#define LOCALAVG_WINDOW_HALFSIZE_SAMPLES (1)

#ifndef MIN
#   define MIN(A, B) (A < B) ? (A) : (B)
#endif

static void mldsp_maxMinWindow(HALAnalogSample_t* buffer, uint16_t len, uint16_t idx, HALAnalogSample_t *pMin, HALAnalogSample_t* pMax)
{
    int32_t startIdx, endIdx;    
    startIdx    = MAX(0, (idx - MAXMIN_WINDOW_HALFSIZE_SAMPLES));
    endIdx      = MIN(len, (idx + MAXMIN_WINDOW_HALFSIZE_SAMPLES));

    HALAnalogSample_t min = HALANALOGSAMPLE_MAX, max = HALANALOGSAMPLE_MIN;
    
    if((idx - MAXMIN_WINDOW_HALFSIZE_SAMPLES < 0))
    {
        min = s_mldsp.lastMin;
        max = s_mldsp.lastMax;
    }

    for(uint16_t i = startIdx; i < endIdx; ++i)
    {
        HALAnalogSample_t cur = buffer[i];
        if(cur < min) min = cur;
        if(cur > max) max = cur;
    }
    *pMin = min;
    *pMax = max;
}

static void mldsp_movingAvg(HALAnalogSample_t* buffer, uint16_t len, uint16_t idx, HALAnalogSample_t *avg)
{
    int32_t _avg = 0;
    int32_t startIdx, endIdx;    
    startIdx    = MAX(0, (idx - LOCALAVG_WINDOW_HALFSIZE_SAMPLES));
    endIdx      = MIN(len, (idx + LOCALAVG_WINDOW_HALFSIZE_SAMPLES));
    
    if((idx - LOCALAVG_WINDOW_HALFSIZE_SAMPLES < 0))
    {
        endIdx += (LOCALAVG_WINDOW_HALFSIZE_SAMPLES - idx);
    }

    for(uint16_t i = startIdx; i < endIdx; ++i)
    {
        _avg += buffer[i];
    }

    _avg /= ((endIdx - startIdx) + 1);
    *avg = (HALAnalogSample_t)_avg;
}

/*
void mldsp_feedSamples(HALAnalogSample_t* buffer, uint16_t len)
{
    //runningAverage
//    Initially:
//
//    average = 0
//    counter = 0
//    For each value:
//
//    counter += 1
//    average = average + (value - average) / min(counter, FACTOR)

    static uint32_t ts = 0;
    bool isUp = false;

    for(uint16_t i = 0; i < len; ++i)
    {        
        HALAnalogSample_t cur = buffer[i];
        HALAnalogSample_t last = 0;
        
        if(i == 0)
        {
            last = s_mldsp.lastSample;
            s_mldsp.lastSample = buffer[len-1];
        } else {
            last = buffer[i-1];
        }        

        int32_t diff = cur - last;
        
        if(diff <= 0)
        {
            diff = -diff;
            isUp = false;
        } else {
            isUp = true;
        }

        HALAnalogSample_t min, max;

        mldsp_maxMinWindow(buffer, len, i, &min, &max);

        uint16_t th = (max - min) / 3;

        bool edgeDetected = false;

        if(s_mldsp.nextExpectedEdgeIsUp == isUp)
        {
            if(diff > th)
            {
                edgeDetected = true;
                if(!isUp)
                {                
                    mldec_WaveCycle_t w = {.h = s_mldsp.timeStamp - s_mldsp.risingEdgeTimeStamp, .l = s_mldsp.risingEdgeTimeStamp};
                    mldsp_onWaveCycle(w);
                    s_mldsp.timeStamp = 0;
                    s_mldsp.nextExpectedEdgeIsUp = true;                    
                } else {
                    s_mldsp.risingEdgeTimeStamp = s_mldsp.timeStamp;
                    s_mldsp.nextExpectedEdgeIsUp = false;
                }
            }
        }

        s_mldsp.timeStamp += SAMPLE_INTERVAL_MS;

        //printf("%d %d %d %d %d %d\n", ts, cur, diff, edgeDetected ? (isUp ? 10000 : -10000) : 0, max, min);

        printf("%d %d %d\n", ts, cur, diff);
        ts += SAMPLE_INTERVAL_MS;

        s_mldsp.lastMin = min;
        s_mldsp.lastMax = max;
    }
    
}
*/

void mldsp_feedSamples(HALAnalogSample_t* buffer, uint16_t len)
{
    static uint32_t ts = 0;
    bool isUp = false;

    for(uint16_t i = 0; i < len; ++i)
    {        
        HALAnalogSample_t cur = buffer[i];

        mldsp_movingAvg(buffer, len, i, &cur);  // TEST        
        HALAnalogSample_t last = 0;
        
        if(i == 0)
        {
            last = s_mldsp.lastSample;
            //s_mldsp.lastSample = buffer[len-1];
            mldsp_movingAvg(buffer, len, len-1, &s_mldsp.lastSample);  // TEST
        } else {
            mldsp_movingAvg(buffer, len, i-1, &last);  // TEST
            //last = buffer[i-1];
        }        

        int32_t diff = cur - last;
        
        if(diff <= 0)
        {
            diff = -diff;
            isUp = false;
        } else {
            isUp = true;
        }

        HALAnalogSample_t min, max;

        mldsp_maxMinWindow(buffer, len, i, &min, &max);

        uint16_t th = (max - min) / 8;

        uint8_t edgeDirection = 0;

        if(max - min > 200)
        {
            switch(s_mldsp.state)
            {
                case MDS_EXPECTINGDOWN:
                {
                    if(!isUp && (diff > th))   // Slope large enough?
                    {
                        uint32_t totalTime = s_mldsp.timeStamp /*- SAMPLE_INTERVAL_MS*/;
                        mldec_WaveCycle_t w = {.h = totalTime - s_mldsp.risingEdgeTimeStamp, .l = s_mldsp.risingEdgeTimeStamp};
                        mldsp_onWaveCycle(w);

                        s_mldsp.state = MDS_DOWN;
                        s_mldsp.timeStamp = SAMPLE_INTERVAL_MS; // Assume we're one sample late
                    }
                }
                break;
                case MDS_DOWN:
                {
                    if((!isUp && (diff <= th)) || isUp)
                    {
                        s_mldsp.state = MDS_EXPECTINGUP;
                    }
                }
                break;
                case MDS_EXPECTINGUP:
                {
                    if(isUp)
                    {
                        if(diff > th)
                        {
                            s_mldsp.state = MDS_UP;
                        }
                    }                
                }
                break;
                case MDS_UP:
                {
                    if((isUp && (diff <= th)) || !isUp)                 
                    {
                        s_mldsp.state = MDS_EXPECTINGDOWN;
                        s_mldsp.risingEdgeTimeStamp = s_mldsp.timeStamp - SAMPLE_INTERVAL_MS;
                    }
                }
                break;
            }
        }
        s_mldsp.timeStamp += SAMPLE_INTERVAL_MS;

        //printf("%d %d %d %d %d %d\n", ts, cur, diff, (uint32_t)s_mldsp.state * 1000, max, min);
#ifndef _MLINK_OUTPUT
        printf("%d %d %d\n", ts, cur, (uint32_t)s_mldsp.state * 1000);        
#endif
        ts += SAMPLE_INTERVAL_MS;
        s_mldsp.lastMin = min;
        s_mldsp.lastMax = max;
    }    
}
