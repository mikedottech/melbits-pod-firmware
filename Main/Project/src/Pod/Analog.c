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

#include "common.h"
#include "mbt_config.h"
#include "Analog.h"
#include "HAL/HAL.h"
#include "HAL/Debug.h"
#include "Pod/Power.h"
#include "utils.h"
#include <string.h>

// This module receives the multiplexed analog stream, demultiplexes
// the data and routes it to the corresponding modules

#define POD_ANALOG_AVDD_MASK            ((1 << POD_ANALOG_OFFSET_TEMP) | (1 << POD_ANALOG_OFFSET_LIGHT) | (1 << POD_ANALOG_OFFSET_ML))

typedef struct
{
    Pod_analogClientState_t         clientState[POD_ANALOG_CLIENTS_COUNT];
    const Pod_analogClientMap_t*    clientMap;
    uint8_t                         enabledOffsets;
    MBT_TIMER_DEFINE(avddDelayTimer);
} Pod_analogState_t;

static Pod_analogState_t s_Pod_analogState;

// When the timer elapses, it's auto-disabled (i.e: set to MBT_TIMER_INVALID_VALUE)
// It either means that the timeout has elapsed, or that it's disabled
// In either case we can safely use the samples from AVDD-controlled analog sources
#define AVDD_DELAY_ELAPSED (s_Pod_analogState.avddDelayTimer_timer == MBT_TIMER_INVALID_VALUE)

void Pod_analogInit(void)
{
    memset(&s_Pod_analogState, 0, sizeof(s_Pod_analogState));
    MBT_TIMER_DISABLE(s_Pod_analogState.avddDelayTimer);
}

void Pod_analogSetClientsMap(const Pod_analogClientMap_t* pMap)
{
    MBT_ASSERT(pMap->nEntries <= POD_ANALOG_CLIENTS_COUNT);
    s_Pod_analogState.clientMap = pMap;
}

void Pod_analogTick(uint32_t ticks)
{
    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_analogState.avddDelayTimer, ticks,
        {
            // TODO: Disable power client?
            MBT_LOG("[Analog] AVDD delay expired\n");
        }
    )

    if(s_Pod_analogState.enabledOffsets) Pod_powerSetActivity(PWR_ACT_ANALOG);
    else Pod_powerClearActivity(PWR_ACT_ANALOG);
}

void Pod_analogFeedStream(HAL_analogSample_t *pData, uint16_t len)
{
    // Process stream, fill buffers and dispatch callbacks

        
    const Pod_analogClientConfig_t* pCfg = &s_Pod_analogState.clientMap->entries[0];
    Pod_analogClientState_t* pSt = &s_Pod_analogState.clientState[0];

    for(uint8_t j = 0; j < s_Pod_analogState.clientMap->nEntries; ++j, ++pSt, ++pCfg)
    {   
        uint8_t offsetMsk = 1 << pCfg->offset;

        // Depends on AVDD but the delay hasn't yet elapsed?
        if((offsetMsk & POD_ANALOG_AVDD_MASK) &&
            !AVDD_DELAY_ELAPSED)
        {
            // Skip
            continue;
        }

        switch(pSt->mode)
        {
            case ACM_IDLE:
                continue;
            case ACM_STREAM_1X:
            case ACM_STREAM_0_5X:
            case ACM_STREAM_0_25X:
            case ACM_STREAM_0_125X:
            {
                // 1X = 1, 0.5X = 2, 0.25X = 4, 0.125X = 8
                uint8_t skip = (1 << (pSt->mode - (uint8_t)ACM_STREAM_1X));
                
                for(uint16_t i = pCfg->offset; i < len; i += POD_ANALOG_STRIDE_SAMPLES)
                {
                    MBT_ASSERT(i < len);
                    if(++pSt->sampleCounter >= skip)
                    {
                        HAL_analogSample_t smp = MAX(0, pData[i]);
                        // Append to buffer
                        MBT_ASSERT(pSt->bufferIndex < pCfg->bufferSize);
                        pCfg->pBuffer[pSt->bufferIndex ++] = smp;
                        pSt->sampleCounter = 0;
                        if(pSt->bufferIndex >= pCfg->bufferSize)
                        {
                            if(pCfg->cb) pCfg->cb(j, pCfg->pBuffer, pCfg->bufferSize);
                            pSt->bufferIndex = 0;
                        }
                    }
                }                
            }
            break;
            case ACM_SINGLESHOT:
            // Average all samples and call callback
            {
                if(++pSt->sampleCounter >= 2)
                {
                    pSt->sampleCounter = 0;
                    uint32_t avg = 0;
                    uint16_t avgSamples = (len / POD_ANALOG_STRIDE_SAMPLES);
                    for(uint16_t i = pCfg->offset; i < len; i += POD_ANALOG_STRIDE_SAMPLES)
                    {
                        MBT_ASSERT(i < len);
                        avg += MAX(0, pData[i]);
                    }
                    avg /= avgSamples;
                    pCfg->pBuffer[0] = avg;
                    if(pCfg->cb) pCfg->cb(j, pCfg->pBuffer, 1);
                    // Disable client
                    Pod_analogDisableClient(j);
                }
            }
            break;
        }
    }
}

