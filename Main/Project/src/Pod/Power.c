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

#include "Power.h"
#include "mbt_config.h"
#include "HAL/Debug.h"
#include "HAL/HAL.h"
#include "Pod/Analog.h"
#include "utils.h"

#define POD_POWER_BATT_MEAS_VOLTAGE_TO_SOC_DELTA_MV  (10)     // mV between each element in the SoC vector.
#define POD_POWER_BATT_MEAS_VOLTAGE_TO_SOC_ELEMENTS  (111)     // Number of elements in the state of charge vector.

// DEBUG
//#define MBT_CFG_POWER_BATT_LOW_THRESHOLD_MV   (4900)             // Low battery voltage threshold
//#define MBT_CFG_POWER_BATT_DEAD_THRESHOLD_MV  (3000)             // Battery dead voltage threshold

/*static*/ Pod_powerState_t s_Pod_powerState;
//Pod_powerStatusData_t *g_Pod_pPowerStatusData = &s_Pod_powerState.status;

HAL_analogSample_t s_Pod_powerBattSampleBuffer;

static void _Pod_powerBattStatusFSMTransitionToState(Pod_powerBatteryStatus_t newState);
static void _Pod_powerSampleBattery(void);

// BATTERY MEASUREMENT ----------------------------------------------------------------------------------------------
/** Converts voltage to state of charge (SoC) [%]. The first element corresponds to the voltage 
BATT_MEAS_LOW_BATT_LIMIT_MV and each element is BATT_MEAS_VOLTAGE_TO_SOC_DELTA_MV higher than the previous.
Numbers are obtained via model fed with experimental data. */
static const uint8_t k_Pod_Power_BattMeasVoltageToSoc[] = { 
     0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,
     4,  5,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 11, 12, 13, 13, 14, 15, 16,
    18, 19, 22, 25, 28, 32, 36, 40, 44, 47, 51, 53, 56, 58, 60, 62, 64, 66, 67, 69,
    71, 72, 74, 76, 77, 79, 81, 82, 84, 85, 85, 86, 86, 86, 87, 88, 88, 89, 90, 91,
    91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 100
};

static void _Pod_PowerBattVoltageToLevel(uint16_t voltage_mv, uint8_t * const battery_level_percent)
{
    int16_t soc_vector_element = (voltage_mv - MBT_CFG_POWER_BATT_DEAD_THRESHOLD_MV) /
                                               POD_POWER_BATT_MEAS_VOLTAGE_TO_SOC_DELTA_MV;
    
    // Ensure that only valid vector entries are used.
    if (soc_vector_element < 0)
    {
        soc_vector_element = 0;
    }
    else if (soc_vector_element > (POD_POWER_BATT_MEAS_VOLTAGE_TO_SOC_ELEMENTS - 1) )
    {
        soc_vector_element = (POD_POWER_BATT_MEAS_VOLTAGE_TO_SOC_ELEMENTS - 1);
    }
    
    *battery_level_percent = k_Pod_Power_BattMeasVoltageToSoc[soc_vector_element];
}

// BATTCHG MONITOR FSM -------------------------------------------------------------------------------------
static void _Pod_powerBCInit(void)
{
    memset(&s_Pod_powerState.BCFSM, 0, sizeof(s_Pod_powerState.BCFSM));
    MBT_TIMER_DISABLE(s_Pod_powerState.BCFSM.timer);
    s_Pod_powerState.BCFSM.lastLineState = 3;    
}

