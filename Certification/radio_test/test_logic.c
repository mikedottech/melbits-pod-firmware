
#include "app_timer.h"
#include "test_logic.h"
#include "gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_clock.h"
#include "radio_test.h"

#define LED_MASK_R      (0x1)
#define LED_MASK_G      (0x2)
#define LED_MASK_B      (0x4)
#define LED_MASK_RGB    (uint8_t)(LED_MASK_R | LED_MASK_G | LED_MASK_B)

#define STATE_MIN       (1)
#define STATE_MAX       (14)

#define TXPOWER (RADIO_TXPOWER_TXPOWER_Neg4dBm)

static uint8_t s_testStep;
static uint8_t s_ledPostScaler;
static uint8_t s_currentLedPostScaler;
static uint8_t s_ledMask;

static uint32_t s_sleepTimer = 0;

#define SLEEP_TIMEOUT_100MS (18000) // 30 min
//#define SLEEP_TIMEOUT_100MS (50) // 5 sec

extern uint32_t g_resetIsPressed;
static uint8_t s_resetPressedCounter = 0;

extern volatile transmit_pattern_t g_pattern;                           /**< Radio transmission pattern. */

extern void gotoSleep(void);

static void testlogic_transitionToState(uint8_t nextState);

extern void feedWatchdog(void);

APP_TIMER_DEF(s_LEDTimer);

static void testLogic_toggleUnmaskedLEDs(void)
{
    if(s_ledMask & LED_MASK_R) gpio_toggleDigital(MBT_CFG_PIN_LED_R);
    if(s_ledMask & LED_MASK_G) gpio_toggleDigital(MBT_CFG_PIN_LED_G);
    if(s_ledMask & LED_MASK_B) gpio_toggleDigital(MBT_CFG_PIN_LED_B);
}

static void testlogic_nextState(void)
{
    if(s_testStep >= STATE_MAX)
    {
        testlogic_transitionToState(STATE_MIN);
    } else {
        testlogic_transitionToState(s_testStep + 1);
    }
}

static void testlogic_setLEDMask(uint8_t msk)
{
    s_ledMask = msk;
    gpio_setDigital(MBT_CFG_PIN_LED_R, 0);
    gpio_setDigital(MBT_CFG_PIN_LED_G, 0);
    gpio_setDigital(MBT_CFG_PIN_LED_B, 0);    
}


static void testlogic_ledHandler(void * p_context)
{
    feedWatchdog();
    if(s_sleepTimer ++ >= SLEEP_TIMEOUT_100MS)
    {
        gotoSleep();
        return;
    }
    if(s_ledPostScaler > 0 && ++s_currentLedPostScaler >= s_ledPostScaler)
    {
        // Timeout
        s_currentLedPostScaler = 0;
        testLogic_toggleUnmaskedLEDs();
    }

    HAL_gpioReadDigital(MBT_CFG_RF_TEST_BUTTON, &g_resetIsPressed);

    //g_resetIsPressed = nrf_gpio_pin_read(MBT_CFG_PIN_RESET);

    if(!g_resetIsPressed)
    {
        ++s_resetPressedCounter;

        if (s_resetPressedCounter == 20)
        {
            testlogic_setLEDMask(0);
            nrf_delay_ms(100);
            uint32_t t;
            do
            {
                HAL_gpioReadDigital(MBT_CFG_RF_TEST_BUTTON, &t);
            } while(!t);

            nrf_delay_ms(100);

            gotoSleep();
            //NVIC_SystemReset();

        }

    } else {
        
        if(s_resetPressedCounter >= 1 && s_resetPressedCounter < 20)
        {
            testlogic_nextState();
        } 
        s_resetPressedCounter = 0;
    }
}



static void testlogic_setLEDRate(uint16_t ms)
{
    s_ledPostScaler = ms / 100;
    s_currentLedPostScaler = 0;       
}


static void testlogic_setLEDRateAndMask(uint16_t ms, uint8_t msk)
{
    testlogic_setLEDRate(ms);
    testlogic_setLEDMask(msk);
//    if(ms == 0)
    {
        testLogic_toggleUnmaskedLEDs();
    }
}

static uint32_t testlogic_calcChannel(uint8_t bleChannel)
{
    // Base is 2400 MHz
    return (2*bleChannel);
}

