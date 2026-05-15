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

#include "HAL.h"
#include "RTC.h"
#include "SoftPWM.h"
#include "I2C.h"
#include "Accel.h"
#include "GPIO.h"
#include "PWMAudio.h"
#include "SAADC.h"
#include "BLE_SD.h"
#include "BLE_Internal.h"
#include "Clocks.h"
#include "Flash.h"
#include "Debug.h"

#include "Drivers/Accel/SC7A20.h"

#include "mbt_config.h"

#include <nrf_drv_wdt.h>
#include <app_util_platform.h>
#include <nrf_pwr_mgmt.h>
#include <nrf_atomic.h>

#include "utils.h"

#include <nrf52810.h>
#include <nrf52810_bitfields.h>
#include <nrf_delay.h>
#include <nrf_mpu_lib.h>

// SVCI TO CALL BOOTLOADER FUNCTIONS ----------------------------------
#include <nrf_svci.h>
#include <nrf_svci_async_function.h>
#include <nrf_dfu_types.h>
#include <nrf_dfu_ble_svci_bond_sharing.h>

#ifdef SOFTDEVICE_PRESENT
#   include <nrf_sdm.h>
#   include <nrf_sdh_soc.h>
#endif

#define NRF_DFU_SVCI_SET_ADV_NAME         3

/**@brief Sets up the async SVCI interface for exchanging advertisement name to use when entering DFU mode.
 *
 * @details The advertisement name will be stored in flash by the bootloader. This requires memory management
 *          and handling forwarding of system events and state from the main application to the bootloader.
 *
 * @note    This is only available in the buttonless DFU that does not support bond sharing.
 */
//NRF_SVCI_ASYNC_FUNC_DECLARE(NRF_DFU_SVCI_SET_ADV_NAME, nrf_dfu_set_adv_name, nrf_dfu_adv_name_t, nrf_dfu_set_adv_name_state_t);

/**@brief Define functions for async interface to set new advertisement name for DFU mode.  */
NRF_SVCI_ASYNC_FUNC_DEFINE(NRF_DFU_SVCI_SET_ADV_NAME, nrf_dfu_set_adv_name, nrf_dfu_adv_name_t);


// --------------------------------------------------------------------

// LOG ----------------------------------------------------------------
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

// PRIVATE ============================================================

//! Test if in interrupt mode
inline static bool _Pod_HAL_sysIsInterrupt()
{
    return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0 ;
}


// APP CRASH HANDLER --------------------------------------------------
#ifdef PRODUCTION
void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    __disable_irq();
    NRF_BREAKPOINT_COND;
    // On assert, the system can only recover with a reset.
#if SOFTDEVICE_PRESENT
    sd_nvic_SystemReset();
#else
    NVIC_SystemReset();
#endif
}
#endif

// IO -----------------------------------------------------------------
extern uint32_t g_HAL_GPIOLatch;

// WDT ----------------------------------------------------------------
nrf_drv_wdt_channel_id s_HALWDTChannelID;

// SYS ----------------------------------------------------------------
static uint8_t s_nestedCS = 0;
extern uint32_t s_HAL_rtcCurrentTickIntervalMs;
nrfx_atomic_flag_t s_HAL_pendingTickFlag = 0;

// RAM RETENTION ------------------------------------------------------

NRF_SECTION_DEF(non_init, uint8_t);

// DEVICE INFO ------------------------------------------------------------------

 HAL_DeviceInfo_t g_HAL_devInfo;

// ------------------------------------------------------------------------------

#define MBT_OFFSETFROM(P, START)            (P - START)
#define MBT_PLATFORM_RAM_SLAVE(OFFSET)      (OFFSET / 8192)
#define MBT_PLATFORM_RAM_SECTION(OFFSET)    ((OFFSET % 8192) / 4096)

// ------------------------------------------------------------------------------
#define APP_START_ADDR CODE_START

