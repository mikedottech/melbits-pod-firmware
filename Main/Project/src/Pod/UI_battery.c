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

#include "Pod/UI.h"
#include "utils.h"
#include "mbt_config.h"
#include "Pod/Power.h"
#include "Pod/BoxMode.h"
#include "Pod/LEDs.h"
#include "Pod/Motion.h"
#include "HAL/Debug.h"

typedef struct
{    
    MBT_TIMER_DEFINE(userActivityTimer);
    Pod_powerBatteryStatus_t lastBattStatus;
    bool isLow;
} Pod_UIBattState_t;

Pod_UIBattState_t s_Pod_UIBattState;

// ------------------------------------------------------------------------------

void Pod_UIBattInit(void)
{
    memset(&s_Pod_UIBattState, 0, sizeof(s_Pod_UIBattState));    
}

// ------------------------------------------------------------------------------

static void _Pod_UIBattLogic(uint32_t ticks)
{
    //uint16_t t = ((MBT_TICKS_TO_US(ticks) / 1000) % 1000) * 512 / 1000;
    switch(s_Pod_UIBattState.lastBattStatus)
    {
        case BS_CHARGING:            
            Pod_LEDsCenterSetIdleColor(&k_Pod_LEDColorRed);
        break;
        case BS_CHARGED:
            Pod_LEDsCenterSetIdleColor(&k_Pod_LEDColorGreen);
        break;
        default:
        break;
    }
}

// ------------------------------------------------------------------------------

static void _Pod_UIBattWarningTimerReset(void)
{
    MBT_TIMER_RESET_TIMEOUT(s_Pod_UIBattState.userActivityTimer, MBT_US_TO_TICKS(MBT_CFG_UI_BATT_USERACTIVITY_TIMEOUT_MS * 1000));
}

// ------------------------------------------------------------------------------

static void _Pod_UIEmitBattWarning(void)
{
    Pod_UIPlaySystemClip(SC_LOWBATTERY);
}

// ------------------------------------------------------------------------------

void Pod_UIBattTick(uint32_t ticks)
{
    _Pod_UIBattLogic(ticks);

    if(s_Pod_UIBattState.isLow)
    {
        if(Pod_powerIsUserActivity())
        {
            MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_UIBattState.userActivityTimer, ticks,
                {
                    _Pod_UIBattWarningTimerReset();
                    _Pod_UIEmitBattWarning();
                }
            );
        }
    }
}

// POWER ------------------------------------------------------------------------
void Pod_UIpowerEventHandler(const Pod_powerEventData_t *pData)
{   
#ifdef DEBUG 
    static const char *dbgEvents[] = {
        "PET_DEADBATT",
        "PET_LOWBATT",
        "PET_BATTSTATUS",
        "PET_EXTPOWER_ATTACHED",
        "PET_EXTPOWER_REMOVED"
        };

    MBT_LOG("[PWR] %s\n", dbgEvents[pData->type]);
#endif

    switch(pData->type)
    {
        case PET_BATTSTATUS:
        {
            // To keep the LED IDLE color
            s_Pod_UIBattState.lastBattStatus = pData->data.battStatus.status;


            if (pData->data.battStatus.status == BS_BATTPROBLEM)
            {
                Pod_BMEnter(BMR_HWPANIC);
            }

#ifdef DEBUG 
            static const char *dbgSt[] = {
                "BS_INVALID",
                "BS_DISCHARGING",
                "BS_CHARGING",
                "BS_CHARGED",
                "BS_BATTPROBLEM"
            };
            
            MBT_LOG("[PWR] BATTSTATUS = %s\n", dbgSt[pData->data.battStatus.status]);
#endif

            if(pData->data.battStatus.status == BS_DISCHARGING)
            {
                Pod_LEDsCenterSetIdleColor(&k_Pod_LEDColorBlack);
            }            
        }
        break;
        case PET_DEADBATT:
            // We don't warn about the battery dead if the tickrate is very low
            // which means is in the lowest power mode and not actively used.
            if(/*Pod_powerGetTickRate() < HAL_SYS_TICKRATE_1HZ*/ /*Pod_powerIsUserActivity()*/ Pod_motionIsAware())
            {
                Pod_BMEnter(BMR_BATTDEAD);
            } else {
                Pod_BMEnterImmediately();
            }
        break;
        case PET_LOWBATT:
            if(!s_Pod_UIBattState.isLow)
            {
                s_Pod_UIBattState.isLow = true;
                _Pod_UIEmitBattWarning();
            }
        break;
        case PET_EXTPOWER_ATTACHED:
            s_Pod_UIBattState.isLow = false;
        break;
        case PET_EXTPOWER_REMOVED:
            _Pod_UIBattWarningTimerReset();            
        break;
    }
}
