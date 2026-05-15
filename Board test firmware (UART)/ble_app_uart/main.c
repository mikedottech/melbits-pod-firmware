/**
/**
 * Copyright (c) 2014 - 2019, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup ble_sdk_uart_over_ble_main main.c
 * @{
 * @ingroup  ble_sdk_app_nus_eval
 * @brief    UART over BLE application main file.
 *
 * This file contains the source code for a sample application that uses the Nordic UART service.
 * This application uses the @ref srvlib_conn_params module.
 */

#include "sdk_config.h"


#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_drv_saadc.h"
#include "nrf_drv_pwm.h"
#include "nrf_drv_twi.h"
#include "nrf_delay.h"

#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "app_timer.h"
#include "ble_nus.h"
#if UART_ENABLED
#include "app_uart.h"
#endif
#include "app_util_platform.h"
#include "app_gpiote.h"
#include "bsp_btn_ble.h"
#include "app_pwm.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_drv_clock.h"
#include "low_power_pwm.h"

#include "testcmdparser.h"

#if UART_ENABLED
#if defined (UART_PRESENT)
#include "nrf_uart.h"
#endif
#if defined (UARTE_PRESENT)
#include "nrf_uarte.h"
#endif
#endif

#include "nrfx_comp.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "drv_speaker.h"

#include "Poppler/LED.h"
#include "Poppler/FaceAnimation.h"

#include "Audio/synth.h"
#include "Audio/mel.h"

#include "mlink_dsp.h"
#include "MLDecoder.h"

#include "nrf_twi_mngr.h"
#include "SC7A20.h"

//#include "app_scheduler.h"

//#define COMP_TEST
//#define ENABLE_BT

#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */

#define DEVICE_NAME                     "MELBOT TEST"                               /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */

#define APP_ADV_INTERVAL                64                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */

#define APP_ADV_DURATION                12000   /*500*/                                    /**< The advertising duration (120 seconds) in units of 10 milliseconds. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(75, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define UART_TX_BUF_SIZE                256                                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE                256                                         /**< UART RX buffer size. */

#define SAMPLES_IN_BUFFER               32

static volatile uint8_t s_saadc_running = 0;

BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                                   /**< BLE NUS service instance. */
NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                             /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static uint16_t   m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};

//APP_PWM_INSTANCE(PWM1,1);                   // Create the instance "PWM1" using TIMER1.

APP_TIMER_DEF(m_repeated_timer_id);     

//#define TWI_INSTANCE_ID     0
/* TWI instance. */
//static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);

#define TWI_INSTANCE_ID             0
#define MAX_PENDING_TRANSACTIONS    5

NRF_TWI_MNGR_DEF(m_nrf_twi_mngr, MAX_PENDING_TRANSACTIONS, TWI_INSTANCE_ID);

const nrf_twi_mngr_t *gp_nrf_twi_mngr = &m_nrf_twi_mngr;

void twi_event_handler(nrf_drv_twi_evt_t const * p_event,
                       void *                    p_context);

void HALLEDSetState(const HALLEDColor* value, uint8_t index);
void HALLEDToggle(uint8_t index);

void accel2();
void accel3();

static void setdigital(uint32_t pin, cmp_activation_t act);

static low_power_pwm_t low_power_pwm_0; //R
static low_power_pwm_t low_power_pwm_1; //G
static low_power_pwm_t low_power_pwm_2; //B

static low_power_pwm_t low_power_pwm_3; //M
static low_power_pwm_t low_power_pwm_4; //Q1
static low_power_pwm_t low_power_pwm_5; //Q2
static low_power_pwm_t low_power_pwm_6; //Q3
static low_power_pwm_t low_power_pwm_7; //Q4

static SC7A20_t s_accel;

uint16_t USER_i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, uint16_t len);
#ifdef FACTORY
// 0x208
// Enable debug port protection
const uint32_t __attribute__((used)) __attribute__((section(".uicr_appprotect"))) UICR_ADDR_0x208  __attribute__ ((aligned (4))) = 0x00000000;
#endif

/**@brief Function for assert macro callback.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

static bool tmpToggle = false;

static void repeated_timer_handler(void * p_context)
{
/*
    static int16_t toggleCount = 0;
    if(tmpToggle)
    {
        tmpToggle = false;
        toggleCount = 1;
    }

    if(toggleCount-- >= 0)
    {
        nrf_gpio_pin_toggle(OUTID_G);     
    }
*/
    
    HALLEDToggle(0);
}


static char s_printBuffer[256];

static void send_string(const char* fmt, ...)
{    
    va_list args;
    va_start(args, fmt);

    vsnprintf(&s_printBuffer[0], 255, fmt, args);
    s_printBuffer[255] = 0;
    uint16_t len = strlen(s_printBuffer);
    ret_code_t err_code = ble_nus_data_send(&m_nus, s_printBuffer, &len, m_conn_handle);
    printf("%s", s_printBuffer);
    va_end (args);
}

/**@brief Function for initializing the timer module.
 */
static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_repeated_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                repeated_timer_handler);
    APP_ERROR_CHECK(err_code);

}

void gpio_reset()
{
    nrf_gpio_pin_clear(OUTID_AVDD); //nrf_gpio_pin_set(OUTID_AVDD);

    nrf_gpio_pin_set(OUTID_FLASH_nCS);
    nrf_gpio_pin_set(OUTID_SPI_SCK);
    nrf_gpio_pin_set(OUTID_SPI_MISO);
    nrf_gpio_pin_set(OUTID_SPI_MOSI);
    nrf_gpio_pin_clear(OUTID_SPK);
    nrf_gpio_pin_clear(OUTID_R);
    nrf_gpio_pin_clear(OUTID_G);
    nrf_gpio_pin_clear(OUTID_B);
    nrf_gpio_pin_clear(OUTID_N);
    nrf_gpio_pin_clear(OUTID_S);
    nrf_gpio_pin_clear(OUTID_E);
    nrf_gpio_pin_clear(OUTID_W);
    
}

void accel_off();

void app_prepare_reset()
{
    // Turns everything off
        
    //accel_off(); //TEMP

    low_power_pwm_stop((&low_power_pwm_0));
    low_power_pwm_stop((&low_power_pwm_1));
    low_power_pwm_stop((&low_power_pwm_2));
    low_power_pwm_stop((&low_power_pwm_3));
    low_power_pwm_stop((&low_power_pwm_4));
    low_power_pwm_stop((&low_power_pwm_5));
    low_power_pwm_stop((&low_power_pwm_6));
    low_power_pwm_stop((&low_power_pwm_7));
    
    gpio_reset();

    //nrfx_twim_uninit(&(m_twi.u.twim));
    nrf_twi_mngr_uninit(&m_nrf_twi_mngr);
    nrfx_comp_uninit();
    nrf_drv_saadc_uninit();
    nrfx_clock_lfclk_stop();
    
    nrf_gpio_cfg_output((uint32_t)OUTID_I2C_SDA);
    nrf_gpio_cfg_output((uint32_t)OUTID_I2C_SCL);
    
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
        OUTID_AUDIO,
        NRF_GPIO_PIN_DIR_INPUT,
        NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL,
        NRF_GPIO_PIN_S0H1,
        NRF_GPIO_PIN_NOSENSE);


    nrf_gpio_pin_set(OUTID_I2C_SDA);
    nrf_gpio_pin_set(OUTID_I2C_SCL);

    drv_speaker_uninit();
    nrf_drv_gpiote_out_task_disable(OUTID_AUDIO);
    app_timer_stop_all();

}

static void indicate_advertising(bool isAdvertising)
{
    ret_code_t err_code;
    if(isAdvertising)
    {
        err_code = app_timer_start(m_repeated_timer_id, APP_TIMER_TICKS(500), NULL);
    } else {
        err_code = app_timer_stop(m_repeated_timer_id);
        //nrf_gpio_pin_clear(OUTID_G); 
        HALLEDColor c = {0};
        HALLEDSetState(&c, 0);
    }
    APP_ERROR_CHECK(err_code);
}



/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_evt       Nordic UART Service event.
 */
/**@snippet [Handling the data received over BLE] */
static void nus_data_handler(ble_nus_evt_t * p_evt)
{

    if (p_evt->type == BLE_NUS_EVT_RX_DATA)
    {
        uint32_t err_code;

        NRF_LOG_DEBUG("Received data from BLE NUS. Writing data on UART.");
        NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);

        /*
        for (uint32_t i = 0; i < p_evt->params.rx_data.length; i++)
        {            
            do
            {            
                err_code = app_uart_put(p_evt->params.rx_data.p_data[i]);
                if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_BUSY))
                {
                    NRF_LOG_ERROR("Failed receiving NUS message. Error 0x%x. ", err_code);
                    APP_ERROR_CHECK(err_code);
                }            
            } while (err_code == NRF_ERROR_BUSY);            
        }
        */

        cmp_feed_data(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
        /*
        if (p_evt->params.rx_data.p_data[p_evt->params.rx_data.length - 1] == '\r')
        {
            while (app_uart_put('\n') == NRF_ERROR_BUSY);
        }*/
    }

}
/**@snippet [Handling the data received over BLE] */


/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t           err_code;
    ble_nus_init_t     nus_init;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    uint32_t err_code = 0;
/*
    uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);
*/
    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    app_prepare_reset();

#if 1
    //do
    //{
        err_code = sd_power_system_off();
    //} while (err_code != 0);
    
    NVIC_SystemReset(); // Should not get here
    #else

while(1)
{
#if NRF_LOG_ENABLED
    if (NRF_LOG_PROCESS() == false)
#endif
    {
        nrf_pwr_mgmt_run();
    }

}   
#endif

//    if(err_code != NRF_ERROR_SOC_POWER_OFF_SHOULD_NOT_RETURN) 
//        APP_ERROR_CHECK(err_code);
        
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            indicate_advertising(true);
            //err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            //APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:            
            sleep_mode_enter();
            break;
        default:
            break;
    }
}


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t err_code = 0;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            indicate_advertising(false);
            NRF_LOG_INFO("Connected");
            //err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            indicate_advertising(true);
            NRF_LOG_INFO("Disconnected");
            // LED indication will be changed when advertising starts.
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}


/**@brief Function for handling events from the GATT library. */
void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}


/**@brief Function for initializing the GATT library. */
void gatt_init(void)
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
 /*
void bsp_event_handler(bsp_event_t event)
{
    uint32_t err_code;
    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
            break;

        case BSP_EVENT_DISCONNECT:
            err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BSP_EVENT_WHITELIST_OFF:
            if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
            {
                err_code = ble_advertising_restart_without_whitelist(&m_advertising);
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
            }
            break;

        default:
            break;
    }
}
*/

/**@brief   Function for handling app_uart events.
 *
 * @details This function will receive a single character from the app_uart module and append it to
 *          a string. The string will be be sent over BLE when the last character received was a
 *          'new line' '\n' (hex 0x0A) or if the string has reached the maximum data length.
 */
/**@snippet [Handling the data received over UART] */
#if UART_ENABLED
void uart_event_handle(app_uart_evt_t * p_event)
{
    static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
    static uint8_t index = 0;
    uint32_t       err_code;

    switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:
            UNUSED_VARIABLE(app_uart_get(&data_array[index]));
            index++;

            if ((data_array[index - 1] == '\n') ||
                (data_array[index - 1] == '\r') ||
                (index >= m_ble_nus_max_data_len))
            {
                if (index > 1)
                {
                    NRF_LOG_DEBUG("Ready to send data over BLE NUS");
                    NRF_LOG_HEXDUMP_DEBUG(data_array, index);

                    do
                    {
                        uint16_t length = (uint16_t)index;
                        err_code = ble_nus_data_send(&m_nus, data_array, &length, m_conn_handle);
                        if ((err_code != NRF_ERROR_INVALID_STATE) &&
                            (err_code != NRF_ERROR_RESOURCES) &&
                            (err_code != NRF_ERROR_NOT_FOUND))
                        {
                            APP_ERROR_CHECK(err_code);
                        }
                    } while (err_code == NRF_ERROR_RESOURCES);
                }

                index = 0;
            }
            break;

        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;

        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;

        default:
            break;
    }
}
#endif
/**@snippet [Handling the data received over UART] */


