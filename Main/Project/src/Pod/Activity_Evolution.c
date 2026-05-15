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

#include "Pod/Activity_Evolution.h"
#include "Pod/Vibration.h"
#include "Pod/Analog.h"
#include "Pod/Activity.h"
#include "HAL/Debug.h"
#include "Pod/LEDs.h"
#include "Pod/Power.h"
#include "Pod/Motion.h"
#include "Pod/Storage.h"
#include "Pod/BLE.h"
#include "Pod/EventServer.h"

#include "Pod/UI.h"
#include "utils.h"

enum
{
    ESE_ACT_EVO_NEWSAMPLE = ESE_ACT_BASE    
};

// TODO:
// - Shake while inc in progress (random?)
// - Tap / dbltap

#define MBT_CFG_ACT_EVO_SHAKE_TIME_MS (2300)

static Pod_ActMelbitDNAFile_t   s_melbitDNA;
static Pod_ActEvoFile_t         s_Pod_ActEvoRecipe;

#define MBT_ACT_EVO_MOVEMENT_FLAG_SHAKE     (0x1)
#define MBT_ACT_EVO_MOVEMENT_FLAG_MOVE      (0x2)
#define MBT_ACT_EVO_INTERNAL_FLAG_WAITSMP_L (0x4)
#define MBT_ACT_EVO_INTERNAL_FLAG_WAITSMP_T (0x8)
#define MBT_ACT_EVO_INTERNAL_FLAG_END       (0x10)


typedef struct
{
    MBT_TIMER_DEFINE(evoTimer);
    Pod_ActEvoStatus_t  fsm;
    //uint16_t            nRemainingSamples;
    uint16_t            nTakenSamples;
    uint8_t             internalFlags;
    uint32_t            movementTime_ms;
    uint8_t             progress_percent;
    uint32_t            accumTempUS;
} Pod_ActEvoState_t;

static Pod_ActEvoState_t s_Pod_ActEvoState;
Pod_ActEvoReportFile_t g_Pod_ActEvoReportFile;

static void _Pod_ActEvoResetSampleTimer(void)
{
    MBT_TIMER_RESET_TIMEOUT(s_Pod_ActEvoState.evoTimer, MBT_US_TO_TICKS(s_Pod_ActEvoRecipe.sampleInterval_s * 1000000));
}

static void _Pod_ActEvoTakeNextSample(void)
{
    if(s_Pod_ActEvoState.nTakenSamples < s_Pod_ActEvoRecipe.nSamples)
    {
        _Pod_ActEvoResetSampleTimer();
        Pod_LEDsSetMute(true);
        Pod_analogSetClientMode(POD_ACT_LIGHT_ANALOG_CH_ID, ACM_SINGLESHOT);
        Pod_analogSetClientMode(POD_ACT_TEMP_ANALOG_CH_ID, ACM_SINGLESHOT);
        MBT_FLAG_SET(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_INTERNAL_FLAG_WAITSMP_L);
        MBT_FLAG_SET(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_INTERNAL_FLAG_WAITSMP_T);
    }
}

static void _Pod_ActEvoUpdateProgress(void)
{
    uint8_t prog = ((s_Pod_ActEvoState.nTakenSamples) * 100) / s_Pod_ActEvoRecipe.nSamples;
    
    Pod_UISetUserParam(prog);



    if((s_Pod_ActEvoState.progress_percent / 25) != (prog / 25))
    {   
        //Pod_UIPlaySystemClip(SC_INCUBATION_REPORTPROGRESSAUTO);
        Pod_UIPlaySystemClip(SC_INCUBATION_REPORTPROGRESS);
    }
    s_Pod_ActEvoState.progress_percent = prog;
}

static void _Pod_ActEvoFreeUpResources(void)
{    
    Pod_motionSetMinimumLevel(ACCEL_LEVEL_DORMANT);
    Pod_LEDsSetMute(false);
    Pod_powerClearActivity(PWR_ACT_INC);
}

