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

#ifndef _HAL_CLOCKS_H_
#define _HAL_CLOCKS_H_
#include "common.h"

#define HAL_CLK_CLIENT_LPPWM    (0x1)   // Uses LFCLK (used by lppwm -> relies on app_timer)
#define HAL_CLK_CLIENT_PWM      (0x2)   // Uses HFCLK (used by PWM audio)
#define HAL_CLK_CLIENT_ANALOG_TIMER   (0x4)   // Uses HFCLK (used by TIMER module needed by Analog)
#define HAL_CLK_CLIENT_RTC      (0x8)   // Uses LFCLK (used by app_timer)

void _HAL_clkHFSharedRequestClient(uint8_t mask);
void _HAL_clkHFSharedReleaseClient(uint8_t mask);

bool _HAL_clkHFIsRunning(void);


#endif