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

#include "Pod/MLINK.h"
#include "Pod/Analog.h"
#include "HAL/HAL.h"

#include "ML/mlink_dsp.h"
#include "ML/MLDecoder.h"
#include "utils.h"
#include "Pod/Power.h"

//#define TIMER_CARRIER_TIMEOUT   MBT_US_TO_TICKS(550   * 1000)     // Max period of the signal: 4 frames (0.26s @ 15 fps) => Carrier lost [Allow up to 8 frames]
#define TIMER_CARRIER_TIMEOUT    /*MBT_US_TO_TICKS(2550   * 1000)*/ MBT_US_TO_TICKS(650 * 1000)     // Max period of the signal: 4 frames (0.26s @ 15 fps) => Carrier lost [Allow up to 8 frames]
//#define TIMER_SOFT_TIMEOUT      MBT_US_TO_TICKS(15000 * 1000)     // Shutdown ML if no light activity is detected after this interval
#define TIMER_HARD_TIMEOUT      MBT_US_TO_TICKS(MBT_CFG_ML_TIMEOUT_MS * 1000)     // Shutdown ML if no valid ML prologs have been detected after this interval even if there is light activity

HAL_analogSample_t s_Pod_MLSampleBuffer[MBT_CFG_ML_SAMPLE_BUFFER_SIZE_SAMPLES];

typedef struct
{
    bool hasCarrier;
    bool enabled;
    uint8_t nWaveCycles;
    MBT_TIMER_DEFINE(carrierTimer);
    MBT_TIMER_DEFINE(hardTimer);
    //MBT_TIMER_DEFINE(softTimer);
} Pod_MLState_t;

static Pod_MLState_t s_Pod_MLState;

static void _Pod_MLReset(void)
{
    memset(&s_Pod_MLState, 0, sizeof(s_Pod_MLState));
    mldec_reset();
    mldsp_reset();
    MBT_TIMER_DISABLE(s_Pod_MLState.carrierTimer);
    MBT_TIMER_DISABLE(s_Pod_MLState.hardTimer);
}

void Pod_MLInit(void)
{
    _Pod_MLReset();
}

static void _Pod_MLHardwareSwitch(bool enabled)
{
    Pod_analogSetClientMode(POD_POWER_ML_SENSOR_CH_ID, enabled ? /*ACM_STREAM_0_25X*/ ACM_STREAM_1X : ACM_IDLE);
}

static void _Pod_MLUpdateCarrierState(bool state)
{
    if(state == s_Pod_MLState.hasCarrier) return;

    s_Pod_MLState.hasCarrier = state;

    const Pod_MLEventData_t e =
    {
        .type = MLE_SIGNAL,
        .data = {
            .signal = {
                .locked = state
            }                    
        }
    };

    if(!state)
    {
        mldsp_reset();
        MBT_TIMER_DISABLE(s_Pod_MLState.carrierTimer);        
    }

    Pod_MLEventHandler(&e);
}

static void _Pod_MLSessionStartFinish(bool start)
{
    if(s_Pod_MLState.enabled == start)
        return;

    if(start)
    {
        _Pod_MLReset();
        MBT_TIMER_RESET_TIMEOUT(s_Pod_MLState.hardTimer, TIMER_HARD_TIMEOUT);
    } else {
        _Pod_MLUpdateCarrierState(false);
    }
    s_Pod_MLState.enabled = start;

    _Pod_MLHardwareSwitch(start);

    // Report power state
    if(s_Pod_MLState.enabled)
        Pod_powerSetActivity(PWR_ACT_MLINK);
    else
        Pod_powerClearActivity(PWR_ACT_MLINK);
}

void Pod_MLTick(uint32_t ticks)
{
    if(!s_Pod_MLState.enabled)
        return;

    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_MLState.carrierTimer, ticks,
        {
            _Pod_MLUpdateCarrierState(false);
        }
    );
    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_MLState.hardTimer, ticks,
        {
            _Pod_MLSessionStartFinish(false);
        }
    );
}

void _Pod_MLAnalogCallback(uint8_t clientID, HAL_analogSample_t *pBuffer, uint8_t count)
{
    if(!s_Pod_MLState.enabled)
        return;
    mldsp_feedSamples(pBuffer, count);
}

void mldsp_onWaveCycle(mldec_WaveCycle_t cycle)
{   
    if(s_Pod_MLState.hasCarrier)
    {
        if(++ s_Pod_MLState.nWaveCycles > 14)
        {
            _Pod_MLUpdateCarrierState(false);
        } else {
            MBT_TIMER_RESET_TIMEOUT(s_Pod_MLState.carrierTimer, TIMER_CARRIER_TIMEOUT);
        }
    }
    mldec_decode(cycle);
}

void mldec_onProlog()
{
    s_Pod_MLState.nWaveCycles = 0;
    _Pod_MLUpdateCarrierState(true);
    MBT_TIMER_RESET_TIMEOUT_MAX(s_Pod_MLState.hardTimer, TIMER_HARD_TIMEOUT);
    MBT_TIMER_RESET_TIMEOUT(s_Pod_MLState.carrierTimer, TIMER_CARRIER_TIMEOUT);
    //MBT_LOG("[ML] PRO\n");
}

void mldec_onByte(uint8_t data)
{
    MBT_TIMER_RESET_TIMEOUT_MAX(s_Pod_MLState.hardTimer, TIMER_HARD_TIMEOUT);
    
    _Pod_MLUpdateCarrierState(true);

    Pod_MLEventData_t e =
    {
        .type = MLE_DATA,
        .data =
        {
            .data = data
        }
    };
    Pod_MLEventHandler(&e);
    //MBT_LOG("[ML] BYTE 0x%x\n", data);
}

void mldec_onError(uint32_t errorType)
{    
    Pod_MLEventData_t e =
    {
        .type = MLE_ERROR
    };
    Pod_MLEventHandler(&e);
}

void Pod_MLEnable(bool state)
{
    _Pod_MLSessionStartFinish(state);
}

void Pod_MLSetHardTimeout(uint32_t timeout_ms)
{
    if(s_Pod_MLState.enabled && MBT_TIMER_ISENABLED(s_Pod_MLState.hardTimer))
    {
        MBT_TIMER_RESET_TIMEOUT(s_Pod_MLState.hardTimer, MBT_US_TO_TICKS(timeout_ms * 1000));
    }
}
