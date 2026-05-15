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
#include "Debug.h"
#include "mbt_config.h"

#include "HAL.h"
#include "BLE_Internal.h"

#include "app_error.h"
#include "ble_advertising.h"

#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"

#include "ble_nus.h"

// --------------------------------------------------------------

BLE_ADVERTISING_DEF(s_HALbleAdvertising);                           /**< Advertising module instance. */

#define APP_ADV_INTERVAL                (64)                        /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */
#define APP_ADV_DURATION                (1000)                       /**< The advertising duration (10 seconds) in units of 10 milliseconds. */

#define APP_ADV_SLOW_INTERVAL            (0x0C80 >> 2) /*0x0C80*/                       /**< The advertising interval (in units of 0.625 ms. This value corresponds to 2 s). */
#define APP_ADV_SLOW_DURATION            18000                          /**< The advertising duration (180 seconds) in units of 10 milliseconds. */

#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN  /*BLE_UUID_TYPE_BLE = 16-bit UUID*/                 /**< UUID type for the Nordic UART Service (vendor specific). */

#define BLE_ADV_MANUFDATA_MAX_SIZE      (sizeof(HAL_BroadcastPacket_t))

typedef struct 
{
    ble_advdata_t advData;
    ble_advdata_manuf_data_t manufData;
    ble_gap_adv_data_t gap_advData;
    uint8_t manufDataStorage[BLE_ADV_MANUFDATA_MAX_SIZE];
    uint8_t enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
} HAL_bleAdvBuffer_t;

static HAL_bleAdvBuffer_t s_HAL_bleAdvBuffer[2];                     /**< Double buffered advertising manufecturer data */
static uint8_t s_HAL_bleAdvBufferIdx = 0;                            /**< Points to the next available manufacturer data slot */
static HAL_bleAdvState_t s_HAL_bleAdvState = HAL_BLE_ADV_STATE_OFF;

// TODO: Remove / change this?
static ble_uuid_t s_HAL_bleAdvUuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};

// --------------------------------------------------------------
void _HALbleAdvTransitionToState(HAL_bleAdvState_t next)
{
    if(s_HAL_bleAdvState == next)
        return;

    nrf_atomic_u32_or(&s_HALbleEventFlags, HAL_BLE_EVENT_FLAG_ADVERTISEMENT);

    s_HAL_bleAdvState = next;
}

// --------------------------------------------------------------

/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void _HAL_bleOnAdvEvt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            _HALbleAdvTransitionToState(HAL_BLE_ADV_STATE_FAST);            
            break;
        case BLE_ADV_EVT_SLOW:
            _HALbleAdvTransitionToState(HAL_BLE_ADV_STATE_SLOW);
            break;
        case BLE_ADV_EVT_IDLE:      
            _HALbleAdvTransitionToState(HAL_BLE_ADV_STATE_OFF);
            break;
            /*
        case BLE_ADV_EVT_WHITELIST_REQUEST:
            {
                // TODO: Provide whitelist
                ble_gap_addr_t    p_whitelist_addr[1];
                ble_gap_irk_t     p_whitelist_irk[1];

                err_code = ble_advertising_whitelist_reply(&s_HALbleAdvertising,
                                             p_whitelist_addr,
                                             1,
                                             p_whitelist_irk,
                                             1);
                APP_ERROR_CHECK(err_code);    
            }
            break;
            */
        default:
            MBT_LOG("ADV_EVT_UNKNOWN\n");
            break;
    }
}

// --------------------------------------------------------------

static HAL_bleAdvBuffer_t *_HAL_bleGetNextAdvBuffer(void)
{
    HAL_bleAdvBuffer_t *ret = &s_HAL_bleAdvBuffer[s_HAL_bleAdvBufferIdx++];
    s_HAL_bleAdvBufferIdx &= 0x1;
    return ret;
}

// --------------------------------------------------------------

void _HAL_bleInitManufData(HAL_bleAdvBuffer_t *pAdvBuffer, HAL_smallBuffer_t *pManufDataBuffer)
{
    pAdvBuffer->manufData.company_identifier = MBT_CFG_BLE_COMPANY_IDENTIFIER;
    pAdvBuffer->manufData.data.p_data = &pAdvBuffer->manufDataStorage[0];
    pAdvBuffer->manufData.data.size = MIN(BLE_ADV_MANUFDATA_MAX_SIZE, pManufDataBuffer->len);
    memcpy(&pAdvBuffer->manufDataStorage[0], pManufDataBuffer->pData, MIN(BLE_ADV_MANUFDATA_MAX_SIZE, pManufDataBuffer->len));
}

// --------------------------------------------------------------

