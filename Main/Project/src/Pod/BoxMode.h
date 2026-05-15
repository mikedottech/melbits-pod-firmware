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

#ifndef _POD_BOXMODE_H_
#define _POD_BOXMODE_H_

#include "common.h"

typedef enum
{
    BMR_NONE,
    BMR_BATTDEAD,
    BMR_HWPANIC,
    BMR_COUNT
} Pod_BMReason_t;

void Pod_BMInit(void);
void Pod_BMTick(uint32_t ticks);
void Pod_BMEnter(Pod_BMReason_t reason);
void Pod_BMEnterImmediately(void);

#endif
