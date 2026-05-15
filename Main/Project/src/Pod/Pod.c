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

//SDK = 15.3.0

// PROJECT MACROS ---------------------------------------------------------------
//PRODUCT_HW_VERSION=   (platform (nRF52810...))
//  01 - First hardware version
//APP_VERSION=  
//  1
//PRODUCT_CODE_NUMERIC=
//  0001 - Melbits Pod
//PRODUCT_ID=0x$(PRODUCT_CODE_NUMERIC)$(PRODUCT_HW_VERSION)
//  0x000101
//HW_REVISION=  (variant of the PCB)
//0
//TARGET_VARIANT=
//    DEV
//    PROD
//    QA
//
//
// Bootloader DFU images
// --application-version=APP_VERSION
// --hw-version=PRODUCT_CODE_NUMERIC
//      ex: 0001 converted to decimal (1)
// ------------------------------------------------------------------------------
#include "common.h"

#include "HAL/Debug.h"
#include "HAL/HAL.h"
#include "HAL/Accel.h"

#include "Pod/Vibration.h"
#include "Pod/LEDs.h"
#include "Pod/Analog.h"
#include "Pod/Motion.h"
#include "Pod/MLINK.h"
#include "Pod/Power.h"
#include "Pod/UI.h"
#include "Pod/UI_ML.h"
#include "Pod/BoxMode.h"
#include "Pod/BLE.h"
#include "Pod/Storage.h"
#include "Pod/FileServer.h"
#include "Pod/StreamServer.h"
#include "Pod/EventServer.h"

#include "Pod/Activity.h"
#include "Pod/TestMode.h"

#include "Pod/Playground.h"

#include "Pod/UserSettings.h"

#include "Pod/RetainedRAM.h"

#include "utils.h"

// TODO
// [X] LOG
// [X] BLE
// [X] Analog SAADC
// [X] Mute LEDs when sampling LIGHT
// Accelerometer disable I2C module when all transmissions are finished
// [X] LED control -> Synth track callback
//
// - Shared resources
// ---------------------
//   [X] SAADC -> Analog service
//   [X] AVDD  -> Handled by the Analog service
//   [X] LFCLK? -> Does not exist?
//
// - Storage (prioritize tasks)
// ---------------------
//   [-] Main Pod FSM to hold boot until Storage is IDLE
//   [?] Decide on file IDs
//   [X] Melbit file format -> Make ActEvo/MelbitHolder read from Storage
//   [X] Recipe file format -> Make ActEvo read from Storage
//   [X] PCM file format -> Make synth PCM read from Storage (properly)
//   (NO)- Synth file format -> Make synth read from Storage
//
// - Evolution
// ---------------------
//  [X] Store data in RAM
//  (NO) Try to start evolution when booting if there is evolution data in flash
//
// [X] Protocol
// ---------------------
// [X] Design (check use cases), data xfer, stream mode, ...
// [X] Implement parser / executor (check ring buffers)
// [X] Implement host in C# BLE app
// [X] Implement DFU
// [X] Get rid of ring buffers for BLE
//
// [X] Test mode
// ---------------------
// [X] implement test features
//
// [X] Stream mode
// ---------------------
// [X] Implement stream server

// - Wishlist
// ---------------------
// + Add signature of CHIPID and verification on boot
// [X] User settings

#if MBT_CFG_UI_WELCOMEFEEDBACK_DELAY_MS >= MBT_CFG_RESETBTN_MAX_DELAY_MS
#error MBT_CFG_UI_WELCOMEFEEDBACK_DELAY_MS cant be equal or higher than MBT_CFG_RESETBTN_MAX_DELAY_MS
#endif


typedef enum
{
    PS_BOOT = 0,    // Start HAL, basic systems and storage
    PS_BOOT2,       // Start Activity / restore state
    PS_RUN          // Run mode
} Pod_state_t;

static Pod_state_t s_Pod_state;

static void Pod_transitionToState(Pod_state_t nextState);

// MLINK ------------------------------------------------------------------------
void Pod_MLEventHandler(const Pod_MLEventData_t *pData)
{
    Pod_UIMLHALEventHandler(pData);
    Pod_BLEMLHALEventHandler(pData);
    Pod_PGMLHALEventHandler(pData);
    Pod_ESMLEventHandler(pData);
    

    if(pData->type == MLE_SIGNAL && pData->data.signal.locked == false)
    {
        // ML Signal lost. Re-enable motion timers.
        Pod_motionMakeAware();
    }
}