uint32_t _HAL_bleUpdateAdvDataInternal(ble_advertising_t *p_advertising, ble_advdata_t *p_adv_data, uint16_t adv_data_len, uint8_t *enc_data_storage, ble_advdata_t *p_sr_data, uint16_t sr_data_len)
{
    ASSERT(p_advertising->initialized);

    uint32_t ret = NRF_SUCCESS;
    ble_gap_adv_data_t _new_advdata = {0};
    ble_gap_adv_data_t *new_advdata = &_new_advdata;

    new_advdata->adv_data.len = adv_data_len;      
    new_advdata->adv_data.p_data = enc_data_storage;

    ret = ble_advdata_encode(p_adv_data, new_advdata->adv_data.p_data, &new_advdata->adv_data.len);
    APP_ERROR_CHECK(ret);

    if (p_sr_data)
    {
        new_advdata->scan_rsp_data.len = sr_data_len;

        ret = ble_advdata_encode(p_sr_data,
                               new_advdata->scan_rsp_data.p_data,
                               &new_advdata->scan_rsp_data.len);
        APP_ERROR_CHECK(ret);
    }
    else
    {
        new_advdata->scan_rsp_data.p_data = NULL;
        new_advdata->scan_rsp_data.len    = 0;
    }

    ret = sd_ble_gap_adv_set_configure(&p_advertising->adv_handle, new_advdata, NULL);
    
    APP_ERROR_CHECK(ret);

    return ret;
}

// --------------------------------------------------------------

/**@brief Function for initializing the Advertising functionality.
 */
static uint32_t _HAL_bleAdvInit(HAL_smallBuffer_t *pInitialAdvertisingData)
{
    uint32_t               err_code;
    ble_advertising_init_t init;

    HAL_bleAdvBuffer_t *buf = &s_HAL_bleAdvBuffer[0];

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_SHORT_NAME; /*BLE_ADVDATA_FULL_NAME;*/
    init.advdata.short_name_len     = strlen(MBT_CFG_BLE_DEVICE_NAME);
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;
    static int8_t txpower           = MBT_CFG_BLE_TX_POWER_DBM;
    init.advdata.p_tx_power_level   = &txpower;

    //init.srdata.uuids_complete.uuid_cnt = sizeof(s_HAL_bleAdvUuids) / sizeof(s_HAL_bleAdvUuids[0]);
    //init.srdata.uuids_complete.p_uuids  = s_HAL_bleAdvUuids;

    _HAL_bleInitManufData(buf, pInitialAdvertisingData);

    init.advdata.p_manuf_specific_data = &buf->manufData;
    
    // Enable scan response data (more than 31 bytes)
    //init.srdata.name_type = BLE_ADVDATA_NO_NAME;
    //init.srdata.p_manuf_specific_data = &buf->manufData;
    
    // WARNING: Don't advartise again when disconnected
    init.config.ble_adv_on_disconnect_disabled = true;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;

    init.config.ble_adv_slow_enabled  = true;
    init.config.ble_adv_slow_interval = APP_ADV_SLOW_INTERVAL;
    init.config.ble_adv_slow_timeout  = APP_ADV_SLOW_DURATION;
    
    // TODO: Enable SLOW advertising too if requested
    // TODO: Directed advertising?
    init.evt_handler = _HAL_bleOnAdvEvt;
    

    s_HAL_bleAdvBuffer[0].advData = s_HAL_bleAdvBuffer[1].advData = init.advdata;
    s_HAL_bleAdvBufferIdx = 1;

    err_code = ble_advertising_init(&s_HALbleAdvertising, &init);
    APP_ERROR_CHECK(err_code);
    
//    err_code |= sd_ble_gap_adv_stop(s_HALbleAdvertising.adv_handle);
//    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&s_HALbleAdvertising, APP_BLE_CONN_CFG_TAG);
    
    err_code |= sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, s_HALbleAdvertising.adv_handle, MBT_CFG_BLE_TX_POWER_DBM);
    APP_ERROR_CHECK(err_code);
    
    return err_code;
}

// --------------------------------------------------------------

uint32_t HAL_bleStopAdvertising()
{
    s_HALbleAdvertising.adv_mode_current = BLE_ADV_MODE_IDLE;
    return sd_ble_gap_adv_stop(s_HALbleAdvertising.adv_handle);
}

// --------------------------------------------------------------

uint32_t HAL_bleUpdateAdvertisingData(HAL_smallBuffer_t *pData)
{
    HAL_bleAdvBuffer_t *advBuf = _HAL_bleGetNextAdvBuffer();
    _HAL_bleInitManufData(advBuf, pData);
  
    return _HAL_bleUpdateAdvDataInternal(&s_HALbleAdvertising, &advBuf->advData, s_HALbleAdvertising.adv_data.adv_data.len, &advBuf->enc_advdata[0], NULL, 0);
}

// --------------------------------------------------------------

uint32_t HAL_bleStartAdvertising(HAL_bleAdvertisingConfig_t *adv)
{
    uint32_t err_code = 0;
    static bool firstTime = true;
    if(firstTime)
    {
        err_code = _HAL_bleAdvInit(&adv->initialAdvertisingData);
        firstTime = false;
    } else {
        err_code = HAL_bleUpdateAdvertisingData(&adv->initialAdvertisingData);
    }
    s_HALbleAdvertising.adv_modes_config.ble_adv_fast_enabled = adv->fast;
    s_HALbleAdvertising.adv_modes_config.ble_adv_slow_enabled = !adv->fast;
    err_code = ble_advertising_start(&s_HALbleAdvertising, adv->fast ? BLE_ADV_MODE_FAST : BLE_ADV_MODE_SLOW);
    return err_code;
}

// --------------------------------------------------------------

HAL_bleAdvState_t HAL_bleGetAdvState(void)
{
    return s_HAL_bleAdvState;
}

// --------------------------------------------------------------

bool HAL_bleIsAdvertising(void)
{
    return s_HALbleAdvertising.adv_mode_current != BLE_ADV_MODE_IDLE;
}
