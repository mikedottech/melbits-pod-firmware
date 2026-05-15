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

#include "UI.h"
#include "Audio/synth.h"
#include "Pod/LEDs.h"
#include "Pod/Vibration.h"
#include "HAL/Debug.h"
#include "Pod/UI_Battery.h"

#include "utils.h"

typedef struct
{
    uint16_t                currentPrio;
    Utils_RGB888_t          userSlotColor;
    Pod_UISystemClipID_t    currentId;
    uint32_t                param;
    uint16_t                fileID;
    MBT_TIMER_DEFINE        (testModeTimer);
    uint8_t                 test;
} Pod_UIState_t;

static Pod_UIState_t s_Pod_UIState;

extern const Pod_UISystemClipEntry_t k_PodUIClipPriorities[SC_COUNT];

// ------------------------------------------------------------------------------

void Pod_UIInit(void)
{
    synth_init();
    memset(&s_Pod_UIState, 0, sizeof(s_Pod_UIState));
    Pod_UIBattInit();
    MBT_TIMER_DISABLE(s_Pod_UIState.testModeTimer);
}

// ------------------------------------------------------------------------------
/*
static void _Pod_UISpecialFeedbackTick(uint32_t ticks)
{
    if(s_Pod_UIState.currentId == SC_INCUBATION_REPORTPROGRESS)
    {
        if(MBT_TIMER_ISENABLED(s_Pod_UIState.specialFBTimer))
        {
            if(MBT_TIMER_HASEXPIRED(s_Pod_UIState.specialFBTimer, ticks)
                || !synth_isPlaying())
            {
                MBT_TIMER_DISABLE(s_Pod_UIState.specialFBTimer);
                // Fade out all the LEDs
                for(uint8_t i = LED_Q1, i <= LED_Q4; ++i
                    Pod_LEDsSetLevel(i, 0, kLEDSpeedHalfSecond);
            } else {
                MBT_TIMER_UPDATE(s_Pod_UIState.specialFBTimer, ticks);
            }
        }
    }
}
*/
// ------------------------------------------------------------------------------

static bool _Pod_UIIsClipPlaying()
{
    return (synth_isPlaying() || Pod_LEDsAnimationsInProgress());
}

static void Pod_UITestModeUpdate(void)
{
    if(s_Pod_UIState.currentId == SC_LIGHTSENSORTEST)
    { 
        ++ s_Pod_UIState.test;
        MBT_TIMER_RESET_TIMEOUT(s_Pod_UIState.testModeTimer, MBT_US_TO_TICKS(1000000));
        const Utils_RGB888_t *pColor;
        if(s_Pod_UIState.test & 0x1)
        {
            pColor = &k_Pod_LEDColorWhite;
        } else {
            pColor = &k_Pod_LEDColorBlack;
        }
        Pod_LEDsCenterSetIdleColor(pColor);        
    }
}

// ------------------------------------------------------------------------------

void Pod_UITick(uint32_t ticks)
{
    synth_tick(ticks);

    if(!_Pod_UIIsClipPlaying())
    {
        s_Pod_UIState.currentId = SC_NONE;        // WARNING: This does not apply to LED-only feedbacks!
    }

    //_Pod_UISpecialFeedbackTick(ticks);

    Pod_UIBattTick(ticks);


    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_UIState.testModeTimer, ticks,
    {
        Pod_UITestModeUpdate();
    }
    );
}

// ------------------------------------------------------------------------------

void Pod_UIPlaySystemClip(Pod_UISystemClipID_t id)
{
    MBT_ASSERT(id < SC_COUNT);
    if(id >= SC_COUNT) return;
    const Pod_UISystemClipEntry_t *pEntry = &k_PodUIClipPriorities[id];
    Pod_UIPlaySystemClipPrio(id, pEntry->prio);
}

// ------------------------------------------------------------------------------

uint8_t Pod_UIGetCurrentClipID(void)
{
    return s_Pod_UIState.currentId;
}

// ------------------------------------------------------------------------------

bool Pod_UIPlayFilePrio(uint16_t id, uint16_t priority)
{
    s_Pod_UIState.fileID = id;
    Pod_UIPlaySystemClipPrio(SC_FILE, priority);
    return (s_Pod_UIState.fileID != 0);
}

// ------------------------------------------------------------------------------

static uint8_t _Pod_UIShowProgress(uint8_t progress_percent, uint8_t nHalfCycles)
{    
    uint8_t ledID = POD_LED_Q1;
    uint8_t t = 0;
    for(uint8_t i = 0; i < 4; ++i)
    {
        ledID = POD_LED_Q1 + i;
        t += 25;
        if(progress_percent >= t)
        {            
            //(percent % 25) * 255 / 25
            Pod_LEDsSetAnimation(ledID, 0, 255, nHalfCycles, k_Pod_LEDSpeedOneSecond);
        } else {
            break;
        }
    }
    return ledID;
}

// ------------------------------------------------------------------------------

