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

#include "I2C.h"

#include "HAL.h"
#include "mbt_config.h"
#include "nrf_twi_mngr.h"

NRF_TWI_MNGR_DEF(s_nrfTwiMngr, MBT_CFG_MAX_PENDING_TRANSACTIONS, MBT_CFG_TWI_INSTANCE_ID);

const nrf_twi_mngr_t *g_pnrfTwiMngr = &s_nrfTwiMngr;

uint32_t _HAL_i2cInit(void)
{
    uint32_t err_code;

    nrf_drv_twi_config_t const config = {
       .scl                = MBT_CFG_PIN_I2C_SCL,
       .sda                = MBT_CFG_PIN_I2C_SDA,
       .frequency          = NRF_DRV_TWI_FREQ_400K,
       .interrupt_priority = APP_IRQ_PRIORITY_LOWEST,
       .clear_bus_init     = true
    };

    err_code = nrf_twi_mngr_init(&s_nrfTwiMngr, &config);
    APP_ERROR_CHECK(err_code);
    
    return err_code;
}

uint32_t _HAL_i2cUninit(void)
{
    nrf_twi_mngr_uninit(&s_nrfTwiMngr);
    return 0;
}