/**@brief  Function for initializing the UART module.
 */
/**@snippet [UART Initialization] */
#if UART_ENABLED
static void uart_init(void)
{
    uint32_t                     err_code;
    app_uart_comm_params_t const comm_params =
    {
        .rx_pin_no    = RX_PIN_NUMBER,
        .tx_pin_no    = TX_PIN_NUMBER,
        .rts_pin_no   = RTS_PIN_NUMBER,
        .cts_pin_no   = CTS_PIN_NUMBER,
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity   = false,
#if defined (UART_PRESENT)
        .baud_rate    = NRF_UART_BAUDRATE_115200
#else
        .baud_rate    = NRF_UARTE_BAUDRATE_115200
#endif
    };

    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_event_handle,
                       APP_IRQ_PRIORITY_LOWEST,
                       err_code);
    APP_ERROR_CHECK(err_code);
}
#endif
/**@snippet [UART Initialization] */


/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t               err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;

    init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;
    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}


/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
 /*
static void buttons_leds_init(bool * p_erase_bonds)
{
    bsp_event_t startup_event;

    uint32_t err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}
*/

/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    uint32_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}


static void setdigital(uint32_t pin, cmp_activation_t act)
{
    HALLEDColor c = {0};
    uint8_t ledIdx = 0;

    switch(pin)
    {
        case OUTID_R:            
            c.r = (act == ACTIVATION_ON ? 255 : (act == ACTIVATION_OFF ? 0 : (c.r > 0 ? 255 : 0)));
            ledIdx = 0;
            if(act == ACTIVATION_TOGGLE) HALLEDToggle(ledIdx);
            else HALLEDSetState(&c, ledIdx);
        break;
        case OUTID_G:
            c.g = (act == ACTIVATION_ON ? 255 : (act == ACTIVATION_OFF ? 0 : (c.g > 0 ? 255 : 0)));
            ledIdx = 0;
            if(act == ACTIVATION_TOGGLE) HALLEDToggle(ledIdx);
            else HALLEDSetState(&c, ledIdx);
        break;
        case OUTID_B:
            c.b = (act == ACTIVATION_ON ? 255 : (act == ACTIVATION_OFF ? 0 : (c.b > 0 ? 255 : 0)));
            ledIdx = 0;
            if(act == ACTIVATION_TOGGLE) HALLEDToggle(ledIdx);
            else HALLEDSetState(&c, ledIdx);
        break;
        case OUTID_N:
            c.r = c.g = c.b = (act == ACTIVATION_ON ? 255 : (act == ACTIVATION_OFF ? 0 : (c.r > 0 ? 255 : 0)));
            ledIdx = 1;
            if(act == ACTIVATION_TOGGLE) HALLEDToggle(ledIdx);
            else HALLEDSetState(&c, ledIdx);
        break;
        case OUTID_S:
            c.r = c.g = c.b = (act == ACTIVATION_ON ? 255 : (act == ACTIVATION_OFF ? 0 : (c.r > 0 ? 255 : 0)));
            ledIdx = 3;
            if(act == ACTIVATION_TOGGLE) HALLEDToggle(ledIdx);
            else HALLEDSetState(&c, ledIdx);
            break;
        case OUTID_E:
            c.r = c.g = c.b = (act == ACTIVATION_ON ? 255 : (act == ACTIVATION_OFF ? 0 : (c.r > 0 ? 255 : 0)));
            ledIdx = 2;
            if(act == ACTIVATION_TOGGLE) HALLEDToggle(ledIdx);
            else HALLEDSetState(&c, ledIdx);
            break;
        case OUTID_W:
            c.r = c.g = c.b = (act == ACTIVATION_ON ? 255 : (act == ACTIVATION_OFF ? 0 : (c.r > 0 ? 255 : 0)));
            ledIdx = 4;
            if(act == ACTIVATION_TOGGLE) HALLEDToggle(ledIdx);
            else HALLEDSetState(&c, ledIdx);
            break;
        default:
            switch(act)
            {
                case ACTIVATION_OFF:
                    nrf_gpio_pin_clear(pin);
                break;
                case ACTIVATION_ON:
                    nrf_gpio_pin_set(pin);
                break;
                default:
                    nrf_gpio_pin_toggle(pin);
                break;
            }
        break;
    }
}

static void readdigital(uint32_t pin)
{
    uint32_t result = nrf_gpio_pin_read(pin);
    send_string("DIGITAL RESULT: %d\r\n", result);
}


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

volatile bool s_readIntSource = false;

void in_pin_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    ret_code_t err_code;

    if(pin == INID_RESET && action == NRF_GPIOTE_POLARITY_HITOLO)
    {        
        // Reset
        app_prepare_reset();
        NVIC_SystemReset();
    } else 
    if(pin == INID_USB)
    {
        send_string(">> USB EVENT -> %d\n", (uint32_t)nrf_drv_gpiote_in_is_set((uint32_t)INID_USB));
    } else
    if(pin == INID_ACCELINT1)
    {
        //send_string(">> ACCEL EVENT\n");
        //NRFX_CRITICAL_SECTION_ENTER();
        //s_readIntSource = true;
        //NRFX_CRITICAL_SECTION_EXIT();

        _isr_sc7a20_io_event_handler(&s_accel, SC7A20_IRQ_INT1);
    } else if(pin == INID_ACCELINT2)
    {
        _isr_sc7a20_io_event_handler(&s_accel, SC7A20_IRQ_INT2);
    }
}

#define MLINK_EPSILON (1000) //(1200) //(400) // TODO: Make it depend on the detected bit period?

typedef struct
{
    uint32_t h, l;
} MLWaveCycle;

typedef struct
{
    uint32_t h1, l1;
    uint32_t h2, l2;
    uint16_t inBuffer;
    uint8_t state;
    uint8_t substate;
    uint32_t bph, bpl;
    uint32_t eh, el;    // Epsilon (high and low)
    uint8_t parity;
    MLWaveCycle buffer[2];
    uint8_t ins;
    uint8_t cap;
} MLINKState_t;

static MLINKState_t mlink = {0};

enum
{
    MD_PROLOG = 0,
    MD_BIT0, MD_BIT1, MD_BIT2, MD_BIT3, MD_BIT4, MD_BIT5, MD_BIT6, MD_BIT7,
    MD_PARITY,
    MD_EPILOG
};

static bool mlink_push(uint32_t l, uint32_t h)
{
    mlink.ins = (mlink.ins + 1) & 0x1;
    mlink.buffer[mlink.ins].h = h;
    mlink.buffer[mlink.ins].l = l;    
    if(mlink.cap < 2) ++ mlink.cap;
}

static MLWaveCycle mlink_peek(uint32_t offset)
{
    MLWaveCycle ret = mlink.buffer[mlink.ins];
    uint8_t idx = (mlink.ins - offset) & 0x1;
    ret.h = mlink.buffer[idx].h;
    ret.l = mlink.buffer[idx].l;
    return ret;
}

static void mlink_flush()
{
    mlink.cap = 0;
}

static bool mlink_in_range(uint32_t val, uint32_t period)
{
    return (abs(val - period) <= MLINK_EPSILON);
}

static void mlink_decoder_reset(void)
{
    memset(&mlink, 0, sizeof(MLINKState_t));
}

static void mlink_snap(uint32_t *v)
{
    uint32_t flr = *v / 4166;
    uint32_t tmp = flr;    
    flr *= 4166;
    if(*v - flr >= /*(4166/2)*/ 2749)    // 66%
    {
        *v = flr + 4166;
        ++tmp;
    }
    else *v = flr;

    if(*v < 4166) *v = 4166;        // Min = At least 1 frame
    if(*v > 4166*3) *v = 4166*3;    // Max = 3 fames

    send_string("%d, ", tmp);
}

static void mlink_decode(uint32_t h, uint32_t l)
{

    if(mlink.state == MD_PROLOG || mlink.state == MD_EPILOG)
    {
        // Snap prolog and epilog
        send_string("[");
        mlink_snap(&l);
        mlink_snap(&h);
        mlink_push(l, h);
        send_string("]\n");
    }
    bool quit = true;

    do
    {
        quit = true;
        switch(mlink.state)
        {
            case MD_PROLOG:
            {   
                // At least 2 cycles in buffer required
                if(mlink.cap < 2) break;

                mlink.h1 = 0;

                MLWaveCycle c2 = mlink_peek(0);
                MLWaveCycle c1 = mlink_peek(1);

                if( mlink_in_range(c2.l, c1.l >> 1) &&
                    mlink_in_range(c2.h, c1.h >> 1) &&
                    mlink_in_range(c1.h, c1.l) &&
                    mlink_in_range(c2.h, c2.l))
                {
                    // PROLOG RECOGNIZED
                    ++mlink.state;
                    mlink_flush();
                    send_string("MLINK [PROLOG OK]\n");
                } else {
                    // BAD PROLOG                    
                    // Nothing to do
                    send_string("MLINK [BAD PROLOG] (diffH = %d, diffL = %d)\n", ((c1.h >> 1) - c2.h), ((c1.l >> 1) - c2.l));
                }

                /*
                if(mlink.h1 == 0)
                {
                    mlink.h1 = h;
                    mlink.l1 = l;
                } else {
                    mlink.h2 = h;
                    mlink.l2 = l;
                
                    mlink.bph = h;
                    mlink.bpl = l;                
                
                    if( mlink_in_range(mlink.l1, mlink.l2 >> 1) &&
                        mlink_in_range(mlink.h1, mlink.h2 >> 1))
                    {
                        // PROLOG RECOGNIZED
                        //++mlink.state;
                    } else {
                        // BAD PROLOG                    
                        // Nothing to do
                        send_string("MLINK [BAD PROLOG]\n");
                    }   
                    mlink.h1 = 0;
                }*/
            }
            break;
            case MD_BIT0:
            case MD_BIT1:
            case MD_BIT2:
            case MD_BIT3:
            case MD_BIT4:
            case MD_BIT5:
            case MD_BIT6:
            case MD_BIT7:
            case MD_PARITY:
            {                
                uint8_t bit;
                if(h > l) bit = 1;
                else bit = 0;

                send_string("MLINK [BIT %d] -> %d\n", mlink.state, bit);

                if(mlink.state != MD_PARITY)
                {
                    mlink.inBuffer |= bit << (mlink.state - 1);
                } else {
                    mlink.parity = bit;                    
                    //mlink.inBuffer <<= 1;
                    //mlink.inBuffer |= bit & 0x1;
                }
                ++mlink.state;
            }
            break;
            case MD_EPILOG:
            {
                if(mlink.h1 == 0)
                {
                    mlink.h1 = h;
                    mlink.l1 = l;
                } else {
                    mlink.h2 = h;
                    mlink.l2 = l;
                
                    if( mlink_in_range(mlink.l1, mlink.l2) &&
                        mlink_in_range(mlink.h1, mlink.h2))
                    {
                        // EPILOG RECOGNIZED
                        // Check parity                    
                        uint8_t temp = mlink.inBuffer;
                        uint8_t p = 0;
                        for(uint8_t i = 0; i < 8; ++i)
                        {
                            if(temp & 0x1)
                            {
                                ++p;
                            }
                            temp >>= 1;
                        }
                    
                        p = (p % 2 == 0);

                        if(p == mlink.parity)
                        {
                            // Notify received bit or error
                            send_string("MLINK [EPILOG OK!]\n");
                            send_string("===================== MLINK BYTE: 0x%x =======================\n", mlink.inBuffer);
                            mlink_flush();
                        } else {
                            send_string("MLINK [BAD PARITY!]\n");
                        }                    
                    } else {
                        // BAD EPILOG
                        // Nothing to do
                        send_string("MLINK [BAD EPILOG]\n");
                    }   
                    //mlink_decoder_reset();
                    mlink.state = MD_PROLOG;
                    mlink.inBuffer = 0;
                    quit = false; // Check prolog again
                }            
            }
            break;
        }
    } while(!quit);
}