static void _HAL_sys_ble_dfu_buttonless_on_sys_evt(uint32_t sys_evt, void * p_context);
// Register SoC observer for the Buttonless Secure DFU service
NRF_SDH_SOC_OBSERVER(_HAL_sys_dfu_buttonless_soc_obs, BLE_DFU_SOC_OBSERVER_PRIO, _HAL_sys_ble_dfu_buttonless_on_sys_evt, NULL);
static volatile bool _HAL_sysDFUIsWritingName = false;

static uint32_t _HAL_nrf_dfu_svci_vector_table_set(void)
{
    uint32_t err_code;
    uint32_t bootloader_addr = BOOTLOADER_ADDRESS;

    if (bootloader_addr != 0xFFFFFFFF)
    {
        NRF_LOG_INFO("Setting vector table to bootloader: 0x%08x", bootloader_addr);
        err_code = sd_softdevice_vector_table_base_set(bootloader_addr);
        if (err_code != NRF_SUCCESS)
        {
            NRF_LOG_ERROR("Failed running sd_softdevice_vector_table_base_set");
            return err_code;
        }

        return NRF_SUCCESS;
    }

    NRF_LOG_ERROR("No bootloader was found");
    return NRF_ERROR_NO_MEM;
}

static uint32_t _HAL_nrf_dfu_svci_vector_table_unset(void)
{
    uint32_t err_code;

    NRF_LOG_INFO("Setting vector table to main app: 0x%08x", APP_START_ADDR);
    err_code = sd_softdevice_vector_table_base_set(APP_START_ADDR);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Failed running sd_softdevice_vector_table_base_set");
        return err_code;
    }

    return NRF_SUCCESS;
}

static void _HAL_sys_ble_dfu_buttonless_on_sys_evt(uint32_t sys_evt, void * p_context)
{
    if (sys_evt != NRF_EVT_FLASH_OPERATION_ERROR && sys_evt != NRF_EVT_FLASH_OPERATION_SUCCESS)
            return;

    if (!_HAL_sysDFUIsWritingName)
            return;

    _HAL_sysDFUIsWritingName = false;

    if (nrf_dfu_set_adv_name_is_initialized())
    {
        uint32_t err_code = nrf_dfu_set_adv_name_on_sys_evt(sys_evt);

        if (err_code == NRF_SUCCESS)
        {            
        }
    }
}

static void _HAL_dfu_init(void)
{
    _HAL_nrf_dfu_svci_vector_table_set();
    nrf_dfu_set_adv_name_init();
    _HAL_nrf_dfu_svci_vector_table_unset();
}

static uint32_t _HAL_write_dfu_adv_name(const char* inName)
{
    static nrf_dfu_adv_name_t dfuAdvName;
    dfuAdvName.crc = 0xFFFFFFFF;
    dfuAdvName.len = MIN(sizeof(dfuAdvName.name)-1, strlen(inName));
    memcpy(dfuAdvName.name, inName, dfuAdvName.len);
    dfuAdvName.name[dfuAdvName.len] = '\0';
        
    return nrf_dfu_set_adv_name(&dfuAdvName);
}

// Call this just once when booting up
static uint32_t _HAL_sysInitDFUName(void)
{
    _HAL_dfu_init();
    char buf[20] = "MPDFU-\0\0\0\0\0\0\0\0";
    uint32_t devIDLSW = NRF_FICR->DEVICEID[0];
    itoa(devIDLSW, &buf[6], 16);
    
    uint32_t ret = _HAL_write_dfu_adv_name(&buf[0]);
    
    if (ret == NRF_SUCCESS)
    {
        _HAL_sysDFUIsWritingName = true;
        while(_HAL_sysDFUIsWritingName)
            _HAL_gpioCheckSysReset();
    }

    return ret;
}
// ------------------------------------------------------------------------------