static void testlogic_transitionToState(uint8_t nextState)
{
    s_sleepTimer = 0;
    radio_sweep_end();

    switch(nextState)
    {
        case 0:
        break;
        case 1:
            // TX CW, 2402 MHz,  4dBm
            // RED 1 second on / 1 second off
            testlogic_setLEDRateAndMask(1000, LED_MASK_R);
            radio_unmodulated_tx_carrier(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(1));
        break;
        case 2:
            // TX CW, 2440 MHz,  4dBm
            // RED 0.5second on / 0.5second off
            testlogic_setLEDRateAndMask(500, LED_MASK_R);
            radio_unmodulated_tx_carrier(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(20));
        break;
        case 3:
            // TX CW, 2480 MHz,  4dBm
            // RED 0.2second on / 0.2second off
            testlogic_setLEDRateAndMask(200, LED_MASK_R);
            radio_unmodulated_tx_carrier(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(40));
        break;
        case 4:
            // TX MOD, 2402 MHz  4dBm
            // green 1 second on / 1 second off
            testlogic_setLEDRateAndMask(1000, LED_MASK_G);
            //radio_modulated_tx_carrier(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(1));
            radio_modulated_tx_carrier_duty_cycle(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(1), 50);
        break;
        case 5:
            // TX MOD, 2440 MHz  4dBm
            // green 0.5second on / 0.5second off
            testlogic_setLEDRateAndMask(500, LED_MASK_G);
            //radio_modulated_tx_carrier(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(20));
            radio_modulated_tx_carrier_duty_cycle(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(20), 50);
        break;
        case 6:
            // TX MOD, 2480 MHz  4dBm
            // green 0.2second on / 0.2second off
            testlogic_setLEDRateAndMask(200, LED_MASK_G);
            //radio_modulated_tx_carrier(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(40));
            radio_modulated_tx_carrier_duty_cycle(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(40), 50);
        break;
        case 7:
            // RX  2402 MHz Standard gain
            // blue 1 second on / 1 second off
            testlogic_setLEDRateAndMask(1000, LED_MASK_B);
            radio_rx(RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(1));
        break;
        case 8:
            // RX  2440 MHz Standard gain
            // blue 0.5second on / 0.5second off
            testlogic_setLEDRateAndMask(500, LED_MASK_B);
            radio_rx(RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(20));
        break;
        case 9:
            // RX  2480 MHz Standard gain
            // blue 0.2second on / 0.2second off
            testlogic_setLEDRateAndMask(200, LED_MASK_B);
            radio_rx(RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(40));
        break;
        case 10:
            // 2402 Brust
            // Red   light is always on
            testlogic_setLEDRateAndMask(0, LED_MASK_R);
            //radio_modulated_tx_carrier_duty_cycle(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(1), 100);
            radio_modulated_tx_carrier(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(1));
        break;
        case 11:
            // 2440 Brust
            // green light is always on
            testlogic_setLEDRateAndMask(0, LED_MASK_G);
            //radio_modulated_tx_carrier_duty_cycle(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(20), 100);
            radio_modulated_tx_carrier(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(20));
        break;
        case 12:
            // 2480 Brust
            // blue  light is always on
            testlogic_setLEDRateAndMask(0, LED_MASK_B);
            //radio_modulated_tx_carrier_duty_cycle(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(40), 100);
            radio_modulated_tx_carrier(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(40));
        break;
        case 13:
            // HOPPING MODE
            // White 1 second on / 1 second off
            testlogic_setLEDRateAndMask(1000, LED_MASK_RGB);
            radio_tx_sweep_start(TXPOWER, RADIO_MODE_MODE_Nrf_1Mbit, testlogic_calcChannel(1), testlogic_calcChannel(40), 10);
        break;
        case 14:
            // Finish TEST
            // White light is always on
            testlogic_setLEDRateAndMask(0, LED_MASK_RGB);
        break;
    }
    s_testStep = nextState;
}

void testlogic_init(void)
{
    s_testStep = 0;
    s_ledPostScaler = 1;
    s_ledMask = LED_MASK_RGB;
    s_resetPressedCounter = 0;
    app_timer_create(&s_LEDTimer, APP_TIMER_MODE_REPEATED, testlogic_ledHandler);
    app_timer_start(s_LEDTimer, APP_TIMER_TICKS(100), NULL);

    g_pattern = TRANSMIT_PATTERN_11001100;

    testlogic_transitionToState(1);
}