void comp_event_handler(nrf_comp_event_t event)
{
    uint32_t tt = NRF_TIMER1->CC[1];
    uint32_t l = NRF_TIMER1->CC[0];
    uint32_t h = tt - l;
    static uint32_t last = 0;
    cmp_activation_t act = ACTIVATION_OFF;
    uint32_t now = app_timer_cnt_get();
    uint32_t diff = app_timer_cnt_diff_compute(now, last);
        
    uint32_t t = 0;

    static int n = 0;
    const char* str;
    if(event == NRF_COMP_EVENT_UP)
    {
        str = "UP";
        act = ACTIVATION_ON;
        //t = NRF_TIMER1->CC[0];
    } else if (event == NRF_COMP_EVENT_DOWN)
    {
        str = "DOWN";
        act = ACTIVATION_OFF;
        //t = NRF_TIMER1->CC[1];
    } else if (event == NRF_COMP_EVENT_CROSS){
        str = "CROSS"; 
        return;
    } else if (event == NRF_COMP_EVENT_READY){
        str = "READY";
        return;
    } else {
        str = "UNKNOWN";
        return;
    }
    
    last = now;

#ifdef COMP_TEST
    setdigital(OUTID_G, act);
#else
#endif

    //printf("[%d] >> COMP EVENT %d (%s)\n", n++, (int32_t)event, str);
    if(act == ACTIVATION_OFF)
    {        
        
        //diff = NRF_TIMER1->CC[1] - NRF_TIMER1->CC[0];
        send_string(">> ML -> %d <l: %d - h: %d> [%d %%]\n", (uint32_t)((act == ACTIVATION_ON) ? 1: 0), l, h, (h*100/tt));
        mlink_decode(h, l);
    }
}

static void comp_init(void)
{
    nrfx_comp_config_t cfg =
    {
        .reference = NRF_COMP_REF_Int1V8,   // 1.8V
        //.ext_ref = NRF_COMP_EXT_REF_0,
        .main_mode = NRF_COMP_MAIN_MODE_SE,
        .threshold = {
                .th_down = NRFX_VOLTAGE_THRESHOLD_TO_INT(0.9, 1.8), //(0.176, 1.8)
                //.th_up   = NRFX_VOLTAGE_THRESHOLD_TO_INT(1.232, 1.8)
                .th_up   = NRFX_VOLTAGE_THRESHOLD_TO_INT(1.1, 1.8) //(1.204, 1.8)
                },
        .speed_mode = NRF_COMP_SP_MODE_Low,
        .hyst = /*NRF_COMP_HYST_NoHyst,*/ NRF_COMP_HYST_50mV,        
        .input = NRF_COMP_INPUT_2,          // AIN2
        //.input = NRF_COMP_INPUT_3,          // AIN3
        .interrupt_priority = 6
    };

    nrfx_err_t err = nrfx_comp_init(&cfg,
                          &comp_event_handler);
    APP_ERROR_CHECK(err);

    nrfx_comp_start(NRFX_COMP_EVT_EN_UP_MASK | NRFX_COMP_EVT_EN_DOWN_MASK, 0);
}

void __setChannelPWM(uint8_t ch, uint8_t val)
{
    low_power_pwm_t *ppwm = NULL;
    switch(ch)
    {
        case 4: // R
            ppwm = &low_power_pwm_0;
        break;            
        case 5: // G
            ppwm = &low_power_pwm_1;
        break;
        case 6: // B
            ppwm = &low_power_pwm_2;
        break;
        case 7: // M
            ppwm = &low_power_pwm_3;
        break;
        case 0: // Q1
            ppwm = &low_power_pwm_4;
        break;
        case 1: // Q2
            ppwm = &low_power_pwm_5;
        break;
        case 2: // Q3
            ppwm = &low_power_pwm_6;
        break;
        case 3: // Q4
            ppwm = &low_power_pwm_7;
        break;
    }
    low_power_pwm_duty_set(ppwm, val);
}

static void io_init(void)
{
    ret_code_t err_code;

    //Initialize gpiote module
    err_code = nrf_drv_gpiote_init();
    APP_ERROR_CHECK(err_code);

    // ========= OUTPUTS =========
    nrf_gpio_cfg_output((uint32_t)OUTID_AVDD);    
    nrf_gpio_cfg_output((uint32_t)OUTID_VIBRATION);
    nrf_gpio_cfg_output((uint32_t)OUTID_SPK);
    nrf_gpio_cfg_output((uint32_t)OUTID_FLASH_nCS);

    nrf_gpio_cfg_output((uint32_t)OUTID_SPI_SCK);
    nrf_gpio_cfg_output((uint32_t)OUTID_SPI_MISO);
    nrf_gpio_cfg_output((uint32_t)OUTID_SPI_MOSI);
    

    // High drive    
    const uint32_t table[] = {OUTID_R, OUTID_G, OUTID_B, OUTID_N, OUTID_S, OUTID_E, OUTID_W};
    for(uint32_t i = 0; i < 7; ++i)
    {
        led_init(table[i]);
    }


    // ========= DIGITAL INPUTS =========
    nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);        //Configure to generate interrupt and wakeup on pin signal low. "false" means that gpiote will use the PORT event, which is low power, i.e. does not add any noticable current consumption (<<1uA). Setting this to "true" will make the gpiote module use GPIOTE->IN events which add ~8uA for nRF52 and ~1mA for nRF51.
    in_config.pull = NRF_GPIO_PIN_NOPULL;                                               //Configure pullup for input pin to prevent it from floting. Pin is pulled down when button is pressed on nRF5x-DK boards, see figure two in http://infocenter.nordicsemi.com/topic/com.nordic.infocenter.nrf52/dita/nrf52/development/dev_kit_v1.1.0/hw_btns_leds.html?cp=2_0_0_1_4		
    err_code = nrf_drv_gpiote_in_init(INID_USB, &in_config, in_pin_handler);            //Initialize the pin with interrupt handler in_pin_handler
    APP_ERROR_CHECK(err_code);                                                          //Check potential error
    

    nrf_drv_gpiote_in_config_t in_config3 = GPIOTE_CONFIG_IN_SENSE_LOTOHI(false);
    in_config3.pull = NRF_GPIO_PIN_NOPULL;
    err_code = nrf_drv_gpiote_in_init(INID_ACCELINT1, &in_config3, in_pin_handler);   //Initialize the pin with interrupt handler in_pin_handler
    APP_ERROR_CHECK(err_code);                                                          //Check potential error
    
    //nrf_drv_gpiote_in_config_t in_config3 = GPIOTE_CONFIG_IN_SENSE_LOTOHI(false);
    //in_config3.pull = NRF_GPIO_PIN_NOPULL;
    err_code = nrf_drv_gpiote_in_init(INID_ACCELINT2, &in_config3, in_pin_handler);   //Initialize the pin with interrupt handler in_pin_handler
    APP_ERROR_CHECK(err_code);                                                          //Check potential error

    nrf_drv_gpiote_in_config_t in_config2 = GPIOTE_CONFIG_IN_SENSE_HITOLO(false);
    in_config2.pull = NRF_GPIO_PIN_PULLUP;
    err_code = nrf_drv_gpiote_in_init(INID_RESET, &in_config2, in_pin_handler);   //Initialize the pin with interrupt handler in_pin_handler
    APP_ERROR_CHECK(err_code);                                                          //Check potential error

    nrf_drv_gpiote_in_event_enable(INID_RESET, true);                            //Enable event and interrupt for the wakeup pin    
    nrf_drv_gpiote_in_event_enable(INID_USB, true);                            //Enable event and interrupt for the wakeup pin
    nrf_drv_gpiote_in_event_enable(INID_ACCELINT1, true);                            //Enable event and interrupt for the wakeup pin    
    nrf_drv_gpiote_in_event_enable(INID_ACCELINT2, true);                            //Enable event and interrupt for the wakeup pin    

/*
    nrf_gpio_cfg(
        INID_BATTCHG,
        NRF_GPIO_PIN_DIR_INPUT,
        NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL,
        NRF_GPIO_PIN_S0H1,
        NRF_GPIO_PIN_NOSENSE);

    nrf_gpio_cfg(
        INID_ACCELINT2,
        NRF_GPIO_PIN_DIR_INPUT,
        NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL,
        NRF_GPIO_PIN_S0H1,
        NRF_GPIO_PIN_NOSENSE);
*/
    gpio_reset();
}

static nrf_saadc_value_t m_adc_buffer_pool[2][SAMPLES_IN_BUFFER];

#define ADC_REF_VOLTAGE_IN_MILLIVOLTS     600       // Internal ref: 0.6V
#define ADC_RES_10BIT                     1024      // 2^10 bits
#define ADC_PRE_SCALING_COMPENSATION      6         // Gain = 1/6
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE)\
        ((((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS) / ADC_RES_10BIT) * ADC_PRE_SCALING_COMPENSATION)

static uint32_t adc_to_mv(uint32_t adc_value, uint32_t ref_voltage_mv, uint32_t adc_max_value, uint32_t gain_divisor)
{
    return ((((adc_value) * ref_voltage_mv) / adc_max_value) * gain_divisor);
}

static void __mlink_analyze_sample_data(nrf_saadc_value_t* buffer, size_t len)
{   
//    static long timestamp = 0; 
//    int64_t avg = 0;
//    for(uint8_t i = 0; i < len; ++i)
//    {
//        avg += buffer[i];
//        send_string("%d %d\n", timestamp, buffer[i]);
//        timestamp += 16;
//    }
//    avg /= len;
    //send_string("[MLINK] GOT %d SAMPLES! >> %d\n", len, (uint32_t)avg);

    mldsp_feedSamples(buffer, len);
}

static void saadc_event_handle(nrfx_saadc_evt_t const *p_event)
{
    ret_code_t err_code;
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE)
    {
        __mlink_analyze_sample_data(p_event->data.done.p_buffer, SAMPLES_IN_BUFFER);
        // Prepare used buffer for conversion
        err_code = nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, SAMPLES_IN_BUFFER);
        APP_ERROR_CHECK(err_code);
        
//        send_string("ANALOG RESULT = %d (%d mV)\r\n", p_event->data.done.p_buffer[0], adc_to_mv(p_event->data.done.p_buffer[0], 600, 16384, 6));
//        nrf_drv_saadc_channel_uninit(0);
        s_saadc_running = 0;
    } else if (p_event->type == NRF_DRV_SAADC_EVT_CALIBRATEDONE)
    {
        send_string("SAADC CALIBRATION DONE\r\n");
        s_saadc_running = 0;
    }
}

