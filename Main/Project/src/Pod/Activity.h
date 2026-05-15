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

#ifndef _POD_ACTIVITY_H_
#define _POD_ACTIVITY_H_

#include "common.h"
#include "Pod/Motion.h"
#include "Pod/Analog.h"

#define POD_ACT_LIGHT_ANALOG_CH_ID (0)
#define POD_ACT_TEMP_ANALOG_CH_ID (1)

typedef enum
{
    ACT_NONE = 0,
    ACT_EVOLUTION,
    //ACT_TRANSPORT,
    ACT_COUNT
} Pod_ActType_t;

enum
{
    ACT_STATE_NOACTIVITY = 0
};

enum
{
    ACT_ERR_OK = 0
};

typedef struct
{
    uint8_t (*start)(void);
    void (*stop)(void);
    void (*tick)(uint32_t ticks);
    void (*motionHandler)(const Pod_motionEventData_t *pData);
    uint16_t (*getState)(void);
    Pod_AnalogCallback_t analogHandler;
} Pod_ActCB_t;

extern HAL_analogSample_t g_Pod_ActLightBuffer;
extern HAL_analogSample_t g_Pod_ActTempBuffer;

void Pod_ActInit(void);
void Pod_ActTick(uint32_t ticks);

uint8_t Pod_ActStart(Pod_ActType_t actType);
Pod_ActType_t Pod_ActGetCurrentType();
void Pod_ActMotionHandler(const Pod_motionEventData_t *pData);
void Pod_ActAnalogHandler(uint8_t clientID, HAL_analogSample_t *pBuffer, uint8_t count);

uint16_t Pod_ActGetState(void);

void Pod_ActStop();

void _Pod_ActivityStateChanged();

#endif