static uint32_t _HAL_sysSetupFlashProtection()
{
    uint32_t err_code = 0;
#ifdef DEBUG
    err_code = sd_protected_register_write(&NRF_BPROT->DISABLEINDEBUG, 1);
#endif
    //err_code |= sd_protected_register_write(&NRF_BPROT->CONFIG1, 0xFF00); // Protect pages 40..47 occupied by the bootloader
    
    // Protect the firmware and the MBR
    uint8_t *pStorageAddr = NULL;
    uint16_t storageLen = 0;
    uint32_t mask = 0;
    HAL_flashGetStorageAbsoluteAddr(&pStorageAddr, &storageLen);

    for(uint8_t i = 0; i < 64; ++i)
    {
        // If the address is not covered by the storage, then protect it
        if((i*4096) < (uint32_t)pStorageAddr || (i*4096) >= ((uint32_t)pStorageAddr + storageLen))
        {
            mask |= 1 << (i & 0x1F);
        }
        if((i > 0) && ((i % 31) == 0))
        {
            //static const uint8_t* regTable = {NRF_BPROT->CONFIG0, NRF_BPROT->CONFIG1, NRF_BPROT->CONFIG2, NRF_BPROT->CONFIG3};
            //*((uint32_t*)&(NRF_BPROT->CONFIG0) + (i / 32)) = mask;
            
            err_code |= sd_protected_register_write(((uint32_t*)&(NRF_BPROT->CONFIG0) + (i / 32)), mask);
            mask = 0;
        }
    }

    return err_code;
}

static uint32_t _HAL_sysSetupMemProtection()
{
    uint32_t err_code = 0;
    err_code = nrf_mpu_lib_init();
    
    nrf_mpu_lib_region_t region;
    uint32_t attributes = 0;

    // Prevent execution from RAM. Read only by privileged software
/*
    // TODO: Ask for default permissions in nRF forums
    uint32_t attributes = (0x00 << MPU_RASR_TEX_Pos)    // Not cached
                        | (0 << MPU_RASR_B_Pos)         // 
                        | (0x01 << MPU_RASR_AP_Pos)     // RW only by privileged software
                        | (1 << MPU_RASR_XN_Pos);       // Never eXecute
    err_code |= nrf_mpu_lib_region_create(&region, (void *)0x20000000u, 32 * 1024, attributes);
*/
    // Forbid access to bootloader
#ifndef DEBUG   // Debug builds don't have the bootloader present
    attributes = (0x00 << MPU_RASR_TEX_Pos)             // Not cached
                        | (0 << MPU_RASR_B_Pos)         // 
                        | (0x00 << MPU_RASR_AP_Pos)     // No access
                        | (1 << MPU_RASR_XN_Pos);       // Never eXecute
    err_code |= nrf_mpu_lib_region_create(&region, (void *)BOOTLOADER_ADDRESS, 32 * 1024, attributes);
#endif
    return err_code;
}

// Only one 4 KB section is retained.
//MBT_STATIC_ASSERT(NRF_SECTION_LENGTH(non_init) <= 4096);
static uint32_t _HAL_sysSetupRAMRetention()
{
    const uint32_t ptr = (uint32_t)&__start_non_init;    
    const uint32_t p_offset         = ptr - MBT_CFG_PLATFORM_DATARAM_START_ADDRESS;
    const uint8_t ram_slave_n       = MBT_PLATFORM_RAM_SLAVE(p_offset);
    const uint8_t ram_section_n     = MBT_PLATFORM_RAM_SECTION(p_offset);
    uint32_t ret                    = NRF_SUCCESS;

//MBT_LOG("0x%x, %d, %d\n", ptr, (int32_t)ram_slave_n, (int32_t)ram_section_n);

#if SOFTDEVICE_PRESENT
    ret = sd_power_ram_power_set(ram_slave_n, ((1 << ram_section_n) << POWER_RAM_POWER_S0RETENTION_Pos));
    APP_ERROR_CHECK(ret);
#else
    NRF_POWER->RAM[ram_slave_n].POWERSET = ((1 << ram_section_n) << POWER_RAMON_OFFRAM0_Pos);
#endif

    return ret;
}

extern const MBT_FactoryInfo_t sk_FactoryInfo;

static void _HAL_InitDevInfo()
{
    g_HAL_devInfo.magic         = 0xd1;
    g_HAL_devInfo.version       = 0;
    g_HAL_devInfo.factoryInfo   = sk_FactoryInfo;
    g_HAL_devInfo.fwVersion     = MBT_CFG_FW_VERSION;
    g_HAL_devInfo.fwFeatureLevel= MBT_CFG_FW_FEATURELEVEL;
    g_HAL_devInfo.deviceId      = (uint64_t)(((uint64_t)NRF_FICR->DEVICEID[1] << 32) | NRF_FICR->DEVICEID[0]);
}

