/*
 * fxpt_atan2.h
 *
 * Copyright (C) 2012, Xo Wang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef _FXPT_ATAN2_H_
#define _FXPT_ATAN2_H_

#include <inttypes.h>

/*
 * The fix16 type and helper macros below (FIX16_INT_BITS,
 * FIX16_FRAC_BITS, FIX16, intToFix16, fix16ToInt) are adapted from
 * SGDK (Sega Genesis Development Kit), inc/maths.h, by Stéphane
 * Dallongeville (https://github.com/Stephane-D/SGDK), licensed under
 * the MIT License. The full MIT notice is reproduced in
 * `Main/Project/src/fpmath.h`. The integer-bit width has been changed
 * to 1 (Q15) to match the normalized [-1, 1] range used by the atan2
 * routine. The atan2 implementation itself remains under Xo Wang's
 * MIT license as stated above.
 */
#define FIX16_INT_BITS              1
#define FIX16_FRAC_BITS             (16 - FIX16_INT_BITS)

typedef int16_t fix16;

#define FIX16(value)                ((fix16) ((value) * (1 << FIX16_FRAC_BITS)))

/**
 *  \brief
 *      Convert integer to fix16.
 */
#define intToFix16(value)           ((value) << FIX16_FRAC_BITS)
/**
 *  \brief
 *      Convert fix16 to integer.
 */
#define fix16ToInt(value)           ((value) >> FIX16_FRAC_BITS)

uint16_t fxpt_atan2(const int16_t y, const int16_t x);

#endif