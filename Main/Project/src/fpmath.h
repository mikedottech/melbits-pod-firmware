/*
 * Fixed-point math primitives — fix16 / fix32 helpers.
 *
 * Adapted from SGDK (Sega Genesis Development Kit), inc/maths.h:
 *   Copyright (c) 2011 Stéphane Dallongeville
 *   https://github.com/Stephane-D/SGDK
 *   Licensed under the MIT License (see below).
 *
 * Adaptation for the Melbits POD firmware (2017-2020) by
 * Miguel Angel Exposito while employed by Melbot Studios, S.L.
 * This adaptation is distributed under the same MIT License as
 * the upstream SGDK code.
 *
 * ---------------------------------------------------------------
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * ---------------------------------------------------------------
 */

#ifndef _FPMATH_H_
#define _FPMATH_H_

#include "common.h"

typedef int16_t fix16_t;
typedef int32_t fix32_t;

#ifndef PI
/**
 *  \brief
 *      PI number (3,1415..)
 */
#define PI 3.14159265358979323846
#endif


#define FIX32_INT_BITS              22
#define FIX32_FRAC_BITS             (32 - FIX32_INT_BITS)

#define FIX32_INT_MASK              (((1 << FIX32_INT_BITS) - 1) << FIX32_FRAC_BITS)
#define FIX32_FRAC_MASK             ((1 << FIX32_FRAC_BITS) - 1)

#define FIX32_PRINT_FORMAT          "%d.%05lu"
#define FIX32_PRINT_ARG(F)         fix32ToInt(F), ((fix32_t)(F & FIX32_FRAC_MASK) * 100000 / (1 << FIX32_FRAC_BITS))

/**
 *  \brief
 *      Convert specified value to fix32.
 *
 *  EX:<br>
 *      f32 v = FIX32(34.567);
 */
#define FIX32(value)                ((fix32_t) ((value) * (1 << FIX32_FRAC_BITS)))

/**
 *  \brief
 *      Convert integer to fix32.
 */
#define intToFix32(value)           ((value) << FIX32_FRAC_BITS)
/**
 *  \brief
 *      Convert fix32 to integer.
 */
#define fix32ToInt(value)           ((value) >> FIX32_FRAC_BITS)

/**
 *  \brief
 *      Round the specified value to nearest integer (fix32).
 */
#define fix32Round(value)           \
    ((fix32Frac(value) > FIX32(0.5))?fix32Int(value + FIX32(1)) + 1:fix32Int(value))

/**
 *  \brief
 *      Round and convert the specified fix32 value to integer.
 */
#define fix32ToRoundedInt(value)    \
    ((fix32Frac(value) > FIX32(0.5))?fix32ToInt(value) + 1:fix32ToInt(value))

/**
 *  \brief
 *      Return fractional part of the specified value (fix32).
 */
#define fix32Frac(value)            ((value) & FIX32_FRAC_MASK)
/**
 *  \brief
 *      Return integer part of the specified value (fix32).
 */
#define fix32Int(value)             ((value) & FIX32_INT_MASK)

/**
 *  \brief
 *      Compute and return the result of the addition of val1 and val2 (fix32).
 */
#define fix32Add(val1, val2)        ((val1) + (val2))
/**
 *  \brief
 *      Compute and return the result of the substraction of val2 from val1 (fix32).
 */
#define fix32Sub(val1, val2)        ((val1) - (val2))
/**
 *  \brief
 *      Return negate of specified value (fix32).
 */
#define fix32Neg(value)             (0 - (value))

/**
 *  \brief
 *      Compute and return the result of the multiplication of val1 and val2 (fix32).<br>
 *      WARNING: result can easily overflow so its recommended to stick with fix16 type for mul and div operations.
 */
