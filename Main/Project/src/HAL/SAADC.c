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

// TODO:
// [X] Channel management
// [X] Transition to state WAITHFCLK or STOPPING depending on enabled channels
// [X] Buffer management (callback, return buffer, current buffer)
// [X] Event callbacks
// [X] Function to convert value to mV
// - Try to use the RTC to generate the 120 Hz signal for triggering the SAADC

#include "common.h"
#include <string.h>

#include "mbt_config.h"

#include "SAADC.h"

#include "HAL.h"

#include "nrf_soc.h"
#include "nrf_saadc.h"
#include "nrf_drv_saadc.h"
#include "nrfx_saadc.h"
#include "nrf_drv_clock.h"
#include "nrf_atomic.h"

#include "Clocks.h"

#include "Debug.h"

typedef enum
{
    AS_STOPPED = 0,
    AS_WAITHFCLK,
    AS_WAITSAADC,
    AS_WAITCAL,
    
    AS_SAMPLING,
    AS_STOPPING
} HAL_analogStates_t;

static struct
{
    HAL_analogStates_t  fsmState;
    nrfx_atomic_u32_t   flags;
    nrfx_atomic_u32_t   internalFlags;
    nrfx_atomic_flag_t  bufferConvertedFlag;
    bool                pendingState;
    uint8_t             currentBuffer;
    volatile nrf_saadc_value_t   adcBuffer[2][MBT_CFG_ANALOG_BUFFER_SIZE_SAMPLES];
} s_HAL_analogState;

static void _HAL_analogPpiInit(void);
static void _HAL_analogTimerInit(void);
static void _HAL_analogTimerUninit(void);

static void _HAL_analogSAADCInit(void);
static void _HAL_analogSAADCUninit(void);

static void _HAL_analogSAADCInitChannels(void);
static void _HAL_analogSAADCSetupBuffers(void);

// ------------------------------------------------------------------------------

// Combinations of reference and gain
#define _MBT_SAADC_GAIN_REF_VDD_4_X4    (NRF_SAADC_GAIN1)
#define _MBT_SAADC_GAIN_REF_VDD_4_X2    (NRF_SAADC_GAIN1_2)
#define _MBT_SAADC_GAIN_REF_VDD_4_X1    (NRF_SAADC_GAIN1_4)

#define _MBT_SAADC_GAIN_REF_600MV_X1    (NRF_SAADC_GAIN1_4)
#define _MBT_SAADC_GAIN_REF_600MV_X2    (NRF_SAADC_GAIN1_3)
#define _MBT_SAADC_GAIN_REF_600MV_X3    (NRF_SAADC_GAIN1_2)

// Voltage references
#define _MBT_SAADC_REF_600MV            (NRF_SAADC_REFERENCE_INTERNAL)
#define _MBT_SAADC_REF_VDD_4            (NRF_SAADC_REFERENCE_VDD4)

// Tokens for macro concatenation
#define MBT_SAADC_CH_REF_VDD_OVER_4     VDD_4
#define MBT_SAADC_CH_REF_600MV          600MV

#define MBT_SAADC_CH_GAIN_X1            X1
#define MBT_SAADC_CH_GAIN_X2            X2
#define MBT_SAADC_CH_GAIN_X3            X3
#define MBT_SAADC_CH_GAIN_X4            X4

// Concatenating macros
#define MBT_GAIN_CONCAT(A, B) _MBT_SAADC_GAIN_REF_##A##_##B
#define MBT_REF_CONCAT(A) _MBT_SAADC_REF_##A

#define MBT_SAADC_N_CHANNELS (4)

#define MBT_SAADC_CH_CFG(PIN, REFERENCE, GAIN)\
    {\
        .pin        = PIN,\
        .gain       = MBT_GAIN_CONCAT(REFERENCE, GAIN),\
        .reference  = MBT_REF_CONCAT(REFERENCE)\
    }

typedef struct
{
    nrf_saadc_input_t       pin;
    nrf_saadc_gain_t        gain;
    nrf_saadc_reference_t   reference;
} HAL_analogChannelConfig_t;


#define _MBT_SAADC_RESOLUTION_BITS_10 (NRF_SAADC_RESOLUTION_10BIT)
#define _MBT_SAADC_RESOLUTION_BITS_14 (NRF_SAADC_RESOLUTION_14BIT)