static void _Pod_ActEvoResetReportFile(void)
{
    memset(&g_Pod_ActEvoReportFile, 0, sizeof(g_Pod_ActEvoReportFile));
    g_Pod_ActEvoReportFile.magic    = POD_EVOLUTION_REPORT_FILE_MAGIC;
    g_Pod_ActEvoReportFile.version  = POD_EVOLUTION_FILE_VERSION;
}

void Pod_ActEvoTransitionToState(Pod_ActEvoStatus_t nextState)
{
    //if(s_Pod_ActEvoState.fsm == nextState) return;   

    switch(nextState)
    {
        case ES_READY:
            MBT_FLAG_CLEAR(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_MOVEMENT_FLAG_SHAKE | MBT_ACT_EVO_INTERNAL_FLAG_END | MBT_ACT_EVO_INTERNAL_FLAG_WAITSMP_L | MBT_ACT_EVO_INTERNAL_FLAG_WAITSMP_T);
            MBT_TIMER_RESET_TIMEOUT(s_Pod_ActEvoState.evoTimer, MBT_US_TO_TICKS(MBT_CFG_ACT_EVO_SHAKE_TIME_MS * 1000));
        break;
        case ES_WAITSTART:
            Pod_powerSetActivity(PWR_ACT_INC);
            Pod_UIPlaySystemClip(SC_SHAKE_START_SUCCESS);
            MBT_TIMER_RESET_TIMEOUT(s_Pod_ActEvoState.evoTimer, MBT_US_TO_TICKS(5000000)); // Wait 5 seconds before starting incubation
        break;
        case ES_ONGOING:
            s_Pod_ActEvoState.nTakenSamples = 0;
            _Pod_ActEvoResetReportFile();
            Pod_motionSetMinimumLevel(ACCEL_LEVEL_2);            
            _Pod_ActEvoResetSampleTimer();
            _Pod_ActEvoTakeNextSample();
            Pod_BLESetInactiveAdvertisementRetries(5);
            Pod_UISetUserParam(0);
            Pod_UIPlaySystemClip(SC_INCUBATION_REPORTPROGRESS);
            if(Pod_motionIsActivity())
                MBT_FLAG_SET(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_MOVEMENT_FLAG_MOVE);
        break;
        case ES_FINISHED:
            Pod_UIPlaySystemClip(SC_INCUBATION_COMPLETE);
            _Pod_ActEvoFreeUpResources();

            {
                MBT_LOG("   MOV = %d\n", g_Pod_ActEvoReportFile.movementTime_ms);
                for(uint16_t i = 0; i < s_Pod_ActEvoRecipe.nSamples; i++)
                {
                    MBT_LOG("   L=%d, T=%d\n",  g_Pod_ActEvoReportFile.samples[i].light,
                                                g_Pod_ActEvoReportFile.samples[i].temp);
                }
            }

            Pod_BLESetInactiveAdvertisementRetries(0);
        break;
    }
    
    if(nextState < ES_WAITSTART)
    {
        Pod_powerClearActivity(PWR_ACT_INC);
    }

    if(s_Pod_ActEvoState.fsm != nextState)
        _Pod_ActivityStateChanged();

    s_Pod_ActEvoState.fsm = nextState;
    
}

