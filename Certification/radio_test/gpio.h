#ifndef _GPIO_H_
#define _GPIO_H_
    #include <inttypes.h>
    #include <stdbool.h>

// PIN ASSIGNMENTS ===================================
// DIGITAL OUTPUTS -----------------------------------
#define MBT_CFG_PIN_LED_R       (23)
#define MBT_CFG_PIN_LED_G       (20)
#define MBT_CFG_PIN_LED_B       (22)
#define MBT_CFG_PIN_LED_Q1      (26)
#define MBT_CFG_PIN_LED_Q2      (25)
#define MBT_CFG_PIN_LED_Q3      (31)
#define MBT_CFG_PIN_LED_Q4      (24)
#define MBT_CFG_PIN_MOTOR       (19)

#define MBT_CFG_PIN_FLASH_NCS   (5)

#define MBT_CFG_PIN_AUDIO_OUT   (27)
#define MBT_CFG_PIN_AUDIO_PA    (29)

#define MBT_CFG_PIN_AVDD        (30)

// DIGITAL BUSSES ------------------------------------
#define MBT_CFG_PIN_I2C_SCL     (8)
#define MBT_CFG_PIN_I2C_SDA     (7)

#define MBT_CFG_PIN_SPI_SCK     (16)
#define MBT_CFG_PIN_SPI_MISO    (15)
#define MBT_CFG_PIN_SPI_MOSI    (14)

// DIGITAL INPUTS ------------------------------------
#define MBT_CFG_PIN_RESET       (21)

#define MBT_CFG_PIN_ACCELINT1   (12)
#define MBT_CFG_PIN_ACCELINT2   (18)
#define MBT_CFG_PIN_VBUS        (13)
#define MBT_CFG_PIN_BATTCHG     (17)

// ANALOG INPUTS -------------------------------------
#define MBT_CFG_PIN_AIN_LIGHT   (2) //(NRF_SAADC_INPUT_AIN1)
#define MBT_CFG_PIN_AIN_TEMP    (1) //(NRF_SAADC_INPUT_AIN0)
#define MBT_CFG_PIN_AIN_BATT    (5) //(NRF_SAADC_INPUT_AIN4)
#define MBT_CFG_PIN_AIN_ML      (3) //(NRF_SAADC_INPUT_AIN2)

#define MBT_CFG_RF_TEST_BUTTON  (MBT_CFG_PIN_RESET) //(MBT_CFG_PIN_SPI_MISO)

    void gpio_init(void);
    void gpio_setDigital(uint8_t pin, uint8_t val);
    void gpio_toggleDigital(uint8_t pin);
    uint8_t gpio_resetIsPressed(void);
    void HAL_gpioReadDigital(uint8_t pin, uint32_t *data);
    void gpio_reset(void);
    void gpio_setVibration(bool enabled);
#endif
