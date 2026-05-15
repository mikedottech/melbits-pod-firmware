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

#ifndef _POD_STREAMSERVER_H_
#define _POD_STREAMSERVER_H_

#include "common.h"
#include "HAL/HAL.h"

// Must be contiguous and follow this order
#define POD_SS_LIGHT_ANALOG_CH_ID   (4)
#define POD_SS_TEMP_ANALOG_CH_ID    (5)
#define POD_SS_ML_ANALOG_CH_ID      (6)

#define POD_SS_MIN_ANALOG_CH_ID (POD_SS_LIGHT_ANALOG_CH_ID)

enum
{
// Must be contiguous and follow this order
  SS_MSK_ITEM_ACCEL = 0x1,
  SS_MSK_ITEM_LIGHT = 0x2,
  SS_MSK_ITEM_TEMP  = 0x4,

  SS_MSK_ITEM_ML    = 0x8,
  SS_MSK_ITEM_MAX   = 0x10,

  SS_MSK_ITEM_ALL   = 0x7
};

void Pod_SSInit(void);
void Pod_SSTick(uint32_t tick);

void Pod_SSReset(void);
void Pod_SSSetStreamMask(uint8_t msk);

void Pod_SSAnalogHandler(uint8_t clientID, HAL_analogSample_t *pBuffer, uint8_t count);

void Pod_SSTXSlotHandler(void);


extern HAL_analogSample_t g_Pod_SSLightBuffer;
extern HAL_analogSample_t g_Pod_SSTempBuffer;
extern HAL_analogSample_t g_Pod_SSMLBuffer;
#endif
