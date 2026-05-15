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

#include "Clocks.h"
#include "nrf_drv_clock.h"
#include "nrf_soc.h"
#include "utils.h"

static void _HAL_clkHFUpdate(bool enable)
{
    if(enable)
    {
        /* Start 16 MHz crystal oscillator */
#if SOFTDEVICE_PRESENT
        sd_clock_hfclk_request();
#else
        NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
        NRF_CLOCK->TASKS_HFCLKSTART = 1;
#endif
    } else {
        /* Stop 16 MHz crystal oscillator */
#if SOFTDEVICE_PRESENT
        sd_clock_hfclk_release();  
#else
        NRF_CLOCK->TASKS_HFCLKSTOP = 1;
#endif
    }
}

MBT_DEF_SHARED_RES(_HAL_clkHFShared, uint8_t, _HAL_clkHFUpdate)

bool _HAL_clkHFIsRunning(void)
{
#if SOFTDEVICE_PRESENT
    uint32_t running = 0;
    sd_clock_hfclk_is_running(&running);
    return !!running;
#else
    return !!(NRF_CLOCK->EVENTS_HFCLKSTARTED);
#endif
}
