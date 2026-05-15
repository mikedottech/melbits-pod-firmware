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

#include "HAL/HAL.h"
#include "Pod/StreamServer.h"
#include "Pod/Motion.h"
#include "Pod/Analog.h"
#include "Pod/Playground.h"

#include "HAL/Debug.h"

#include "utils.h"

#define MBT_CFG_POD_STREAMSERVER_INTERVAL_MS (40)


HAL_analogSample_t g_Pod_SSLightBuffer;
HAL_analogSample_t g_Pod_SSTempBuffer;
HAL_analogSample_t g_Pod_SSMLBuffer;

typedef struct 
{
    Pod_PGCommonHeader_t        header;
    Pod_PGStrmPayloadPayload_t  strm;
    uint8_t                     samples[sizeof(HAL_accelSampleScaled_t) + sizeof(HAL_analogSample_t) * 2 - sizeof(HAL_analogSample_t)]; // We substract the size of an analog sample since it's already allocated in Pod_PGStrmPayloadPayload_t.firstSample    
}MBT_CFG_PLATFORM_PACKED Pod_SSSamplePacket_t;

typedef struct
{
    uint8_t mask;
    HAL_accelSampleScaled_t accelBuffer;
    bool                    pktPending;
    uint8_t                     sampleReportedMask;
    MBT_TIMER_DEFINE(streamTimer);
} Pod_SSState_t;

Pod_SSState_t s_Pod_SSState;

void Pod_SSInit(void)
{
    memset(&s_Pod_SSState, 0, sizeof(Pod_SSState_t));
    Pod_SSReset();
}


void Pod_SSReset(void)
{
    Pod_SSSetStreamMask(0);
}

static void _Pod_SSResetTimer(void)
{
    MBT_TIMER_RESET_TIMEOUT(s_Pod_SSState.streamTimer, MBT_US_TO_TICKS(MBT_CFG_POD_STREAMSERVER_INTERVAL_MS * 1000));
}

static void _Pod_SSOnStreamInterval(void)
{
    // All sensors read?
    if((s_Pod_SSState.sampleReportedMask & s_Pod_SSState.mask) == s_Pod_SSState.mask)
    {
        s_Pod_SSState.pktPending = true;
        Pod_SSTXSlotHandler();
#ifdef MBT_CFG_SS_WAIT_FOR_ALL_SAMPLES
        s_Pod_SSState.sampleReportedMask = 0;
#endif
    }
}

void Pod_SSTXSlotHandler(void)
{
    if(s_Pod_SSState.pktPending)
    {
        Pod_SSSamplePacket_t    pkt;

        // Assemble and send stream packet
        pkt.header.chan = POD_PG_CHANNEL_STREAM;
        pkt.strm.strmSources = s_Pod_SSState.mask;
        uint8_t *pData = (uint8_t *)&pkt.strm.firstSample;

        //MBT_LOG("[0x%x] ", s_Pod_SSState.mask);
        if(MBT_FLAG_IS_SET(s_Pod_SSState.mask, SS_MSK_ITEM_ACCEL))
        {
            memcpy(pData, &s_Pod_SSState.accelBuffer, sizeof(HAL_accelSampleScaled_t));
            pData += sizeof(HAL_accelSampleScaled_t);
            //MBT_LOG("%d, %d, %d ", (int32_t)s_Pod_SSState.accelBuffer.x, (int32_t)s_Pod_SSState.accelBuffer.y, (int32_t)s_Pod_SSState.accelBuffer.z);
        }
        if(MBT_FLAG_IS_SET(s_Pod_SSState.mask, SS_MSK_ITEM_LIGHT))
        {
            memcpy(pData, &g_Pod_SSLightBuffer, sizeof(HAL_analogSample_t));
            pData += sizeof(HAL_analogSample_t);
            //MBT_LOG("%d ", (int32_t)g_Pod_SSLightBuffer);
        }
        if(MBT_FLAG_IS_SET(s_Pod_SSState.mask, SS_MSK_ITEM_TEMP))
        {
            memcpy(pData, &g_Pod_SSTempBuffer, sizeof(HAL_analogSample_t));
            pData += sizeof(HAL_analogSample_t);
            //MBT_LOG("%d ", (int32_t)g_Pod_SSTempBuffer);
        }

        // MLINK takes precedence. If it's set, any other items are removed
        if(MBT_FLAG_IS_SET(s_Pod_SSState.mask, SS_MSK_ITEM_ML))
        {
            pkt.strm.strmSources = SS_MSK_ITEM_ML;
            pData = (uint8_t *)&pkt.strm.firstSample;
            memcpy(pData, &g_Pod_SSMLBuffer, sizeof(HAL_analogSample_t));
            pData += sizeof(HAL_analogSample_t);
        }
            
        uint8_t size = sizeof(Pod_PGCommonHeader_t) + sizeof(Pod_PGStrmPayloadPayload_t) + ((uint8_t)(pData - (uint8_t *)&pkt.strm.firstSample)) - sizeof(HAL_analogSample_t);
        //MBT_LOG("SIZE: %d\n", (uint8_t)size);
        // Send packet and invalidate buffer if OK
        if(Pod_PGSend((Pod_PGPDU_t *)&pkt, size) == 0)
        {
            s_Pod_SSState.pktPending = false;
        }
    }
}

