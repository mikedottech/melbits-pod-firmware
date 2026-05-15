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

#ifndef _USERSETTINGS_H_
#define _USERSETTINGS_H_

#include "common.h"
#include "Pod/FileServer.h"

#define POD_US_FILE_MAGIC   (0x05)
#define POD_US_FILE_VERSION (0x00)

#define POD_US_BIN_SETTTINGS_BYPASSML (0x1)

typedef struct
{
    uint16_t    crc16;
    uint8_t     magic;
    uint8_t     version;    

    uint16_t    volume      : 4;        // 0 = mute, 1 = 25%, 2 = 50%, 3 = 100%
    uint16_t    vibration   : 1;        // 0 = off, 1 = on
    uint16_t    dimming     : 1;        // 0 = off, 1 = on
    uint16_t    binSettings :10;        // Reserved
    uint16_t    reserved;    
} Pod_USUserSettingsFile_t;

void Pod_USInit(void);
void Pod_USTick(uint32_t ticks);

uint16_t _Pod_USFileCB(uint16_t id, uint8_t evt);

void Pod_USResetToFactory(void);

const Pod_USUserSettingsFile_t* Pod_USGetUserSettings();

void Pod_USSetUserSettings(Pod_USUserSettingsFile_t* pData);

#endif
