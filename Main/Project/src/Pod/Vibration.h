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

#ifndef _POD_VIBRATION_H_
#define _POD_VIBRATION_H_

#include "common.h"

void Pod_vibInit(void);
void Pod_vibTick(uint32_t ticks);
void Pod_vibSetVibration(uint8_t level, uint16_t time_ms);
void Pod_vibSetEnabled(bool enabled);

#endif