#define fix32Mul(val1, val2)        (((val1) >> (FIX32_FRAC_BITS / 2)) * ((val2) >> (FIX32_FRAC_BITS / 2)))
/**
 *  \brief
 *      Compute and return the result of the division of val1 by val2 (fix32).<br>
 *      WARNING: result can easily overflow so its recommended to stick with fix16 type for mul and div operations.
 */
#define fix32Div(val1, val2)        (((val1) << (FIX32_FRAC_BITS / 2)) / ((val2) >> (FIX32_FRAC_BITS / 2)))


#define FIX16_INT_BITS              10
#define FIX16_FRAC_BITS             (16 - FIX16_INT_BITS)

#define FIX16_INT_MASK              (((1 << FIX16_INT_BITS) - 1) << FIX16_FRAC_BITS)
#define FIX16_FRAC_MASK             ((1 << FIX16_FRAC_BITS) - 1)

#define FIX16_PRINT_FORMAT          "%d.%05lu"
#define FIX16_PRINT_ARG(F)         fix16ToInt(F), ((int32_t)(F & FIX16_FRAC_MASK) * 100000 / (1 << FIX16_FRAC_BITS))


/**
 *  \brief
 *      Convert specified value to fix16
 *
 *  EX:<br>
 *      f16 v = FIX16(-27.12);
 */
#define FIX16(value)                ((fix16_t) ((value) * (1 << FIX16_FRAC_BITS)))

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

/**
 *  \brief
 *      Round the specified value to nearest integer (fix16).
 */
#define fix16Round(value)           \
    (fix16Frac(value) > FIX16(0.5))?fix16Int(value + FIX16(1)) + 1:fix16Int(value)

/**
 *  \brief
 *      Round and convert the specified fix16 value to integer.
 */
#define fix16ToRoundedInt(value)    \
    (fix16Frac(value) > FIX16(0.5))?fix16ToInt(value) + 1:fix16ToInt(value)

/**
 *  \brief
 *      Return fractional part of the specified value (fix16).
 */
#define fix16Frac(value)            ((value) & FIX16_FRAC_MASK)
/**
 *  \brief
 *      Return integer part of the specified value (fix16).
 */
#define fix16Int(value)             ((value) & FIX16_INT_MASK)

/**
 *  \brief
 *      Compute and return the result of the addition of val1 and val2 (fix16).
 */
#define fix16Add(val1, val2)        ((val1) + (val2))
/**
 *  \brief
 *      Compute and return the result of the substraction of val2 from val1 (fix16).
 */
#define fix16Sub(val1, val2)        ((val1) - (val2))
/**
 *  \brief
 *      Return negate of specified value (fix16).
 */
#define fix16Neg(value)             (0 - (value))

/**
 *  \brief
 *      Compute and return the result of the multiplication of val1 and val2 (fix16).
 */
#define fix16Mul(val1, val2)        (((val1) * (val2)) >> FIX16_FRAC_BITS)
/**
 *  \brief
 *      Compute and return the result of the division of val1 by val2 (fix16).
 */
#define fix16Div(val1, val2)        (((val1) << FIX16_FRAC_BITS) / (val2))


#if (MATH_BIG_TABLES != 0)

/**
 *  \brief
 *      Compute and return the result of the Log2 of specified value (fix16).
 */
#define fix16Log2(v)                log2tab16[v]
/**
 *  \brief
 *      Compute and return the result of the Log10 of specified value (fix16).
 */
#define fix16Log10(v)               log10tab16[v]
/**
 *  \brief
 *      Compute and return the result of the root square of specified value (fix16).
 */
#define fix16Sqrt(v)                sqrttab16[v]

#endif


/**
 *  \brief
 *      Convert specified fix32 value to fix16.
 */
#define fix32ToFix16(value)         (((value) << FIX16_FRAC_BITS) >> FIX32_FRAC_BITS)
/**
 *  \brief
 *      Convert specified fix16 value to fix32.
 */
#define fix16ToFix32(value)         (((value) << FIX32_FRAC_BITS) >> FIX16_FRAC_BITS)


#endif