static void _Pod_powerBCTransitionToState(Pod_powerStateBCState_t nextState)
{
    //if(s_Pod_powerState.BCFSM.state == nextState)
        //return;
    MBT_LOG("[BCS] %d -> %d\n", s_Pod_powerState.BCFSM.state, nextState);

    s_Pod_powerState.BCFSM.lineChanges = 0;
    s_Pod_powerState.BCFSM.loops = 0;
    
    switch(nextState)
    {
        case BCS_MONITOR:
            s_Pod_powerState.BCFSM.timerTimeout_ticks = MBT_US_TO_TICKS(200 * 1000);
        break;
        case BCS_MEASURE:
            s_Pod_powerState.BCFSM.timerTimeout_ticks = MBT_US_TO_TICKS(10 * 1000);
        break;
        case BCS_DISABLED:
            _Pod_powerBCInit();
        return;
    }

    MBT_TIMER_RESET_TIMEOUT(s_Pod_powerState.BCFSM.timer, s_Pod_powerState.BCFSM.timerTimeout_ticks);

    s_Pod_powerState.BCFSM.state = nextState;
}

static void _Pod_powerBCResultHandler(Pod_powerStateBCResult_t result)
{
    //static const char *dbgNames[] = {"-", "CHARGING", "NOTCHARGING", "ERROR"};
    //MBT_LOG(">> CHG: %s\n", dbgNames[result]);
    
    // VBUS not present?. Discard this event.
    uint32_t vbus = false;
    HAL_gpioReadDigital(MBT_CFG_PIN_VBUS, &vbus);
    if(!!!vbus) return;

    Pod_powerBatteryStatus_t res = (Pod_powerBatteryStatus_t)result;
        
    _Pod_powerBattStatusFSMTransitionToState(res);
}

static void _Pod_powerBCFireEvent(Pod_powerStateBCResult_t res)
{
    if(s_Pod_powerState.BCFSM.result != res)
    {
        s_Pod_powerState.BCFSM.result = res;
        _Pod_powerBCResultHandler(res);
    }
}

static void _Pod_powerBCOnTimer()
{    
    uint32_t battchg = 0;            
    HAL_gpioReadDigital(MBT_CFG_PIN_BATTCHG, &battchg);
    bool lineState = !!battchg;
    if(lineState != s_Pod_powerState.BCFSM.lastLineState)
    {
        s_Pod_powerState.BCFSM.lastLineState = lineState;
        if(s_Pod_powerState.BCFSM.state == BCS_MONITOR)
        {
            // We're monitoring changes in the line
            _Pod_powerBCTransitionToState(BCS_MEASURE);
            _Pod_powerBCFireEvent(lineState ? BCR_NOTCHARGING : BCR_CHARGING);
        } else {
            // BCS_MEASURE
            ++s_Pod_powerState.BCFSM.lineChanges;
        }
    }
    if(s_Pod_powerState.BCFSM.state == BCS_MEASURE)
    {
        if(++s_Pod_powerState.BCFSM.loops >= 200)
        {
            /*if(s_Pod_powerState.BCFSM.lineChanges >= 100)
            {
                _Pod_powerBCTransitionToState(BCS_MEASURE);
                _Pod_powerBCFireEvent(BCR_ERROR);                        
            } else */ {

                _Pod_powerBCTransitionToState(BCS_MONITOR);
                _Pod_powerBCFireEvent(lineState ? BCR_NOTCHARGING : BCR_CHARGING);
            }
        }
    }
    MBT_TIMER_RESET_TIMEOUT(s_Pod_powerState.BCFSM.timer, s_Pod_powerState.BCFSM.timerTimeout_ticks);
}

static void _Pod_powerBCTick(uint32_t ticks)
{
    if(s_Pod_powerState.BCFSM.state == BCS_DISABLED)
        return;

    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_powerState.BCFSM.timer, ticks,
        {
            _Pod_powerBCOnTimer();
        }
    );
}

#define _Pod_powerBCEnable() _Pod_powerBCTransitionToState(BCS_MONITOR)
#define _Pod_powerBCDisable() _Pod_powerBCTransitionToState(BCS_DISABLED)

// VBUS DEBOUNCE -------------------------------------------------------------------------------------------

