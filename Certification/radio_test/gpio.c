
#include "gpio.h"
#include "nrf_gpio.h"
#include <stdbool.h>

#include "nrf_gpio.h"

uint32_t g_HAL_GPIOLatch;

volatile uint8_t g_resetIsPressed;

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
    //TODO: System to control AVDD?
    static const uint8_t k_pinList[] = {MBT_CFG_PIN_AUDIO_PA, MBT_CFG_PIN_LED_R, MBT_CFG_PIN_LED_G,
                                    MBT_CFG_PIN_LED_B, MBT_CFG_PIN_LED_Q1, MBT_CFG_PIN_LED_Q2, MBT_CFG_PIN_LED_Q3, MBT_CFG_PIN_LED_Q4
                                    , 0xFF,
                                    MBT_CFG_PIN_AVDD, MBT_CFG_PIN_FLASH_NCS, MBT_CFG_PIN_SPI_SCK, MBT_CFG_PIN_SPI_MISO, MBT_CFG_PIN_SPI_MOSI};
    
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

uint32_t _HAL_GPIOInit(void)
{
    static const uint8_t k_pinList[] = {// These are output pins (standard drive)
                                        MBT_CFG_PIN_AVDD, MBT_CFG_PIN_AUDIO_PA, MBT_CFG_PIN_FLASH_NCS, MBT_CFG_PIN_MOTOR,
                                        MBT_CFG_PIN_SPI_SCK, MBT_CFG_PIN_SPI_MISO, MBT_CFG_PIN_SPI_MOSI
                                        , 0xFF,
                                        // These are sense input pins with no pull-up using PORT events on HIGH level
                                        /*MBT_CFG_PIN_ACCELINT1, MBT_CFG_PIN_ACCELINT2,*/ MBT_CFG_PIN_VBUS/*, MBT_CFG_PIN_BATTCHG*/};

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

    nrf_gpio_cfg_sense_input(MBT_CFG_RF_TEST_BUTTON, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);    

    // We don't need interrupts in BATTCHG as it will be polled continuously when VBUS is ptnt
    nrf_gpio_cfg_input(MBT_CFG_PIN_BATTCHG, NRF_GPIO_PIN_NOPULL);

    // High drive
    static const uint8_t table[] = {MBT_CFG_PIN_LED_R, MBT_CFG_PIN_LED_G, MBT_CFG_PIN_LED_B, MBT_CFG_PIN_LED_Q1, MBT_CFG_PIN_LED_Q2, MBT_CFG_PIN_LED_Q3, MBT_CFG_PIN_LED_Q4};
    for(uint8_t i = 0; i < sizeof(table) / sizeof(uint8_t); ++i)
        _HAL_GPIOLEDInit(table[i]);

    // RESET is important, so it uses a PIN event
    nrf_gpio_cfg_sense_input(MBT_CFG_PIN_RESET, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);

    //NRF_GPIOTE->CONFIG[0] = ((MBT_CFG_PIN_RESET << GPIOTE_CONFIG_PSEL_Pos) & GPIOTE_CONFIG_PSEL_Msk) | (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos) | (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos);

    // Use the latch for PORT events
    //NRF_P0->DETECTMODE = GPIO_DETECTMODE_DETECTMODE_LDETECT << GPIO_DETECTMODE_DETECTMODE_Pos;

    _HAL_GPIOReset();

    nrf_gpio_cfg_output((uint32_t)MBT_CFG_PIN_I2C_SCL);
    nrf_gpio_cfg_output((uint32_t)MBT_CFG_PIN_I2C_SDA);
    
    // Battery
    nrf_gpio_cfg(
        28,
        NRF_GPIO_PIN_DIR_INPUT,
        NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL,
        NRF_GPIO_PIN_S0H1,
        NRF_GPIO_PIN_NOSENSE);

    // Audio
    nrf_gpio_cfg(
        MBT_CFG_PIN_AUDIO_OUT,
        NRF_GPIO_PIN_DIR_INPUT,
        NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL,
        NRF_GPIO_PIN_S0H1,
        NRF_GPIO_PIN_NOSENSE);


    nrf_gpio_pin_set(MBT_CFG_PIN_I2C_SDA);
    nrf_gpio_pin_set(MBT_CFG_PIN_I2C_SCL);


    //NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Msk | GPIOTE_INTENSET_IN0_Msk;

    //NVIC_EnableIRQ(GPIOTE_IRQn);

    return 0;
}

uint32_t HAL_gpioWriteDigital(uint8_t pin, bool val)
{
    if(val)
        nrf_gpio_pin_set(pin);
    else
        nrf_gpio_pin_clear(pin);
}

void HAL_gpioReadDigital(uint8_t pin, uint32_t *data)
{
    *data = nrf_gpio_pin_read(pin);
}

void GPIOTE_IRQHandler(void)
{
    // RESET button pressed ?
    if(NRF_GPIOTE->EVENTS_IN[0])
    {
        // Do system reset immediately
        //NVIC_SystemReset();
        // TODO: FIX THIS
        //g_resetIsPressed = nrf_gpio_pin_read(MBT_CFG_PIN_RESET);
    }

    // Any other input signal turned HIGH?
//    if(NRF_GPIOTE->EVENTS_PORT)
//    {
//	NRF_GPIOTE->EVENTS_PORT = 0;
//        g_HAL_GPIOLatch = NRF_P0->LATCH;        
//    }
//
//    // VBUS can't be cleared, so invert the sense to avoid blocking the DETECT signal
//    if(NRF_P0->LATCH & (1 << MBT_CFG_PIN_VBUS))
//        nrf_gpio_cfg_sense_set(MBT_CFG_PIN_VBUS, nrf_gpio_pin_read(MBT_CFG_PIN_VBUS) ? NRF_GPIO_PIN_SENSE_LOW : NRF_GPIO_PIN_SENSE_HIGH);
}

void gpio_init(void)
{
    _HAL_GPIOInit();
}

void gpio_setDigital(uint8_t pin, uint8_t val)
{
    HAL_gpioWriteDigital(pin, (bool)val);
}

void gpio_toggleDigital(uint8_t pin)
{
    nrf_gpio_pin_toggle(pin);
}

uint8_t gpio_resetIsPressed(void)
{
    return g_resetIsPressed;
}

void gpio_reset(void)
{
    _HAL_GPIOReset();
}

void gpio_setVibration(bool enabled)
{
    if(enabled)
    {
        nrf_gpio_pin_set(MBT_CFG_PIN_MOTOR);
    } else {
        nrf_gpio_pin_clear(MBT_CFG_PIN_MOTOR);
    }
}