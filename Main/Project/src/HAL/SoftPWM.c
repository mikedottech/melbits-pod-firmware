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
#include "mbt_config.h"

#include "app_timer.h"
#include "low_power_pwm.h"

enum
{
    LPP_CH_Q1 = 0,
    LPP_CH_Q2,
    LPP_CH_Q3,
    LPP_CH_Q4,
    LPP_CH_R,
    LPP_CH_G,
    LPP_CH_B,
    LPP_CH_MOTOR,
    //LPP_CH_TEST,
    LPP_CH_NUM,
    LPP_CH_LEDS_LAST = LPP_CH_B,
};

static struct
{
    app_timer_t app_timer;
    low_power_pwm_t lpp;
} s_lppChannels[LPP_CH_NUM] = {0};

static const uint8_t s_kPinNumbers[] = {
    MBT_CFG_PIN_LED_Q1,
    MBT_CFG_PIN_LED_Q2,
    MBT_CFG_PIN_LED_Q3,
    MBT_CFG_PIN_LED_Q4,
    MBT_CFG_PIN_LED_R,
    MBT_CFG_PIN_LED_G,
    MBT_CFG_PIN_LED_B,
    MBT_CFG_PIN_MOTOR,
    //MBT_CFG_PIN_AVDD
};

static const app_timer_id_t app_timer_ids[] =
{
    &s_lppChannels[0].app_timer, &s_lppChannels[1].app_timer,
    &s_lppChannels[2].app_timer, &s_lppChannels[3].app_timer,
    &s_lppChannels[4].app_timer, &s_lppChannels[5].app_timer,
    &s_lppChannels[6].app_timer, &s_lppChannels[7].app_timer,
    //&s_lppChannels[8].app_timer
};

#define PIN_MASK(_pin)  /*lint -save -e504 */                     \
                        (1u << (uint32_t)((_pin) & (~P0_PIN_NUM))) \
                        /*lint -restore    */

#define CHANNEL_DEF(ID, PIN) { .pin = PIN, .pTimer = &lpp_timer_##ID }

uint32_t _HAL_softPWMInit()
{
    uint32_t err_code = 0;
    low_power_pwm_config_t low_power_pwm_config;

    low_power_pwm_config.active_high    = true;
    low_power_pwm_config.period         = 255;
    low_power_pwm_config.p_port         = NRF_GPIO;
    

    for(uint8_t i = 0; i < LPP_CH_NUM; ++i)
    {
        low_power_pwm_config.bit_mask       = PIN_MASK(s_kPinNumbers[i]);        
        low_power_pwm_config.p_timer_id     = &(app_timer_ids[i]);
        err_code = low_power_pwm_init(&(s_lppChannels[i].lpp), &low_power_pwm_config, NULL);
        APP_ERROR_CHECK(err_code);
        err_code = low_power_pwm_duty_set(&(s_lppChannels[i].lpp), 0);
        APP_ERROR_CHECK(err_code);
    }
    return err_code;
}

static uint32_t _HAL_setPWMValue(uint8_t channel, uint8_t level)
{
    ASSERT(channel < LPP_CH_NUM);

    uint8_t err_code = 0;
    uint8_t lastDuty = s_lppChannels[channel].lpp.duty_cycle;
    
    if(level == lastDuty) return 0;

    if(level == 0)
    {
        if(lastDuty > 0)
        {
            low_power_pwm_stop(&s_lppChannels[channel].lpp);            
        }
    } else {
        if(lastDuty == 0)
        {
            low_power_pwm_start((&s_lppChannels[channel].lpp), s_lppChannels[channel].lpp.bit_mask);                
        }
    }

    err_code = low_power_pwm_duty_set(&s_lppChannels[channel].lpp, level);
    APP_ERROR_CHECK(err_code);

    return err_code;
}

uint32_t HAL_LEDSetMultiple(const uint8_t* values)
{
    uint8_t err_code = 0;

    for(uint8_t i = 0; i <= LPP_CH_LEDS_LAST; ++i)
    {
        err_code = _HAL_setPWMValue(i, *(values + i));
        APP_ERROR_CHECK(err_code);
    }

    return err_code;
}

uint32_t HAL_motorSetLevel(uint8_t level)
{
    return _HAL_setPWMValue(LPP_CH_MOTOR, level);
}

uint32_t HAL_avddSetLevel(uint8_t level)
{
    //return _HAL_setPWMValue(LPP_CH_TEST, level);
    return 1;
}
