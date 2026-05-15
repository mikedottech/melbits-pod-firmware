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

#include "Vibration.h"
#include "utils.h"
#include "HAL/HAL.h"

typedef struct
{
    MBT_TIMER_DEFINE(timer);
    bool enabled;
} Pod_vibState_t;

Pod_vibState_t s_Pod_vibState;

// ------------------------------------------------------------------------------

void Pod_vibInit(void)
{
    memset(&s_Pod_vibState, 0, sizeof(s_Pod_vibState));
}

// ------------------------------------------------------------------------------

void Pod_vibTick(uint32_t ticks)
{
    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_vibState.timer, ticks,
        {
            HAL_motorSetLevel(0);
        }
    )
}

// ------------------------------------------------------------------------------

void Pod_vibSetVibration(uint8_t level, uint16_t time_ms)
{
    if(s_Pod_vibState.enabled)
    {
        HAL_motorSetLevel(level);
        if(level > 0)
        {
            if(time_ms == 0) time_ms = MBT_CFG_VIB_DEFAULT_TIME_MS;
            MBT_TIMER_RESET_TIMEOUT(s_Pod_vibState.timer, MBT_US_TO_TICKS(time_ms * 1000));
        }
    }
}

// ------------------------------------------------------------------------------

void Pod_vibSetEnabled(bool enabled)
{
    s_Pod_vibState.enabled = enabled;
    if(!enabled)
    {
        HAL_motorSetLevel(0);
    }
}