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

#include "BoxMode.h"
#include "UI.h"
#include "HAL/HAL.h"

#include "utils.h"

typedef struct
{
    Pod_UISystemClipID_t clip;
    bool entering;
    //MBT_TIMER_DEFINE(
} Pod_BMState_t;

Pod_BMState_t s_Pod_BMState;

void Pod_BMInit(void)
{
    memset(&s_Pod_BMState, 0, sizeof(s_Pod_BMState));
}

void Pod_BMTick(uint32_t ticks)
{
    if(s_Pod_BMState.entering && Pod_UIGetCurrentClipID() == SC_NONE)
    {
        HAL_powerEnterLockupMode();
        // Shouldn't get here
    }
}

void Pod_BMEnter(Pod_BMReason_t reason)
{
    static const Pod_UISystemClipID_t k_BMClips[BMR_COUNT] = {SC_EMPTY, SC_SHAKE_START_ERROR, SC_HWPANIC};

    s_Pod_BMState.entering = true;

    Pod_UIPlaySystemClipPrio(k_BMClips[reason], 0);
}

void Pod_BMEnterImmediately(void)
{
    HAL_powerEnterLockupMode();
}