void Pod_UIPlaySystemClipPrio(Pod_UISystemClipID_t id, uint16_t priority)
{
    MBT_ASSERT(id < SC_COUNT);
    if(id >= SC_COUNT) return;

    if(_Pod_UIIsClipPlaying())
    {
        if(priority > s_Pod_UIState.currentPrio)
        {
            return;
        }
    }
    
    s_Pod_UIState.currentPrio = priority;

    const Pod_UISystemClipEntry_t *pEntry = &k_PodUIClipPriorities[id];

    // Will only stop the current song if the pointer is NULL
    synth_playSong(pEntry->pClipData);

    // Special feedbacks
    switch(id)
    {
        case SC_ML_SYNC:
            Pod_LEDsCenterSetAnimation(&k_Pod_LEDColorBlack, &k_Pod_LEDColorWhite, 20, k_Pod_LEDSpeedHalfSecond);
        break;
        case SC_ML_CONNECT:
            Pod_LEDsCenterSetAnimation(&k_Pod_LEDColorBlack, &k_Pod_LEDColorWhite, 160, k_Pod_LEDSpeedSixteenthSecond);
        break;
        case SC_INCUBATION_REPORTPROGRESSAUTO:            
            _Pod_UIShowProgress((uint8_t)s_Pod_UIState.param, 2);
        break;
        case SC_INCUBATION_REPORTPROGRESS:
        {
            uint8_t blinkingLED = _Pod_UIShowProgress((uint8_t)s_Pod_UIState.param, 1);
            Pod_LEDsSetAnimation(blinkingLED, 0, 255, 10, k_Pod_LEDSpeedQuarterSecond);
            //MBT_TIMER_RESET_TIMEOUT(s_Pod_UIState.specialFBTimer, MBT_US_TO_TICKS(2200 * 1000));
        }
        break;
        case SC_EMPTY:
            Pod_LEDsCenterSetAnimation(&k_Pod_LEDColorBlack, &k_Pod_LEDColorWhite, 4, k_Pod_LEDSpeedQuarterSecond);
            Pod_vibSetVibration(150, 150);
        break;
        case SC_HWPANIC:
            Pod_LEDsCenterSetAnimation(&k_Pod_LEDColorBlack, &k_Pod_LEDColorPurple, 20, k_Pod_LEDSpeedQuarterSecond);
        break;
        case SC_FILE:
        {
            // TODO: Detect clip file or pcm file
            bool res = synth_playPCM(s_Pod_UIState.fileID);
            if(!res)
                s_Pod_UIState.fileID = 0;
                return;
        }
        break;
        /// FACTORY TESTING
        case SC_LEDSON:
            Pod_LEDsCenterSetAnimation(&k_Pod_LEDColorBlack, &k_Pod_LEDColorWhite, 1, k_Pod_LEDSpeedQuarterSecond);
            for(uint8_t i = POD_LED_Q1; i <= POD_LED_Q4; ++i)
                Pod_LEDsSetAnimation(i, 0, 255, 1, k_Pod_LEDSpeedQuarterSecond);
        break;
        case SC_LIGHTSENSORTEST:
            //Pod_LEDsCenterSetAnimation(&k_Pod_LEDColorBlack, &k_Pod_LEDColorWhite, 10, k_Pod_LEDSpeedOneSecond * 2);
            s_Pod_UIState.test = 0;
            Pod_LEDsSetAnimation(POD_LED_Q1, 0, 0, 1, 6000); // HACK
            MBT_TIMER_RESET_TIMEOUT(s_Pod_UIState.testModeTimer, 1);
        break;
        ///////////////////
        default:
        break;
    }

    s_Pod_UIState.currentId = id;
}

// ------------------------------------------------------------------------------

void Pod_UISetUserParam(uint32_t param)
{
    s_Pod_UIState.param = param;
}

// ------------------------------------------------------------------------------

void Pod_UISetUserSlotColor(const Utils_RGB888_t *pColor)
{
    s_Pod_UIState.userSlotColor = *pColor;
}

// ------------------------------------------------------------------------------

void Pod_UIStopCurrentClip(void)
{
    synth_stopSong();
}

void Pod_UIStopSystemClip(Pod_UISystemClipID_t id)
{
    if(s_Pod_UIState.currentId == id)
    {
        synth_stopSong();
    }
}
// ------------------------------------------------------------------------------

void synth_onTrackEvent(uint8_t channel, uint8_t event, uint8_t param)
{
    MBT_ASSERT(channel <= 7);
    
    // TODO: Recheck this
    if(event >= SYNTH_TRK_EVT_CMD_PLATPCM) return;
      
    if(param & 0x80)
    {
        // The upper bit set means the value is a user slot index

        // User slot
        param &= 0x7f;

        if(channel == 4) param = s_Pod_UIState.userSlotColor.r;
        else if(channel == 5) param = s_Pod_UIState.userSlotColor.g;
        else if(channel == 6) param = s_Pod_UIState.userSlotColor.b;
    } else {
        // The upper bit cleared means that the value is embedded in the instruction but divided by 2
        param <<= 1;    // Multiply by 2
        if(param == 0xfe) 
            param = 0xff;
    }

    if(channel == 7)
    {
        if(event == SYNTH_TRK_EVT_PWM_SET_LEVEL_ABS)
        {
            Pod_vibSetVibration(param, 200);
        }
    } else {
        switch (event)
        {
            case SYNTH_TRK_EVT_PWM_FADEIN:        
                Pod_LEDsSetAnimation(channel, 0, param, 1, 500);            
            break;
            case SYNTH_TRK_EVT_PWM_FADETO_ABS:
                Pod_LEDsSetLevel(channel, param, 500);
            break;
            case SYNTH_TRK_EVT_PWM_SET_LEVEL_ABS:        
                Pod_LEDsSetLevel(channel, param, 0);
            break;
            default:
                // Not implemented
            break;
        }
    }
}

// ------------------------------------------------------------------------------

