#ifndef _MLINK_DSP_H_
#define _MLINK_DSP_H_

#include <inttypes.h>
#include "MLDecoder.h"
#include <limits.h>

typedef int16_t HALAnalogSample_t;  // nrf_saadc_value_t
#define HALANALOGSAMPLE_MIN (SHRT_MIN)
#define HALANALOGSAMPLE_MAX (SHRT_MAX)


void mldsp_reset(void);
void mldsp_feedSamples(HALAnalogSample_t* buffer, uint16_t len);

// CALLBACKS
void mldsp_onWaveCycle(mldec_WaveCycle_t cycle);

#endif