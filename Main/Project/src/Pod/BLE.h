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

#ifndef _BLE_H_
#define _BLE_H_

#include "common.h"
#include "HAL/HAL.h"
#include "Pod/MLINK.h"

#define POD_BLE_ADV_FLAGS_HASLINKPIN (1 << 0)
#define POD_BLE_ADV_FLAGS_MLDETECTED (1 << 1)
#define POD_BLE_ADV_FLAGS_RECONNECTION (1 << 2)

typedef enum
{
    BLS_OFF = 0,
    BLS_ADVERTISING,
    BLS_CONNECTING,
    BLS_CONNECTED
} Pod_BLEFSM_t;

void Pod_BLEInit(void);
void Pod_BLETick(uint32_t ticks);

void Pod_BLEHALEventHandler(const HAL_event_t *pEventData);

void Pod_BLEMLHALEventHandler(const Pod_MLEventData_t *pData);

Pod_BLEFSM_t Pod_BLEGetState(void);
void Pod_BLEUpdateAdvPacket(void);

void Pod_BLESetInactiveAdvertisementRetries(uint8_t retries);

void Pod_BLEAdvertisingLogic(void);

void Pod_BLESetAdvFlags(uint8_t flags);
void Pod_BLEClearAdvFlags(uint8_t flags);

void Pod_BLEDisconnect(void);

#endif