// ------------------------------------------------------------------------------

void HAL_sysEnterCriticalSection()
{
  app_util_critical_region_enter(&s_nestedCS);
}

void HAL_sysLeaveCriticalSection()
{
  app_util_critical_region_exit(s_nestedCS);
}

// ------------------------------------------------------------------------------

static void _HAL_hardwarePanicHandler(uint32_t code)
{
    // What to do?
    // Call panic handler?
    NVIC_SystemReset();
}

// ------------------------------------------------------------------------------

static void _HAL_wdtEventHandler(void)
{
    //NOTE: The max amount of time we can spend in WDT interrupt is two cycles of 32768[Hz] clock - after that, reset occurs
}

// ------------------------------------------------------------------------------

static uint32_t _HAL_wdtInit(void)
{
    uint32_t err_code = NRF_SUCCESS;
    // Configure WDT.
    nrf_drv_wdt_config_t config = NRF_DRV_WDT_DEAFULT_CONFIG;
    config.reload_value = MBT_CFG_WDT_TIMEOUT_MS;
    err_code = nrf_drv_wdt_init(&config, _HAL_wdtEventHandler);
    APP_ERROR_CHECK(err_code);
    if(err_code != 0) return err_code;
    err_code = nrf_drv_wdt_channel_alloc(&s_HALWDTChannelID);
    APP_ERROR_CHECK(err_code);
    if(err_code != 0) return err_code;
    nrf_drv_wdt_enable();    
    return err_code;
}

static void _HAL_wdtFeed(void)
{
    nrf_drv_wdt_channel_feed(s_HALWDTChannelID);
}

// ------------------------------------------------------------------------------

extern volatile uint32_t __start_bootloader;

// Runtime checks that prevent a unit to have a non-functional bootloader because of incorrect image generation or flashing.
// i.e:
//  - Providing to the manufacturer an image without bootloader in error
//  - Manufacturer not erasing the chip, or not erasing the UICR and leaving incorrect values on PSELRESET / customer UICR
//  - Manufacturer not programming the bootloader area

static uint32_t _HAL_PreChecks(void)
{
    // Make sure that PSELRESET is correctly cleared and P0.28 is used as GPIO, not as RESET
    if((NRF_UICR->PSELRESET[0] & NRF_UICR->PSELRESET[1]) != 0xFFFFFFFF)
        return -1;

#ifdef PRODUCTION
    // Make sure the bootloader is present
    if(__start_bootloader == 0xFFFFFFFF)    // It usually is 00 60 00 20 when correctly programmed
        return -1;
#endif

    return 0;
}

// PUBLIC =============================================================

void HAL_preinit()
{
    _HAL_wdtInit();
}

// ------------------------------------------------------------------------------

void HAL_init()
{
    uint32_t err = 0;    

    err =_HAL_PreChecks();

    _HAL_InitDevInfo();

    err |= _HAL_rtcInit();

    err |= _HAL_bleInit();

#ifdef DEBUG
    err |= _HAL_dbgInit();
#endif
    
    err |= _HAL_softPWMInit();

    err |= _HAL_i2cInit();

    err |= _HAL_accelInit();

    err |= _HAL_GPIOInit();

#ifdef PRODUCTION
    err |= _HAL_sysInitDFUName();
#endif

    err |= _HAL_sysSetupFlashProtection();

    err |= _HAL_sysSetupMemProtection();

    //_HAL_gpioCheckSysReset();

    err |= _HAL_audioOutInit();

    err |= _HAL_analogInit();

    err |= _HAL_sysSetupRAMRetention();

    if(err) _HAL_hardwarePanicHandler((uint32_t)err);
        
}

// ------------------------------------------------------------------------------

