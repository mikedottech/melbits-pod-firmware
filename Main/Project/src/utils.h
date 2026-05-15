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

#ifndef _UTILS_H_
#define _UTILS_H_

#   include "mbt_config.h"
#   include "common.h"

#   define MBT_ROUNDED_DIV(A, B)   (((A) + ((B) / 2)) / (B))
#   define MBT_TICKS_TO_US(TICKS) (TICKS * MBT_RTC_US_PER_TICK)
#   define MBT_US_TO_TICKS(US) MBT_ROUNDED_DIV(US, MBT_RTC_US_PER_TICK)
#   define MBT_MINUTES_TO_MS(M) (60000 * M)
#   define MBT_SECONDS_TO_MS(M) (1000 * M)
#   define MBT_FLAG_SET(F, MSK) (F) |= (MSK)
#   define MBT_FLAG_IS_SET(F, MSK) (((F) & (MSK)) != 0)
#   define MBT_FLAG_CLEAR(F, MSK) (F) &= ~(MSK)
#   define MBT_NULL_PARAM_CHECK(param)  do { if ((param) == NULL) return 1; } while(0)

#   define MBT_TIMER_INVALID_VALUE                 ((uint32_t)(-1))
#   define MBT_TIMER_ISENABLED(T)                  (T##_timer != MBT_TIMER_INVALID_VALUE)
#   define MBT_TIMER_HASEXPIRED(T, DT)             (MBT_TIMER_ISENABLED(T) && (T##_timer <= DT))
#   define MBT_TIMER_DISABLE(T)                    (T##_timer = MBT_TIMER_INVALID_VALUE)
#   define MBT_TIMER_RESET(T)                      (T##_timer = TIMER_##T##_TIMEOUT)
#   define MBT_TIMER_RESET_TIMEOUT(T, TO)          (T##_timer = TO)
#   define MBT_TIMER_RESET_TIMEOUT_MAX(T, TO)          (T##_timer = (T##_timer > TO) ? T##_timer : TO)
#   define MBT_TIMER_UPDATE(T, DT)                 (T##_timer = (T##_timer == MBT_TIMER_INVALID_VALUE) ? T##_timer : (T##_timer - DT))
#   define MBT_TIMER_DEFINE(T)                     uint32_t T##_timer /*= TIMER_INVALID_VALUE;*/
#   define MBT_UPDATE_TIMER_ACTION_EXPIRED(T, DT, AC)\
        if(MBT_TIMER_ISENABLED(T)){\
            if(MBT_TIMER_HASEXPIRED(T, DT)){\
                MBT_TIMER_DISABLE(T);\
                do {\
                 AC\
                } while(0);\
            } else {\
                MBT_TIMER_UPDATE(T, DT);\
            }\
        }

#   define MBT_CONCAT_2(A, B) A##B
#   define MBT_CONCAT_3(A, B, C) A##B##C

#define MBT_DEF_SHARED_RES(NAME, TYPE, CBUPDATE) \
static TYPE s_##NAME##_sharedClients = 0;\
void NAME##RequestClient(TYPE msk)\
{\
    TYPE lastState = s_##NAME##_sharedClients;\
    s_##NAME##_sharedClients |= msk;\
    if(lastState == 0 && msk != 0)\
        CBUPDATE(!!( s_##NAME##_sharedClients != 0));\
}\
void NAME##ReleaseClient(TYPE msk)\
{\
    TYPE lastState = s_##NAME##_sharedClients;\
    s_##NAME##_sharedClients &= ~(msk);\
    if(lastState != 0 && s_##NAME##_sharedClients == 0)\
        CBUPDATE(!!(s_##NAME##_sharedClients != 0));\
}

typedef struct
{
    uint8_t r : 8;
    uint8_t g : 8;
    uint8_t b : 8;
} MBT_CFG_PLATFORM_PACKED Utils_RGB888_t;

extern const Utils_RGB888_t k_Utils_ColorBlack;
extern const Utils_RGB888_t k_Utils_ColorWhite;


//#ifndef abs
//#   define abs(X) ((X) < 0 ? -(X) : (X))
//#endif


#ifndef MIN
#   define MIN(A, B) (A < B) ? (A) : (B)
#endif

#ifndef MAX
#   define MAX(A, B) (A > B) ? (A) : (B)
#endif

#endif

uint32_t Utils_getApproximatedDistance2D(int32_t dx, int32_t dy);
uint64_t Utils_getApproximatedDistance2D_64(int64_t dx, int64_t dy);
uint32_t Utils_getApproximatedDistance3D(int32_t dx, int32_t dy, int32_t dz);
uint32_t Utils_adler32(const void *buf, size_t buflength);
void Utils_xorBuffer(void* buf, size_t bufLength, const void* xKey, size_t xKeyLength);