// MOTION -----------------------------------------------------------------------
void Pod_motionEventHandler(const Pod_motionEventData_t *pData)
{
    if(pData->type == MET_SHAKE)
    {
        if(pData->data.shake.state == SHK_STARTED)
        {
            if(Pod_ActGetCurrentType() == ACT_NONE)
            {
                Pod_UIPlaySystemClip(SC_EMPTY);                
            }            
        }        

        uint32_t notif = 0;
        switch(pData->data.shake.state)
        {
            case SHK_STARTED:
                notif = ESE_MOTION_SHAKESTART;
                break;
            case SHK_STOPPED:
                notif = ESE_MOTION_SHAKEEEND;
                break;
            case SHK_PEAK:
                notif = ESE_MOTION_SHAKEPEAK;
                break;
        }
        Pod_ESNotifyEventSource(notif);


        Pod_PGNotifyPINAccepted();
    } else if(pData->type == MET_ACTIVITY)
    {
        MBT_LOG("[ACTIVITY] %s\n", pData->data.activity.moving ? "YES" : "NO");
        if(pData->data.activity.moving)
        {
            Pod_MLEnable(true);
            Pod_BLEAdvertisingLogic();            
        }
        Pod_ESNotifyEventSource(pData->data.activity.moving ? ESE_MOTION_MOTIONSTART : ESE_MOTION_MOTIONEND);
    } else if(pData->type == MET_AWARENESS)
    {
        if(pData->data.awareness.aware)
        {
            const HAL_accelMotionDetectionConfig_t act =
            {
                .axis               = HAL_ACCEL_ACTIVITY_X_HIGH | HAL_ACCEL_ACTIVITY_Y_HIGH | HAL_ACCEL_ACTIVITY_Z_HIGH,
                .threshold_mg       = 40,
                .duration_ms        = 40
            };

            HAL_accelSetMotionDetectionConfig(&act);
        } else {
            const HAL_accelMotionDetectionConfig_t act =
            {
                .axis               = HAL_ACCEL_ACTIVITY_X_HIGH | HAL_ACCEL_ACTIVITY_Y_HIGH | HAL_ACCEL_ACTIVITY_Z_HIGH,
                .threshold_mg       = 100,
                .duration_ms        = 40
            };

            HAL_accelSetMotionDetectionConfig(&act);
        }
    }
    Pod_ActMotionHandler(pData);
}

// STORAGE ----------------------------------------------------------------------
void Pod_storageEventHandler(Pod_storageEventType_t evt)
{
    if(s_Pod_state == PS_BOOT)
    {
        if(evt == STGEVENT_IDLE)
        {
            Pod_transitionToState(PS_BOOT2);            
        } else if(evt == STGEVENT_ERASE_ERROR)
        {
            Pod_BMEnter(BMR_HWPANIC);
        }
    }
    Pod_FSStorageEventHandler(evt);
}

// POWER ------------------------------------------------------------------------
void Pod_powerEventHandler(const Pod_powerEventData_t *pData)
{
    Pod_UIpowerEventHandler(pData);
    Pod_ESpowerEventHandler(pData);
}
// ANALOG CLIENTS MAP -----------------------------------------------------------
static const Pod_analogClientConfig_t k_Pod_analogClientsCfg[POD_ANALOG_CLIENTS_COUNT] =
{
    {
        .pBuffer = &g_Pod_ActLightBuffer,
        .bufferSize = 1,
        .offset = POD_ANALOG_OFFSET_LIGHT,
        .cb = Pod_ActAnalogHandler
    },
    {
        .pBuffer = &g_Pod_ActTempBuffer,
        .bufferSize = 1,
        .offset = POD_ANALOG_OFFSET_TEMP,
        .cb = Pod_ActAnalogHandler
    },
    {   //POD_POWER_ML_SENSOR_CH_ID
        .pBuffer = &s_Pod_MLSampleBuffer[0],
        .bufferSize = MBT_CFG_ML_SAMPLE_BUFFER_SIZE_SAMPLES,
        .offset = POD_ANALOG_OFFSET_ML,
        .cb = _Pod_MLAnalogCallback
    },
    {   //POD_POWER_BATT_ANALOG_CH_ID
        .pBuffer = &s_Pod_powerBattSampleBuffer,
        .bufferSize = 1,
        .offset = POD_ANALOG_OFFSET_BATT,
        .cb = _Pod_powerAnalogCallback
    },
    {
        .pBuffer = &g_Pod_SSLightBuffer,
        .bufferSize = 1,
        .offset = POD_ANALOG_OFFSET_LIGHT,
        .cb = Pod_SSAnalogHandler
    },
    {
        .pBuffer = &g_Pod_SSTempBuffer,
        .bufferSize = 1,
        .offset = POD_ANALOG_OFFSET_TEMP,
        .cb = Pod_SSAnalogHandler
    },
    {   // MLINK DEBUG
        .pBuffer = &g_Pod_SSMLBuffer,
        .bufferSize = 1,
        .offset = POD_ANALOG_OFFSET_ML,
        .cb = Pod_SSAnalogHandler
    }
};

static const Pod_analogClientMap_t k_Pod_analogClientsMap = 
{
    &k_Pod_analogClientsCfg[0],
    sizeof(k_Pod_analogClientsCfg) / sizeof(Pod_analogClientConfig_t)
};

// ------------------------------------------------------------------------------

//uint8_t MBT_CFG_PLATFORM_SECTION_RETAINED_DATA g_Pod_nResetPresses;
static MBT_TIMER_DEFINE(s_Pod_resetPressTimer);
static MBT_TIMER_DEFINE(s_Pod_welcomeFeedbackTimer);

