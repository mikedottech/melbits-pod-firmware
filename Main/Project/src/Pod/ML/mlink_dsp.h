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

#ifndef _MLINK_DSP_H_
#define _MLINK_DSP_H_

#include "common.h"
#include "HAL/HAL.h"
#include "MLDecoder.h"
#include <limits.h>

#define HALANALOGSAMPLE_MIN (SHRT_MIN)
#define HALANALOGSAMPLE_MAX (SHRT_MAX)


void mldsp_reset(void);
void mldsp_feedSamples(HAL_analogSample_t* buffer, uint16_t len);

// CALLBACKS
void mldsp_onWaveCycle(mldec_WaveCycle_t cycle);

#endif