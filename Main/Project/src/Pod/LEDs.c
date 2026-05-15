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

#include "LEDs.h"

#include "mbt_config.h"
#include "HAL/HAL.h"
#include "Pod/Power.h"

#include "HAL/Debug.h"

#include <string.h>

// TODO:
// [X] Channel status struct and array
// [X] Background color [and priorities]
// [X] Shared window -> Mute
// [X] Lerp logic, fp arithmetic
// [NO] Global dimmer?
// [X] Power module integration!
// - Remap function to map luminosity to PWM value?


#define POD_LEDS_FLAG_MUTED                     (0x1)
#define POD_LEDS_FLAG_ANIMATING                 (0x2)
#define POD_LEDS_FLAG_BOUNCEBACK                (0x4)
#define POD_LEDS_FLAG_DIMMING                   (0x8)

const fix32_t k_Pod_LEDSpeedOneSecond         = (1000);
const fix32_t k_Pod_LEDSpeedHalfSecond        = (500);
const fix32_t k_Pod_LEDSpeedQuarterSecond     = (250);
const fix32_t k_Pod_LEDSpeedSixteenthSecond   = (62);

const Utils_RGB888_t k_Pod_LEDColorBlack    = {0, 0, 0};
const Utils_RGB888_t k_Pod_LEDColorWhite    = {255, 255, 255};
const Utils_RGB888_t k_Pod_LEDColorRed      = {255, 0, 0};
const Utils_RGB888_t k_Pod_LEDColorGreen    = {0, 255, 0};
const Utils_RGB888_t k_Pod_LEDColorPurple   = {255, 0, 255};

typedef struct
{
    uint8_t         values[POD_LED_COUNT];
    Pod_LEDState_t  states[POD_LED_COUNT];
    bool            dirty;
    uint8_t         flags;
    bool            somethingAnimating;
} Pod_LEDsState_t;

Pod_LEDsState_t     s_Pod_LEDsState;

// ------------------------------------------------------------------------------

static void _Pod_LEDsSetDirty(void)
{
    s_Pod_LEDsState.dirty = true;
    HAL_sysSchedulePendingTick();
}

// ------------------------------------------------------------------------------

void Pod_LEDsInit(void)
{
    memset(&s_Pod_LEDsState, 0, sizeof(s_Pod_LEDsState));
}

// ------------------------------------------------------------------------------

static void _Pod_LEDsSetToIdle(uint8_t id)
{
    MBT_ASSERT(id < POD_LED_COUNT);
    s_Pod_LEDsState.values[id] = s_Pod_LEDsState.states[id].idleLevel;
}

// ------------------------------------------------------------------------------

static bool _Pod_LEDsAnimate(uint32_t ticks)
{
    bool somethingAnimating = false;

    uint8_t nAnimatingLEDS = 0;

    for(uint8_t i = 0; i < POD_LED_COUNT; ++i)
    {
        Pod_LEDState_t *pState = &s_Pod_LEDsState.states[i];
        if(pState->remainingHalfCycles > 0)
        {
            ++nAnimatingLEDS;
            somethingAnimating = true;
            int32_t lerp = MBT_FLAG_IS_SET(pState->flags, POD_LEDS_FLAG_BOUNCEBACK) ?  255 - ((pState->elapsed_ticks * 255) / pState->tween_ticks) : ((pState->elapsed_ticks * 255) / pState->tween_ticks);

            if(lerp < 0)
            {
                lerp = 0;                
                MBT_FLAG_CLEAR(pState->flags, POD_LEDS_FLAG_BOUNCEBACK);                
                pState->elapsed_ticks = 0;
                -- pState->remainingHalfCycles;
            } else if(lerp > 255)
            {
                lerp = 255;
                MBT_FLAG_SET(pState->flags, POD_LEDS_FLAG_BOUNCEBACK);
                pState->elapsed_ticks = 0;
                -- pState->remainingHalfCycles;
            }

            if(pState->remainingHalfCycles == 0)
            {
                _Pod_LEDsSetToIdle(i);
                    
            } else {
                pState->elapsed_ticks += ticks;

                uint32_t oneminuslerp = (1 << 8) - lerp;

                uint32_t f1 = (pState->sourceLevel * oneminuslerp);
                uint32_t f2 = (pState->targetLevel * lerp);
            
                uint32_t v = (f1 + f2) >> 8;
                s_Pod_LEDsState.values[i] = v;
            }
            s_Pod_LEDsState.dirty = true;
        }
    }

    if(nAnimatingLEDS > 0)  Pod_powerSetActivity(PWR_ACT_LED);
    else                    Pod_powerClearActivity(PWR_ACT_LED);

    return somethingAnimating;
}

// ------------------------------------------------------------------------------

void Pod_LEDsTick(uint32_t ticks)
{
    bool animationsInProgress = _Pod_LEDsAnimate(ticks);
    
    s_Pod_LEDsState.somethingAnimating = animationsInProgress;

    if(s_Pod_LEDsState.dirty && !MBT_FLAG_IS_SET(s_Pod_LEDsState.flags, POD_LEDS_FLAG_MUTED))
    { 
        if(MBT_FLAG_IS_SET(s_Pod_LEDsState.flags, POD_LEDS_FLAG_DIMMING))
        {
            uint8_t dimValues[POD_LED_COUNT];
            for(uint8_t i = 0; i < POD_LED_COUNT; ++i) dimValues[i] = s_Pod_LEDsState.values[i] >> 1;
            HAL_LEDSetMultiple(&dimValues[0]);
        } else {
            HAL_LEDSetMultiple(&s_Pod_LEDsState.values[0]);
        }
        s_Pod_LEDsState.dirty = false;
    }    
}

