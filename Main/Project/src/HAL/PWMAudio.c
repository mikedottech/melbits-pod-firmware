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

#include "PWMAudio.h"

#include "mbt_config.h"
#include "nrf_drv_pwm.h"

#include "HAL.h"
#include "GPIO.h"



static nrf_drv_pwm_t m_speaker_pwm = NRF_DRV_PWM_INSTANCE(0);

#define SEQ_REPEATS     (3)

typedef struct
{
    uint16_t           seq_buf[2][MBT_CFG_AUDIO_BUFFER_SIZE_SAMPLES];
    nrf_pwm_sequence_t seq[2];
    bool               is_active;
} pwm_pcm_t;

extern uint16_t HAL_onAudioRequestSamples(uint16_t* buffer, uint16_t len);

static pwm_pcm_t s_pcm  = {0};

static void _HAL_audioOutPASetPower(bool on)
{
    HAL_gpioWriteDigital(MBT_CFG_PIN_AUDIO_PA, on);
}

static uint32_t seq_buffer_update(uint8_t idx)
{
    uint32_t pwm_flag;

    uint16_t *bfr = &(s_pcm.seq_buf[idx][0]);

    pwm_flag = ((idx == 0 ? NRF_DRV_PWM_FLAG_SIGNAL_END_SEQ0 : NRF_DRV_PWM_FLAG_SIGNAL_END_SEQ1) | NRF_DRV_PWM_FLAG_NO_EVT_FINISHED);

    uint16_t readSamples = HAL_onAudioRequestSamples(bfr, MBT_CFG_AUDIO_BUFFER_SIZE_SAMPLES);

    if(readSamples < MBT_CFG_AUDIO_BUFFER_SIZE_SAMPLES)
    {        
        return NRF_DRV_PWM_FLAG_STOP;
    }

    nrf_pwm_sequence_t *seq = &(s_pcm.seq[idx]);

    seq->values.p_common = bfr;
    seq->length          = MBT_CFG_AUDIO_BUFFER_SIZE_SAMPLES;
    seq->repeats         = SEQ_REPEATS;
    seq->end_delay       = 0;

    return pwm_flag;
}

void HAL_audioAbort()
{    
    nrfx_pwm_uninit(&m_speaker_pwm);
    _HAL_audioOutPASetPower(false);
}

static void _HAL_PWMHandler(nrf_drv_pwm_evt_type_t event_type)
{
    uint32_t evt = 0;
    
    HAL_sysSchedulePendingTick();

    switch (event_type)
    {
        case NRF_DRV_PWM_EVT_STOPPED:
            _HAL_audioOutPASetPower(false);
            break;
        case NRF_DRV_PWM_EVT_FINISHED:
            break;
        case NRF_DRV_PWM_EVT_END_SEQ0:
            if(s_pcm.is_active)
            {
                evt = seq_buffer_update(0);
                if(evt == NRF_DRV_PWM_FLAG_STOP)
                {
                    //nrf_pwm_shorts_set(m_speaker_pwm.p_registers, NRF_PWM_SHORT_SEQEND0_STOP_MASK);                    
                    HAL_audioOutEnd();
                }            
            } else {
                HAL_audioAbort();
            }
            break;
        case NRF_DRV_PWM_EVT_END_SEQ1:
            if(s_pcm.is_active)
            {
                evt = seq_buffer_update(1);
                if(evt == NRF_DRV_PWM_FLAG_STOP)
                {
                    //nrf_pwm_shorts_set(m_speaker_pwm.p_registers, NRF_PWM_SHORT_SEQEND1_STOP_MASK);
                    HAL_audioOutEnd();
                }     
            } else {
                HAL_audioAbort();
            }
            break;
        default:
            break;
    }
}

uint32_t _HAL_audioOutInit()
{
    if(s_pcm.is_active)
    {
        return 1;
    }
    
    return 0;
}

uint32_t HAL_audioOutBegin(bool enablePA)
{
    //if(s_pcm.is_active)
      //  return 1;

    uint32_t err_code = 0;

    nrf_drv_pwm_config_t const pwm_config =
    {
        .output_pins =
        {
            MBT_CFG_PIN_AUDIO_OUT,      // channel 0
            NRF_DRV_PWM_PIN_NOT_USED,   // channel 1
            NRF_DRV_PWM_PIN_NOT_USED,   // channel 2
            NRF_DRV_PWM_PIN_NOT_USED,   // channel 3
        },
        .irq_priority = APP_IRQ_PRIORITY_LOWEST, //APP_IRQ_PRIORITY_LOW,
        .base_clock   = NRF_PWM_CLK_16MHz,
        .count_mode   = NRF_PWM_MODE_UP,
        .top_value    = 250,
        .load_mode    = NRF_PWM_LOAD_COMMON,
        .step_mode    = NRF_PWM_STEP_AUTO
    };
    err_code = nrf_drv_pwm_init(&m_speaker_pwm, &pwm_config, _HAL_PWMHandler);
    
    if(err_code == NRFX_ERROR_INVALID_STATE) err_code = 0;
    
    nrf_drv_pwm_stop(&m_speaker_pwm, true);

    uint32_t pwm_flags;

    pwm_flags = seq_buffer_update(0);
    pwm_flags |= seq_buffer_update(1);

    if((pwm_flags & NRF_DRV_PWM_FLAG_STOP) == 0)
    {
        s_pcm.is_active = true;

        (void)nrf_drv_pwm_complex_playback(&m_speaker_pwm, &s_pcm.seq[0], &s_pcm.seq[1], 1, (pwm_flags | NRF_DRV_PWM_FLAG_LOOP));

        _HAL_audioOutPASetPower(enablePA);
    } else {
        HAL_audioAbort();
    }
    return err_code;
}

uint32_t HAL_audioOutEnd()
{    
    // Mark as inactive. In the next call to the handler it will stop the module
    s_pcm.is_active = false;
    return 0;
}


void HAL_setAudioPAState(bool state)
{
    if(state && !s_pcm.is_active) return;
    _HAL_audioOutPASetPower(state);
}