static void _HAL_dispatchMappedEvents(HAL_event_t* e, uint32_t bitmap, const uint8_t *masksShift, const uint8_t *evtCodes, uint8_t len)
{
    for(uint8_t i = 0; i < len; ++i)
    {
        if(bitmap & (1 << masksShift[i]))
        {
            e->data.accelEvent.type = evtCodes[i];
            HAL_onEvent(e);
        }
    }
}

// ------------------------------------------------------------------------------

void HAL_preUpdate()
{
    static uint32_t latch = 0;

    NRFX_CRITICAL_SECTION_ENTER();
        //MBT_FLAG_SET(latch, g_HAL_GPIOLatch);
        latch           = g_HAL_GPIOLatch;
        g_HAL_GPIOLatch = 0;
        NRF_P0->LATCH   = 0xFFFFFFFF; // Reset the latch
    NRFX_CRITICAL_SECTION_EXIT();

    // GPIO INTERRUPTS ----------------------------------------
#ifndef MBT_CFG_ACCEL_HANDLE_GPIO_ON_INT
    _HAL_accelHandleGPIO(latch);
#endif

    // Other GPIO (VBUS, BATTCHG, ...)
    {
        HAL_event_t e =
        {
            .type = HALEVENTTYPE_GPIO,
        };
        
        for(uint8_t i = 0; i < 32; ++i)
        {
            if(latch & (1UL << i))
            {
                e.data.gpioEvent.pin = i;
                HAL_onEvent(&e);
            }
        }
    }

    // ACCELEROMETER FLAGS ------------------------------------
    {
        uint32_t accelFlags = _HAL_accelGetClearEventFlags();
        HAL_event_t e =
        {        
            .type = HALEVENTTYPE_ACCEL                
        };

        static const uint8_t flgs[] = {SC7A20_EVTFLAG_DATA, SC7A20_EVTFLAG_MOTION, SC7A20_EVTFLAG_CLICK, SC7A20_EVTFLAG_DBLCLICK};
        static const uint8_t evts[] = {HALACCELEVENTTYPE_DATA, HALACCELEVENTTYPE_MOTION, HALACCELEVENTTYPE_TAP, HALACCELEVENTTYPE_DBLTAP};

        for(uint8_t i = 0; i < sizeof(flgs) / sizeof(uint8_t); ++i)
        {
            if(accelFlags & flgs[i])
            {
                e.data.accelEvent.type = evts[i];
                HAL_onEvent(&e);
            }
        }
    }

    // BLUETOOTH FLAGS ---------------------------------------
    {
      uint32_t bleFlags = _HAL_bleGetAndClearEventFlags();

      static const uint8_t flgs[] = {HAL_BLE_EVENT_POS_CONNECTION, HAL_BLE_EVENT_POS_ADVERTISEMENT, HAL_BLE_EVENT_POS_DATARX, HAL_BLE_EVENT_POS_RX_OVERFLOW, HAL_BLE_EVENT_POS_TX_READY};
      static const uint8_t evts[] = {HALBLEEVENTTYPE_CONNECTION, HALBLEEVENTTYPE_ADVERTISEMENT, HALBLEEVENTTYPE_DATARX, HALBLEEVENTTYPE_RX_OVERFLOW, HALBLEEVENTTYPE_TX_READY};

      HAL_event_t e =
      {        
          .type = HALEVENTTYPE_BLE
      };

      _HAL_dispatchMappedEvents(&e, bleFlags, flgs, evts, sizeof(flgs) / sizeof(uint8_t));
    }

    // SAADC FLAGS -------------------------------------------
    {
        // This goes first since it checks a flag
        _HAL_analogTick();
        uint32_t analogFlags = _HAL_analogGetAndClearEventFlags();
        
        if(analogFlags & HAL_ANALOG_FLAG_CONVDONE)
        {
            HAL_event_t e =
            {        
              .type = HALEVENTTYPE_ANALOG
            };

            e.data.analogEvent.type = HALANALOGEVENTTYPE_CONVDONE;

            HAL_onEvent(&e);
        }
    }

    // FLASH FLAGS -------------------------------------------
    {
        uint32_t flashFlags = _HAL_flashGetAndClearEventFlags();

        if(flashFlags & (HAL_FLASH_EVENT_FLAG_SUCCESS | HAL_FLASH_EVENT_FLAG_ERROR))
        {
            HAL_event_t e = 
            {
                .type = HALEVENTTYPE_FLASH
            };

            e.data.flashEvent.success = !!(flashFlags & HAL_FLASH_EVENT_FLAG_SUCCESS);
            HAL_onEvent(&e);
        }
    }
}

