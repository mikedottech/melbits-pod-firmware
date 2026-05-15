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

#include "nrf_gpio.h"

uint32_t g_HAL_GPIOLatch;
bool g_HAL_GPIOVBUSState;

static void _HAL_GPIOLEDInit(uint32_t pin)
{
    nrf_gpio_cfg(
        pin,
        NRF_GPIO_PIN_DIR_OUTPUT,
        NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL,
        NRF_GPIO_PIN_S0H1,
        NRF_GPIO_PIN_NOSENSE);
}

void _HAL_GPIOReset(void)
{
    static const uint8_t k_pinList[] = {MBT_CFG_PIN_AUDIO_PA, MBT_CFG_PIN_LED_R, MBT_CFG_PIN_LED_G,
                                    MBT_CFG_PIN_LED_B, MBT_CFG_PIN_LED_Q1, MBT_CFG_PIN_LED_Q2, MBT_CFG_PIN_LED_Q3, MBT_CFG_PIN_LED_Q4
                                    , 0xFF,
                                    MBT_CFG_PIN_AVDD, MBT_CFG_PIN_FLASH_NCS, MBT_CFG_PIN_SPI_SCK, MBT_CFG_PIN_SPI_MISO, MBT_CFG_PIN_SPI_MOSI,
                                    MBT_CFG_PIN_I2C_SDA, MBT_CFG_PIN_I2C_SCL};
    
    bool set = false;

    for(uint8_t i = 0; i < sizeof(k_pinList) / sizeof(uint8_t); ++i)
    {
        uint8_t cur = k_pinList[i];
        if(cur == 0xFF)
        {
            set = true;
        } else {
            if(set)
            {
                nrf_gpio_pin_set(cur);
            } else {
                nrf_gpio_pin_clear(cur);
            }
        }
    }
}

