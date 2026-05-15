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

#ifndef _POWER_H_
#define _POWER_H_
#include "common.h"
#include "HAL/HAL.h"

#include "utils.h"

// Sources of interrupts
// (1) - SAADC
// (2) - USB port (VBUS on GPIO)
// (3) - RESET button (GPIO)
// (4) - PWM (Audio)
// (5) - RTC (timers and lppwm)
// (6) - RADIO / Softdevice
// (7) - TIMER used with SAADC (120Hz)
// (8) - Accelerometer (INT1 = Data available // INT2 - Motion)

// System activities that require a specific ticking interval
#define PWR_ACT_INC         (0x2)   // [-] 1 Hz (incubation timeouts - sensor sampling is interrupt-driven)
#define PWR_ACT_CLIP        (0x4)   // [X] (4) 50 Hz (tracker/clip player)
#define PWR_ACT_LED         (0x8)   // [X] (5) 50 Hz (kept on by lp_pwm, but all channels can be off temporarily in one tick)
#define PWR_ACT_MLINK       (0x10)  // [X] [X] (1-7) 1 Hz (window timeout) - Protocol decoding is interrupt-driven. 
#define PWR_ACT_ANALOG      (0x20)  // [X] (8) 50 Hz (motion (shake,...) - accelerometer)
#define PWR_ACT_MOTION      (0x40)  // [X] (8) 50 Hz (motion (shake,...) - accelerometer)
#define PWR_ACT_POWER_CHG   (0x80)  // (2) 10 Hz Power when charging (update LED color)

#define PWR_ACT_STRM        (0x100) // (1) 50 Hz Stream server
#define PWR_ACT_SERV        (0x200) // (1) 100 Hz File server

// User activities that are already coveredsy. Used to determine if the Pod is being used
#define PWR_ACT_BLE_ACTIVE  (0x400) // (6) No ticking required. Only for user activity accounting.
#define PWR_ACT_SHK_ACTIVE  (0x800) // [X](8) Ticking already covered by PWR_ACT_SHK. Only for user activity accounting.

#define PWR_ACT_FLAG_CHANGED     (0x80000000)

#define PWR_MSK_100HZ       (PWR_ACT_SERV)
#define PWR_MSK_50HZ        (PWR_ACT_CLIP | PWR_ACT_LED | PWR_ACT_STRM | PWR_ACT_MOTION | PWR_ACT_ANALOG)
#define PWR_MSK_10HZ        (PWR_ACT_POWER_CHG)
#define PWR_MSK_1HZ         (PWR_ACT_INC)

// Is the Pod being used? (for low battery warnings)
#define PWR_MSK_USER_ACTIVITY    (PWR_ACT_INC | PWR_ACT_SHK_ACTIVE | PWR_ACT_BLE_ACTIVE)

#define PWR_INVALID_BATTERY_LEVEL (0xFF)

#define POD_POWER_BATT_ANALOG_CH_ID (3)

typedef enum
{
    BS_INVALID = 0,
    BS_DISCHARGING,
    BS_CHARGING,
    BS_CHARGED,
    BS_BATTPROBLEM
} Pod_powerBatteryStatus_t;

typedef enum
{
    EPS_INVALID = 0,
    EPS_PRESENT,
    EPS_NOTPRESENT
} Pod_powerExtPwrStatus_t;

typedef enum
{
    BLL_INVALID = 0,
    BLL_DEAD,
    BLL_LOW,
    BLL_OK
} Pod_powerBattLogicLevel_t;

typedef struct
{
    uint16_t                    batteryVoltage_mV;
    uint8_t                     batteryLevel_pc; // 0 .. 100 %
    Pod_powerExtPwrStatus_t     externalPowerPresent;
    Pod_powerBatteryStatus_t    batteryStatus;
    Pod_powerBattLogicLevel_t   batteryLogicLevel; // Logic level of the battery: OK, LOW or DEAD
} Pod_powerStatusData_t;

typedef enum
{
    PET_DEADBATT = 0,
    PET_LOWBATT,
    PET_BATTSTATUS,
    PET_EXTPOWER_ATTACHED,
    PET_EXTPOWER_REMOVED
} Pod_powerEventType_t;

typedef struct
{
    Pod_powerEventType_t type;
    union
    {
        struct
        {
            Pod_powerBatteryStatus_t status;
        } battStatus;
    } data;
} Pod_powerEventData_t;

typedef enum
{
    BCS_DISABLED,
    BCS_MONITOR,
    BCS_MEASURE
} Pod_powerStateBCState_t;

typedef enum
{
    BCR_INVALID,
    BCR_CHARGING = BS_CHARGING,
    BCR_NOTCHARGING = BS_CHARGED,
    BCR_ERROR = BS_BATTPROBLEM
} Pod_powerStateBCResult_t;

typedef struct
{
    // Activity monitoring
    uint32_t activeClients;

    // Battery monitoring
    MBT_TIMER_DEFINE(battMonTimer);           // Timer for monitoring the battery voltage. Timeout depends on read voltage.
    MBT_TIMER_DEFINE(vBusDebounceTimer);      // Timer for debouncing the VBUS signal

    uint32_t battMonTimerTimeout_ticks;

    Pod_powerStatusData_t status;             // Status of power (external or battery, batt voltage, %, ...)
    uint32_t tickRate;    
   
    // FSM for monitoring the BATTCHG line
    // - Low when charging
    // - High when the charge is complete
    // - Oscilates at certain frequency when the batt is not connected or there is a problem
    // - Low when VBUS is not present (No external power attached)
    struct
    {
        Pod_powerStateBCState_t state;
        Pod_powerStateBCResult_t result;        
        MBT_TIMER_DEFINE(timer);
        uint8_t loops;
        uint8_t lineChanges;
        uint8_t lastLineState;
        uint32_t timerTimeout_ticks;
    } BCFSM;
} Pod_powerState_t;

void Pod_powerInit(void);
void Pod_powerTick(uint32_t ticks);

void Pod_powerGetPowerStatus();

void Pod_powerSetActivity(uint32_t clients);
void Pod_powerClearActivity(uint32_t clients);

void Pod_powerGetStatus(Pod_powerStatusData_t *pData);
void Pod_powerTriggerManualBatterySample(void);

bool Pod_powerIsUserActivity(void);

void Pod_powerHALEventHandler(const HAL_event_t *pEvent); // IN

void Pod_powerEventHandler(const Pod_powerEventData_t *pEvent); // OUT

uint32_t Pod_powerGetTickRate(void);

void _Pod_powerAnalogCallback(uint8_t clientID, HAL_analogSample_t *pBuffer, uint8_t count); // IN

extern HAL_analogSample_t s_Pod_powerBattSampleBuffer;

#endif