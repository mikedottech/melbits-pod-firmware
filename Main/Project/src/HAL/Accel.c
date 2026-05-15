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

#include "Accel.h"
#include "Drivers/Accel/SC7A20.h"
#include "mbt_config.h"
#include "utils.h"

static SC7A20_t s_accel;

uint32_t _HAL_accelInit(void)
{
    return sc7a20_init(&s_accel, MBT_CFG_ACCEL_I2C_ADDR);
}

void HAL_accelForceReset(void)
{
    sc7a20_forceReset(&s_accel);    
}

uint32_t HAL_accelSetDataReporting(bool enabled)
{
    sc7a20_setDataReporting(&s_accel, enabled);
    return 0;
}

uint32_t HAL_accelSetConfig(const HAL_accelMode_t *acfg)
{
    if(acfg)
    {    
        sc7a20_start(&s_accel, (SC7A20_mode_t*)acfg);
    } else {
        sc7a20_stop(&s_accel);
    }
    return 0;
}

uint32_t HAL_accelSetFilterConfig(const HAL_accelFilterConfig_t *acfg)
{
    sc7a20_setHPF(&s_accel, (SC7A20_filter_t*)acfg);
    return 0;
}

uint32_t HAL_accelSetMotionDetectionConfig(const HAL_accelMotionDetectionConfig_t *mcfg)
{
    sc7a20_setActivityInterrupt(&s_accel, (SC7A20_activityDetectionConfig_t*)mcfg);
    return 0;
}

uint32_t HAL_accelSetClickDetectionConfig(const HAL_accelClickDetectionConfig_t *ccfg)
{
    sc7a20_setClickInterrupt(&s_accel, (SC7A20_clickDetectionConfig_t*)ccfg);
    return 0;
}

uint32_t HAL_accelApplyConfig()
{
    return sc7a20_commitWrites(&s_accel);
}

uint32_t _HAL_accelGetClearEventFlags()
{
    return sc7a20_getEventsFlags(&s_accel);
}

bool HAL_accelPullSample(HAL_accelSample_t *buffer)
{
// Pod standard axes (face of the toy facing you):
//      
// y     z
// ^    ^
// |  /
// |/
// ----> x

// Accelerometer axes:

//           x     z
//           ^    ^
//           |  /
//           |/
//    y <---- 


    bool res = sc7a20_pullSample(&s_accel, (SC7A20_sample_t*)buffer);

    if(res)
    {
        HAL_accelSample_t *sample = &buffer[0];
        // Swap axes
        int16_t tmp;
        tmp = sample->x;
        sample->x = -sample->y;
        sample->y = tmp;
    }
    return res;
}

//void HAL_accelGetLastSampleScaled(HAL_accelSampleScaled_t *sample)
//{
//    sc7a20_getLastScaledSample(&s_accel, sample);
//
//    // Swap axes
//    int32_t tmp;
//    tmp = sample->x;
//    sample->x = -sample->y;
//    sample->y = tmp;
//}

void HAL_accelScaleSample(const HAL_accelSample_t *sample_in, HAL_accelSampleScaled_t *sample_out)
{
    sc7a20_scaleSample(&s_accel, (SC7A20_sample_t*)sample_in, sample_out);
}

void _HAL_accelInterruptEventHandler(uint8_t irqs)
{
    _isr_sc7a20_io_event_handler(&s_accel, irqs);
}

void _HAL_accelHandleGPIO(uint32_t gpioPins)
{
    uint8_t accel_intFlags = 0;
    if(gpioPins & (1 << MBT_CFG_PIN_ACCELINT1))
    {
        MBT_FLAG_SET(accel_intFlags, SC7A20_IRQ_INT1);            
    } else if(gpioPins & (1 << MBT_CFG_PIN_ACCELINT2))
    {
        MBT_FLAG_SET(accel_intFlags, SC7A20_IRQ_INT2);            
    }
    _HAL_accelInterruptEventHandler(accel_intFlags);
}