void _HAL_GPIOSetLockup(void)
{
    NVIC_DisableIRQ(GPIOTE_IRQn);
    _HAL_GPIOReset();
    nrf_gpio_cfg_input(MBT_CFG_PIN_ACCELINT1, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_input(MBT_CFG_PIN_ACCELINT2, NRF_GPIO_PIN_NOPULL);    
    NRF_P0->LATCH   = 0xFFFFFFFF; // Reset the latch
}

static void _HAL_GPIOConfigVBUS(void)
{
    g_HAL_GPIOVBUSState ^= 1;
    nrf_gpio_cfg_sense_input(MBT_CFG_PIN_VBUS, NRF_GPIO_PIN_NOPULL, g_HAL_GPIOVBUSState ? NRF_GPIO_PIN_SENSE_LOW : NRF_GPIO_PIN_SENSE_HIGH);
    
}

uint32_t _HAL_GPIOInit(void)
{
    static const uint8_t k_pinList[] = {// These are output pins (standard drive)
                                        MBT_CFG_PIN_AVDD, MBT_CFG_PIN_AUDIO_PA, MBT_CFG_PIN_FLASH_NCS,
                                        MBT_CFG_PIN_SPI_SCK, MBT_CFG_PIN_SPI_MISO, MBT_CFG_PIN_SPI_MOSI,
                                        MBT_CFG_PIN_I2C_SDA, MBT_CFG_PIN_I2C_SCL
                                        , 0xFF,
                                        // These are sense input pins with no pull-up using PORT events on HIGH level
                                        MBT_CFG_PIN_ACCELINT1, MBT_CFG_PIN_ACCELINT2, MBT_CFG_PIN_VBUS/*, MBT_CFG_PIN_BATTCHG*/};

    bool out = true;

    ret_code_t err_code;

    g_HAL_GPIOLatch = 0;

    // Prevent spurious interrupts
    NRF_GPIOTE->INTENCLR = GPIOTE_INTENSET_PORT_Msk | GPIOTE_INTENSET_IN0_Msk;

    for(uint8_t i = 0; i < sizeof(k_pinList) / sizeof(uint8_t); ++i)
    {
        uint8_t cur = k_pinList[i];
        if(cur == 0xFF)        
            out = false;
        else {
            if(out)
                nrf_gpio_cfg_output(cur);
            else
                nrf_gpio_cfg_sense_input(cur, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_SENSE_HIGH);
        }
    }

    // We don't need interrupts in BATTCHG as it will be polled continuously when VBUS is present
    nrf_gpio_cfg_input(MBT_CFG_PIN_BATTCHG, NRF_GPIO_PIN_NOPULL);

    // High drive
    static const uint8_t table[] = {MBT_CFG_PIN_LED_R, MBT_CFG_PIN_LED_G, MBT_CFG_PIN_LED_B, MBT_CFG_PIN_LED_Q1, MBT_CFG_PIN_LED_Q2, MBT_CFG_PIN_LED_Q3, MBT_CFG_PIN_LED_Q4};
    for(uint8_t i = 0; i < sizeof(table) / sizeof(uint8_t); ++i)
        _HAL_GPIOLEDInit(table[i]);

    // RESET is important, so it uses a PIN event
    nrf_gpio_cfg_sense_input(MBT_CFG_PIN_RESET, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);

    NRF_GPIOTE->CONFIG[0] = ((MBT_CFG_PIN_RESET << GPIOTE_CONFIG_PSEL_Pos) & GPIOTE_CONFIG_PSEL_Msk) | (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos) | (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos);

    // Use the latch for PORT events
    NRF_P0->DETECTMODE = GPIO_DETECTMODE_DETECTMODE_LDETECT << GPIO_DETECTMODE_DETECTMODE_Pos;

    _HAL_GPIOReset();

    g_HAL_GPIOVBUSState = nrf_gpio_pin_read(MBT_CFG_PIN_VBUS);
    _HAL_GPIOConfigVBUS();

    NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Msk | GPIOTE_INTENSET_IN0_Msk;

    NVIC_EnableIRQ(GPIOTE_IRQn);

    return 0;
}

uint32_t HAL_gpioWriteDigital(uint8_t pin, bool val)
{
    if(val)
        nrf_gpio_pin_set(pin);
    else
        nrf_gpio_pin_clear(pin);
    return 0;
}

void HAL_gpioReadDigital(uint8_t pin, uint32_t *data)
{
    *data = nrf_gpio_pin_read(pin);
}

#ifdef MBT_CFG_ACCEL_HANDLE_GPIO_ON_INT
#   include "Accel.h"
#endif

//#define MBT_CFG_DISABLE_RESET

static void HAL_gpioResetLogic(void)
{
#ifndef MBT_CFG_DISABLE_RESET
    // Do system reset immediately
#if SOFTDEVICE_PRESENT
    sd_nvic_SystemReset();
#else
    NVIC_SystemReset();
#endif
#else    
#warning Macro MBT_CFG_DISABLE_RESET is ENABLED. The RESET button is disabled. This build is NOT SUITABLE FOR PRODUCTION!
#endif
}

void _HAL_gpioCheckSysReset(void)
{
    if(nrf_gpio_pin_read(MBT_CFG_PIN_RESET) == 0)
    {
        HAL_gpioResetLogic();
    }
}


void GPIOTE_IRQHandler(void)
{
    // RESET button pressed ?
    if(NRF_GPIOTE->EVENTS_IN[0])
    {
        NRF_GPIOTE->EVENTS_IN[0] = 0;
        HAL_gpioResetLogic();
    }

    // Any other input signal turned HIGH?
    if(NRF_GPIOTE->EVENTS_PORT)
    {
	NRF_GPIOTE->EVENTS_PORT = 0;
        g_HAL_GPIOLatch = NRF_P0->LATCH;        
    }

#ifdef MBT_CFG_ACCEL_HANDLE_GPIO_ON_INT
    _HAL_accelHandleGPIO(g_HAL_GPIOLatch);
#endif

    // VBUS can't be cleared, so invert the sense to avoid blocking the DETECT signal
    if(NRF_P0->LATCH & (1 << MBT_CFG_PIN_VBUS))
    {
        //MBT_LOG("TOGGLE %d -> %d\n", g_HAL_GPIOVBUSState, (g_HAL_GPIOVBUSState ^ 1));
        _HAL_GPIOConfigVBUS();
        //nrf_gpio_cfg_sense_set(MBT_CFG_PIN_VBUS, nrf_gpio_pin_read(MBT_CFG_PIN_VBUS) ? NRF_GPIO_PIN_SENSE_LOW : NRF_GPIO_PIN_SENSE_HIGH);
    }    
    HAL_sysSchedulePendingTick();
}
