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

#include "BLE_Internal.h"

#include "app_error.h"
#include "ble_nus.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_qwr.h"

#include "nrf_ringbuf.h"

BLE_NUS_DEF(s_HALbleNus, NRF_SDH_BLE_TOTAL_LINK_COUNT);  
NRF_BLE_QWR_DEF(s_HALbleQwr); 

#if MBT_CFG_BLE_MAX_PAYLOAD_LEN > BLE_NUS_MAX_DATA_LEN
#error MBT_CFG_BLE_MAX_PAYLOAD_LEN too large!
#endif

//NRF_RINGBUF_DEF(s_HALbleRXRingBuf, MBT_CFG_BLE_RX_BUFFER_SIZE_BYTES);
//NRF_RINGBUF_DEF(s_HALbleTXRingBuf, MBT_CFG_BLE_TX_BUFFER_SIZE_BYTES);

volatile bool s_HALbleNUSReady = false;
extern uint16_t   s_HALbleConnHandle;

// --------------------------------------------------------------

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

// --------------------------------------------------------------

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
    switch(p_evt->type)
    {
        case BLE_NUS_EVT_RX_DATA:
        {
            nrf_atomic_u32_or(&s_HALbleEventFlags, HAL_BLE_EVENT_FLAG_DATARX);        
            //size_t len = p_evt->params.rx_data.length;
            //uint32_t err_code = nrf_ringbuf_cpy_put(&s_HALbleRXRingBuf, p_evt->params.rx_data.p_data, &len);
            
            HAL_ISR_onBLERX(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);

            size_t len = 0;
            uint32_t err_code = 0;

            if(err_code != NRF_SUCCESS || len < p_evt->params.rx_data.length)
            {
                nrf_atomic_u32_or(&s_HALbleEventFlags, HAL_BLE_EVENT_FLAG_RX_OVERFLOW);
            }
            break;
        }
        
        case BLE_NUS_EVT_TX_RDY:
        {
            // For flow control
            nrf_atomic_u32_or(&s_HALbleEventFlags, HAL_BLE_EVENT_FLAG_TX_READY);
        
            // Send queued data in packets
            /*
            uint8_t *pData;
            size_t len;
            do
            {
                len = HAL_bleGetMTU();
                uint32_t err_code = nrf_ringbuf_get(&s_HALbleTXRingBuf, &pData, &len, true);
                if(err_code == NRF_SUCCESS)
                {
                    if(len > 0)
                    {
                        uint16_t sLen = (uint16_t)len;
                        err_code = ble_nus_data_send(&s_HALbleNus, pData, &sLen, s_HALbleConnHandle);
                        MBT_ASSERT(err_code == NRF_SUCCESS);
                        if(err_code != NRF_SUCCESS)
                        {
                            // Other error
                            // TODO: ERROR: Pulled data cannot be sent
                            //MBT_LOG("DATA SEND RINGBUF ERR: %d\n", err_code);
                            // Free 0 bytes from ring buffer (keep the data to try again later)
                            // and quit the loop
                            len = 0;                            
                        }
                    }
                } else {
                    MBT_LOG("RINGBUF PULL ERR: %d\n", err_code);
                    len = 0;
                }
                nrf_ringbuf_free(&s_HALbleTXRingBuf, len);                                
            } while(len > 0);
            */
            break;        
        }        
        
        case BLE_NUS_EVT_COMM_STARTED:
        {
            s_HALbleNUSReady = true;
            nrf_atomic_u32_or(&s_HALbleEventFlags, HAL_BLE_EVENT_FLAG_CONNECTION);            
            break;
        }
        
        case BLE_NUS_EVT_COMM_STOPPED:
        {
            _ISR_HAL_bleNusOnDisconnect();
            break;
        }
    }
}

void _ISR_HAL_bleNusOnDisconnect(void)
{
    s_HALbleNUSReady = false;
    //nrf_ringbuf_init(&s_HALbleRXRingBuf);
    //nrf_ringbuf_init(&s_HALbleTXRingBuf);
}

// -----------------------------------------------------------------

/**@brief Function for initializing services that will be used by the application.
 */
uint32_t _HAL_bleServicesInit(void)
{
    uint32_t           err_code;
    ble_nus_init_t     nus_init;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize the ring buffers
    //nrf_ringbuf_init(&s_HALbleRXRingBuf);
    //nrf_ringbuf_init(&s_HALbleTXRingBuf);

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&s_HALbleQwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&s_HALbleNus, &nus_init);
    APP_ERROR_CHECK(err_code);

    return err_code;
}

// -----------------------------------------------------------------

uint32_t _HAL_bleAssignConnectionHandleToQWR(uint16_t handle)
{
    return nrf_ble_qwr_conn_handle_assign(&s_HALbleQwr, handle);
}

// -----------------------------------------------------------------

uint32_t HAL_bleEnqueueSendNotification(const uint8_t *pData, uint16_t* pLen, bool enqueue)
{
    uint32_t err_code = ble_nus_data_send(&s_HALbleNus, (uint8_t*)pData, pLen, s_HALbleConnHandle);

/*
    if(err_code != NRF_SUCCESS && enqueue)
    {   
        size_t bLen = *pLen;
        err_code = nrf_ringbuf_cpy_put(&s_HALbleTXRingBuf, pData, &bLen);
        *pLen = bLen;
    }
*/

    return err_code;
}

// -----------------------------------------------------------------
/*
uint32_t HAL_bleReadBytes(const uint8_t **ppData, size_t *pLen)
{
    return nrf_ringbuf_get(&s_HALbleRXRingBuf, (uint8_t **)ppData, pLen, true);
}

// -----------------------------------------------------------------

uint32_t HAL_bleFreeBytes(size_t len)
{
    return nrf_ringbuf_free(&s_HALbleRXRingBuf, len);
}

// -----------------------------------------------------------------

size_t HAL_bleGetAvailableReadBytes()
{
    return (size_t)(s_HALbleRXRingBuf.p_cb->wr_idx - s_HALbleRXRingBuf.p_cb->tmp_rd_idx);
}
*/
// -----------------------------------------------------------------

HAL_bleConnState_t HAL_bleGetConnState(void)
{
    if(s_HALbleConnHandle != BLE_CONN_HANDLE_INVALID)
    {
        if(s_HALbleNUSReady) return HAL_BLE_CONN_STATE_CONNECTED;
        else return HAL_BLE_CONN_STATE_CONNECTING;
    }

    return HAL_BLE_CONN_STATE_DISCONNECTED;    
}
