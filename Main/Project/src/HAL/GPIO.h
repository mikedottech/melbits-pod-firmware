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

#ifndef _HAL_GPIO_H_
#define _HAL_GPIO_H_
#include "common.h"

uint32_t _HAL_GPIOInit(void);
//uint32_t _HAL_GPIOSetDigital(uint8_t pin, uint8_t val);
void _HAL_GPIOReset(void);
void _HAL_GPIOSetLockup(void);
void _HAL_gpioCheckSysReset(void);
#endif