#ifdef _OLD_MLINK
static void adc_init(void)
{
    ret_code_t err_code;
    nrf_drv_saadc_config_t cfg =
    {                                                                               
        .resolution         = NRF_SAADC_RESOLUTION_14BIT, 
        .oversample         = NRF_SAADC_OVERSAMPLE_8X, 
        .interrupt_priority = NRFX_SAADC_CONFIG_IRQ_PRIORITY,                       
        .low_power_mode     = NRFX_SAADC_CONFIG_LP_MODE                             
    };

    err_code = nrf_drv_saadc_init(&cfg, saadc_event_handle);
    APP_ERROR_CHECK(err_code);

    nrf_drv_saadc_calibrate_offset();
}
#else
static void adc_init(void)
{
    ret_code_t err_code;
    nrf_drv_saadc_config_t cfg =
    {                                                                               
        .resolution         = NRF_SAADC_RESOLUTION_14BIT, 
        .oversample         = NRF_SAADC_OVERSAMPLE_DISABLED, 
        .interrupt_priority = NRFX_SAADC_CONFIG_IRQ_PRIORITY,                       
        .low_power_mode     = NRFX_SAADC_CONFIG_LP_MODE                             
    };

    err_code = nrf_drv_saadc_init(&cfg, saadc_event_handle);
    APP_ERROR_CHECK(err_code);

    while(nrf_drv_saadc_is_busy());

    //nrf_drv_saadc_calibrate_offset();

    // Enable channel 0 on MLINK (Front sensor testing)

    nrf_saadc_channel_config_t channel_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN2/*NRF_SAADC_INPUT_AIN1*/); //NRF_SAADC_INPUT_AIN0        

    //channel_config.burst = NRF_SAADC_BURST_ENABLED;
    channel_config.burst = NRF_SAADC_BURST_DISABLED;

    channel_config.reference = NRF_SAADC_REFERENCE_VDD4;

    // REF = VDD/4
    //channel_config.gain = NRF_SAADC_GAIN1_6; // x4/6 = 2/3
    //channel_config.gain = NRF_SAADC_GAIN1_5; // x4/5
    //channel_config.gain = NRF_SAADC_GAIN1_4; //x1
    channel_config.gain = NRF_SAADC_GAIN1_2; //x2 
    //channel_config.gain = NRF_SAADC_GAIN1; //x4
    //channel_config.gain = NRF_SAADC_GAIN2; //x8
    //channel_config.gain = NRF_SAADC_GAIN4; //x16

    // REF = 0.6V
    //channel_config.gain = NRF_SAADC_GAIN1_3; //x2
    //channel_config.gain = NRF_SAADC_GAIN1_2; //x3
    err_code = nrf_drv_saadc_channel_init(0, &channel_config);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(m_adc_buffer_pool[0], SAMPLES_IN_BUFFER);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_saadc_buffer_convert(m_adc_buffer_pool[1], SAMPLES_IN_BUFFER);
    APP_ERROR_CHECK(err_code);

    //err_code = nrf_drv_saadc_sample();
    //APP_ERROR_CHECK(err_code);
    
}
#endif

static void adc_sample_channel(nrf_saadc_input_t ain)
{
    ret_code_t err_code;

    nrf_saadc_channel_config_t channel_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(ain); //NRF_SAADC_INPUT_AIN0
        

    channel_config.burst = NRF_SAADC_BURST_ENABLED;

    channel_config.reference = NRF_SAADC_REFERENCE_VDD4;

    if(ain == AINID_BATT)
    {
        // Use internal reference for batt
        // REF = 0.6V
        // Max VBatt = 4.2
        // Voltage divider = /2
        // 4.2 / 2 = 2.1
        // 0.6 * 4 = 2.4V (Max range = 2.4V with gain = 1/4)
        channel_config.reference = NRF_SAADC_REFERENCE_INTERNAL;        
        channel_config.gain = NRF_SAADC_GAIN1_4; //x1

        // REF = 0.6V
        //channel_config.gain = NRF_SAADC_GAIN1_3; //x2
        //channel_config.gain = NRF_SAADC_GAIN1_2; //x3
    } else {
        // REF = VDD/4
        channel_config.gain = NRF_SAADC_GAIN1_4; //x1
        //channel_config.gain = NRF_SAADC_GAIN1_2; //x2
        //channel_config.gain = NRF_SAADC_GAIN1; //x4
    }
    err_code = nrf_drv_saadc_channel_init(0, &channel_config);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(m_adc_buffer_pool[0], SAMPLES_IN_BUFFER);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_sample();
    APP_ERROR_CHECK(err_code);
    s_saadc_running = 1;
}

static void playTone(uint8_t start)
{
/*
    if(start)
    {
        app_pwm_enable(&PWM1);
        while (app_pwm_channel_duty_set(&PWM1, 0, 50) == NRF_ERROR_BUSY);
    } else {
        app_pwm_disable(&PWM1);
    }
    */

    //if(start)
    {
      drv_speaker_sample_play(0);
    }
}

void pwm_ready_callback(uint32_t pwm_id)    // PWM callback function
{    
}

static void pwm_init(void)
{
#if 0
    ret_code_t err_code;

    /* 1-channel PWM, 200Hz, output on DK LED pins. */
    app_pwm_config_t pwm1_cfg = APP_PWM_DEFAULT_CONFIG_1CH(3000L, OUTID_AUDIO);

    pwm1_cfg.pin_polarity[0] = APP_PWM_POLARITY_ACTIVE_HIGH;

    /* Initialize and enable PWM. */
    err_code = app_pwm_init(&PWM1,&pwm1_cfg,pwm_ready_callback);
    APP_ERROR_CHECK(err_code);
#endif
}



static void i2c_init(void)
{
    uint32_t err_code;

    nrf_drv_twi_config_t const config = {
       .scl                = PIN_I2C_SCL,
       .sda                = PIN_I2C_SDA,
       .frequency          = NRF_DRV_TWI_FREQ_400K,
       .interrupt_priority = APP_IRQ_PRIORITY_LOWEST,
       .clear_bus_init     = true //false
    };

    err_code = nrf_twi_mngr_init(&m_nrf_twi_mngr, &config);
    APP_ERROR_CHECK(err_code);
}

#if 0
static void i2c_init(void)
{
    ret_code_t err_code;

    const nrf_drv_twi_config_t twi_config = {
       .scl                = PIN_I2C_SCL,
       .sda                = PIN_I2C_SDA,
       .frequency          = NRF_DRV_TWI_FREQ_400K,
       .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
       .clear_bus_init     = true // false
    };

    //set sensing
    nrf_gpio_cfg_input(PIN_I2C_SCL, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_input(PIN_I2C_SDA, NRF_GPIO_PIN_NOPULL);

    err_code = nrf_drv_twi_init(&m_twi, &twi_config, &twi_event_handler, &s_accel);
    APP_ERROR_CHECK(err_code);
//nrf_drv_tw
    nrf_drv_twi_enable(&m_twi);

    sc7a20_init(&s_accel, 0x19);
}


uint16_t USER_i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, uint16_t len)
{
    ret_code_t err_code;

    err_code = nrf_drv_twi_tx(&m_twi, dev_addr, &reg_addr, 1, true);
    //printf("ERR = %d\n", err_code);    
    if(err_code != 0) 
    {
        send_string("** I2C ADDRESS ERROR 0x%x **\n", err_code);
        return 1;    
    }

    //APP_ERROR_CHECK(err_code);
    
    err_code = nrf_drv_twi_rx(&m_twi, dev_addr, read_data, len);
    //APP_ERROR_CHECK(err_code);
    //printf("ERR = %d\n", err_code);
    if(err_code != 0)
    {
        send_string("** I2C READ ERROR 0x%x **\n", err_code);
        return 1;
    }
    return 0;
}

uint16_t USER_i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    static uint8_t buffer[65];
    static uint32_t i = 0;
/*
    {
        ret_code_t err_code;
        err_code = nrf_drv_twi_tx(&m_twi, dev_addr, &reg_addr, 1, true);
        printf("[%d] >W (R) -> 0x%x ptr=0x%x, len=%d ERR = %d\n", i, dev_addr, data, len, err_code);
        if(err_code != 0) return BMA4_E_FAIL;

        //APP_ERROR_CHECK(err_code);
    }
*/
    {
        buffer[0] = reg_addr;
        memcpy(buffer+1, data, len);
        ret_code_t err_code;
        err_code = nrf_drv_twi_tx(&m_twi, dev_addr, buffer, len + 1, false);
        //printf("[%d] >W (D) -> 0x%x ptr=0x%x, len=%d ERR = %d\n", i, dev_addr, data, len, err_code);
        if(err_code != 0) 
        {
            send_string("** I2C WRITE ERROR 0x%x **\n", err_code);
            return 1;
        }
        //APP_ERROR_CHECK(err_code);    
    }

    return 0;
}
#else
uint16_t USER_i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len)
{return 0;}
uint16_t USER_i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, uint16_t len)
{return 0;}
#endif

void USER_delay_ms(uint32_t ms)
{
    nrf_delay_ms(ms);
}



// =============================================================================================
#if 0
static void i2c_test(void)
{
    bool found = false;
    uint8_t sample_data;
    uint8_t reg = 0;    // CHIP ID
    const uint8_t addresses[] = {0x19, 0x18};

    ret_code_t err_code;
    // Test communication
    uint8_t data;

    for(uint8_t i = 0; i < sizeof(addresses); ++i)
    {
        send_string("Trying address 0x%x...\r\n", (int32_t)addresses[i]);

        uint8_t res = USER_i2c_read(addresses[i], 0x0F, &data, 1);   // Read chip ID (WHO_AM_I)
        if(res != 0)
        {
          continue;
        }
        if(data == 0x11)
        {
            send_string("SUCCESS!: SC7A200 FOUND AT 0x%x. WHO_AM_I = 0x%x", (uint32_t)addresses[i], (uint32_t)data);
            found = true;
            break;
        } else {
            send_string("ERROR: DEVICE AT 0x%x CHIP_ID=0x%x. Expected 0x11.", (uint32_t)addresses[i], (uint32_t)data);
        }

    }
    if(!found)
    {
        send_string("NOT FOUND\r\n");
    }
}
#else
static void i2c_test(void)
{}
#endif

static void playSequence(uint8_t seqId);

static void commandparser_event_handle(cmp_event_t event)
{
    printf("\r\nEVENT received:\r\n");

    switch(event.type)
    {
        case EVENTTYPE_DIGITAL_SET:
        {
            send_string("DIGITAL SET: %d -> %d\n", (int)event.digitalset.outId, (int)event.digitalset.activation);
            setdigital((uint32_t)event.digitalset.outId, event.digitalset.activation);
        }
        break;
        case EVENTTYPE_ANALOG_READ:
            if(s_saadc_running)
            {
                send_string("ERROR: THE SAADC IS BUSY\n");
            } else {
                send_string("ANALOG READ %d...\n", (uint32_t)event.analogread.ainID);
                adc_sample_channel((nrf_saadc_input_t)event.analogread.ainID);                
            }
        break;
        case EVENTTYPE_TONE:
        {
            static uint8_t toneEnabled = 0;            
            toneEnabled = !toneEnabled;
            send_string("TOGGLING TONE %s\n", toneEnabled ? "ON" : "OFF");
            playTone(toneEnabled);
        }
        break;
        case EVENTTYPE_ACCELTEST:
        {
            send_string("TESTING ACCEL ...\n");
            i2c_test();
        }
        break;
        case EVENTTYPE_DIGITAL_READ:
        {
            send_string("DIGITAL READ %d\n", (int)event.digitalread.inId);
            readdigital((uint32_t)event.digitalread.inId);
        }
        break;
        case EVENTTYPE_SEQ:
        {
            send_string("RUN SEQ %d\n", (int)event.seqtrigger.seqID);
            playSequence(event.seqtrigger.seqID);
        }
        break;
        case EVENTTYPE_ACCELTEST2:
            send_string("ACCEL TEST 2\n");
            accel2();
        break;
        case EVENTTYPE_ACCELTEST3:
            send_string("ACCEL TEST 3\n");
            accel3();
        break;
        case EVENTTYPE_I2C:
        {
            if(event.i2c.isRead)
            {
                send_string("I2C READ OPERATION (0x%x)\n", (uint32_t)event.i2c.addr);
                uint8_t ret = 0;
                uint8_t r = USER_i2c_read(0x19, event.i2c.addr, &ret, 1);
                if (r == 0)
                {
                    send_string("READ RESULT: 0x%x\n", ret);
                }
            } else {
                send_string("I2C WRITE OPERATION (0x%x -> 0x%x)\n", (uint32_t)event.i2c.addr, (uint32_t)event.i2c.value);
                USER_i2c_write(0x19, event.i2c.addr, &event.i2c.value, 1);
            }
        }
        break;
    }
}