#define _MBT_SAADC_RESOLUTION_BITS_10_RANGEMAX (1024)
#define _MBT_SAADC_RESOLUTION_BITS_14_RANGEMAX (16384)

#define MBT_RES_CONCAT(A) _MBT_SAADC_RESOLUTION_BITS_##A
#define MBT_RES_RANGEMAX_CONCAT(A) _MBT_SAADC_RESOLUTION_BITS_##A##_RANGEMAX

#define MBT_SELECT_RES(R) MBT_RES_CONCAT(R)
#define MBT_SELECT_RES_RANGEMAX(R) MBT_RES_RANGEMAX_CONCAT(R)

// ------------------------------------------------------------------------------

static const HAL_analogChannelConfig_t k_HAL_analogChannelConfig[MBT_SAADC_N_CHANNELS] =
{
    MBT_SAADC_CH_CFG(MBT_CFG_PIN_AIN_TEMP,  MBT_SAADC_CH_REF_VDD_OVER_4,    MBT_SAADC_CH_GAIN_X1),
    MBT_SAADC_CH_CFG(MBT_CFG_PIN_AIN_LIGHT, MBT_SAADC_CH_REF_VDD_OVER_4,    MBT_SAADC_CH_GAIN_X1),
    MBT_SAADC_CH_CFG(MBT_CFG_PIN_AIN_ML,    MBT_SAADC_CH_REF_VDD_OVER_4,    MBT_SAADC_CH_GAIN_X1),
    MBT_SAADC_CH_CFG(MBT_CFG_PIN_AIN_BATT,  MBT_SAADC_CH_REF_600MV,         MBT_SAADC_CH_GAIN_X1)
};

// ------------------------------------------------------------------------------

uint32_t _HAL_analogInit(void)
{
    memset(&s_HAL_analogState, 0, sizeof(s_HAL_analogState));
    s_HAL_analogState.currentBuffer = 1;
    return 0;
}

// ------------------------------------------------------------------------------

static void _HAL_analogTransitionToState(HAL_analogStates_t next)
{
    if(next == s_HAL_analogState.fsmState) return;

    //MBT_LOG("ANALOG: [%d] -> [%d]\n", (int32_t)s_HAL_analogState.fsmState, (int32_t)next);

    switch(next)
    {
        case AS_STOPPED:
            {
                bool pState = s_HAL_analogState.pendingState;
                _HAL_analogInit();
                s_HAL_analogState.pendingState = pState;
            }
            break;
        case AS_WAITHFCLK:
            _HAL_analogPpiInit();
            _HAL_clkHFSharedRequestClient(HAL_CLK_CLIENT_ANALOG_TIMER);
            break;
        case AS_WAITSAADC:
            // Start SAADC
            _HAL_analogSAADCInit();
            break;
        case AS_WAITCAL:
        {
            uint32_t err_code = 0;
            // Start SAADC calibration
            err_code = nrf_drv_saadc_calibrate_offset();
            APP_ERROR_CHECK(err_code);
            break;
        }
        case AS_SAMPLING:            
            // Configure channels
            _HAL_analogSAADCInitChannels();
            // Setup conversion buffers
            _HAL_analogSAADCSetupBuffers();            
            // Start timer
            _HAL_analogTimerInit();
            break;
        case AS_STOPPING:
            // Abort ongoing conversions if any                
            //nrfx_saadc_abort();
            // Release timer, HFCLK, PPI and stop SAADC
            _HAL_analogTimerUninit();
            _HAL_analogSAADCUninit();
            _HAL_clkHFSharedReleaseClient(HAL_CLK_CLIENT_ANALOG_TIMER);
        default:
        break;
    }

    s_HAL_analogState.fsmState = next;
}

// ------------------------------------------------------------------------------

