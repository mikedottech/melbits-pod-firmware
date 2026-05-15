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

#include "common.h"
#include "mbt_config.h"

#include "HAL.h"
#include "Debug.h"

#include "app_error.h"

#include "BLE_SD.h"
#include "BLE_Internal.h"

#include "ble_hci.h"

#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "ble_nus.h"

#include "app_timer.h"

#define HAL_BLE_INITIAL_MTU (BLE_GATT_ATT_MTU_DEFAULT - 3)

// INTERNAL -----------------------------------------------------
nrf_atomic_u32_t s_HALbleEventFlags;    // Event flags

// BLE ----------------------------------------------------------
#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(75, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3      

// NUS ----------------------------------------------------------
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN  
                                 /**< BLE NUS service instance. */

uint16_t   s_HALbleConnHandle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static uint32_t   s_HALbleMaxDataLen          = HAL_BLE_INITIAL_MTU;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
NRF_BLE_GATT_DEF(s_HALbleGatt);                                                           /**< GATT module instance. */
                                                            /**< Context for the Queued Write module.*/
// --------------------------------------------------------------

uint32_t HAL_bleGetMTU()
{
  return s_HALbleMaxDataLen;
}

// --------------------------------------------------------------

uint32_t HAL_bleDisconnect()
{
    uint32_t err_code;
    err_code = sd_ble_gap_disconnect(s_HALbleConnHandle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    APP_ERROR_CHECK(err_code);
    return err_code;
}

// --------------------------------------------------------------

static void _ISR_HAL_bleOnDisconnected(void)
{
    s_HALbleConnHandle = BLE_CONN_HANDLE_INVALID;
    s_HALbleMaxDataLen = HAL_BLE_INITIAL_MTU;
    _ISR_HAL_bleNusOnDisconnect();
    nrf_atomic_u32_or(&s_HALbleEventFlags, HAL_BLE_EVENT_FLAG_CONNECTION);
}

// --------------------------------------------------------------

/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void _ISR_HAL_bleEvtHandler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t err_code = 0;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            s_HALbleConnHandle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = _HAL_bleAssignConnectionHandleToQWR(s_HALbleConnHandle);
            APP_ERROR_CHECK(err_code);
            err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_CONN, s_HALbleConnHandle, MBT_CFG_BLE_TX_POWER_DBM);
            APP_ERROR_CHECK(err_code);
            // This is set from BLE_SD
            //nrf_atomic_u32_or(&s_HALbleEventFlags, HAL_BLE_EVENT_FLAG_CONNECTION);
            _HALbleAdvTransitionToState(HAL_BLE_ADV_STATE_OFF);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            _ISR_HAL_bleOnDisconnected();
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
            break;
        }
        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(s_HALbleConnHandle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(s_HALbleConnHandle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            _ISR_HAL_bleOnDisconnected();
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            _ISR_HAL_bleOnDisconnected();
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}

// --------------------------------------------------------------

static void _HAL_bleGattEvtHandler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if ((s_HALbleConnHandle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        s_HALbleMaxDataLen = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        MBT_LOG("Data len is set to 0x%X(%d)\n", s_HALbleMaxDataLen, s_HALbleMaxDataLen);
    }
    MBT_LOG("ATT MTU exchange completed. central 0x%x peripheral 0x%x\n",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}

// --------------------------------------------------------------
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
static void _HAL_bleOnConnParamsEvt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        // TODO: Connection error?
        err_code = sd_ble_gap_disconnect(s_HALbleConnHandle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void _HAL_bleOnConnParamsErrorHandler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

// -------------------------------------------------------------

// =======================================================================================
/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 */
static uint32_t _HAL_bleStackInit(void)
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
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, _ISR_HAL_bleEvtHandler, NULL);
    
    return err_code;
}

// --------------------------------------------------------------

/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static uint32_t _HAL_bleGapParamsInit(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode); // No security

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) MBT_CFG_BLE_DEVICE_NAME,
                                          strlen(MBT_CFG_BLE_DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);

    return err_code;
}

// --------------------------------------------------------------

/**@brief Function for initializing the GATT library. */
static uint32_t _HAL_bleGattInit(void)
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&s_HALbleGatt, _HAL_bleGattEvtHandler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&s_HALbleGatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);

    return err_code;
}

static uint32_t _HAL_bleConnParamsInit(void)
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
    cp_init.evt_handler                    = _HAL_bleOnConnParamsEvt;
    cp_init.error_handler                  = _HAL_bleOnConnParamsErrorHandler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
    return err_code;
}

// --------------------------------------------------------------

uint32_t _HAL_bleInit()
{
    uint32_t err_code = 0;

    // SoftDevice
    err_code = _HAL_bleStackInit();

    // Connection params (Security, Timeouts, ...)
    err_code |= _HAL_bleGapParamsInit();

    // MTU, ...
    err_code |= _HAL_bleGattInit();

    // PG NUS service
    err_code |= _HAL_bleServicesInit();
    
    // Connectionp params
    err_code |= _HAL_bleConnParamsInit();

#ifdef MBT_CFG_PLATFORM_NRF52_USE_DCDC
    sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
#else
    sd_power_dcdc_mode_set(NRF_POWER_DCDC_DISABLE);
#endif
    

    s_HALbleEventFlags = 0;

    return err_code;
}

// --------------------------------------------------------------

uint32_t _HAL_bleGetAndClearEventFlags(void)
{
    return nrf_atomic_u32_fetch_store(&s_HALbleEventFlags, 0);
}

