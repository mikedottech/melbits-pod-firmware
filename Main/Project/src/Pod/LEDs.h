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

#ifndef _LEDS_H_
#define _LEDS_H_

#include "common.h"
#include "utils.h"
#include "fpmath.h"

#define POD_LED_Q1      (0)
#define POD_LED_Q2      (1)
#define POD_LED_Q3      (2)
#define POD_LED_Q4      (3)
#define POD_LED_R       (4)
#define POD_LED_G       (5)
#define POD_LED_B       (6)
#define POD_LED_COUNT   (7)

typedef struct
{
    uint8_t remainingHalfCycles;
    uint8_t idleLevel;
    uint8_t sourceLevel;
    uint8_t targetLevel;
    uint32_t tween_ticks;
    uint32_t elapsed_ticks;
    uint8_t flags;
} Pod_LEDState_t;

#define POD_LEDS_STATE_SIZE (sizeof(Pod_LEDState_t))

extern const fix32_t k_Pod_LEDSpeedOneSecond;
extern const fix32_t k_Pod_LEDSpeedHalfSecond;
extern const fix32_t k_Pod_LEDSpeedQuarterSecond;
extern const fix32_t k_Pod_LEDSpeedSixteenthSecond;

extern const Utils_RGB888_t k_Pod_LEDColorBlack;
extern const Utils_RGB888_t k_Pod_LEDColorWhite;
extern const Utils_RGB888_t k_Pod_LEDColorRed;
extern const Utils_RGB888_t k_Pod_LEDColorGreen;
extern const Utils_RGB888_t k_Pod_LEDColorPurple;

void Pod_LEDsInit(void);
void Pod_LEDsTick(uint32_t ticks);

void Pod_LEDsSetAnimation(uint8_t id, uint8_t from, uint8_t to, uint8_t nHalfCycles, uint16_t time_ms);
void Pod_LEDsSetIdleLevel(uint8_t id, uint8_t level);
void Pod_LEDsSetLevel(uint8_t id, uint8_t level, uint16_t time_ms);

bool Pod_LEDsAnimationsInProgress(void);

void Pod_LEDsCenterSetAnimation(const Utils_RGB888_t *pFrom, const Utils_RGB888_t *pTo, uint8_t nHalfCycles, uint16_t time_ms);
void Pod_LEDsCenterSetIdleColor(const Utils_RGB888_t *pColor);
void Pod_LEDsCenterSetColor(const Utils_RGB888_t *pColor, uint16_t time_ms);
void Pod_LEDsSetMute(bool muted);

void Pod_LEDsGetStateCopy(uint8_t *pState);
void Pod_LEDsSetState(uint8_t *pState);

void Pod_LEDsSetDimmingEnabled(bool enabled);

#endif