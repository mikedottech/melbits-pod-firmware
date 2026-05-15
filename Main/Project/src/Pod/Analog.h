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

#ifndef _POD_ANALOG_H_
#define _POD_ANALOG_H_

#include "common.h"
#include "HAL/HAL.h"

#define POD_ANALOG_CLIENTS_COUNT (7) //(6)

typedef enum
{
    ACM_IDLE,
    ACM_STREAM_1X,
    ACM_STREAM_0_5X,
    ACM_STREAM_0_25X,
    ACM_STREAM_0_125X,
    ACM_SINGLESHOT    
} Pod_analogClientMode_t;

typedef void (*Pod_AnalogCallback_t)(uint8_t clientID, HAL_analogSample_t *pBuffer, uint8_t count);

#define POD_ANALOG_OFFSET_TEMP      (0)
#define POD_ANALOG_OFFSET_LIGHT     (1)
#define POD_ANALOG_OFFSET_ML        (2)
#define POD_ANALOG_OFFSET_BATT      (3)

#define POD_ANALOG_STRIDE_SAMPLES   (4)

typedef struct
{    
    HAL_analogSample_t                *pBuffer;
    uint8_t                 bufferSize;
    uint8_t                 offset;
    Pod_AnalogCallback_t    cb;
} Pod_analogClientConfig_t;

typedef struct
{
    Pod_analogClientMode_t  mode;
    uint8_t                 bufferIndex;
    uint8_t                 sampleCounter;
} Pod_analogClientState_t;

typedef struct
{
    const Pod_analogClientConfig_t *entries;
    uint8_t nEntries;
} Pod_analogClientMap_t;

void Pod_analogInit(void);
void Pod_analogSetClientsMap(const Pod_analogClientMap_t* pMap);

void Pod_analogSetClientMode(uint8_t clientIndex, Pod_analogClientMode_t mode);

void Pod_analogTick(uint32_t ticks);
void Pod_analogFeedStream(HAL_analogSample_t *pData, uint16_t len);

#define Pod_analogDisableClient(C) Pod_analogSetClientMode(C, ACM_IDLE)

#endif