// ------------------------------------------------------------------------------

bool Pod_LEDsAnimationsInProgress(void)
{
    return s_Pod_LEDsState.somethingAnimating;
}

// ------------------------------------------------------------------------------

void Pod_LEDsSetAnimation(uint8_t id, uint8_t from, uint8_t to, uint8_t nHalfCycles, uint16_t time_ms)
{
    MBT_ASSERT(id < POD_LED_COUNT);

    Pod_LEDState_t *pState = &s_Pod_LEDsState.states[id];
    pState->tween_ticks = time_ms * (1000UL / MBT_RTC_US_PER_TICK);
    pState->sourceLevel = from;
    pState->targetLevel = to;
    pState->remainingHalfCycles = nHalfCycles;
    pState->idleLevel = (nHalfCycles % 2) == 0 ? from : to;
    MBT_FLAG_CLEAR(pState->flags, POD_LEDS_FLAG_BOUNCEBACK);
    _Pod_LEDsSetDirty();
    s_Pod_LEDsState.somethingAnimating = true;
}

// ------------------------------------------------------------------------------

void Pod_LEDsCenterSetAnimation(const Utils_RGB888_t *pFrom, const Utils_RGB888_t *pTo, uint8_t nHalfCycles, uint16_t time_ms)
{
    Pod_LEDsSetAnimation(POD_LED_R, pFrom->r, pTo->r, nHalfCycles, time_ms);
    Pod_LEDsSetAnimation(POD_LED_G, pFrom->g, pTo->g, nHalfCycles, time_ms);
    Pod_LEDsSetAnimation(POD_LED_B, pFrom->b, pTo->b, nHalfCycles, time_ms);
}

// ------------------------------------------------------------------------------

void Pod_LEDsSetMute(bool muted)
{
    if(muted)
    {
        MBT_FLAG_SET(s_Pod_LEDsState.flags, POD_LEDS_FLAG_MUTED);
        uint8_t black[7] = {0};
        HAL_LEDSetMultiple(&black[0]);
    }
    else
        MBT_FLAG_CLEAR(s_Pod_LEDsState.flags, POD_LEDS_FLAG_MUTED);
    _Pod_LEDsSetDirty();
}

// ------------------------------------------------------------------------------

void Pod_LEDsSetIdleLevel(uint8_t id, uint8_t level)
{
    MBT_ASSERT(id < POD_LED_COUNT);
    s_Pod_LEDsState.states[id].idleLevel = level;    
    _Pod_LEDsSetDirty();
}

// ------------------------------------------------------------------------------

void Pod_LEDsCenterSetIdleColor(const Utils_RGB888_t *pColor)
{
    Pod_LEDsSetIdleLevel(POD_LED_R, pColor->r); _Pod_LEDsSetToIdle(POD_LED_R);
    Pod_LEDsSetIdleLevel(POD_LED_G, pColor->g); _Pod_LEDsSetToIdle(POD_LED_G);
    Pod_LEDsSetIdleLevel(POD_LED_B, pColor->b); _Pod_LEDsSetToIdle(POD_LED_B);
}

// ------------------------------------------------------------------------------

void Pod_LEDsSetLevel(uint8_t id, uint8_t level, uint16_t time_ms)
{
    MBT_ASSERT(id < POD_LED_COUNT);

    if(time_ms == 0)
    {
        // This will stop any ongoing animnation on this channel
        s_Pod_LEDsState.states[id].remainingHalfCycles = 0;
        Pod_LEDsSetIdleLevel(id, level);
        _Pod_LEDsSetToIdle(id);        
    } else {
        Pod_LEDsSetAnimation(id, s_Pod_LEDsState.values[id], level, 1, time_ms);
    }
}

// ------------------------------------------------------------------------------

void Pod_LEDsCenterSetColor(const Utils_RGB888_t *pColor, uint16_t time_ms)
{
    Pod_LEDsSetLevel(POD_LED_R, pColor->r, time_ms);
    Pod_LEDsSetLevel(POD_LED_G, pColor->g, time_ms);
    Pod_LEDsSetLevel(POD_LED_B, pColor->b, time_ms);
}

// ------------------------------------------------------------------------------

void Pod_LEDsGetStateCopy(uint8_t *pState)
{
    memcpy(pState, &s_Pod_LEDsState.states[0], sizeof(Pod_LEDState_t) * POD_LED_COUNT);    
}

// ------------------------------------------------------------------------------

void Pod_LEDsSetState(uint8_t *pState)
{
    memcpy(&s_Pod_LEDsState.states[0], pState, sizeof(Pod_LEDState_t) * POD_LED_COUNT);
    _Pod_LEDsSetDirty();
}

void Pod_LEDsSetDimmingEnabled(bool enabled)
{
    if(enabled)
    {
        MBT_FLAG_SET(s_Pod_LEDsState.flags, POD_LEDS_FLAG_DIMMING);
    } else {
        MBT_FLAG_CLEAR(s_Pod_LEDsState.flags, POD_LEDS_FLAG_DIMMING);
    }
}
