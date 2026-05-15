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

#ifndef _POD_FILESERVER_H_
#define _POD_FILESERVER_H_

#include "common.h"
#include "Pod/Storage.h"
#include "Pod/Playground_Defs.h"

enum
{
    FSRESULT_OK = 0,
    FSRESULT_OP_IN_PROGRESS,
    FSRESULT_NOT_ENOUGH_SPACE,
    FSRESULT_NOTEMPTY,
    FSRESULT_BAD_ID,
    FSRESULT_BAD_SIZE,
    FSRESULT_ACCESS_DENIED,
    FSRESULT_INTERNAL_ERROR,
    FSRESULT_CANT_MOUNT,
    FSRESULT_INVALID_STATE,
    FSRESULT_INTERRUPTED
};

void Pod_FSInit(void);
void Pod_FSReset(void);
void Pod_FSTick(uint32_t ticks);
void Pod_FSAbortXfer(void);

// Transfer an archive to flash
uint8_t Pod_FSStartArchiveXfer(uint16_t offset, uint16_t size);

// Overwrite a virtual file with write permission
uint8_t Pod_FSStartFileUpload(uint16_t id, uint16_t size);

// Download a virtual or flash file
uint8_t Pod_FSStartFileDownload(uint16_t id, uint16_t *pSize);

uint8_t Pod_FSErase(void);

uint8_t Pod_FSDataPacketHandler(const uint8_t *pData, size_t size);
void Pod_FSEventPacketHandler(Pod_PGPDU_t *pdu);

void Pod_ISR_FSDataPacketHandler(const uint8_t *pData, size_t size);

void Pod_FSStorageEventHandler(Pod_storageEventType_t evt);
void Pod_FSTXSlotHandler(void);

#endif