static void _Pod_powerOnVBUSChanged(void)
{
    MBT_LOG("//// VBUS DEBOUNCE\n");
    MBT_TIMER_RESET_TIMEOUT(s_Pod_powerState.vBusDebounceTimer, MBT_US_TO_TICKS(MBT_CFG_POWER_VBUS_FSM_DELAY_TIME_MS * 1000));
    s_Pod_powerState.status.batteryLogicLevel = BLL_INVALID;

    // Turn off the chg line monitor immediately to avoid false positives:
    //  - When VBUS is not present, the BATTCHG line reads as CHARGING
    _Pod_powerBCDisable();
}

void Pod_powerHALEventHandler(const HAL_event_t *pEvent)
{
    if(pEvent->data.gpioEvent.pin == MBT_CFG_PIN_VBUS)
    {
        _Pod_powerOnVBUSChanged();
    }
}

static void _Pod_powerOnVBUSDebounceTimer(void)
{
    uint32_t vbus = false;
    HAL_gpioReadDigital(MBT_CFG_PIN_VBUS, &vbus);
    Pod_powerExtPwrStatus_t ps = !!vbus ? EPS_PRESENT : EPS_NOTPRESENT;
    Pod_powerEventData_t evt;
    if(vbus)
    {
        // Just plugged in
        evt.type = PET_EXTPOWER_ATTACHED;                    
        _Pod_powerBCEnable();
        
    } else {
        // Just disconnected
        evt.type = PET_EXTPOWER_REMOVED;
        _Pod_powerBattStatusFSMTransitionToState(BS_DISCHARGING);
        _Pod_powerBCDisable();
    }
    if(s_Pod_powerState.status.externalPowerPresent != ps)
    {
        s_Pod_powerState.status.externalPowerPresent = ps;
        Pod_powerEventHandler(&evt);
    }
    _Pod_powerSampleBattery();
}

// BATTERY VOLTAGE MONITORING ------------------------------------------------------------------------------

static void _Pod_powerSampleBattery(void)
{
    Pod_analogSetClientMode(POD_POWER_BATT_ANALOG_CH_ID, ACM_SINGLESHOT);
}

void Pod_powerTriggerManualBatterySample(void)
{
    _Pod_powerSampleBattery();
}

static void _Pod_powerBattStatusFSMTransitionToState(Pod_powerBatteryStatus_t newState)
{
    if(s_Pod_powerState.status.batteryStatus == newState)
    {
        return;
    }

    s_Pod_powerState.status.batteryStatus = newState;

    Pod_powerEventData_t evt = { .type = PET_BATTSTATUS };
    evt.data.battStatus.status = newState;
        
    Pod_powerEventHandler(&evt);    
}

// Tells if the battery is OK, LOW or DEAD depending on the last reading
static Pod_powerBattLogicLevel_t _Pod_powerEvalBattLevel(void)
{
    // Hardware fault workaround
    if(s_Pod_powerState.status.batteryVoltage_mV == 0)
        return BLL_OK;

    if(s_Pod_powerState.status.batteryVoltage_mV <= MBT_CFG_POWER_BATT_DEAD_THRESHOLD_MV)
    {
        return BLL_DEAD;
    } else if(s_Pod_powerState.status.batteryVoltage_mV <= MBT_CFG_POWER_BATT_LOW_THRESHOLD_MV)
    {
        return BLL_LOW;
    }
    return BLL_OK;
}

static void _Pod_powerTriggerBattLevelEvents(Pod_powerBattLogicLevel_t llev)
{
    // If the VBUS debounce is in progress or there is external
    // power attached we don't want to trigger low or dead batt events
    if(MBT_TIMER_ISENABLED(s_Pod_powerState.vBusDebounceTimer) ||
        s_Pod_powerState.status.externalPowerPresent == EPS_PRESENT)
    {
        return;
    }

    Pod_powerEventData_t d;
    
    if(llev == BLL_LOW)
    {
        d.type = PET_LOWBATT;
    } else if(llev == BLL_DEAD)
    {
        d.type = PET_DEADBATT;
    } else {
        return;
    }
    Pod_powerEventHandler(&d);
}