uint32_t _HAL_analogTick(void)
{
    uint32_t err_code = NRF_SUCCESS;
    switch(s_HAL_analogState.fsmState)
    {
        case AS_STOPPED:
            if(s_HAL_analogState.pendingState)
            {
                _HAL_analogTransitionToState(AS_WAITHFCLK);
            }
            break;
        case AS_WAITHFCLK:
            // Check if HFCLK is running
            if(_HAL_clkHFIsRunning())
            {
                _HAL_analogTransitionToState(AS_WAITSAADC);
            }
            break;
        case AS_WAITSAADC:
            // Check if SAADC is enabled            
            if(!nrf_drv_saadc_is_busy())
            {
                _HAL_analogTransitionToState(AS_WAITCAL);
                //_HAL_analogTransitionToState(AS_SAMPLING);                
            }
            break;
        case AS_WAITCAL:
            // Wait for calibration to finish
            if(nrf_atomic_u32_fetch_or(&s_HAL_analogState.internalFlags, 0) & HAL_ANALOG_FLAG_CALDONE)
            {
                nrf_atomic_u32_store(&s_HAL_analogState.internalFlags, 0);
                _HAL_analogTransitionToState(AS_SAMPLING);
            }
            break;
        case AS_SAMPLING:            
            if(!s_HAL_analogState.pendingState)
            {
                _HAL_analogTransitionToState(AS_STOPPING);
            }
            break;
        case AS_STOPPING:
            {
                // Wait for everything to stop
                if(/*!nrf_drv_saadc_is_busy() &&*/
                    !_HAL_clkHFIsRunning())
                    {
                        // Stop the timer again in case there was a pending SAADC START task that started it again
                        _HAL_analogTimerUninit();
                        _HAL_analogTransitionToState(AS_STOPPED);
                    }
            }
            break;
    }
    return 0;
}

// ------------------------------------------------------------------------------

static void _HAL_analogPpiInit(void)
{
#if SOFTDEVICE_PRESENT
    uint32_t err = 0;
    uint32_t chn = 0;
    err = sd_ppi_channel_assign(0, (const void*)&NRF_TIMER1->EVENTS_COMPARE[0], (const void*)(uint32_t)&NRF_SAADC->TASKS_SAMPLE);
    err = sd_ppi_channel_assign(1, (const void*)&NRF_SAADC->EVENTS_END, (const void*)(uint32_t)&NRF_TIMER1->TASKS_STOP);
    err = sd_ppi_channel_assign(2, (const void*)&NRF_SAADC->EVENTS_STARTED, (const void*)(uint32_t)&NRF_TIMER1->TASKS_START);
    APP_ERROR_CHECK(err);
    err = sd_ppi_channel_enable_get(&chn);
    APP_ERROR_CHECK(err);
    chn |= (PPI_CHEN_CH0_Enabled << PPI_CHEN_CH0_Pos) | (PPI_CHEN_CH1_Enabled << PPI_CHEN_CH1_Pos) | (PPI_CHEN_CH2_Enabled << PPI_CHEN_CH2_Pos);
    err = sd_ppi_channel_enable_set(chn);
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

#define FFTTEST
// ------------------------------------------------------------------------------

static void _HAL_analogTimerInit(void)
{
    NRF_TIMER1->SHORTS        = 0x1;            // COMPARE[0] -> CLEAR

    #ifdef FFTTEST
    NRF_TIMER1->PRESCALER     = 3;              // Ftimer = 16 MHz / 2^prescaler
                                                //          16 MHz / 256 = 62500 Hz

    NRF_TIMER1->CC[0]         = 15625;     // 62500 Hz / (1042/2) = 119.96 Hz                                              
    #else
    NRF_TIMER1->PRESCALER     = 8;              // Ftimer = 16 MHz / 2^prescaler
                                                //          16 MHz / 256 = 62500 Hz

    NRF_TIMER1->CC[0]         = (1042) / 2;     // 62500 Hz / (1042/2) = 119.96 Hz                                              
    #endif
                                              
    NRF_TIMER1->MODE          = TIMER_MODE_MODE_Timer;
    NRF_TIMER1->BITMODE       = TIMER_BITMODE_BITMODE_32Bit;
    
    //NRF_TIMER1->TASKS_START   = 1;              // Start timer
}

// ------------------------------------------------------------------------------

static void _HAL_analogTimerUninit(void)
{
    NRF_TIMER1->TASKS_STOP = 1;
}

__WEAK bool _ISR_HAL_analogRawBufferHandler(HAL_analogSample_t *pBuffer, uint16_t len)
{
    return false;
}

// ------------------------------------------------------------------------------

static void _ISR_HAL_analogSAADCEventHandler(nrfx_saadc_evt_t const *p_event)
{
    ret_code_t err_code;
    
    HAL_sysSchedulePendingTick();

    if (p_event->type == NRF_DRV_SAADC_EVT_DONE)
    {
        nrf_atomic_u32_or(&s_HAL_analogState.flags, HAL_ANALOG_FLAG_CONVDONE);
        nrf_atomic_flag_set(&s_HAL_analogState.bufferConvertedFlag);

        if(_ISR_HAL_analogRawBufferHandler(p_event->data.done.p_buffer, MBT_CFG_ANALOG_BUFFER_SIZE_SAMPLES))
        {
            // Prepare used buffer for conversion
            err_code = nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, MBT_CFG_ANALOG_BUFFER_SIZE_SAMPLES);
            APP_ERROR_CHECK(err_code);
        }
    } else if (p_event->type == NRF_DRV_SAADC_EVT_CALIBRATEDONE)
    {
        // SAADC calibration complete
        nrf_atomic_u32_or(&s_HAL_analogState.internalFlags, HAL_ANALOG_FLAG_CALDONE);
    }
}