void spk_handler(drv_speaker_evt_t evt)
{
    //if(evt == DRV_SPEAKER_EVT_FINISHED)
        //drv_speaker_sample_play(0);
        //drv_speaker_tone_start(440, 1000, 100);
}

// ===========================================================================================

static void pwm_handler(void * p_context)
{
}

void softpwm_init()
{
    uint32_t err_code;
    low_power_pwm_config_t low_power_pwm_config;

    APP_TIMER_DEF(lpp_timer_0);
    low_power_pwm_config.active_high    = true;
    low_power_pwm_config.period         = 255;
    low_power_pwm_config.bit_mask       = PIN_MASK(OUTID_R);
    low_power_pwm_config.p_timer_id     = &lpp_timer_0;
    low_power_pwm_config.p_port         = NRF_GPIO;

    err_code = low_power_pwm_init((&low_power_pwm_0), &low_power_pwm_config, pwm_handler);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_0, 0);
    APP_ERROR_CHECK(err_code);

    APP_TIMER_DEF(lpp_timer_1);
    low_power_pwm_config.active_high    = true;
    low_power_pwm_config.period         = 255;
    low_power_pwm_config.bit_mask       = PIN_MASK(OUTID_G);
    low_power_pwm_config.p_timer_id     = &lpp_timer_1;
    low_power_pwm_config.p_port         = NRF_GPIO;

    err_code = low_power_pwm_init((&low_power_pwm_1), &low_power_pwm_config, pwm_handler);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_1, 0);
    APP_ERROR_CHECK(err_code);

    APP_TIMER_DEF(lpp_timer_2);
    low_power_pwm_config.active_high    = true;
    low_power_pwm_config.period         = 255;
    low_power_pwm_config.bit_mask       = PIN_MASK(OUTID_B);
    low_power_pwm_config.p_timer_id     = &lpp_timer_2;
    low_power_pwm_config.p_port         = NRF_GPIO;

    err_code = low_power_pwm_init((&low_power_pwm_2), &low_power_pwm_config, pwm_handler);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_2, 0);
    APP_ERROR_CHECK(err_code);

/*
    APP_TIMER_DEF(lpp_timer_3);
    low_power_pwm_config.active_high    = true;
    low_power_pwm_config.period         = 255;
    low_power_pwm_config.bit_mask       = PIN_MASK(OUTID_VIBRATION);
    low_power_pwm_config.p_timer_id     = &lpp_timer_3;
    low_power_pwm_config.p_port         = NRF_GPIO;

    err_code = low_power_pwm_init((&low_power_pwm_3), &low_power_pwm_config, pwm_handler);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_3, 0);
    APP_ERROR_CHECK(err_code);
*/
        APP_TIMER_DEF(lpp_timer_4);
    low_power_pwm_config.active_high    = true;
    low_power_pwm_config.period         = 255;
    low_power_pwm_config.bit_mask       = PIN_MASK(OUTID_E);    // Q1
    low_power_pwm_config.p_timer_id     = &lpp_timer_4;
    low_power_pwm_config.p_port         = NRF_GPIO;

    err_code = low_power_pwm_init((&low_power_pwm_4), &low_power_pwm_config, pwm_handler);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_4, 0);
    APP_ERROR_CHECK(err_code);

        APP_TIMER_DEF(lpp_timer_5);
    low_power_pwm_config.active_high    = true;
    low_power_pwm_config.period         = 255;
    low_power_pwm_config.bit_mask       = PIN_MASK(OUTID_S);    // Q2
    low_power_pwm_config.p_timer_id     = &lpp_timer_5;
    low_power_pwm_config.p_port         = NRF_GPIO;

    err_code = low_power_pwm_init((&low_power_pwm_5), &low_power_pwm_config, pwm_handler);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_5, 0);
    APP_ERROR_CHECK(err_code);

        APP_TIMER_DEF(lpp_timer_6);
    low_power_pwm_config.active_high    = true;
    low_power_pwm_config.period         = 255;
    low_power_pwm_config.bit_mask       = PIN_MASK(OUTID_W);    // Q3
    low_power_pwm_config.p_timer_id     = &lpp_timer_6;
    low_power_pwm_config.p_port         = NRF_GPIO;

    err_code = low_power_pwm_init((&low_power_pwm_6), &low_power_pwm_config, pwm_handler);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_6, 0);
    APP_ERROR_CHECK(err_code);

        APP_TIMER_DEF(lpp_timer_7);
    low_power_pwm_config.active_high    = true;
    low_power_pwm_config.period         = 255;
    low_power_pwm_config.bit_mask       = PIN_MASK(OUTID_N);    // Q4
    low_power_pwm_config.p_timer_id     = &lpp_timer_7;
    low_power_pwm_config.p_port         = NRF_GPIO;

    err_code = low_power_pwm_init((&low_power_pwm_7), &low_power_pwm_config, pwm_handler);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_7, 0);
    APP_ERROR_CHECK(err_code);


    err_code = low_power_pwm_start((&low_power_pwm_0), low_power_pwm_0.bit_mask);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_start((&low_power_pwm_1), low_power_pwm_1.bit_mask);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_start((&low_power_pwm_2), low_power_pwm_2.bit_mask);
    APP_ERROR_CHECK(err_code);
    //err_code = low_power_pwm_start((&low_power_pwm_3), low_power_pwm_3.bit_mask);
    //APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_start((&low_power_pwm_4), low_power_pwm_4.bit_mask);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_start((&low_power_pwm_5), low_power_pwm_5.bit_mask);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_start((&low_power_pwm_6), low_power_pwm_6.bit_mask);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_start((&low_power_pwm_7), low_power_pwm_7.bit_mask);
    APP_ERROR_CHECK(err_code);    
}

static low_power_pwm_t* ledPWMs[7] = { &low_power_pwm_0, &low_power_pwm_1, &low_power_pwm_2, &low_power_pwm_4, &low_power_pwm_5, &low_power_pwm_6, &low_power_pwm_7 };
static size_t ledOffsets[5] = { offsetof(HALLEDState, center.r),
                                offsetof(HALLEDState, north.r), offsetof(HALLEDState, east.r),
                                offsetof(HALLEDState, south.r), offsetof(HALLEDState, west.r)};


void HALLEDSetEnabled(uint8_t enabled)
{
}

static HALLEDState s_ledStates;

void HALLEDSetMultiple(const HALLEDState* values)
{
    uint8_t err_code = 0;

    memcpy(&s_ledStates, values, sizeof(HALLEDState));    
    err_code = low_power_pwm_duty_set(&low_power_pwm_0, values->center.r);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_1, values->center.g);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_2, values->center.b);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_4, values->north.r);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_5, values->east.r);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_6, values->south.r);
    APP_ERROR_CHECK(err_code);
    err_code = low_power_pwm_duty_set(&low_power_pwm_7, values->west.r);
    APP_ERROR_CHECK(err_code);
}

void HALLEDSetState(const HALLEDColor* value, uint8_t index)
{
    HALLEDColor* c = (HALLEDColor*)((uint8_t*)&s_ledStates + ledOffsets[index]);
    *c = *value;
    HALLEDSetMultiple(&s_ledStates);
}

void HALLEDToggle(uint8_t index)
{
    HALLEDColor* c = (HALLEDColor*)((uint8_t*)&s_ledStates + ledOffsets[index]);
    if(c->r > 0 || c->g > 0 || c ->b > 0)
    {
        c->r = c->g = c->b = 0;
    } else {
        c->r = c->g = c->b = 255;
    }    
    HALLEDSetMultiple(&s_ledStates);
}


uint64_t HALSystemGetTimeUS()
{
    static uint32_t lastcnt = 0;    
    uint64_t cnt = app_timer_cnt_get();
    uint64_t elapsed = app_timer_cnt_diff_compute(cnt, lastcnt);
    uint64_t ms = (lastcnt + elapsed) * ((0 + 1) * 1000000 ) / APP_TIMER_CLOCK_FREQ;        
    lastcnt = cnt;
    return ms;    
}

// ===========================================================================================

