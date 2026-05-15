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

#ifndef _POD_EVENTSERVER_H_
#define _POD_EVENTSERVER_H_

#include "common.h"

#include "Pod/MLINK.h"
#include "Pod/Power.h"

enum
{
    ESE_ML_LOCKED               = (1 << 0), //*
    ESE_ML_LOST                 = (1 << 1), //*
    ESE_MOTION_SHAKESTART       = (1 << 2), //*
    ESE_MOTION_SHAKEEEND        = (1 << 3), //*
    ESE_MOTION_SHAKEPEAK        = (1 << 4), //*
    ESE_MOTION_MOTIONSTART      = (1 << 5), //*
    ESE_PWR_EXTPOWERATTACHED    = (1 << 6), //*
    ESE_PWR_EXTPOWERREMOVED     = (1 << 7), //*
    ESE_PWR_BATTCHARGING        = (1 << 8), //*
    ESE_PWR_BATTCHARGED         = (1 << 9), //*
    ESE_PWR_BATTLOW             = (1 << 10),//*
    ESE_PWR_BATTDEAD            = (1 << 11),//*
    ESE_ACT_STATECHANGED        = (1 << 12),//*
    ESE_MOTION_MOTIONEND        = (1 << 13), //*
    ESE_ACT_BASE                = (1 << 24)
};


void Pod_ESInit(void);
void Pod_ESTick(uint32_t ticks);

void Pod_ESReset(void);
void Pod_ESSetEventsMask(uint32_t mask);

void Pod_ESNotifyEventSource(uint32_t mask);

void Pod_ESTXSlotHandler(void);

void Pod_ESMLEventHandler(const Pod_MLEventData_t *pData);
void Pod_ESpowerEventHandler(const Pod_powerEventData_t *pData);

#endif