// ------------------------------------------------------------------------------

void HAL_postUpdate()
{
    _HAL_gpioCheckSysReset();
    _HAL_wdtFeed();
    if(!nrf_atomic_flag_clear_fetch(&s_HAL_pendingTickFlag))
    {
#if NRF_LOG_ENABLED
        if (NRF_LOG_PROCESS() == false)
#endif
            if(s_HAL_rtcCurrentTickIntervalMs != HAL_SYS_TICKRATE_MAX)
              nrf_pwr_mgmt_run();
    }
}

// ------------------------------------------------------------------------------

void HAL_sysSchedulePendingTick()
{
    nrf_atomic_flag_set(&s_HAL_pendingTickFlag);
}

// ------------------------------------------------------------------------------

uint32_t HAL_powerEnterLockupMode()
{
    // Turn accel off
    HAL_accelForceReset();
    // Stop BLE
    //HAL_bleStopAdvertising();
    //HAL_bleDisconnect();
    // Release HFCLK
    _HAL_clkHFSharedReleaseClient(0xFF);
    // PWM stop
    HAL_audioAbort();
    // Timers stop
    _HAL_rtcStopAll();      // This also stops lppwm
    // I2C stop
    _HAL_i2cUninit();
    // GPIO reset
    _HAL_GPIOSetLockup();
    // Stop SAADC and timer
    _HAL_analogForceStop();

    _HAL_wdtFeed();
    
    nrf_delay_us(1000);

    __DSB();
    __ISB();

    // System off
#if SOFTDEVICE_PRESENT
    sd_power_system_off();
#else
    NRF_POWER->SYSTEMOFF = 1;
#endif
    // DEBUG purposes
    while(1)
        _HAL_wdtFeed();

    return 0;
}

// ------------------------------------------------------------------------------

#define BOOTLOADER_DFU_GPREGRET_MASK            (0xF8)      /**< Mask for GPGPREGRET bits used for the magic pattern written to GPREGRET register to signal between main app and DFU. */
#define BOOTLOADER_DFU_GPREGRET                 (0xB0)      /**< Magic pattern written to GPREGRET register to signal between main app and DFU. The 3 lower bits are assumed to be used for signalling purposes.*/
#define BOOTLOADER_DFU_START_BIT_MASK           (0x01)      /**< Bit mask to signal from main application to enter DFU mode using a buttonless service. */

#define BOOTLOADER_DFU_START_BIT_MASK           (0x01)      /**< Bit mask to signal from main application to enter DFU mode using a buttonless service. */

#define BOOTLOADER_DFU_START    (BOOTLOADER_DFU_GPREGRET | BOOTLOADER_DFU_START_BIT_MASK)      /**< Magic number to signal that bootloader should enter DFU mode because of signal from Buttonless DFU in main app.*/

void HAL_sysEnterDFUMode(void)
{
    uint32_t err_code = 0;
#ifdef SOFTDEVICE_PRESENT
    err_code = sd_power_gpregret_clr(0, 0xffffffff);
    APP_ERROR_CHECK(err_code);

    err_code |= sd_power_gpregret_set(0, BOOTLOADER_DFU_START);
    APP_ERROR_CHECK(err_code);

    if(!err_code)
    {        
        HAL_accelForceReset();
        HAL_motorSetLevel(0);
        HAL_audioAbort();        
        nrf_delay_ms(500);
        HAL_bleDisconnect();
        nrf_delay_ms(1000);
        sd_nvic_SystemReset();
    }
#else
    #error DFU enter without softdevice not implemented
#endif
}

HAL_DeviceInfo_t *HAL_sysGetDevInfo()
{
    return &g_HAL_devInfo;
}