// ===========================================================================================
static void playSequence(uint8_t seqId)
{
#if BUILTIN_MODS
    switch(seqId)
    {
        case 0: // INSERT SEED
        {
            FAnimAbortAllAnimations();
        
            FAnimParams p = {0};

            // Light up in sequence
            p.SetColorAnim.index = LED_CENTER;
            p.SetColorAnim.color.r = 0;
            p.SetColorAnim.color.g = 255;
            p.SetColorAnim.color.b = 100;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 1000;
            FAnimEnqueueAnim(&p);


            p.SetColorAnim.index = LED_NORTH;
            p.SetColorAnim.color.r = 255;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 500;
            FAnimEnqueueAnim(&p);

            p.SetColorAnim.index = LED_EAST;
            p.SetColorAnim.color.r = 255;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 500;
            FAnimEnqueueAnim(&p);

            p.SetColorAnim.index = LED_SOUTH;
            p.SetColorAnim.color.r = 255;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 500;
            FAnimEnqueueAnim(&p);

            p.SetColorAnim.index = LED_WEST;
            p.SetColorAnim.color.r = 255;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 300;
            FAnimEnqueueAnim(&p);

            // Blink all four

            p.SetLEDCycleAnim.index = LED_EAST;
            p.SetLEDCycleAnim.colorA.r = 255;
            p.SetLEDCycleAnim.colorB.r = 0;
            p.SetLEDCycleAnim.numCycles = 3;
            p.SetLEDCycleAnim.speed = kLEDSpeedEighthSecond;
            p.type = FANIM_SETLEDCYCLE;
            FAnimEnqueueAnim(&p);
            p.SetLEDCycleAnim.index = LED_WEST;
            FAnimEnqueueAnim(&p);
            p.SetLEDCycleAnim.index = LED_NORTH;
            FAnimEnqueueAnim(&p);
            p.SetLEDCycleAnim.index = LED_SOUTH;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 1000;
            FAnimEnqueueAnim(&p);

            // Turn off all four
            p.SetColorAnim.index = LED_NORTH;
            p.SetColorAnim.color.r = 0;
            p.SetColorAnim.fadeAnim = false;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);
            p.SetColorAnim.index = LED_SOUTH;
            FAnimEnqueueAnim(&p);
            p.SetColorAnim.index = LED_EAST;
            FAnimEnqueueAnim(&p);
            p.SetColorAnim.index = LED_WEST;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 200;
            FAnimEnqueueAnim(&p);

            // Turn center red
            p.SetColorAnim.index = LED_CENTER;
            p.SetColorAnim.color.r = 255;
            p.SetColorAnim.color.g = 0;
            p.SetColorAnim.color.b = 0;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 500;
            FAnimEnqueueAnim(&p);

            // Blink center
            p.SetLEDCycleAnim.index = LED_CENTER;
            p.SetLEDCycleAnim.colorA.r = 0;
            p.SetLEDCycleAnim.colorA.g = 0;
            p.SetLEDCycleAnim.colorA.b = 0;
            p.SetLEDCycleAnim.colorB.r = 255;
            p.SetLEDCycleAnim.colorB.g = 0;
            p.SetLEDCycleAnim.colorB.b = 0;
            p.SetLEDCycleAnim.numCycles = 3;
            p.SetLEDCycleAnim.speed = kLEDSpeedSixteenthSecond;
            p.type = FANIM_SETLEDCYCLE;
            FAnimEnqueueAnim(&p);


            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 1000;
            FAnimEnqueueAnim(&p);

        }
        break;
        case 1: // RECEIVE SEED
        {
            FAnimAbortAllAnimations();
        
            FAnimParams p = {0};

            // Turn center PINK
            p.SetColorAnim.index = LED_CENTER;
            p.SetColorAnim.color.r = 0xFF;
            p.SetColorAnim.color.g = 0x26;
            p.SetColorAnim.color.b = 0x11;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 1000;
            FAnimEnqueueAnim(&p);

            // Light up all four
            p.SetColorAnim.index = LED_NORTH;
            p.SetColorAnim.color.r = 255;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);
            p.SetColorAnim.index = LED_EAST;
            FAnimEnqueueAnim(&p);
            p.SetColorAnim.index = LED_WEST;
            FAnimEnqueueAnim(&p);
            p.SetColorAnim.index = LED_SOUTH;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 1000;
            FAnimEnqueueAnim(&p);
            
            // Blink center in PINK
            p.SetLEDCycleAnim.index = LED_CENTER;
            p.SetLEDCycleAnim.colorA.r = 0;
            p.SetLEDCycleAnim.colorA.g = 0;
            p.SetLEDCycleAnim.colorA.b = 0;
            p.SetLEDCycleAnim.colorB.r = 0xFF;
            p.SetLEDCycleAnim.colorB.g = 0x26;
            p.SetLEDCycleAnim.colorB.b = 0x11;
            p.SetLEDCycleAnim.numCycles = 16;
            p.SetLEDCycleAnim.speed = kLEDSpeedSixteenthSecond;
            p.type = FANIM_SETLEDCYCLE;
            FAnimEnqueueAnim(&p);

            // Turn off in sequence

            p.SetColorAnim.index = LED_WEST;
            p.SetColorAnim.color.r = 0;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 500;
            FAnimEnqueueAnim(&p);

            p.SetColorAnim.index = LED_SOUTH;
            p.SetColorAnim.color.r = 0;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 500;
            FAnimEnqueueAnim(&p);

            p.SetColorAnim.index = LED_EAST;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 500;
            FAnimEnqueueAnim(&p);

            p.SetColorAnim.index = LED_NORTH;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 300;
            FAnimEnqueueAnim(&p);

            // Turn center WHITE

            p.SetColorAnim.index = LED_CENTER;
            p.SetColorAnim.color.r = 255;
            p.SetColorAnim.color.g = 255;
            p.SetColorAnim.color.b = 255;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 500;
            FAnimEnqueueAnim(&p);

            // Blink center
            p.SetLEDCycleAnim.index = LED_CENTER;
            p.SetLEDCycleAnim.colorA.r = 0;
            p.SetLEDCycleAnim.colorA.g = 0;
            p.SetLEDCycleAnim.colorA.b = 0;
            p.SetLEDCycleAnim.colorB.r = 255;
            p.SetLEDCycleAnim.colorB.g = 255;
            p.SetLEDCycleAnim.colorB.b = 255;
            p.SetLEDCycleAnim.numCycles = 3;
            p.SetLEDCycleAnim.speed = kLEDSpeedSixteenthSecond;
            p.type = FANIM_SETLEDCYCLE;
            FAnimEnqueueAnim(&p);


            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 3000;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 10000;
            FAnimEnqueueAnim(&p);

        }
        break;
        case 2: // INCUBATION AR
        {
            FAnimAbortAllAnimations();
        
            FAnimParams p = {0};



            // Flash center in yellow
            p.SetLEDCycleAnim.index = LED_CENTER;
            p.SetLEDCycleAnim.colorA.r = 0;
            p.SetLEDCycleAnim.colorA.g = 0;
            p.SetLEDCycleAnim.colorA.b = 0;
            p.SetLEDCycleAnim.colorB.r = 255;
            p.SetLEDCycleAnim.colorB.g = 255;
            p.SetLEDCycleAnim.colorB.b = 0;
            p.SetLEDCycleAnim.numCycles = 1;
            p.SetLEDCycleAnim.speed = kLEDSpeedQuarterSecond;
            p.type = FANIM_SETLEDCYCLE;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 1000;
            FAnimEnqueueAnim(&p);

            // Turn center YELLOW
            p.SetColorAnim.index = LED_CENTER;
            p.SetColorAnim.color.r = 0xFF;
            p.SetColorAnim.color.g = 0xFF;
            p.SetColorAnim.color.b = 0x0;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);


            // Light up Q1/Q2
            p.SetColorAnim.index = LED_NORTH;
            p.SetColorAnim.color.r = 255;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);
            p.SetColorAnim.index = LED_EAST;
            FAnimEnqueueAnim(&p);
            
            // Blink Q3

            p.SetLEDCycleAnim.index = LED_SOUTH;
            p.SetLEDCycleAnim.colorA.r = 0;
            p.SetLEDCycleAnim.colorA.g = 0;
            p.SetLEDCycleAnim.colorA.b = 0;
            p.SetLEDCycleAnim.colorB.r = 255;
            p.SetLEDCycleAnim.colorB.g = 0;
            p.SetLEDCycleAnim.colorB.b = 0;
            p.SetLEDCycleAnim.numCycles = 7;
            p.SetLEDCycleAnim.speed = kLEDSpeedQuarterSecond;
            p.type = FANIM_SETLEDCYCLE;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 200;
            FAnimEnqueueAnim(&p);            

            // Turn off center
            p.SetColorAnim.index = LED_CENTER;
            p.SetColorAnim.color.r = 0;
            p.SetColorAnim.color.g = 0;
            p.SetColorAnim.color.b = 0;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 1500;
            FAnimEnqueueAnim(&p);  

            // Turn off Q1/Q2/Q3

            p.SetColorAnim.index = LED_NORTH;
            p.SetColorAnim.color.r = 0;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);
            p.SetColorAnim.index = LED_EAST;
            FAnimEnqueueAnim(&p);
            p.SetColorAnim.index = LED_SOUTH;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 500;
            FAnimEnqueueAnim(&p);


            // Flash center in yellow
            p.SetLEDCycleAnim.index = LED_CENTER;
            p.SetLEDCycleAnim.colorA.r = 0;
            p.SetLEDCycleAnim.colorA.g = 0;
            p.SetLEDCycleAnim.colorA.b = 0;
            p.SetLEDCycleAnim.colorB.r = 255;
            p.SetLEDCycleAnim.colorB.g = 255;
            p.SetLEDCycleAnim.colorB.b = 0;
            p.SetLEDCycleAnim.numCycles = 1;
            p.SetLEDCycleAnim.speed = kLEDSpeedQuarterSecond;
            p.type = FANIM_SETLEDCYCLE;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 3000;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 10000;
            FAnimEnqueueAnim(&p);
        }
        break;
        case 3: // Receiving data
        {
            FAnimAbortAllAnimations();
        
            FAnimParams p = {0};


            // Flash center in yellow
            p.SetLEDCycleAnim.index = LED_CENTER;
            p.SetLEDCycleAnim.colorA.r = 0;
            p.SetLEDCycleAnim.colorA.g = 0;
            p.SetLEDCycleAnim.colorA.b = 0;
            p.SetLEDCycleAnim.colorB.r = 255;
            p.SetLEDCycleAnim.colorB.g = 255;
            p.SetLEDCycleAnim.colorB.b = 0;
            p.SetLEDCycleAnim.numCycles = 1;
            p.SetLEDCycleAnim.speed = kLEDSpeedQuarterSecond;
            p.type = FANIM_SETLEDCYCLE;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 1000;
            FAnimEnqueueAnim(&p);

            // Turn center YELLOW
            p.SetColorAnim.index = LED_CENTER;
            p.SetColorAnim.color.r = 0xFF;
            p.SetColorAnim.color.g = 0xFF;
            p.SetColorAnim.color.b = 0x0;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            
            // Blink Q1

            p.SetLEDCycleAnim.index = LED_NORTH;
            p.SetLEDCycleAnim.colorA.r = 0;
            p.SetLEDCycleAnim.colorA.g = 0;
            p.SetLEDCycleAnim.colorA.b = 0;
            p.SetLEDCycleAnim.colorB.r = 255;
            p.SetLEDCycleAnim.colorB.g = 0;
            p.SetLEDCycleAnim.colorB.b = 0;
            p.SetLEDCycleAnim.numCycles = 7;
            p.SetLEDCycleAnim.speed = kLEDSpeedQuarterSecond;
            p.type = FANIM_SETLEDCYCLE;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 200;
            FAnimEnqueueAnim(&p);            

            // Turn off center
            p.SetColorAnim.index = LED_CENTER;
            p.SetColorAnim.color.r = 0;
            p.SetColorAnim.color.g = 0;
            p.SetColorAnim.color.b = 0;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 1500;
            FAnimEnqueueAnim(&p);  

            // Turn off Q1

            p.SetColorAnim.index = LED_NORTH;
            p.SetColorAnim.color.r = 0;
            p.SetColorAnim.fadeAnim = true;
            p.type = FANIM_SETCOLOR;
            FAnimEnqueueAnim(&p);


            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 500;
            FAnimEnqueueAnim(&p);


            // Flash center in yellow
            p.SetLEDCycleAnim.index = LED_CENTER;
            p.SetLEDCycleAnim.colorA.r = 0;
            p.SetLEDCycleAnim.colorA.g = 0;
            p.SetLEDCycleAnim.colorA.b = 0;
            p.SetLEDCycleAnim.colorB.r = 255;
            p.SetLEDCycleAnim.colorB.g = 255;
            p.SetLEDCycleAnim.colorB.b = 0;
            p.SetLEDCycleAnim.numCycles = 1;
            p.SetLEDCycleAnim.speed = kLEDSpeedQuarterSecond;
            p.type = FANIM_SETLEDCYCLE;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 3000;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 10000;
            FAnimEnqueueAnim(&p);
        }
        break;
        case 4:
        {
            FAnimAbortAllAnimations();
        
            FAnimParams p = {0};

            // Flash center in white
            p.SetLEDCycleAnim.index = LED_CENTER;
            p.SetLEDCycleAnim.colorA.r = 0;
            p.SetLEDCycleAnim.colorA.g = 0;
            p.SetLEDCycleAnim.colorA.b = 0;
            p.SetLEDCycleAnim.colorB.r = 255;
            p.SetLEDCycleAnim.colorB.g = 255;
            p.SetLEDCycleAnim.colorB.b = 255;
            p.SetLEDCycleAnim.numCycles = 7;
            p.SetLEDCycleAnim.speed = kLEDSpeedSixteenthSecond;
            p.type = FANIM_SETLEDCYCLE;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 1000;
            FAnimEnqueueAnim(&p);

            p.type = FANIM_DELAY;
            p.DelayAnim.delayms = 10000;
            FAnimEnqueueAnim(&p);
        }
        break;
        default:
        break;
    }
#endif
}

// ===========================================================================================

static void lfclk_init(void)
{
    uint32_t err_code;
    err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);

    nrf_drv_clock_lfclk_request(NULL);
}

// ===========================================================================================

void accel_off()
{
    uint8_t d = 0;
    USER_i2c_write(0x19, 0x20, &d, 1);
}

