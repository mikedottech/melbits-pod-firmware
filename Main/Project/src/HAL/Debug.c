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

#ifdef DEBUG
#include "common.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include <stdarg.h>
#include <stdio.h>

#include "SEGGER_RTT.h"

uint32_t _HAL_dbgInit()
{
    uint32_t err_code = 0;
#if NRF_LOG_ENABLED
    NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
    //NRF_LOG_DEBUG("App started\n");
#endif
    return err_code;
}

/*
void HAL_dbgLog(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);  
    vprintf(fmt, args);
    va_end(args);
}*/

void HAL_dbgLog(const char *fmt,...) {
    char buffer[128];
    va_list args;
    va_start (args, fmt);
    int n = vsnprintf(buffer, sizeof(buffer), fmt, args);
    SEGGER_RTT_Write(0, buffer, n);
    va_end(args);
    //return n;
}
#endif