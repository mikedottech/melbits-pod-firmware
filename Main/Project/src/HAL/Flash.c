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
#include "HAL/HAL.h"

#include "HAL/Flash.h"

#include "nrf_section.h"
#include "nrf_atomic.h"

#include <nrf52810.h>
#include <nrf52810_bitfields.h>
#include <nrf_soc.h>
#include <app_error.h>
#include <nrf_atomic.h>

#ifdef SOFTDEVICE_PRESENT
#   include <nrf_sdh.h>
#   include <nrf_sdh_soc.h>
#endif

#include "Debug.h"

#include "utils.h"

NRF_SECTION_DEF(flash_storage, uint8_t);

#define APP_FLASH_OBSERVER_PRIO           1                                           /**<  Flash operation result observer priority */

nrf_atomic_u32_t s_HALflashEventFlags;

/*
typedef enum
{    
    FSS_IDLE = 0,
    FSS_ERASING
} HAL_flashFSMState_t;

typedef struct
{
    HAL_flashFSMState_t state;
    uint32_t addrStart;
    uint16_t loops;
} HAL_flashState_t;

HAL_flashState_t s_HAL_flashState;
*/


void HAL_flashInit(void)
{
    s_HALflashEventFlags = 0;
//    memset(&s_HAL_flashState, 0, sizeof(s_HAL_flashState));
}

/*
uint32_t _HAL_flashGetAndClearEventFlags(void)
{    
}

static void _HAL_flashFSMTransitionToState(HAL_flashFSMState_t next)
{
    if(s_HAL_flashState.state == next) return;

    switch(next)
    {
        case FSS_ERASING:
            MBT_LOG("ERASING...\n");
        break;
        case FSS_IDLE:
            MBT_LOG("IDLE\n");
        break;
    }

    s_HAL_flashState.state = next;
}

void HAL_flashTick(uint32_t ticks)
{
    switch(s_HAL_flashState.state)
    {
        case FSS_ERASING:
            MBT_FLAG_SET(NRF_NVMC->CONFIG, NVMC_CONFIG_WEN_Een);
            NRF_NVMC->ERASEPAGEPARTIALCFG = 5; // 5 ms increments
            NRF_NVMC->ERASEPAGEPARTIAL = s_HAL_flashState.addrStart;
            MBT_FLAG_CLEAR(NRF_NVMC->CONFIG, NVMC_CONFIG_WEN_Een);
            if(--s_HAL_flashState.loops == 0)
            {
                _HAL_flashFSMTransitionToState(FSS_IDLE);
            }
        break;
    }
}

bool HAL_flashErasePage(uint32_t offset)
{
    if(!NRF_NVMC->READY)
    {
        return false;
    }
    s_HAL_flashState.addrStart = offset;
    s_HAL_flashState.loops = 85 / 5;
    _HAL_flashFSMTransitionToState(FSS_ERASING);
    return true;
}
*/
#define HAL_FLASH_EVENT_FLAG_SUCCESS    (1)
#define HAL_FLASH_EVENT_FLAG_ERROR      (2)

uint32_t _HAL_flashGetAndClearEventFlags(void)
{
    return nrf_atomic_u32_fetch_store(&s_HALflashEventFlags, 0);
}

void HAL_flashGetStorageAbsoluteAddr(uint8_t **ppAddr, uint16_t *pLen)
{
    *ppAddr = (uint8_t *)NRF_SECTION_START_ADDR(flash_storage);
    *pLen = (uint16_t)NRF_SECTION_LENGTH(flash_storage);
}

static bool _HAL_flashCheckRange(uint32_t offset)
{
    return (offset < NRF_SECTION_LENGTH(flash_storage));
}
/*
extern const uint8_t* s_pTmp;
void HAL_testErase(void)
{
    static const uint8_t ref[] = {0xd9, 0x24, 0x9a, 0xba, 0xe4};
    
    if(memcmp(&ref[0], s_pTmp, sizeof(ref)))
    {
        MBT_LOG("ERASED");
    } else {
        MBT_LOG("NOT ERASED");
    }
}
*/

#ifdef SOFTDEVICE_PRESENT

/**
 * @brief SoftDevice SoC event handler.
 *
 * @param[in] evt_id    SoC event.
 * @param[in] p_context Context.
 */
