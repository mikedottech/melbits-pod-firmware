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

#ifndef _POD_MOTION_H_
#define _POD_MOTION_H_

#include "common.h"
#include "HAL/HAL.h"

#define POD_MOTION_ALGORITHM_ID_SHAKE   (0)
#define POD_MOTION_ALGORITHM_ID_TAP     (1)
#define POD_MOTION_ALGORITHMS_NUM       (2)

typedef enum
{
    MET_SHAKE,
    MET_ACTIVITY,
    MET_AWARENESS
} Pod_motionEventType_t;

typedef enum
{
    SHK_INVALID = 0,
    SHK_STARTED,
    SHK_STOPPED,
    SHK_PEAK
} Pod_motionShakeState_t;

typedef struct
{
    Pod_motionEventType_t type;
    union
    {
        struct
        {
            Pod_motionShakeState_t state;
        } shake;
        struct
        {
            bool moving;
        } activity;
        struct
        {
            bool aware;
        } awareness;
    } data;
} Pod_motionEventData_t;

typedef enum
{
    ACCEL_LEVEL_INVALID = 0,
    ACCEL_LEVEL_OFF,        // 0 Hz
    ACCEL_LEVEL_DORMANT,    // 1 Hz
    ACCEL_LEVEL_IDLE,       // 10 Hz
    ACCEL_LEVEL_1,          // 25 Hz
    ACCEL_LEVEL_2,          // 400 Hz
    ACCEL_LEVEL_COUNT
} Pod_motionAccelLevel_t;

void Pod_motionInit(void);
void Pod_motionTick(uint32_t ticks);
void Pod_motionEnableAlgorithm(uint8_t id);
void Pod_motionDisableAlgorithm(uint8_t id);

void Pod_motionHALEventHandler(const HAL_event_t *pEvent);

void Pod_motionSetSubStreamEnabled(bool enabled);

void Pod_motionMakeAware(void);

void Pod_motionEventHandler(const Pod_motionEventData_t *pData);
void Pod_motionsubStreamHandler(const HAL_accelSample_t *pData, uint16_t len);

// Sets the minimum accel power level allowed
void Pod_motionSetMinimumLevel(Pod_motionAccelLevel_t lvl);

bool Pod_motionIsActivity(void);

bool Pod_motionIsAware(void);

#endif

