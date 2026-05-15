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

#ifndef _POD_STORAGE_H_
#define _POD_STORAGE_H_

#include "common.h"
#include "mbt_config.h"
#include "HAL/HAL.h"

#define POD_STORAGE_FID_FEEDBACK_PCM_BASE   (0x0300)
#define POD_STORAGE_FID_FB_PCM_INSERT_SEED  POD_STORAGE_FID_FEEDBACK_BASE + (0)

#define POD_STORAGE_FS_HEADER_MAGIC     (0x3C1A)
#define POD_STORAGE_FS_HEADER_VERSION   (0x0)
#define POD_STORAGE_FS_CRC16_POLY       (0x143A)
#define MBT_CFG_POD_STORAGE_MAX_FILES   (16)

typedef enum
{
    STGEVENT_IDLE = 0,
    STGEVENT_BUSY,
    STGEVENT_ERASE_OK,    
    STGEVENT_ERASE_ERROR,
    STGEVENT_WRITE_OK,
    STGEVENT_WRITE_ERROR,

    STGEVENT_ERASE_BASE = STGEVENT_ERASE_OK,
    STGEVENT_WRITE_BASE = STGEVENT_WRITE_OK
} Pod_storageEventType_t;

typedef struct
{
    uint16_t magic;                 // File system header magic
    uint8_t  version;               // File system header version
    uint8_t  flags;                 // Reserved for future use (default to 0xFF)
} MBT_CFG_PLATFORM_PACKED Pod_storageHeader_t;

typedef struct
{
    uint16_t crc16;                 // CRC16 of the file and header excluding this field. 0xFFFF ? = file added and not finished
    uint16_t id;                    // File ID
    uint16_t size;                  // File size (max. 64 KB)
    uint16_t offsetNext;            // Offset to the next file descriptor. 0xFFFF ? = last file
} MBT_CFG_PLATFORM_PACKED Pod_storageFD_t;

void Pod_storageInit(void);
const Pod_storageFD_t *Pod_storageGetFD(uint16_t id);
const uint8_t* Pod_storageGetPtr(const Pod_storageFD_t *pFD);
bool Pod_storageIsEmpty(void);
uint16_t Pod_storageGetCapacity(void);
bool Pod_storageIsBusy(void);

bool Pod_storageMountFS(void);

void Pod_storageHALEventHandler(const HAL_event_t *pEventData);

void Pod_storageEventHandler(Pod_storageEventType_t evt);

bool Pod_storageErase(void);

bool Pod_storageWrite(uint16_t offset, const uint8_t *pData, uint16_t len);

#endif