void accel_test()
{
    uint8_t d;

    /*
    d = 0x97; // Power on, default data rate (1.25 kHz). Low power = off, XYZ axes active
    //d = 0x00; // Power off
    USER_i2c_write(0x19, 0x20, &d, 1);    

    // Switch to 400 Hz
    d = 0x77;
    USER_i2c_write(0x19, 0x20, &d, 1);    

    d = 0x80;   // I1 click interrupt assigned to INT1
    USER_i2c_write(0x19, 0x22, &d, 1);

    //d = 0x10;   // Full range = +/- 4 G
    d = 0x00;   // Full range = +/- 2 G
    USER_i2c_write(0x19, 0x23, &d, 1);

    //d = 0x0C;   // Single and double click interrupt on Y axis enabled
    //d = 0x08;   // Double click interrupt on Y axis
    //d = 0x02;   // Double click interrupt on X axis
    //d = 0x15;   // Single click on any axe
    d = 0x2A;   // Double click on any axe
    USER_i2c_write(0x19, 0x38, &d, 1);

    //d = 0x05;   // CLICK_THS
    d = 0x80;   // CLICK_THS
    USER_i2c_write(0x19, 0x3A, &d, 1);

//    d = 0x55;   // TIME_LIMIT
    d = 0x04;   // TIME_LIMIT
    USER_i2c_write(0x19, 0x3B, &d, 1);

    //d = 0xff;   // TIME_LATENCY
    d = 0x24;   // TIME_LATENCY
    USER_i2c_write(0x19, 0x3C, &d, 1);

    //d = 0xff;   // TIME_WINDOW
    d = 0x7f;   // TIME_WINDOW
    USER_i2c_write(0x19, 0x3D, &d, 1);
*/
    static const SC7A20_reg_t cfg[] =
    {
        { CTRL_REG1,
          CTRL_REG1_LPEN | CTRL_REG1_XEN | CTRL_REG1_YEN | CTRL_REG1_ZEN |
          CTRL_REG1_ODR_NORMAL_LP_400HZ
        },
        { CTRL_REG2,
          CTRL_REG2_HPCLICK | CTRL_REG2_HPIS1_EN
        },
        { CTRL_REG3,
          CTRL_REG3_I1_AOI1 | CTRL_REG3_I1_CLICK
        },
        { CTRL_REG4,
          CTRL_REG4_DLPF_EN | CTRL_REG4_FS_2G | CTRL_REG4_BDU
        },
        { CTRL_REG5,
          0
        },
        { CTRL_REG6,
          0
        },
        { REFERENCE,
          0
        },
        { FIFO_CTRL_REG,
          0
        },
        { AOI1_CTRL_REG,
          AOI1_CTRL_REG_XHIE  //?
        },
        { AOI1_THS,
          0x05
        },
        { AOI1_DURATION,
          0x04
        },
        {
          AOI2_CTRL_REG,
          0
        },
        {
          AOI2_THS,
          0
        },
        {
          AOI2_DURATION,
          0
        },
        {
          CLICK_CTRL_REG,
          CLICK_CTRL_REG_ZD | CLICK_CTRL_REG_YD | CLICK_CTRL_REG_XD
        },
        {
          CLICK_THS,
          0x15
        },
        {
          TIME_LIMIT,
          0x05
        },
        {
          TIME_LATENCY,
          0x34
        },        
        {
          TIME_WINDOW,
          0x7f
        }
    };

    uint8_t addr[] = { 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x2E, 0x30, 0x32, 0x33, 0x34, 0x36, 0x37, 0x38, 0x3A, 0x3B, 0x3C, 0x3D };
  //uint8_t wr[]   = { 0x00, 0x77, 0x04, 0x80, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa5, 0xa5, 0x00, 0x00, 0x00, 0x2a, 0x12, 0x04, 0x34, 0x7f };
    uint8_t wr[]   = { 0x00, 0x7F, 0x05, 0xC0, 0x88, 0x00, 0x00, 0x00, 0x00, 0x02, 0x05, 0x04, 0x00, 0x00, 0x00, 0x2a, 0x10, 0x20, 0x34, 0x7f };

    for(uint8_t i = 0; i < sizeof(cfg) / sizeof(SC7A20_reg_t); ++i)
    {        
        //USER_i2c_write(0x19, addr[i], &wr[i], 1);
        USER_i2c_write(0x19, cfg[i].reg, (uint8_t*)&(cfg[i].val), 1);
    }

#if 1
    int16_t accel[3] = {0};
    float ax, ay, az;
bool hadInt = false;
uint32_t ms = 0;
char tmp[8];
uint8_t tmpi = 0;
    while(1)
    {
        tmp[0] = '-';
        tmp[1] = 0;
        if(s_readIntSource)
        {
            NRFX_CRITICAL_SECTION_ENTER();
                s_readIntSource = false;
            NRFX_CRITICAL_SECTION_EXIT();
            uint8_t data = 0;
            uint8_t res = USER_i2c_read(0x19, 0x39, &data, 1); // Read interrupt source
            if(data & 0x40)
            {
                tmpi = 0;
                if(data & 0x10) tmp[tmpi++] = 'S';
                if(data & 0x20) tmp[tmpi++] = 'D';
                if(data & 0x01) tmp[tmpi++] = 'X';
                if(data & 0x02) tmp[tmpi++] = 'Y';
                if(data & 0x04) tmp[tmpi++] = 'Z';
                tmp[tmpi++] = 0;
            
                send_string("INT SOURCE = 0x%x\n", (uint32_t)data);
                if(data & 0x10) send_string("SINGLE CLICK\n");                
                if(data & 0x20) send_string("DOUBLE CLICK\n");
                if(data & 0x01) send_string("X AXIS\n");
                if(data & 0x02) send_string("Y AXIS\n");
                if(data & 0x04) send_string("Z AXIS\n");
                send_string("===\n");
                hadInt = true;
                //drv_speaker_sample_play(0);
                playSequence(4);
                
            }
            data = 0;

            res = USER_i2c_read(0x19, 0x31, &data, 1); // Read interrupt source
            if(data & 0x40)
            {
                printf("AOI1: 0x%x\n", data);
            }
            
        }
        static uint8_t r = 0;
        uint8_t c = (uint8_t)nrf_gpio_pin_read(INID_ACCELINT1);
        if(c != r)
        {
            printf(" >> INT1 -> %d \n", (uint32_t)c);
            r = c;
        }
#if 0
        uint8_t res = USER_i2c_read(0x19, 0xA8, (uint8_t*)&accel[0], 6); // Read acceleration
//        uint8_t l, h;
//        uint8_t res = USER_i2c_read(0x19, 0x28, &l, 1); // Read acceleration
//        res = USER_i2c_read(0x19, 0x29, &h, 1); // Read acceleration
        ax = (accel[0] / 16384.f) * 1.0f;
        ay = (accel[1] / 16384.f) * 1.0f;
        az = (accel[2] / 16384.f) * 1.0f;
        printf("%d, %f, %f, %f, %d, %s\n", ms, ax, ay, az, (hadInt? 1 : 0), tmp);
        if(hadInt) hadInt = false;
        nrf_delay_ms(5);
        
        ms += 5;
#endif
        LEDUpdate(5);
        FAnimUpdate(5);
    }
    #endif
}

void accel2()
{
    // Init accelerometer and read acceleration
    uint8_t addr[] = { 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x2E, 0x30, 0x32, 0x33, 0x34, 0x36, 0x37, 0x38, 0x3A, 0x3B, 0x3C, 0x3D };
    uint8_t wr[]   = { 0x00, 0x77, 0x04, 0x80, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa5, 0xa5, 0x00, 0x00, 0x00, 0x2a, 0x12, 0x04, 0x34, 0x7f };

    for(uint8_t i = 0; i < sizeof(addr); ++i)
    {
        uint8_t ret = 0;
        ret = USER_i2c_write(0x19, addr[i], &wr[i], 1);
        if(ret != 0)
            return;
    }

    nrf_delay_ms(5);

    int16_t accel[3] = {0};

    uint8_t res = USER_i2c_read(0x19, 0xA8, (uint8_t*)&accel[0], 6); // Read acceleration

    if (res == 0)
    {
        float ax, ay, az;
        ax = (accel[0] / 16384.f) * 1.0f;
        ay = (accel[1] / 16384.f) * 1.0f;
        az = (accel[2] / 16384.f) * 1.0f;
        send_string("ACCEL: %f, %f, %f\n", ax, ay, az);
    }
}

/*
void accel3()
{
    // 0x24 = 0x40 // FIFO ENABLE

    uint8_t wr = 0x00;

    USER_i2c_write(0x19, 0x20, &wr, 1);
}
*/

void accel3()
{
    uint8_t wr = 0x00;

    USER_i2c_write(0x19, 0x20, &wr, 1);
}

void accel_update()
{
        if(s_readIntSource)
        {
            uint8_t data = 0;
            uint8_t res = USER_i2c_read(0x19, 0x39, &data, 1); // Read interrupt source
            if(data & 0x40)
            {            
                send_string("INT SOURCE = 0x%x\n", (uint32_t)data);
                //if(data & 0x10) send_string("SINGLE CLICK\n");                
                if(data & 0x20) send_string("DOUBLE CLICK\n");
                if(data & 0x01) send_string("X AXIS\n");
                if(data & 0x02) send_string("Y AXIS\n");
                if(data & 0x04) send_string("Z AXIS\n");
                send_string("===\n");
                //hadInt = true;
                //drv_speaker_sample_play(0);
                playSequence(4);
                
            }
            s_readIntSource = false;
        }

}


void system_power_off_no_sd()
{    
    app_prepare_reset();
    NRF_POWER->SYSTEMOFF = 1;
    while(1);    
}

#define min(A, B) ((A < B)?A:B)

#ifdef _OLD_MLINK
static void ppi_init(void)
{   
#ifdef ENABLE_BT
    uint32_t err = 0;
    err = sd_ppi_channel_assign(0, (const void*)&NRF_COMP->EVENTS_UP, (const void*)(uint32_t)&NRF_TIMER1->TASKS_CAPTURE[0]);
    APP_ERROR_CHECK(err);
    err = sd_ppi_channel_assign(1, (const void*)&NRF_COMP->EVENTS_DOWN, (const void*)(uint32_t)&NRF_TIMER1->TASKS_CAPTURE[1]);
    APP_ERROR_CHECK(err);
    err = sd_ppi_channel_assign(2, (const void*)&NRF_COMP->EVENTS_DOWN, (const void*)(uint32_t)&NRF_TIMER1->TASKS_CLEAR);
    APP_ERROR_CHECK(err);
    err = sd_ppi_channel_enable_set((PPI_CHEN_CH0_Enabled << PPI_CHEN_CH0_Pos)
                                | (PPI_CHEN_CH1_Enabled << PPI_CHEN_CH1_Pos)
                                | (PPI_CHEN_CH2_Enabled << PPI_CHEN_CH2_Pos));
    APP_ERROR_CHECK(err);
#else
    // Low to high -> Capture CC[0]
    NRF_PPI->CH[0].EEP = (uint32_t)&NRF_COMP->EVENTS_UP;
    NRF_PPI->CH[0].TEP = (uint32_t)&NRF_TIMER1->TASKS_CAPTURE[0];

    // High to low -> Capture CC[1]
    NRF_PPI->CH[1].EEP = (uint32_t)&NRF_COMP->EVENTS_DOWN;
    NRF_PPI->CH[1].TEP = (uint32_t)&NRF_TIMER1->TASKS_CAPTURE[1];

    // High to low -> Clear timer
    NRF_PPI->CH[2].EEP = (uint32_t)&NRF_COMP->EVENTS_DOWN;
    NRF_PPI->CH[2].TEP = (uint32_t)&NRF_TIMER1->TASKS_CLEAR;
    
  // Enable only PPI channels 0, 1 and 2
  NRF_PPI->CHEN = (PPI_CHEN_CH0_Enabled << PPI_CHEN_CH0_Pos)
                | (PPI_CHEN_CH1_Enabled << PPI_CHEN_CH1_Pos)
                | (PPI_CHEN_CH2_Enabled << PPI_CHEN_CH2_Pos);
#endif
}