// ------------------------------------------------------------------------------

static void _HAL_analogSAADCInitChannels(void)
{
    for(uint8_t i = 0; i < MBT_SAADC_N_CHANNELS; ++i)
    {
        uint32_t err_code = 0;
        const HAL_analogChannelConfig_t *p = &(k_HAL_analogChannelConfig[i]);
        nrf_saadc_channel_config_t channel_config =
            NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(p->pin);
        channel_config.gain = p->gain;
        channel_config.reference = p->reference;

//if(i == 2)
//{
//    channel_config.resistor_p = NRF_SAADC_RESISTOR_PULLDOWN;
//    channel_config.resistor_n = NRF_SAADC_RESISTOR_PULLDOWN;
//}

        err_code = nrf_drv_saadc_channel_init(i, &channel_config);
        APP_ERROR_CHECK(err_code);
    }
}

// ------------------------------------------------------------------------------

static void _HAL_analogSAADCSetupBuffers(void)
{
    uint32_t err_code = 0;
    err_code = nrf_drv_saadc_buffer_convert((nrf_saadc_value_t *)s_HAL_analogState.adcBuffer[0], MBT_CFG_ANALOG_BUFFER_SIZE_SAMPLES);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_saadc_buffer_convert((nrf_saadc_value_t *)s_HAL_analogState.adcBuffer[1], MBT_CFG_ANALOG_BUFFER_SIZE_SAMPLES);
    APP_ERROR_CHECK(err_code);
}

// ------------------------------------------------------------------------------

static void _HAL_analogSAADCInit(void)
{
    ret_code_t err_code;
    nrf_drv_saadc_config_t cfg =
    {                                                                               
        .resolution         = MBT_SELECT_RES(MBT_CFG_ANALOG_RESOLUTION_BITS), 
        .oversample         = NRF_SAADC_OVERSAMPLE_DISABLED, 
        .interrupt_priority = NRFX_SAADC_CONFIG_IRQ_PRIORITY,                       
        .low_power_mode     = NRFX_SAADC_CONFIG_LP_MODE
    };

    err_code = nrf_drv_saadc_init(&cfg, _ISR_HAL_analogSAADCEventHandler);
    APP_ERROR_CHECK(err_code);

    //channel_config.reference = NRF_SAADC_REFERENCE_VDD4;

    // REF = VDD/4
    //channel_config.gain = NRF_SAADC_GAIN1_6; // x4/6 = 2/3
    //channel_config.gain = NRF_SAADC_GAIN1_5; // x4/5
    //channel_config.gain = NRF_SAADC_GAIN1_4; //x1
    ////channel_config.gain = NRF_SAADC_GAIN1_2; //x2
    //channel_config.gain = NRF_SAADC_GAIN1; //x4
    //channel_config.gain = NRF_SAADC_GAIN2; //x8
    //channel_config.gain = NRF_SAADC_GAIN4; //x16

    // REF = 0.6V
    //channel_config.gain = NRF_SAADC_GAIN1_3; //x2
    //channel_config.gain = NRF_SAADC_GAIN1_2; //x3
}

// ------------------------------------------------------------------------------

uint32_t HAL_analogGetLastBuffer(int16_t **ppBuffer, uint16_t *pLen)
{
    if(nrf_atomic_flag_clear_fetch(&s_HAL_analogState.bufferConvertedFlag))
    {
        s_HAL_analogState.currentBuffer ^= 0x1;
    }
    *pLen = MBT_CFG_ANALOG_BUFFER_SIZE_SAMPLES;
    *ppBuffer = (nrf_saadc_value_t *)&s_HAL_analogState.adcBuffer[s_HAL_analogState.currentBuffer][0];
    return 0;
}