void Pod_SSTick(uint32_t ticks)
{
    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_SSState.streamTimer, ticks,
        {
            _Pod_SSOnStreamInterval();
            _Pod_SSResetTimer();
        }
    );
}

static void _Pod_SSManageAnalogStream(uint8_t msk, uint8_t changes, uint8_t itemMsk, uint8_t analogID)
{
    if(MBT_FLAG_IS_SET(changes, itemMsk))
    {
        Pod_analogClientMode_t mode = ACM_IDLE;
        if(MBT_FLAG_IS_SET(msk, itemMsk))
        {       
            // Just added
            mode = ACM_STREAM_0_125X;
        }
        Pod_analogSetClientMode(analogID, mode);
    }
}

void Pod_SSSetStreamMask(uint8_t msk)
{
    msk &= (SS_MSK_ITEM_MAX - 1); //SS_MSK_ITEM_ALL;

    uint8_t prevMask = s_Pod_SSState.mask;    
    uint8_t changes = (msk ^ prevMask);

    if(MBT_FLAG_IS_SET(changes, SS_MSK_ITEM_ACCEL))
    {
        Pod_motionSetSubStreamEnabled(!!MBT_FLAG_IS_SET(msk, SS_MSK_ITEM_ACCEL));
    }

    _Pod_SSManageAnalogStream(msk, changes, SS_MSK_ITEM_LIGHT, POD_SS_LIGHT_ANALOG_CH_ID);
    _Pod_SSManageAnalogStream(msk, changes, SS_MSK_ITEM_TEMP, POD_SS_TEMP_ANALOG_CH_ID);
    _Pod_SSManageAnalogStream(msk, changes, SS_MSK_ITEM_ML, POD_SS_ML_ANALOG_CH_ID);

    if(msk == 0)
    {
        MBT_TIMER_DISABLE(s_Pod_SSState.streamTimer);
    } else if(prevMask == 0 && msk != 0)
    {
        _Pod_SSResetTimer();
    }

    s_Pod_SSState.mask = msk;
    
    HAL_sysSchedulePendingTick();
}

// Stream server ----------------------------------------------------------------
void Pod_motionsubStreamHandler(const HAL_accelSample_t *pData, uint16_t len)
{
    MBT_FLAG_SET(s_Pod_SSState.sampleReportedMask, SS_MSK_ITEM_ACCEL);
    HAL_accelScaleSample(pData, &s_Pod_SSState.accelBuffer);
}

// Analog client ----------------------------------------------------------------
void Pod_SSAnalogHandler(uint8_t clientID, HAL_analogSample_t *pBuffer, uint8_t count)
{
    uint8_t localID = (uint8_t)MAX(((int16_t)clientID - POD_SS_MIN_ANALOG_CH_ID) + 1, 0);
    MBT_FLAG_SET(s_Pod_SSState.sampleReportedMask, (1 << localID));
}