static void timer1_init(void)
{
  /* Start 16 MHz crystal oscillator */
#ifdef ENABLE_BT
  sd_clock_hfclk_request();
  uint32_t running = 0;
  do
  {
    sd_clock_hfclk_is_running(&running);
  } while(running == 0);
#else
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
      /* Wait for the external oscillator to start up */
    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);
#endif

  NRF_TIMER1->MODE = TIMER_MODE_MODE_Timer;
  NRF_TIMER1->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
  NRF_TIMER1->PRESCALER = 8;
  NRF_TIMER1->TASKS_START = 1; // Start clocks
}
#else
static void ppi_init(void)
{   
#ifdef ENABLE_BT
    uint32_t err = 0;
    uint32_t chn = 0;
    err = sd_ppi_channel_assign(0, (const void*)&NRF_TIMER1->EVENTS_COMPARE[0], (const void*)(uint32_t)&NRF_SAADC->TASKS_SAMPLE);
    APP_ERROR_CHECK(err);
    err = sd_ppi_channel_enable_get(&chn);
    APP_ERROR_CHECK(err);
    chn |= (PPI_CHEN_CH0_Enabled << PPI_CHEN_CH0_Pos);
    err = sd_ppi_channel_enable_set(chn);
    APP_ERROR_CHECK(err);    

    /*
    err = sd_ppi_channel_assign(0, (const void*)&NRF_COMP->EVENTS_UP, (const void*)(uint32_t)&NRF_TIMER1->TASKS_CAPTURE[0]);
    APP_ERROR_CHECK(err);
    err = sd_ppi_channel_assign(1, (const void*)&NRF_COMP->EVENTS_DOWN, (const void*)(uint32_t)&NRF_TIMER1->TASKS_CAPTURE[1]);
    APP_ERROR_CHECK(err);
    err = sd_ppi_channel_assign(2, (const void*)&NRF_COMP->EVENTS_DOWN, (const void*)(uint32_t)&NRF_TIMER1->TASKS_CLEAR);
    APP_ERROR_CHECK(err);
    err = sd_ppi_channel_enable_set((PPI_CHEN_CH0_Enabled << PPI_CHEN_CH0_Pos)
                                | (PPI_CHEN_CH1_Enabled << PPI_CHEN_CH1_Pos)
                                | (PPI_CHEN_CH2_Enabled << PPI_CHEN_CH2_Pos));
    APP_ERROR_CHECK(err);*/
#else
    // Low to high -> Capture CC[0]
    NRF_PPI->CH[0].EEP = (uint32_t)&NRF_COMP->EVENTS_UP;
    NRF_PPI->CH[0].TEP = (uint32_t)&NRF_TIMER1->TASKS_CAPTURE[0];

    // High to low -> Capture CC[1]
    NRF_PPI->CH[1].EEP = (uint32_t)&NRF_COMP->EVENTS_DOWN;
    NRF_PPI->CH[1].TEP = (uint32_t)&NRF_TIMER1->TASKS_CAPTURE[1];

    // High to low -> Clear timer
    NRF_PPI->CH[2].EEP = (uint32_t)&NRF_COMP->EVENTS_DOWN;
    NRF_PPI->CH[2].TEP = (uint32_t)&NRF_TIMER1->TASKS_CLEAR;
    
  // Enable only PPI channels 0, 1 and 2
  NRF_PPI->CHEN = (PPI_CHEN_CH0_Enabled << PPI_CHEN_CH0_Pos)
                | (PPI_CHEN_CH1_Enabled << PPI_CHEN_CH1_Pos)
                | (PPI_CHEN_CH2_Enabled << PPI_CHEN_CH2_Pos);
#endif
}

static void timer1_init(void)
{
  /* Start 16 MHz crystal oscillator */
#ifdef ENABLE_BT
  sd_clock_hfclk_request();
  uint32_t running = 0;
  do
  {
    sd_clock_hfclk_is_running(&running);
  } while(running == 0);
#else
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
      /* Wait for the external oscillator to start up */
    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);
#endif

  NRF_TIMER1->SHORTS        = 0x1;   // COMPARE[0] -> CLEAR
  NRF_TIMER1->CC[0]         = (1042) / 2;
  NRF_TIMER1->MODE          = TIMER_MODE_MODE_Timer;
  NRF_TIMER1->BITMODE       = TIMER_BITMODE_BITMODE_32Bit;
  NRF_TIMER1->PRESCALER     = 8;
  NRF_TIMER1->TASKS_START   = 1; // Start clocks
}
#endif

void sc7a20_readXYZ(SC7A20_t* s);

/**@brief Application main function.
 */
int main(void)
{
    //io_init();
    // Initialize.
#if UART_ENABLED
    uart_init();
#endif

#ifndef COMP_TEST
    log_init();

    printf("RESETREAS = 0x%x\n", NRF_POWER->RESETREAS);

    timers_init();    

    power_management_init();

#ifdef ENABLE_BT
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();
#else
    lfclk_init(); // Already initialized    
#endif
    
    sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE); // Enable DCDC converter
    i2c_init();    
    sc7a20_init(&s_accel, 0x19);
    io_init();
    
    adc_init();
        
#ifdef _OLD_MLINK
    comp_init();
#endif
    drv_speaker_init_t cfg =
    {
        .evt_handler = spk_handler
    };
    drv_speaker_init(&cfg);

    softpwm_init();

    cmp_init(&commandparser_event_handle);    

    

    // Start execution.
    //printf("\r\nUART started.\r\n");
    //NRF_LOG_INFO("Debug logging for UART over RTT started.");
#ifdef ENABLE_BT
    advertising_start();
#endif

#endif

    ppi_init();
    timer1_init();

#ifdef COMP_TEST
    log_init();
    timers_init();  
    lfclk_init();
    io_init();
    softpwm_init();
    comp_init();
#endif

    LEDInit();
    FAnimInit();

    nrf_gpio_pin_clear(OUTID_AVDD);

    //accel_test();
        {            
            SC7A20_mode_t mode =
            {
                .rate       = SC7A20_RATE_25HZ,
                .axis       = SC7A20_X_ENABLE | SC7A20_Y_ENABLE | SC7A20_Z_ENABLE,
                .lowpower   = SC7A20_LOWPOWER
            };
            sc7a20_start(&s_accel, &mode);
            sc7a20_setRange(&s_accel, SC7A20_RANGE_2G);
            SC7A20_filter_t filter =
            {
                .target     = SC7A20_HPF_TARGET_CLICK,
                .mode       = SC7A20_HPF_MODE_NORMAL,
                .cutoff     = SC7A20_HPF_CUTOFF_1,
                .ref = 0
            };
            sc7a20_setHPF(&s_accel, &filter);
            SC7A20_clickDetectionConfig_t clickCfg =
            {
                .axis       = SC7A20_CLICK_AXIS_X_SINGLE | SC7A20_CLICK_AXIS_Y_SINGLE | SC7A20_CLICK_AXIS_Z_SINGLE,
                .threshold_mg       = 400,
                .time_limit_ms      = 2,
                .time_latency_ms    = 200,
                .time_window_ms     = 1000
            };
            sc7a20_setClickInterrupt(&s_accel, &clickCfg);
            sc7a20_setDataReporting(&s_accel, true);
            while(sc7a20_commitWrites(&s_accel));
    
            while(1)
            {
                uint32_t events = sc7a20_getEventsFlags(&s_accel);
                if(events & SC7A20_EVTFLAG_DATA)
                {
                    SC7A20_sample_t smp;
                    sc7a20_getLastSample(&s_accel, &smp);
                    printf(">> %d, %d, %d\n", smp.x, smp.y, smp.z);
                }
                uint32_t result = nrf_gpio_pin_read(INID_ACCELINT1);
                uint32_t result2 = nrf_gpio_pin_read(INID_ACCELINT2);
                static uint32_t r_result1, r_result2;

                if(result != r_result1)
                {
                    printf("INT1: %d -> %d\n", r_result1, result);
                }

                if(result2 != r_result2)
                {
                    printf("INT2: %d -> %d\n", r_result2, result2);
                }
                r_result1 = result;
                r_result2 = result2;
            }


/*            while(1)
            {
                sc7a20_readOperation(&s_accel, SC7A20_READ_OP_XYZ);
            }
            */
        }



#ifdef ENABLE_BT
    //sleep_mode_enter();
#else
    //nrf_delay_ms(1000); // Defective board
   // accel_off();

    //SynthPlaySong(&zelda);
    //drv_speaker_sample_play(0);

    while(1)
    {
        //system_power_off_no_sd();
    }
#endif
       


    // Enter main loop.
    for (;;)
    {

        static uint64_t lastTime = 0;
         if(lastTime == 0)
            lastTime = HALSystemGetTimeUS();
        uint64_t now = HALSystemGetTimeUS();

        float elapsedms = (now - lastTime) / 1000;
        
        if (now < lastTime) 
        {
            lastTime = now;
            continue;
        }

        while(elapsedms > 1.0)
        {
            accel_update();
            LEDUpdate(1);
            FAnimUpdate(1);
            elapsedms -= 1.0;
            lastTime = now;
        }
        
        idle_state_handle();
    }
}

void setVibration(uint8_t level)
{
    low_power_pwm_duty_set(&low_power_pwm_3, level);
}

#ifdef _MLINK_OUTPUT
void mldec_onProlog()
{
    printf("[MLINK] PROLOG\n");
}

void mldec_onByte(uint8_t data)
{
    printf("[MLINK] ======= BYTE: 0x%x\n", data);
    tmpToggle = true;
}

void mldec_onError(uint32_t errorType)
{
    printf("[MLINK] ERROR: 0x%d\n", errorType);
}

void mldsp_onWaveCycle(mldec_WaveCycle_t cycle)
{
    mldec_decode(cycle);
}


////////
/*
uint16_t sc7a20_i2c_write(SC7A20_t* s, uint8_t *data_raw, uint16_t len)
{
    nrf_drv_twi_xfer_desc_t xfer = NRF_DRV_TWI_XFER_DESC_TX(s->dev_addr, data_raw, len);    
    ret_code_t ret = nrf_drv_twi_xfer(&m_twi, &xfer, 0);
    return ret;
}

uint16_t sc7a20_i2c_read(SC7A20_t* s, SC7A20_pendingRead_t* data)
{
    nrf_drv_twi_xfer_desc_t xfer = NRF_DRV_TWI_XFER_DESC_TXRX(s->dev_addr, &(data->reg), 1, (uint8_t*)&(s->rBuffer[0]), data->length);
    
    ret_code_t ret = nrf_drv_twi_xfer(&m_twi, &xfer, 0);
    return ret;
}

void twi_event_handler(nrf_drv_twi_evt_t const * p_event,
                       void *                    p_context)
{
    if ((p_event->type == NRF_DRV_TWI_EVT_DONE))
    {
        if(p_event->xfer_desc.type == NRF_DRV_TWI_XFER_TX)
        {
            sc7a20_i2c_event_handler((SC7A20_t*)p_context, SC7A20_BUS_TX_DONE);
        } else if(p_event->xfer_desc.type == NRF_DRV_TWI_XFER_RX)
        {
            sc7a20_i2c_event_handler((SC7A20_t*)p_context, SC7A20_BUS_RX_DONE);
        }
    } else if(p_event->type == NRF_DRV_TWI_EVT_ADDRESS_NACK ||
              p_event->type == NRF_DRV_TWI_EVT_DATA_NACK)
    {
        sc7a20_i2c_event_handler((SC7A20_t*)p_context, SC7A20_BUS_ERROR);
    }
}

bool sc7a20_i2c_getIsBusy()
{
    return nrf_drv_twi_is_busy(&m_twi);
}
*/
////////



#endif

/**
 * @}
 */