static void soc_evt_handler(uint32_t evt_id, void * p_context)
{
    if (evt_id == NRF_EVT_FLASH_OPERATION_SUCCESS)
    {
        //MBT_LOG("[FLASH] SUCCESS\n");
        //s_HALflashFlags
        nrf_atomic_u32_or(&s_HALflashEventFlags, HAL_FLASH_EVENT_FLAG_SUCCESS);
    } else if (evt_id == NRF_EVT_FLASH_OPERATION_ERROR)
    {
        //MBT_LOG("[FLASH] ERROR\n");        
        nrf_atomic_u32_or(&s_HALflashEventFlags, HAL_FLASH_EVENT_FLAG_ERROR);
    }
}
NRF_SDH_SOC_OBSERVER(m_soc_evt_observer, APP_FLASH_OBSERVER_PRIO, soc_evt_handler, NULL);
#endif

bool HAL_flashErasePage(uint16_t offset)
{
    if(!NRF_NVMC->READY)
    {
        return false;
    }

    if(!_HAL_flashCheckRange(offset))
    {
        return false;
    }

    offset &= ((uint32_t)NRF_SECTION_LENGTH(flash_storage) - 1);

    offset %= 0x1000; // First word in page

    uint32_t finalAddr = (uint32_t)((uint32_t)NRF_SECTION_START_ADDR(flash_storage) + offset);
    
#ifdef SOFTDEVICE_PRESENT
    uint32_t pageNo = (finalAddr >> 12); /// 4096;
    uint32_t err = sd_flash_page_erase(pageNo);
    APP_ERROR_CHECK(err);
    return (err == 0);
#else
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een;
    NRF_NVMC->ERASEPAGE = finalAddr;
    NRF_NVMC->CONFIG = 0;
#endif

    //HAL_testErase();


    return true;
}

#if SOFTDEVICE_PRESENT

bool HAL_flashWrite(const uint32_t* data, uint16_t offset, uint16_t len)
{
    if(!_HAL_flashCheckRange(offset))
    {
        MBT_LOG("BAD RANGE\n");
        return false;
    }

    if(offset + len >= (uint32_t)NRF_SECTION_LENGTH(flash_storage))
    {
        MBT_LOG("BAD RANGE (END)\n");
        return false;
    }

    // Compute pointer and align to 4 bytes
    uint32_t *ptr = (uint32_t*)(((uint32_t)NRF_SECTION_START_ADDR(flash_storage) + offset) & 0xFFFFFFFC);
    
    MBT_LOG("PTR = 0x%x\n, DATA = 0x%x, LEN = %d", ptr, (uint32_t)data, (uint32_t)(len >> 2));

    uint32_t err = sd_flash_write(ptr, (const uint32_t*)data, (len >> 2));

    MBT_LOG("WR RESULT = 0x%x\n");

    return ( err == 0 );
}

#else

bool HAL_flashWrite(const uint8_t* data, uint16_t offset, uint16_t len)
{
    if(!_HAL_flashCheckRange(offset))
    {
        return false;
    }

    if(offset + len >= (uint32_t)NRF_SECTION_LENGTH(flash_storage))
    {
        return false;
    }

    uint32_t buffer = 0;

    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
    uint32_t *ptr = (uint32_t*)(((uint32_t)NRF_SECTION_START_ADDR(flash_storage) + offset) & 0xFFFFFFFC);
    
    uint8_t diff = offset % 4;
    uint8_t bytecnt = 0;

    for(uint16_t i = 0; i < diff; ++i, ++bytecnt)
    {
        buffer >>= 8;
        buffer |= (0xFF000000);
    }
    

    for(uint16_t i = 0; i < len; ++i)
    {
        buffer >>= 8;
        buffer |= (*(data++) << 24);
        

        if(++bytecnt == 4)
        {
            while(!NRF_NVMC->READY);
            *(ptr++) = buffer;
            buffer = 0;
            bytecnt = 0;
        }
    }

    if(bytecnt > 0)
    {
        for(uint16_t i = 0; i < 4-bytecnt; ++i)
        {
            buffer >>= 8;
            buffer |= (0xFF000000);
        }
        while(!NRF_NVMC->READY);
        *(ptr++) = buffer;
    }
    NRF_NVMC->CONFIG = 0;
    return false;
}
#endif