uint8_t Pod_ActEvoStart(void)
{
    memset(&s_Pod_ActEvoState, 0, sizeof(s_Pod_ActEvoState));
    
    _Pod_ActEvoResetReportFile();

    Pod_ActEvoTransitionToState(ES_EMPTY);

    const Pod_storageFD_t *fd_melbit = Pod_storageGetFD(0x200);
    const Pod_storageFD_t *fd_evolution = Pod_storageGetFD(0x201);

    if(!fd_melbit || !fd_evolution) return ACT_EVO_ERR_MISSING_FILE;
    
    if(fd_melbit->size < sizeof(Pod_ActMelbitDNAFile_t)) return ACT_EVO_ERR_INVALID_DNA;

    const uint8_t *ptrMelbit = Pod_storageGetPtr(fd_melbit);

    const Pod_ActMelbitDNAFile_t *pMelbitFile = (Pod_ActMelbitDNAFile_t *)ptrMelbit;

    if(pMelbitFile->magic != POD_MELBITDNA_FILE_MAGIC) return ACT_EVO_ERR_INVALID_DNA;

    s_melbitDNA = *pMelbitFile;

    const uint8_t *ptrEvo = Pod_storageGetPtr(fd_evolution);
    
    if(fd_evolution->size < sizeof(Pod_ActEvoFile_t)) return ACT_EVO_ERR_INVALID_RECIPE;

    const Pod_ActEvoFile_t *pEvoFile = (Pod_ActEvoFile_t *)ptrEvo;

    if(pEvoFile->magic != POD_EVOLUTION_FILE_MAGIC) return ACT_EVO_ERR_INVALID_RECIPE;

    if(pEvoFile->nSamples > MBT_CFG_ACT_EVO_BUFFER_SIZE_SAMPLES) return ACT_EVO_ERR_INVALID_RECIPE;

    s_Pod_ActEvoRecipe = *pEvoFile;

    Pod_UISetUserSlotColor(&s_melbitDNA.color);

    Pod_ActEvoTransitionToState(ES_READY);
    
    return ACT_EVO_ERR_OK;
}

void Pod_ActEvoStop(void)
{
    _Pod_ActEvoFreeUpResources();
    Pod_UISetUserSlotColor(&k_Pod_LEDColorBlack);
}


void Pod_ActEvoTick(uint32_t ticks)
{
    switch(s_Pod_ActEvoState.fsm)
    {
        case ES_READY:
        {
            if(MBT_FLAG_IS_SET(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_MOVEMENT_FLAG_SHAKE))
            {
                MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_ActEvoState.evoTimer, ticks, 
                    {                        
                        Pod_ActEvoTransitionToState(ES_WAITSTART);
                    }
                );
            }
        }
        break;
        case ES_WAITSTART:
            MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_ActEvoState.evoTimer, ticks, 
                {
                    Pod_ActEvoTransitionToState(ES_ONGOING);
                }
            );
        break;
        case ES_ONGOING:
                if(MBT_FLAG_IS_SET(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_INTERNAL_FLAG_END))
                {
                    if(!MBT_FLAG_IS_SET(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_INTERNAL_FLAG_WAITSMP_L | MBT_ACT_EVO_INTERNAL_FLAG_WAITSMP_T))
                    {
                        MBT_FLAG_CLEAR(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_INTERNAL_FLAG_END);
                        Pod_ActEvoTransitionToState(ES_FINISHED);
                    }
                } else {
                    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_ActEvoState.evoTimer, ticks, 
                        {
                            _Pod_ActEvoTakeNextSample();                            
                        }
                    );
                }
                if(MBT_FLAG_IS_SET(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_MOVEMENT_FLAG_MOVE))
                {
                    s_Pod_ActEvoState.accumTempUS += MBT_TICKS_TO_US(ticks);
                    uint32_t ms = s_Pod_ActEvoState.accumTempUS / 1000;
                    if(ms > 0)
                    {
                        g_Pod_ActEvoReportFile.movementTime_ms += ms;
                        s_Pod_ActEvoState.accumTempUS -= ms * 1000;
                    }                    
                    //MBT_LOG("MOVEMENT TIME: %d\n", g_Pod_ActEvoReportFile.movementTime_ms);
                }
                // Report progress right after getting the sample to avoid disrupting the report animation by muting the LEDs, which is
                // necessary to avoid contamination of the light sample
                if(!MBT_FLAG_IS_SET(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_INTERNAL_FLAG_WAITSMP_L))
                {
                    _Pod_ActEvoUpdateProgress();
                }
        break;
    }
}

