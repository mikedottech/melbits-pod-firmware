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

#ifndef _HAL_FLASH_H_
#define _HAL_FLASH_H_

#include "common.h"
#include "HAL/HAL.h"

#define HAL_FLASH_EVENT_FLAG_SUCCESS    (1)
#define HAL_FLASH_EVENT_FLAG_ERROR      (2)

uint32_t _HAL_flashGetAndClearEventFlags(void);

#endif

