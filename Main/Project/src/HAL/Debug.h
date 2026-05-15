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

#ifndef _HAL_DEBUG_H_
#define _HAL_DEBUG_H_

#include "common.h"

uint32_t _HAL_dbgInit();

extern void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info);

#ifdef DEBUG
//#   ifdef __nRF_FAMILY
//#       include "nrf_assert.h"
#       define MBT_ASSERT(expr)\
    if (expr)                                                                 \
    {                                                                         \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        __builtin_trap();\
    }    
//#   endif

void HAL_dbgLog(const char* fmt, ...);

#   define MBT_LOG(...) HAL_dbgLog(__VA_ARGS__)
/*
#   define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(!!(COND))?1:-1]
// token pasting madness:
#   define MBT_STATIC_ASSERT3(X,L) STATIC_ASSERT(X,static_assertion_at_line_##L)
#   define MBT_STATIC_ASSERT2(X,L) MBT_STATIC_ASSERT3(X,L)
#   define MBT_STATIC_ASSERT(X)    MBT_STATIC_ASSERT2(X,__LINE__)
*/
#else
#   define MBT_ASSERT(cond)
#   define MBT_LOG(...)
/*#   define MBT_STATIC_ASSERT(cond)*/
#endif


#endif