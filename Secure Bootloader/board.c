#include "board.h"
#include "nrf_gpio.h"

static void led_init(uint32_t pin)
{
    nrf_gpio_cfg(
        pin,
        NRF_GPIO_PIN_DIR_OUTPUT,
        NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL,
        NRF_GPIO_PIN_S0H1,
        NRF_GPIO_PIN_NOSENSE);
}

void board_init(void)
{
    //nrf_gpio_cfg_sense_input
    ret_code_t err_code;

    // ========= UNUSED PINS AS INPUT =========
    for(uint32_t i = 2; i < 32; ++i)
    {
        if(i == NRF_BL_DFU_ENTER_METHOD_BUTTON_PIN) continue;
        nrf_gpio_cfg(
            i,
            NRF_GPIO_PIN_DIR_INPUT,
            NRF_GPIO_PIN_INPUT_DISCONNECT,
            NRF_GPIO_PIN_NOPULL,
            NRF_GPIO_PIN_H0H1,
            NRF_GPIO_PIN_NOSENSE);
    }

    // ========= LEDS =========================
    // High drive    
    const uint32_t table[] = {OUTID_R, OUTID_G, OUTID_B, OUTID_N, OUTID_S, OUTID_E, OUTID_W};
    for(uint32_t i = 0; i < 7; ++i)
    {
        led_init(table[i]);
    }



    // ========= RESET BUTTON? ================
    // Done by the bootloader
    
}

void board_setDefaultOutputs(void)
{
    board_setLED(OUTID_R, 0);
    board_setLED(OUTID_G, 0);
    board_setLED(OUTID_B, 0);
    board_setLED(OUTID_N, 0);
    board_setLED(OUTID_S, 0);
    board_setLED(OUTID_E, 0);
    board_setLED(OUTID_W, 0);
    board_setLED(OUTID_SPK, 0);
    board_setLED(OUTID_VIBRATION, 0);
}

void board_setLED(cmp_outID_t id, uint8_t state)
{
    switch(state)
    {
        case 0:
            nrf_gpio_pin_clear(id);
        break;
        case 1:
            nrf_gpio_pin_set(id);
        break;
        default:
            nrf_gpio_pin_toggle(id);
        break;
    }
}

void board_setupWakeupSources(void)
{
    // ========= INTERRUPT LINES ==============

        // Wakeup from power off sources:
        //  - RESET button
        //  - USB detect
        // TODO:- Accelerometer (TODO)        
        /*
        nrf_gpio_cfg_sense_input(INID_ACCEL1_IRQ,
                                NRF_GPIO_PIN_NOPULL,
                                ACCELEROMETER_IRQ_POLARITY);                             
        nrf_gpio_cfg_sense_input(INID_ACCEL2_IRQ,
                                NRF_GPIO_PIN_NOPULL,
                                ACCELEROMETER_IRQ_POLARITY);
                                */

        nrf_gpio_input_disconnect(INID_ACCEL1_IRQ);
        nrf_gpio_input_disconnect(INID_ACCEL2_IRQ);
        nrf_gpio_input_disconnect(INID_BATTCHG_IRQ);     
           
        nrf_gpio_cfg_sense_input(NRF_BL_DFU_ENTER_METHOD_BUTTON_PIN,
                                 NRF_GPIO_PIN_PULLUP /*BUTTON_PULL*/,
                                 NRF_GPIO_PIN_SENSE_LOW);

        nrf_gpio_cfg_sense_input(INID_USB_DETECT_IRQ,
                                NRF_GPIO_PIN_NOPULL,
                                NRF_GPIO_PIN_SENSE_HIGH);        
        
}

