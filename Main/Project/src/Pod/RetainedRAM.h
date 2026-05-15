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

#include "mbt_config.h"
#include "Pod/UserSettings.h"
#include "Pod/Playground.h"

// Struct to guarantee the data layout in the .non_init section
typedef struct
{
    Pod_USUserSettingsFile_t    USFile;
    uint8_t                     nResetPresses;    
    Pod_PGRetainedData          PGData;    
} RetainedRAM_t;

extern RetainedRAM_t g_Pod_RetainedRAM;
