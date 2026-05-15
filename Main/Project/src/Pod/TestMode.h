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

#ifndef _POD_TESTMODE_H_
#define _POD_TESTMODE_H_

#include "common.h"

typedef enum
{
    TT_NONE = 0,
    TT_SWEEP
} Pod_TMTestType_t;

void Pod_TMInit(void);
void Pod_TMTick(uint32_t ticks);

void Pod_TMRFTestSetMode(Pod_TMTestType_t mode);

#endif