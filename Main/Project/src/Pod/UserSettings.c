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

#include "Pod/UserSettings.h"
#include "mbt_config.h"
#include "crc16.h"

#include "Audio/synth.h"
#include "Pod/Vibration.h"
#include "Pod/LEDs.h"

#include "Pod/VirtualFiles.h"

#include "utils.h"

#include "Pod/RetainedRAM.h"

// ------------------------------------------------------------------------------

//static Pod_USUserSettingsFile_t MBT_CFG_PLATFORM_SECTION_RETAINED_DATA s_Pod_USFile;
Pod_USUserSettingsFile_t g_Pod_USFileBuffer;

extern uint8_t g_Pod_nResetPresses;


// FACTORY SETTINGS -------------------------------------------------------------

const Pod_USUserSettingsFile_t k_Pod_USFactoryFile =
{
    .crc16          = 0xcd6a,                   // Manually computed. Needs to be recomputed it if settings are changed
    .magic          = POD_US_FILE_MAGIC,
    .version        = POD_US_FILE_VERSION,    
    .volume         = 3,
    .vibration      = 1,
    .dimming        = 0,
    .binSettings    = 0
};

// ------------------------------------------------------------------------------

static bool _Pod_validateUserSettings(const Pod_USUserSettingsFile_t *pData)
{
    uint16_t crc = crc16_compute(((uint8_t*)pData) + sizeof(uint16_t), sizeof(Pod_USUserSettingsFile_t) - sizeof(uint16_t), NULL);    
    return (pData->magic == POD_US_FILE_MAGIC && pData->version == POD_US_FILE_VERSION && pData->crc16 == crc);
}

// ------------------------------------------------------------------------------

static void _Pod_USApplyConfig(const Pod_USUserSettingsFile_t *pCfg)
{
    if(!_Pod_validateUserSettings(pCfg))
    {
        g_Pod_RetainedRAM.USFile = k_Pod_USFactoryFile;
    } else {
        g_Pod_RetainedRAM.USFile = *pCfg;
    }

    g_Pod_USFileBuffer = g_Pod_RetainedRAM.USFile;

    // Audio Mixer volume
    synth_selectGlobalVolume((0x08 >> g_Pod_RetainedRAM.USFile.volume) & 0xFE);

    // Vibration
    Pod_vibSetEnabled(g_Pod_RetainedRAM.USFile.vibration);

    // LEDs
    Pod_LEDsSetDimmingEnabled(g_Pod_RetainedRAM.USFile.dimming);    
}

// ------------------------------------------------------------------------------

void Pod_USInit(void)
{
    if(g_Pod_RetainedRAM.nResetPresses == MBT_CFG_RESETBTN_USER_SETTINGS_FACTORY_RESET_N_PRESSES)
    {
        Pod_vibSetEnabled(true);
        Pod_vibSetVibration(MBT_CFG_RESETBTN_USER_SETTINGS_FACTORY_RESET_VIB_LEVEL, MBT_CFG_RESETBTN_USER_SETTINGS_FACTORY_RESET_VIB_TIME_MS);
        Pod_USResetToFactory();
    } else {
        _Pod_USApplyConfig(&g_Pod_RetainedRAM.USFile);
    }
}

// ------------------------------------------------------------------------------
#include "stdio.h"

uint16_t _Pod_USFileCB(uint16_t id, uint8_t evt)
{
#if 0
    {
        uint8_t *p = (uint8_t*)&g_Pod_USFileBuffer;
        printf("TEST\n");
        printf("USERFILECTS: %x %x %x %x %x %x %x %x\n", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
    }
#endif
    if(evt == VFE_FINISHWRITE)
        _Pod_USApplyConfig(&g_Pod_USFileBuffer);
    return sizeof(Pod_USUserSettingsFile_t);
}

// ------------------------------------------------------------------------------

void Pod_USResetToFactory(void)
{
    g_Pod_RetainedRAM.USFile.magic = 0xFF;
    _Pod_USApplyConfig(&g_Pod_RetainedRAM.USFile);
}

// ------------------------------------------------------------------------------

void Pod_USTick(uint32_t ticks)
{
}

// ------------------------------------------------------------------------------

const Pod_USUserSettingsFile_t* Pod_USGetUserSettings()
{
    return &g_Pod_RetainedRAM.USFile;
}

// ------------------------------------------------------------------------------

void Pod_USSetUserSettings(Pod_USUserSettingsFile_t* pData)
{
    pData->crc16 = crc16_compute(((uint8_t*)pData) + sizeof(uint16_t), sizeof(Pod_USUserSettingsFile_t) - sizeof(uint16_t), NULL);    
    pData->version == POD_US_FILE_VERSION;
    pData->magic == POD_US_FILE_MAGIC;
    _Pod_USApplyConfig(pData);
}

// ------------------------------------------------------------------------------
