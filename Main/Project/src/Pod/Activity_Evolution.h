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

#ifndef _POD_ACT_EVOLUTION_H_
#define _POD_ACT_EVOLUTION_H_

#include "common.h"
#include "Pod/Motion.h"
#include "Pod/Activity.h"
#include "utils.h"

#define POD_MELBITDNA_FILE_MAGIC        (0x3f)
#define POD_EVOLUTION_FILE_MAGIC        (0x35)
#define POD_EVOLUTION_REPORT_FILE_MAGIC (0x36)
#define POD_EVOLUTION_FILE_VERSION      (0)

typedef enum
{
    ES_INVALID = 0, // Reserved code
    ES_EMPTY,       // Incubator is empty
    ES_READY,       // Incubator has seed and it's ready to start
    ES_FINISHED,    // Incubation finished, poppler inside
    ES_WAITSTART,   // Retarded start
    ES_ONGOING,     // Incubation in progress
} Pod_ActEvoStatus_t;

enum
{
    ACT_EVO_ERR_OK = ACT_ERR_OK,
    ACT_EVO_ERR_INVALID_DNA,
    ACT_EVO_ERR_INVALID_RECIPE,
    ACT_EVO_ERR_MISSING_FILE
};

typedef struct
{
    uint16_t light;
    uint16_t temp;
} MBT_CFG_PLATFORM_PACKED Pod_ActEvoSample_t;

typedef struct
{
    uint8_t magic;
    uint8_t version;
    uint8_t flags;
    uint8_t reserved;
    uint32_t movementTime_ms;
    uint16_t nSamples;
    Pod_ActEvoSample_t samples[MBT_CFG_ACT_EVO_BUFFER_SIZE_SAMPLES];
} MBT_CFG_PLATFORM_PACKED Pod_ActEvoReportFile_t;

typedef struct
{
    uint8_t magic;
    uint8_t version;
    uint8_t flags;
    uint8_t reserved;
    uint16_t nSamples;
    uint16_t sampleInterval_s;    
} MBT_CFG_PLATFORM_PACKED Pod_ActEvoFile_t;

typedef struct
{
    uint8_t magic;
    uint8_t version;
    uint8_t flags;
    uint8_t reserved;
    Utils_RGB888_t color;
    uint8_t weight;
    uint8_t dataLength;
    uint8_t dataFirstByte;
} MBT_CFG_PLATFORM_PACKED Pod_ActMelbitDNAFile_t;

uint8_t Pod_ActEvoStart(void);
void Pod_ActEvoStop(void);
void Pod_ActEvoTick(uint32_t ticks);
void Pod_ActEvoMotionHandler(const Pod_motionEventData_t *pData);
void Pod_ActEvoAnalogHandler(uint8_t clientID, HAL_analogSample_t *pBuffer, uint8_t count);
uint16_t Pod_ActEvoGetState(void);

uint16_t _Pod_ActFileCB(uint16_t id, uint8_t evt);

#endif