// ------------------------------------------------------------------------------

uint32_t HAL_analogReturnBuffer(int16_t *pBuffer)
{    
    return nrf_drv_saadc_buffer_convert((nrf_saadc_value_t *)pBuffer, MBT_CFG_ANALOG_BUFFER_SIZE_SAMPLES);
}

// ------------------------------------------------------------------------------

static void _HAL_analogSAADCUninit(void)
{
    nrf_drv_saadc_uninit();
}

// ------------------------------------------------------------------------------

uint32_t _HAL_analogGetAndClearEventFlags(void)
{
    return nrf_atomic_u32_fetch_store(&s_HAL_analogState.flags, 0UL);
}

// ------------------------------------------------------------------------------

uint32_t HAL_analogSetEnabled(bool enabled)
{
    s_HAL_analogState.pendingState = enabled;
    HAL_sysSchedulePendingTick();
    return 0;
}

// ------------------------------------------------------------------------------

bool HAL_analogIsRunning(void)
{
    return (s_HAL_analogState.fsmState == AS_SAMPLING);
}

/*
#define ADC_REF_VOLTAGE_IN_MILLIVOLTS     600       // Internal ref: 0.6V
#define ADC_RES_10BIT                     1024      // 2^10 bits
#define ADC_PRE_SCALING_COMPENSATION      6         // Gain = 1/6
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE)\
        ((((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS) / ADC_RES_10BIT) * ADC_PRE_SCALING_COMPENSATION)

static uint32_t adc_to_mv(uint32_t adc_value, uint32_t ref_voltage_mv, uint32_t adc_max_value, uint32_t gain_divisor)
{
    return ((((adc_value) * ref_voltage_mv) / adc_max_value) * gain_divisor);
}
*/

static uint32_t _HAL_analogGetScalingCompensation(nrf_saadc_gain_t gain)
{
    switch(gain)
    {
        case NRF_SAADC_GAIN1: return 1;
        case NRF_SAADC_GAIN1_2: return 2;
        case NRF_SAADC_GAIN1_3: return 3;
        case NRF_SAADC_GAIN1_4: return 4;
        default:
            MBT_ASSERT(0);
        return 0;
    }
    return 0;
}

uint32_t HAL_analogRawToMv(uint8_t ch, HAL_analogSample_t sample)
{
    MBT_ASSERT(ch < MBT_SAADC_N_CHANNELS);
    const HAL_analogChannelConfig_t *pConfig = &k_HAL_analogChannelConfig[ch];
   
    uint32_t ref = pConfig->reference == _MBT_SAADC_REF_600MV ? 600 : (MBT_CFG_ANALOG_VCC_REF_MV >> 2);
    const uint32_t rangemax = MBT_SELECT_RES_RANGEMAX(MBT_CFG_ANALOG_RESOLUTION_BITS);
    uint32_t sc = _HAL_analogGetScalingCompensation(pConfig->gain);

    return ((((sample) * ref) / rangemax) * sc);

    // Use internal reference for batt
    // REF = 0.6V
    // Max VBatt = 4.2
    // Voltage divider = /2
    // 4.2 / 2 = 2.1
    // 0.6 * 4 = 2.4V (Max range = 2.4V with gain = 1/4)
    ////channel_config.reference = NRF_SAADC_REFERENCE_INTERNAL;        
    ////channel_config.gain = NRF_SAADC_GAIN1_4; //x1

    // REF = 0.6V
    //channel_config.gain = NRF_SAADC_GAIN1_3; //x2
    //channel_config.gain = NRF_SAADC_GAIN1_2; //x3
}

uint32_t _HAL_analogForceStop(void)
{
    _HAL_analogTimerUninit();
    _HAL_analogSAADCUninit();
    return 0;
}
#if 0


static volatile uint8_t s_saadc_running = 0;
static volatile uint32_t s_saadc_result_mv = 0;
static volatile uint32_t s_saadc_result = 0;

static HALPowerLevel_t s_powerLevel = POWER_LEVEL_FULL;

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



#define SAMPLES_IN_BUFFER 1

static nrf_saadc_value_t m_adc_buffer_pool[1][SAMPLES_IN_BUFFER];

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
#endif