// ------------------------------------------------------------------------------

static void pod_updateResetPresses()
{
    ++ g_Pod_RetainedRAM.nResetPresses;    
    MBT_TIMER_RESET_TIMEOUT(s_Pod_resetPressTimer, MBT_US_TO_TICKS(MBT_CFG_RESETBTN_MAX_DELAY_MS * 1000));    
}

// ------------------------------------------------------------------------------

extern void Pod_TMUnitTestsInit(void);

void pod_boot()
{
    HAL_preinit();
    pod_updateResetPresses();
    //s_Pod_state = PS_BOOT;

    Pod_TMUnitTestsInit();

    HAL_init();
    
    HAL_preUpdate();    

    // --------------------------------------

    Pod_UIInit();
    Pod_BMInit();
    Pod_LEDsInit();
    Pod_vibInit();
    Pod_analogInit();
    {
        Pod_analogSetClientsMap(&k_Pod_analogClientsMap);
        HAL_analogSetEnabled(true);
    }

    Pod_MLInit();

    Pod_motionInit();
    {
        Pod_motionEnableAlgorithm(POD_MOTION_ALGORITHM_ID_SHAKE);
    }

    Pod_powerInit();
    Pod_storageInit();
    
    Pod_ActInit();

    Pod_PGInit();

    Pod_FSInit();
    Pod_SSInit();
    Pod_ESInit();

    Pod_BLEInit();

    Pod_TMInit();

    Pod_USInit();
    


    MBT_LOG("Boot\n");

    Pod_MLEnable(true);                                     // MLINK enabled on boot
    Pod_MLSetHardTimeout(MBT_CFG_ML_INITIAL_TIMEOUT_MS);    // AVDD active for 3 minutes on boot (ACCTRON)
    Pod_motionMakeAware();                                  // Aware on boot
    
    MBT_TIMER_RESET_TIMEOUT(s_Pod_welcomeFeedbackTimer, MBT_US_TO_TICKS(MBT_CFG_UI_WELCOMEFEEDBACK_DELAY_MS * 1000));

    Pod_BLEAdvertisingLogic();
}

// ------------------------------------------------------------------------------

void pod_loop()
{
    static uint32_t lastTicks = 0;
    uint32_t ticksNow = HAL_sysGetRawTickCount();
    uint32_t diff = HAL_sysGetDeltaTickCount(ticksNow, lastTicks);
    HAL_preUpdate();

        Pod_PGTick(diff);
        Pod_ActTick(diff);
        Pod_MLTick(diff);        
        Pod_vibTick(diff);
        Pod_analogTick(diff);
        Pod_motionTick(diff);
        Pod_powerTick(diff);
        Pod_UITick(diff);
        Pod_LEDsTick(diff);
        Pod_BMTick(diff);

        Pod_FSTick(diff);
        Pod_SSTick(diff);
        Pod_ESTick(diff);
        
        Pod_TMTick(diff);
        Pod_USTick(diff);

    HAL_postUpdate();
    lastTicks = ticksNow;

    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_resetPressTimer, diff, 
    {
        g_Pod_RetainedRAM.nResetPresses = 0;
    });

    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_welcomeFeedbackTimer, diff, 
    {
        if(g_Pod_RetainedRAM.nResetPresses == MBT_CFG_RESETBTN_TEST_MODE_ENABLE_N_PRESSES)
        {
            Pod_UIPlaySystemClip(SC_EMPTY);

            // Clear pairing PIN
            Pod_PGClearLinkPIN();

        } else if(g_Pod_RetainedRAM.nResetPresses == MBT_CFG_RESETBTN_USER_SETTINGS_FACTORY_RESET_N_PRESSES)
        {        
        } else {
            Pod_UIPlaySystemClip(SC_WELCOME);
        }    
    });
}

// HAL --------------------------------------------------------------------------

void HAL_onEvent(const HAL_event_t *pEventData)
{
    switch(pEventData->type)
    {
        case HALEVENTTYPE_GPIO:    
            Pod_powerHALEventHandler(pEventData);
        break;
        case HALEVENTTYPE_BLE:
            Pod_BLEHALEventHandler(pEventData);
            Pod_PGHALEventHandler(pEventData);
        break;
        case HALEVENTTYPE_ANALOG:
        {
            HAL_analogSample_t *pBuffer;
            uint16_t size;
            HAL_analogGetLastBuffer(&pBuffer, &size);
            Pod_analogFeedStream(pBuffer, size);
            HAL_analogReturnBuffer(pBuffer);
        break;
        }
        case HALEVENTTYPE_ACCEL:
            Pod_motionHALEventHandler(pEventData);
        break;
        case HALEVENTTYPE_FLASH:
            Pod_storageHALEventHandler(pEventData);
        break;
    }
}

// FSM --------------------------------------------------------------------------

static void Pod_transitionToState(Pod_state_t nextState)
{
    s_Pod_state = nextState;
}
