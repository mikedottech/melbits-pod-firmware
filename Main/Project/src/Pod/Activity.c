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

#include "Pod/Activity.h"
#include "Pod/Activity_Evolution.h"
#include "Pod/EventServer.h"

static void dummyNoParams(void) {}
static uint8_t dummyNoParamsUint8(void) { return 1; }
static void dummyTick(uint32_t ticks) {}
static void dummyMotionHandler(const Pod_motionEventData_t *pData) {}
static void dummyAnalogHandler(uint8_t clientID, HAL_analogSample_t *pBuffer, uint8_t count) {}
static uint16_t dummyGetState(void) { return ACT_STATE_NOACTIVITY; }

static const Pod_ActCB_t s_Pod_ActCB[ACT_COUNT] =
{
    {   // ACT_NONE
        .start = dummyNoParamsUint8,
        .stop = dummyNoParams,
        .tick = dummyTick,
        .getState = dummyGetState,
        .motionHandler = dummyMotionHandler,
        .analogHandler = dummyAnalogHandler
    },
    {   // ACT_EVOLUTION
        .start = Pod_ActEvoStart,
        .stop = Pod_ActEvoStop,
        .tick = Pod_ActEvoTick,
        .getState = Pod_ActEvoGetState,
        .motionHandler = Pod_ActEvoMotionHandler,
        .analogHandler = Pod_ActEvoAnalogHandler
    }
};

typedef struct
{
    Pod_ActType_t           currentType;
    const Pod_ActCB_t       *pCurrentAct;
} Pod_ActState_t;

static Pod_ActState_t s_Pod_ActState;

HAL_analogSample_t g_Pod_ActLightBuffer;
HAL_analogSample_t g_Pod_ActTempBuffer;

static void _Pod_ActSetNone(void)
{
    s_Pod_ActState.currentType = ACT_NONE;
    s_Pod_ActState.pCurrentAct = &s_Pod_ActCB[ACT_NONE];    
}

// ------------------------------------------------------------------------------

void Pod_ActInit(void)
{
    _Pod_ActSetNone();
}

// ------------------------------------------------------------------------------

void Pod_ActTick(uint32_t ticks)
{
    s_Pod_ActState.pCurrentAct->tick(ticks);
}

// ------------------------------------------------------------------------------

uint8_t Pod_ActStart(Pod_ActType_t actType)
{
    if(actType >= ACT_COUNT) return 1;
    
    Pod_ActStop();

    s_Pod_ActState.currentType = actType;
    s_Pod_ActState.pCurrentAct = &s_Pod_ActCB[actType];

    uint8_t retCode = s_Pod_ActState.pCurrentAct->start();

    if(retCode != 0)
    {
        _Pod_ActSetNone();
    }

    return retCode;
}

// ------------------------------------------------------------------------------

Pod_ActType_t Pod_ActGetCurrentType()
{
    return s_Pod_ActState.currentType;
}

// ------------------------------------------------------------------------------

void Pod_ActStop()
{
    s_Pod_ActState.pCurrentAct->stop();
    Pod_analogSetClientMode(POD_ACT_LIGHT_ANALOG_CH_ID, ACM_IDLE);
    Pod_analogSetClientMode(POD_ACT_TEMP_ANALOG_CH_ID, ACM_IDLE);
    _Pod_ActSetNone();
}

// ------------------------------------------------------------------------------

void Pod_ActMotionHandler(const Pod_motionEventData_t *pData)
{
    s_Pod_ActState.pCurrentAct->motionHandler(pData);
}

// ------------------------------------------------------------------------------

void Pod_ActAnalogHandler(uint8_t clientID, HAL_analogSample_t *pBuffer, uint8_t count)
{
    s_Pod_ActState.pCurrentAct->analogHandler(clientID, pBuffer, count);
}

// ------------------------------------------------------------------------------

uint16_t Pod_ActGetState(void)
{
    return s_Pod_ActState.pCurrentAct->getState();
}

void _Pod_ActivityStateChanged()
{
     Pod_ESNotifyEventSource(ESE_ACT_STATECHANGED);    
}