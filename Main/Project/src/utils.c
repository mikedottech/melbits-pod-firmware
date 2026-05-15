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

#include "utils.h"

// PREDEFINED COLORS ======================================================
const Utils_RGB888_t k_Utils_ColorBlack = {0};
const Utils_RGB888_t k_Utils_ColorWhite = {255, 255, 255};

/*
 * The two `Utils_getApproximatedDistance2D*` routines below are
 * adapted from SGDK (Sega Genesis Development Kit), `getApproximatedDistance`
 * in `src/maths.c`, by Stéphane Dallongeville
 * (https://github.com/Stephane-D/SGDK), licensed under the MIT License.
 * The 64-bit twin is a type-widening of the same algorithm. The full
 * MIT notice is reproduced at the top of `fpmath.h`. The 3D variant
 * (`Utils_getApproximatedDistance3D`) below is original composition
 * for this firmware (calls the 2D approximation twice).
 */
uint64_t Utils_getApproximatedDistance2D_64(int64_t dx, int64_t dy)
{
    uint64_t min, max;

    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    if (dx < dy )
    {
        min = dx;
        max = dy;
    }
    else
    {
        min = dy;
        max = dx;
    }

    // coefficients equivalent to ( 123/128 * max ) and ( 51/128 * min )
    return ((max << 8) + (max << 3) - (max << 4) - (max << 1) +
             (min << 7) - (min << 5) + (min << 3) - (min << 1)) >> 8;
}


uint32_t Utils_getApproximatedDistance2D(int32_t dx, int32_t dy)
{
    uint32_t min, max;

    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    if (dx < dy )
    {
        min = dx;
        max = dy;
    }
    else
    {
        min = dy;
        max = dx;
    }

    // coefficients equivalent to ( 123/128 * max ) and ( 51/128 * min )
    return ((max << 8) + (max << 3) - (max << 4) - (max << 1) +
             (min << 7) - (min << 5) + (min << 3) - (min << 1)) >> 8;
}

#include <math.h>

uint32_t Utils_getApproximatedDistance3D(int32_t dx, int32_t dy, int32_t dz)
{
    uint32_t d;
    d = Utils_getApproximatedDistance2D(dx, dz);
    d = Utils_getApproximatedDistance2D(d, dy);

    return d;
}

uint32_t Utils_adler32(const void *buf, size_t buflength) {
     const uint8_t *buffer = (const uint8_t*)buf;

     uint32_t s1 = 1;
     uint32_t s2 = 0;

     for (size_t n = 0; n < buflength; n++) {
        s1 = (s1 + buffer[n]) % 65521;
        s2 = (s2 + s1) % 65521;
     }     
     return (s2 << 16) | s1;
}

void Utils_xorBuffer(void* buf, size_t buflength, const void* xKey, size_t xKeyLength)
{
    uint8_t *pBuf = (uint8_t*)buf;
    uint8_t *pxKey = (uint8_t*)xKey;

    for(size_t i = 0; i < buflength; ++i)
        *(pBuf++) ^= pxKey[i % xKeyLength];
}
