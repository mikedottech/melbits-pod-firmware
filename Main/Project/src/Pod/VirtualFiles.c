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

#include "Pod/VirtualFiles.h"

#include "Pod/UserSettings.h"
#include "Pod/Power.h"
#include "Pod/Activity_Evolution.h"

#include "HAL/HAL.h"

#include "Pod/FileSys.h"

#include "Pod/FileIds.h"


/*
RAM FILES
[X] User settings     (RW)
[X] Power state       (RO)

[X] Activity report   (RW)

ROM FILES
[X] Device info       (RO)

***********************
*/

extern Pod_USUserSettingsFile_t g_Pod_USFileBuffer;
extern Pod_powerState_t s_Pod_powerState;
extern Pod_ActEvoReportFile_t g_Pod_ActEvoReportFile;

extern HAL_DeviceInfo_t g_HAL_devInfo;

const Pod_VFDesc_t g_Pod_VFList[POD_VIRTUALFILES_COUNT] =
{
    // User settings
    {
        .id             = POD_FILEID_USERSETTINGS,
        .pData          = (uint8_t *)&g_Pod_USFileBuffer,
        .size           = sizeof(Pod_USUserSettingsFile_t),
        .permissions    = FS_PERM_RW,
        .cb             = _Pod_USFileCB
    },
    // Power status
    {
        .id             = POD_FILEID_POWERSTATUS,
        .pData          = (uint8_t *)&s_Pod_powerState.status,
        .size           = sizeof(Pod_powerStatusData_t),
        .permissions    = FS_PERM_R,
        .cb             = NULL
    },
    // Activity scratchpad memory
    {
        .id             = POD_FILEID_ACT_SCRATCH,
        .pData          = (uint8_t *)&g_Pod_ActEvoReportFile,
        .size           = sizeof(Pod_ActEvoReportFile_t),
        .permissions    = FS_PERM_RW,
        .cb             = _Pod_ActFileCB
    },
    // Device info
    {
        .id             = POD_FILEID_DEVINFO,
        .pData          = (uint8_t *)&g_HAL_devInfo,
        .size           = sizeof(HAL_DeviceInfo_t),
        .permissions    = FS_PERM_R,
        .cb             = NULL
    }
    // 
};
