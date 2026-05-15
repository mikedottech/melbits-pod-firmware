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

#ifndef _POD_UI_H_
#define _POD_UI_H_

#include "common.h"
#include "Audio/synth.h"
#include "utils.h"
#include "Pod/Power.h"

typedef enum
{    
    SC_SEND_SEED = 0,
    SC_SEND_SEED_ERROR,
    SC_RECEIVE_SEED,
    SC_SHAKE_START,
    SC_SHAKE_START_ERROR,
    SC_SHAKE_START_SUCCESS,
    SC_INCUBATION_COMPLETE,
    SC_SHAKE_INCUBATION,
    SC_LOWBATTERY,    
    SC_ML_SYNC, //*    
    SC_ML_CONNECT, //*
    SC_INCUBATION_REPORTPROGRESSAUTO, //*
    SC_INCUBATION_REPORTPROGRESS, //*
    SC_EMPTY, //*
    SC_HWPANIC,
    SC_LEDSON,
    SC_LIGHTSENSORTEST,
    SC_WELCOME,
    SC_FILE,
    SC_COUNT,
    SC_NONE = 0xFF,
    __SC_SPECIAL_FIRST = SC_ML_SYNC    
} Pod_UISystemClipID_t;

typedef struct
{
    uint16_t prio;
    const synth_songDefinition_t *pClipData;
} Pod_UISystemClipEntry_t;

void Pod_UIInit(void);
void Pod_UITick(uint32_t ticks);
bool Pod_UIPlayFilePrio(uint16_t id, uint16_t priority);
void Pod_UIPlaySystemClip(Pod_UISystemClipID_t id);
void Pod_UIPlaySystemClipPrio(Pod_UISystemClipID_t id, uint16_t priority);
void Pod_UIStopCurrentClip(void);
void Pod_UIStopSystemClip(Pod_UISystemClipID_t id);

uint8_t Pod_UIGetCurrentClipID(void);

void Pod_UISetUserSlotColor(const Utils_RGB888_t *pColor);

void Pod_UISetUserParam(uint32_t param);

void Pod_UIpowerEventHandler(const Pod_powerEventData_t *pData);

// Get current clip info
//void Pod_UIPlaySystemClip(uint32_t id, uint16_t priority);

#endif