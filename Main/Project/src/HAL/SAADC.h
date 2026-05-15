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

#ifndef _HAL_SAADC_H_
#define _HAL_SAADC_H_
#include "common.h"

#define HAL_ANALOG_FLAG_CALDONE  (0x1)
#define HAL_ANALOG_FLAG_CONVDONE (0x2)

uint32_t _HAL_analogInit(void);
uint32_t _HAL_analogTick(void);

uint32_t _HAL_analogGetAndClearEventFlags(void);

uint32_t _HAL_analogForceStop(void);

#endif
