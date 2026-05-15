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

#include "HAL.h"

#include "RTC.h"

#include "app_timer.h"

#undef SOFTDEVICE_PRESENT // Until BLE is implemented

#ifndef SOFTDEVICE_PRESENT
#   include "nrf_drv_clock.h"
#endif

APP_TIMER_DEF(s_hal_app_timer);
APP_TIMER_DEF(s_hal_temp_app_timer);

uint32_t s_HAL_rtcCurrentTickIntervalMs = 0;

static void _ISR_HAL_rtcHandler(void * p_context)
{
}

uint32_t _HAL_rtcInit()
{
    ret_code_t ret;
#ifndef SOFTDEVICE_PRESENT    
    ret = nrf_drv_clock_init();
    //APP_ERROR_CHECK(err_code); //NRF_ERROR_MODULE_ALREADY_INITIALIZED is OK
    nrf_drv_clock_lfclk_request(NULL);    
#endif
    ret = app_timer_init();

    APP_ERROR_CHECK(ret);

    // Create and start a timer, otherwise we can't get the elapsed time
    app_timer_create(&s_hal_app_timer, APP_TIMER_MODE_REPEATED, &_ISR_HAL_rtcHandler);
    app_timer_create(&s_hal_temp_app_timer, APP_TIMER_MODE_REPEATED, &_ISR_HAL_rtcHandler);
    //HAL_sysSetTickRate(HAL_SYS_TICKRATE_25HZ);

    return (uint32_t)ret;
}

uint32_t HAL_sysSetTickRate(uint32_t interval_ms)
{
    uint32_t ret = 0;

    ret = app_timer_start(s_hal_temp_app_timer, 0x1000, NULL);
    APP_ERROR_CHECK(ret);

    ret = app_timer_stop(s_hal_app_timer);
    APP_ERROR_CHECK(ret);
            
    uint32_t app_timer_ticks = 0x00FFFFFF; // Max value

    if(interval_ms > 0 && interval_ms != HAL_SYS_TICKRATE_MAX)
    {
        app_timer_ticks = APP_TIMER_TICKS(interval_ms);
    }

    ret = app_timer_start(s_hal_app_timer, app_timer_ticks, NULL);
    APP_ERROR_CHECK(ret);

    ret = app_timer_stop(s_hal_temp_app_timer);
    APP_ERROR_CHECK(ret);
        

    s_HAL_rtcCurrentTickIntervalMs = interval_ms;

    return ret;
}

// PUBLIC =======================================================================

uint32_t HAL_sysGetRawTickCount()
{
    return app_timer_cnt_get();
}

uint32_t HAL_sysGetDeltaTickCount(uint32_t to, uint32_t from)
{
#define MAX_RTC_COUNTER_VAL     0x00FFFFFF                                  /**< Maximum value of the RTC counter. */

    if(to < from)
    {
        return (MAX_RTC_COUNTER_VAL - from) + to;
    } else {
        return (to - from);
    }
    //return app_timer_cnt_diff_compute(to, from);
}

uint32_t _HAL_rtcStopAll()
{
    app_timer_stop_all();
    return 0;
}