static void _Pod_analogDriveAVDD(bool enabled)
{
    MBT_LOG("[Analog] AVDD -> %s\n", enabled ? "ON" : "OFF");
    
    // AVDD is active low
    HAL_gpioWriteDigital(MBT_CFG_PIN_AVDD, !enabled);
}

static void _Pod_analogDriveADC(bool enabled)
{
    MBT_LOG("[Analog] ADC -> %s\n", enabled ? "ON" : "OFF");
    
    HAL_analogSetEnabled(enabled);
}


static void _Pod_analogManageSharedResources(uint8_t oldOffsets, uint8_t newOffsets)
{
    if(newOffsets != oldOffsets)
    {
        bool neededAVDD = !!(oldOffsets & POD_ANALOG_AVDD_MASK);
        bool needsAVDD = !!(newOffsets & POD_ANALOG_AVDD_MASK);
        
        bool needsDelay = false;

        if(neededAVDD != needsAVDD)
        {
            needsDelay = needsAVDD;
            _Pod_analogDriveAVDD(needsAVDD);            
        }

        if(needsDelay)
        {
            // TODO: Enable power client?
            MBT_TIMER_RESET_TIMEOUT(s_Pod_analogState.avddDelayTimer, MBT_US_TO_TICKS(MBT_CFG_ANALOG_AVDD_DELAY_MS * 1000));
        }

        if((oldOffsets == 0) ^ (newOffsets == 0))
            _Pod_analogDriveADC(!!(newOffsets));
    }
}

static uint8_t _Pod_analogComputeeUsedOffsets()
{
    uint8_t usedOffsets = 0;
    for(uint8_t i = 0; i < s_Pod_analogState.clientMap->nEntries; ++i)
    {
        const Pod_analogClientConfig_t* pCfg = &s_Pod_analogState.clientMap->entries[i];
        Pod_analogClientState_t* pSt = &s_Pod_analogState.clientState[i];
        if(pSt->mode != ACM_IDLE)
        {
            MBT_FLAG_SET(usedOffsets, 1 << pCfg->offset);
        }
    }

    return usedOffsets;    
}

void Pod_analogSetClientMode(uint8_t clientIndex, Pod_analogClientMode_t mode)
{
    MBT_ASSERT(clientIndex < POD_ANALOG_CLIENTS_COUNT);
    MBT_ASSERT(s_Pod_analogState.clientMap);
    MBT_ASSERT(clientIndex < s_Pod_analogState.clientMap->nEntries);
    
    uint8_t lastEnabledOffsets  = s_Pod_analogState.enabledOffsets;
    
    //const Pod_analogClientConfig_t* pCfg = &s_Pod_analogState.clientMap->entries[clientIndex];

    Pod_analogClientState_t *pCs = &s_Pod_analogState.clientState[clientIndex];

    pCs->mode = mode;
    pCs->bufferIndex = 0;
    pCs->sampleCounter = 0;

    uint8_t newEnabledOffsets = _Pod_analogComputeeUsedOffsets();

    _Pod_analogManageSharedResources(lastEnabledOffsets, newEnabledOffsets);

    s_Pod_analogState.enabledOffsets = newEnabledOffsets;
    HAL_sysSchedulePendingTick();
}