static void _Pod_powerOnNewBatterySample(void)
{
    Pod_powerBattLogicLevel_t llev = _Pod_powerEvalBattLevel();
    //if(llev != s_Pod_powerState.battLogicLevel)
    {
        s_Pod_powerState.status.batteryLogicLevel = llev;
        _Pod_powerTriggerBattLevelEvents(llev);        
    }

    // Evaluate battery monitor tick rate
    if(s_Pod_powerState.status.externalPowerPresent == EPS_PRESENT)
    {
        // Fixed rate
        s_Pod_powerState.battMonTimerTimeout_ticks = MBT_US_TO_TICKS(MBT_CFG_POWER_BATT_MON_DELAY_EXTPWR_TIME_MS * 1000);
    } else {
        // Rate depending on batt reading
        if(llev == BLL_OK)
        {
            s_Pod_powerState.battMonTimerTimeout_ticks = MBT_US_TO_TICKS(MBT_CFG_POWER_BATT_MON_DELAY_TIME_MS * 1000);
        } else 
        //if(llev == BLL_LOW)
        {
            s_Pod_powerState.battMonTimerTimeout_ticks = MBT_US_TO_TICKS(MBT_CFG_POWER_BATT_MON_DELAY_LOW_TIME_MS * 1000);
        }
    }
    MBT_TIMER_RESET_TIMEOUT(s_Pod_powerState.battMonTimer, s_Pod_powerState.battMonTimerTimeout_ticks);
}

void _Pod_powerAnalogCallback(uint8_t clientID, HAL_analogSample_t *pBuffer, uint8_t count)
{
    Pod_analogSetClientMode(POD_POWER_BATT_ANALOG_CH_ID, ACM_IDLE);
    // Get and cache battery readings
    uint16_t battVoltage = (uint16_t)(HAL_analogRawToMv(HAL_ANALOG_CH_BATT, *pBuffer) << 1); // 50/50 voltage divider
    
    static uint16_t avg[4];
    static uint8_t avgIdx = 0;

    avg[(avgIdx++) & 0x3] = battVoltage;
    
    uint32_t a = 0;

    uint8_t len = MIN(3, avgIdx);
    len = MAX(1, len);

    for(uint8_t i = 0; i < len; ++i)
        a += avg[i];

    //if(battVoltage != 0) // Hardware fault workaround
    s_Pod_powerState.status.batteryVoltage_mV = (a / len); //battVoltage;

    _Pod_PowerBattVoltageToLevel(battVoltage, &s_Pod_powerState.status.batteryLevel_pc);
    MBT_LOG("[BATT] %d mV (%d%%) [%d smp]\n", s_Pod_powerState.status.batteryVoltage_mV, s_Pod_powerState.status.batteryLevel_pc, len);

    // 144 mV offset when connected to charger?

    _Pod_powerOnNewBatterySample();
}

// POWER LEVELS --------------------------------------------------------------------------------------------

#define POWER_LEVEL_CONDITION(LEV) ((s_Pod_powerState.activeClients & (LEV)))

static void _Pod_powerEvalPowerState(void)
{
    if(MBT_FLAG_IS_SET(s_Pod_powerState.activeClients, PWR_ACT_FLAG_CHANGED))
    {
        // Possible power transition
        MBT_FLAG_CLEAR(s_Pod_powerState.activeClients, PWR_ACT_FLAG_CHANGED);
        
        // Compute new power level

        // Set here the minimum level (can be overriden with SetMinimumLevel())
        uint32_t newTickRate = s_Pod_powerState.tickRate;

        if(POWER_LEVEL_CONDITION(PWR_MSK_100HZ))        newTickRate = HAL_SYS_TICKRATE_100HZ;
        else if(POWER_LEVEL_CONDITION(PWR_MSK_50HZ))    newTickRate = HAL_SYS_TICKRATE_50HZ;
        else if(POWER_LEVEL_CONDITION(PWR_MSK_10HZ))    newTickRate = HAL_SYS_TICKRATE_10HZ;
        else if(POWER_LEVEL_CONDITION(PWR_MSK_1HZ))     newTickRate = HAL_SYS_TICKRATE_1HZ;
        else newTickRate = HAL_SYS_TICKRATE_1HZ; //HAL_SYS_TICKRATE_OFF;

        if(newTickRate != s_Pod_powerState.tickRate)
        {
            // Power transition
            MBT_LOG("[PWR] LVL %d -> %d\n", s_Pod_powerState.tickRate, newTickRate);
            s_Pod_powerState.tickRate = newTickRate;
            HAL_sysSetTickRate(newTickRate);
        }
    }
}