void Pod_ActEvoMotionHandler(const Pod_motionEventData_t *pData)
{
    if(pData->type == MET_SHAKE)
    {
        bool bShaking = false;

            switch(pData->data.shake.state)
            {
                case SHK_STARTED:
                case SHK_PEAK:
                    bShaking = true;
                break;
                case SHK_STOPPED:
                    bShaking = false;
                break;                
                default: break;
            }
            if(bShaking)
            {
                if(!MBT_FLAG_IS_SET(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_MOVEMENT_FLAG_SHAKE))
                {
                    if(s_Pod_ActEvoState.fsm == ES_READY)
                    {
                        Pod_UIStopSystemClip(SC_SHAKE_START_ERROR);
                        Pod_UIPlaySystemClip(SC_SHAKE_START);
                    } else if(s_Pod_ActEvoState.fsm == ES_FINISHED)
                    {
                        Pod_UIPlaySystemClip(SC_INCUBATION_COMPLETE);
                    }
                }
                MBT_FLAG_SET(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_MOVEMENT_FLAG_SHAKE);
            } else {
                if(s_Pod_ActEvoState.fsm == ES_READY)
                {                    
                    Pod_UIPlaySystemClip(SC_SHAKE_START_ERROR);
                    Pod_ActEvoTransitionToState(ES_READY);
                }
                MBT_FLAG_CLEAR(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_MOVEMENT_FLAG_SHAKE);
            }
        

        if(pData->data.shake.state == SHK_PEAK)
        {
            Pod_vibSetVibration(s_melbitDNA.weight, 20);
        }
        
    } else if(pData->type == MET_ACTIVITY)
    {
        if(pData->data.activity.moving)
        {
            MBT_FLAG_SET(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_MOVEMENT_FLAG_MOVE);
        } else {
            MBT_FLAG_CLEAR(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_MOVEMENT_FLAG_MOVE);
        }
    }
}

void Pod_ActEvoAnalogHandler(uint8_t clientID, HAL_analogSample_t *pBuffer, uint8_t count)
{
    switch(clientID)
    {
        case POD_ACT_LIGHT_ANALOG_CH_ID:
            MBT_LOG("[EVO] L=%d\n", (uint32_t)pBuffer[0]);
            MBT_FLAG_CLEAR(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_INTERNAL_FLAG_WAITSMP_L);
            Pod_LEDsSetMute(false);
            g_Pod_ActEvoReportFile.samples[s_Pod_ActEvoState.nTakenSamples].light = pBuffer[0];
        break;
        case POD_ACT_TEMP_ANALOG_CH_ID:
            MBT_LOG("[EVO] T=%d\n", (uint32_t)pBuffer[0]);
            MBT_FLAG_CLEAR(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_INTERNAL_FLAG_WAITSMP_T);
            g_Pod_ActEvoReportFile.samples[s_Pod_ActEvoState.nTakenSamples].temp = pBuffer[0];
        break;
    }

    if(!MBT_FLAG_IS_SET(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_INTERNAL_FLAG_WAITSMP_L | MBT_ACT_EVO_INTERNAL_FLAG_WAITSMP_T))
    {
        Pod_ESNotifyEventSource(ESE_ACT_EVO_NEWSAMPLE);
        // Both samples taken
        ++ s_Pod_ActEvoState.nTakenSamples;
        g_Pod_ActEvoReportFile.nSamples = s_Pod_ActEvoState.nTakenSamples;
        if(s_Pod_ActEvoState.nTakenSamples == s_Pod_ActEvoRecipe.nSamples ||
           s_Pod_ActEvoState.nTakenSamples >= MBT_CFG_ACT_EVO_BUFFER_SIZE_SAMPLES)
        {            
            MBT_FLAG_SET(s_Pod_ActEvoState.internalFlags, MBT_ACT_EVO_INTERNAL_FLAG_END);
            HAL_sysSchedulePendingTick();
        }
    }
    return;
}

uint16_t Pod_ActEvoGetState(void)
{
    return (uint16_t)s_Pod_ActEvoState.fsm;
}

uint16_t _Pod_ActFileCB(uint16_t id, uint8_t evt)
{    
    return sizeof(Pod_ActEvoReportFile_t) - sizeof(Pod_ActEvoSample_t) * (MBT_CFG_ACT_EVO_BUFFER_SIZE_SAMPLES - s_Pod_ActEvoState.nTakenSamples);
}
