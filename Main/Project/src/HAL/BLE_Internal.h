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

#ifndef _HAL_BLE_INTERNAL_H_
#define _HAL_BLE_INTERNAL_H_

#include "nrf_atomic.h"
#include "HAL.h"

#define APP_BLE_CONN_CFG_TAG            (1)                       /**< A tag identifying the SoftDevice BLE configuration. */

extern uint16_t s_HALbleConnHandle;

extern nrf_atomic_u32_t s_HALbleEventFlags;

void _ISR_HAL_bleNusOnDisconnect(void);

uint32_t _HAL_bleAssignConnectionHandleToQWR(uint16_t handle);
void _HALbleAdvTransitionToState(HAL_bleAdvState_t next);
uint32_t _HAL_bleServicesInit(void);

uint32_t _HAL_bleGetAndClearEventFlags(void);

#endif
