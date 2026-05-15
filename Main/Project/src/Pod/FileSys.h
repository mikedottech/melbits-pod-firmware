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

#ifndef _POD_FILESYS_H_
#define _POD_FILESYS_H_

#include "common.h"
#include "Pod/VirtualFiles.h"
#include "mbt_config.h"

enum
{
    FS_PERM_R   = 0x1,
    FS_PERM_W   = 0x2,
    FS_PERM_RW  = FS_PERM_R | FS_PERM_W,
    FS_PERM_FLASHOVERLAY = 0x4
};

enum
{
    FS_FLAG_ISVIRTUAL = 0x1
};

typedef struct
{
    const uint8_t *pData;
    size_t size;
    uint8_t permissions;
    uint8_t flags;
    const Pod_VFDesc_t *pvf;
} Pod_FSFileDesc_t;

void Pod_FSysGetFile(uint16_t id, Pod_FSFileDesc_t *pDesc);

#endif