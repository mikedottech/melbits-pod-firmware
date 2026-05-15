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

#ifndef _MLDECODER_H_
#define _MLDECODER_H_

#include <inttypes.h>

#define MLDEC_ERROR_PROLOG 1
#define MLDEC_ERROR_PARITY 2
#define MLDEC_ERROR_EPILOG 3
#define MLDEC_ERROR_OTHER  4

typedef struct
{
    uint32_t h, l;
} mldec_WaveCycle_t;

void mldec_decode(mldec_WaveCycle_t c);
void mldec_reset(void);


// CALLBACKS
void mldec_onProlog();
void mldec_onByte(uint8_t data);
void mldec_onError(uint32_t errorType);

#endif