void Pod_powerSetActivity(uint32_t clients)
{
    uint32_t changed = ((s_Pod_powerState.activeClients & clients) ? 0 : PWR_ACT_FLAG_CHANGED);
    MBT_FLAG_SET(s_Pod_powerState.activeClients, clients | changed);
    if(changed) HAL_sysSchedulePendingTick();
}

void Pod_powerClearActivity(uint32_t clients)
{
    uint32_t changed = ((s_Pod_powerState.activeClients & clients) ? PWR_ACT_FLAG_CHANGED : 0);
    MBT_FLAG_CLEAR(s_Pod_powerState.activeClients, clients);
    MBT_FLAG_SET(s_Pod_powerState.activeClients, changed);
    if(changed) HAL_sysSchedulePendingTick();
}

bool Pod_powerIsUserActivity(void)
{
    return !!(s_Pod_powerState.activeClients & PWR_MSK_USER_ACTIVITY);
}

uint32_t Pod_powerGetTickRate(void)
{
    return s_Pod_powerState.tickRate;
}

// MODULE INTERFACE ----------------------------------------------------------------------------------------

void Pod_powerInit(void)
{
    memset(&s_Pod_powerState, 0, sizeof(s_Pod_powerState));

    _Pod_powerBCInit();    

    MBT_TIMER_RESET_TIMEOUT(s_Pod_powerState.battMonTimer, 1);
    
    MBT_FLAG_SET(s_Pod_powerState.activeClients, PWR_ACT_FLAG_CHANGED);
    s_Pod_powerState.tickRate = HAL_SYS_TICKRATE_MAX;

    _Pod_powerEvalPowerState();
    _Pod_powerOnVBUSChanged();
}

void Pod_powerTick(uint32_t ticks)
{    
    _Pod_powerBCTick(ticks);
        
    // VBUS debounce timer
    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_powerState.vBusDebounceTimer, ticks,
        {
            _Pod_powerOnVBUSDebounceTimer();
        }
    );
    
    if(s_Pod_powerState.status.externalPowerPresent != EPS_INVALID)
    {
        MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_powerState.battMonTimer, ticks,
            {                
                // Async batt sample
                _Pod_powerSampleBattery();
            }            
        );
    }

    if(MBT_TIMER_ISENABLED(s_Pod_powerState.vBusDebounceTimer)
        || s_Pod_powerState.BCFSM.state != BCS_DISABLED)
    {
        Pod_powerSetActivity(PWR_ACT_POWER_CHG);
    } else {
        Pod_powerClearActivity(PWR_ACT_POWER_CHG);
    }    

    //Hack
    if(s_Pod_powerState.status.externalPowerPresent == EPS_PRESENT && !MBT_TIMER_ISENABLED(s_Pod_powerState.vBusDebounceTimer))
    {
        uint32_t vbus = false;
        HAL_gpioReadDigital(MBT_CFG_PIN_VBUS, &vbus);
        if(!!!vbus)
        {
            HAL_event_t fakeEvent =
            {
                .data =
                {
                    .gpioEvent = 
                    {
                        .pin = MBT_CFG_PIN_VBUS
                    }
                }
            };
            Pod_powerHALEventHandler(&fakeEvent);
        }
    }    
    // Hack end

    _Pod_powerEvalPowerState();
}



