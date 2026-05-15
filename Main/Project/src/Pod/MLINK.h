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

#ifndef _MLINK_H_
#define _MLINK_H_

#include "common.h"
#include "HAL/HAL.h"

#define POD_POWER_ML_SENSOR_CH_ID (2)

typedef enum
{
    MLE_SIGNAL,
    MLE_DATA,
    MLE_ERROR
} Pod_MLEventType_t;

typedef struct
{
    Pod_MLEventType_t type;
    union
    {
        struct
        {
            uint8_t byte;
        } data;
        struct
        {
            bool locked;
        } signal;        
    } data;
} Pod_MLEventData_t;

void Pod_MLInit(void);
void Pod_MLTick(uint32_t ticks);
void Pod_MLEnable(bool state);

void Pod_MLSetHardTimeout(uint32_t timeout_ms);

void Pod_MLEventHandler(const Pod_MLEventData_t *pData);
void _Pod_MLAnalogCallback(uint8_t clientID, HAL_analogSample_t *pBuffer, uint8_t count);


extern HAL_analogSample_t s_Pod_MLSampleBuffer[MBT_CFG_ML_SAMPLE_BUFFER_SIZE_SAMPLES];

#endif