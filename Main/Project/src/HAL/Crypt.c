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

#include "nrf_soc.h"
#include "nrf_delay.h"
#include "app_error.h"
#include <string.h> // For memcpy()
//#include <app_util_platform.h>
#include "HAL.h"

#define ECB_KEY_LEN            (16UL)
#define COUNTER_BYTE_LEN       (4UL)
#define NONCE_RAND_BYTE_LEN    (12UL)

// The RNG wait values are typical and not guaranteed. See Product Specifications for more info.
#ifdef NRF51
#define RNG_BYTE_WAIT_US       (677UL)
#elif defined NRF52810_XXAA
#define RNG_BYTE_WAIT_US       (124UL)
#else
#error "Either NRF51 or NRF52 must be defined."
#endif

static bool m_initialized = false;

/**
 * @brief Uses the RNG to write a 12-byte nonce to a buffer
 * @details The 12 bytes will be written to the buffer starting at index 4 to leave
 *          space for the 4-byte counter value.
 *
 * @param[in]    p_buf    An array of length 16
 */
void HAL_cryptNonceGenerate(uint8_t * p_buf)
{
    uint8_t i         = COUNTER_BYTE_LEN;
    uint8_t remaining = NONCE_RAND_BYTE_LEN;

    // The random number pool may not contain enough bytes at the moment so
    // a busy wait may be necessary.
    while(0 != remaining)
    {
        uint32_t err_code;
        uint8_t  available = 0;

        err_code = sd_rand_application_bytes_available_get(&available);
        APP_ERROR_CHECK(err_code);

        available = ((available > remaining) ? remaining : available);
        if (0 != available)
        {
	        err_code = sd_rand_application_vector_get((p_buf + i), available);
	        APP_ERROR_CHECK(err_code);

	        i         += available;
	        remaining -= available;
	    }

	    if (0 != remaining)
	    {
	        nrf_delay_us(RNG_BYTE_WAIT_US * remaining);
	    }
    }    
}

/**
 * @brief Initializes the module with the given nonce and key
 * @details The nonce will be copied to an internal buffer so it does not need to
 *          be retained after the function returns. Additionally, a 32-bit counter
 *          will be initialized to zero and placed into the least-significant 4 bytes
 *          of the internal buffer. The nonce value should be generated in a
 *          reasonable manner (e.g. using this module's nonce_generate function).
 *
 * @param[in]    p_nonce    An array of length 16 containing 12 random bytes
 *                          starting at index 4
 * @param[in]    p_ecb_key  An array of length 16 containing the ECB key
 */
void HAL_cryptCtrInit(HAL_cryptCtx_t *p_ctx, const uint8_t *p_nonce, const uint8_t *p_ecb_key)
{
    m_initialized = true;

    nrf_ecb_hal_data_t *p_ecb_data = (nrf_ecb_hal_data_t *)p_ctx;

    memset(p_ecb_data, 0, sizeof(HAL_cryptCtx_t));

    // Save the key.
    memcpy(&(p_ecb_data->key[0]), p_ecb_key, ECB_KEY_LEN);

    // Copy the nonce.
    memcpy(&(p_ecb_data->cleartext[COUNTER_BYTE_LEN]),
	          &p_nonce[COUNTER_BYTE_LEN],
	          NONCE_RAND_BYTE_LEN);

    // Zero the counter value.
    memset(&(p_ecb_data->cleartext[0]), 0x00, COUNTER_BYTE_LEN);
}

static void _HAL_cryptSetCounter(HAL_cryptCtx_t *p_ctx, CommCtr_t *pCtr)
{
    nrf_ecb_hal_data_t *p_ecb_data = (nrf_ecb_hal_data_t *)p_ctx;

    *((CommCtr_t*)&(p_ecb_data->cleartext[0])) = *pCtr;
}

static void _HAL_cryptRestoreCounter(HAL_cryptCtx_t *p_ctx, CommCtr_t *pCtr)
{
    nrf_ecb_hal_data_t *p_ecb_data = (nrf_ecb_hal_data_t *)p_ctx;
    *pCtr = *((CommCtr_t*)&(p_ecb_data->cleartext[0]));
}

//#include "Debug.h"

//void _dumpArray(uint8_t* a, uint8_t len)
//{
//    for(uint8_t i = 0; i < len; ++i)
//    {
//        MBT_LOG("%x ", a[i]);
//    }
//}

static uint32_t crypt(HAL_cryptCtx_t *p_ctx, uint8_t * buf, uint16_t len, CommCtr_t *pCtr)
{
    uint32_t  i;
    uint32_t err_code;
    nrf_ecb_hal_data_t *p_ecb_data = (nrf_ecb_hal_data_t *)p_ctx;

    //uint16_t nIterations = (len / ECB_KEY_LEN) + ((len % ECB_KEY_LEN) > 0 ? 1 : 0);

    if (!m_initialized)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    _HAL_cryptSetCounter(p_ctx, pCtr);

    i = 0;
    
    while(i < len)
    {
        err_code = sd_ecb_block_encrypt(p_ecb_data);
        if (NRF_SUCCESS != err_code)
        {
            return err_code;
        }

        for (uint32_t j = 0; i < len && j < ECB_KEY_LEN; ++i, ++j)
        {
            buf[i] ^= p_ecb_data->ciphertext[j];
        }

        //for (; i < len ; ++i)
        //{
        //    buf[i] ^= p_ecb_data->ciphertext[i % ECB_KEY_LEN];
        //}
    
        // Increment the counter.
        (*((CommCtr_t*)&(p_ecb_data->cleartext[0])))++;
    }
    _HAL_cryptRestoreCounter(p_ctx, pCtr);

    return NRF_SUCCESS;
}

/**
 * @brief Encrypts the given buffer in-situ
 * @details The encryption step is done separately (using the nonce, counter, and
 *          key) and then the result from the encryption is XOR'd with the given
 *          buffer in-situ. The counter will be incremented only if no error occurs.
 *
 * @param[in]    p_clear_text    An array of length 16 containing the clear text
 *
 * @retval    NRF_SUCCESS                         Success
 * @retval    NRF_ERROR_INVALID_STATE             Module has not been initialized
 * @retval    NRF_ERROR_SOFTDEVICE_NOT_ENABLED    SoftDevice is present, but not enabled
 */
uint32_t HAL_cryptCtrEncrypt(HAL_cryptCtx_t *p_ctx, uint8_t *p_clear_text, uint16_t len, CommCtr_t *pCtr)
{
    return crypt(p_ctx, p_clear_text, len, pCtr);
}

/**
 * @brief Decrypts the given buffer in-situ
 * @details The encryption step is done separately (using the nonce, counter, and
 *          key) and then the result from the encryption is XOR'd with the given
 *          buffer in-situ. The counter will be incremented only if no error occurs.
 *
 * @param[in]    p_cipher_text    An array of length 16 containing the cipher text
 *
 * @retval    NRF_SUCCESS                         Succeess
 * @retval    NRF_ERROR_INVALID_STATE             Module has not been initialized
 * @retval    NRF_ERROR_SOFTDEVICE_NOT_ENABLED    SoftDevice is present, but not enabled
 */
uint32_t HAL_cryptCtrDecrypt(HAL_cryptCtx_t *p_ctx, uint8_t *p_cipher_text, uint16_t len, CommCtr_t *pCtr)
{
    return crypt(p_ctx, p_cipher_text, len, pCtr);
}
