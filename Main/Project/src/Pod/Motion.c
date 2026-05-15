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

#include "Motion.h"
#include "HAL/Accel.h"
#include "utils.h"
#include "HAL/Debug.h"
#include "Pod/Power.h"
//#include "fpmath.h"

//
// [X] Enable/Disable recognition algorithms
// [X] Get and analyze a stream of samples
// [X] Trigger callback with motion events
//      [X] Shake
//      [X] Motion
// [CAN'T GO TO IDLE IF ACTIVE ALGS]- Finish active algorithms before going to idle
// [X] Configure accelerometer for wakeup
// [X] Manage sample rate speed and accel interrupt features
// [X] Generate sub-stream for the Streaming server
// [X] Add "peak" event to shake handler?

typedef struct
{
    void (*pReset)(void);
    void (*pUpdate)(void);
} Pod_motionAlgorithmControl_t;

#define POD_MOTION_SAMPLE_BUFFER_LEN (40)
#define POD_MOTION_AVG_WINDOW_LEN (5)

typedef struct
{
    struct
    {
        bool                enabled;
        uint16_t            prescalerMax;
        uint16_t            prescalerCount;
        HAL_accelSample_t   buffer;
    } subStream;
    struct
    {
        HAL_accelMode_t     mode;
        uint32_t            currentIntervalUs;
    } accel;
    struct
    {
        HAL_accelSampleScaled_t scaledSmmpleBuffer[POD_MOTION_SAMPLE_BUFFER_LEN];
        uint8_t                 scaledSampleBufferIndex;
        HAL_accelSampleScaled_t scaledRawSmmpleBuffer[POD_MOTION_AVG_WINDOW_LEN];
        uint8_t                 scaledRawSampleBufferIndex;
        HAL_accelSampleScaled_t scaledLastSample;
        HAL_accelSampleScaled_t scaledDiff;        
        HAL_accelSampleScaled_t unfilteredScaledLastSample;
        HAL_accelSampleScaled_t unfilteredScaledDiff;
        uint32_t                scaledLength;
        int32_t                 scaledLengthDiff;
    } global;
    uint8_t                 algorithms;
    uint8_t                 busyAlgorithms; // Algorithms that prevent accel power saving
    uint8_t                 flags;
    Pod_motionAccelLevel_t  level;
    Pod_motionAccelLevel_t  wantedLevel;
    Pod_motionAccelLevel_t  minLevel;
    
    uint8_t                 nTaps;

    MBT_TIMER_DEFINE(activityTimer);
    MBT_TIMER_DEFINE(awareTimer);
    //MBT_TIMER_DEFINE(tapTimer);
    //MBT_TIMER_DEFINE(interTapTimer);
    
} Pod_motionState_t;

#define POD_ACCEL_COMPUTE_PRESCALER(HZ)         (HZ / MBT_CFG_MOTION_SUBSTREAM_RATE_HZ)
#define POD_MOTION_INACTIVITY_TIMEOUT_TICKS     (MBT_US_TO_TICKS(MBT_CFG_MOTION_INACTIVITY_TIMEOUT_MS * 1000))
#define POD_MOTION_AWARE_TIMEOUT_TICKS          (MBT_US_TO_TICKS(MBT_CFG_MOTION_AWARENESS_TIMEOUT_MS * 1000))

#define POD_MOTION_FLAGS_FIRSTSAMPLE (1)

Pod_motionState_t s_Pod_motionState;


static void Pod_motionGetBufferSample(int8_t ref, int8_t offset, HAL_accelSampleScaled_t* smp)
{
    int8_t loc = s_Pod_motionState.global.scaledSampleBufferIndex - 1 + ref + offset;
    if(loc < 0) loc += POD_MOTION_SAMPLE_BUFFER_LEN;
    if(loc >= POD_MOTION_SAMPLE_BUFFER_LEN) loc -= POD_MOTION_SAMPLE_BUFFER_LEN;
    //loc %= POD_MOTION_SAMPLE_BUFFER_LEN;
    
    MBT_ASSERT(loc >= 0 && loc < POD_MOTION_SAMPLE_BUFFER_LEN);

    if(loc >= 0 && loc < POD_MOTION_SAMPLE_BUFFER_LEN)
    {
        *smp = s_Pod_motionState.global.scaledSmmpleBuffer[loc];
    } else {
        memset(&smp, 0, sizeof(HAL_accelSampleScaled_t));
    }
}


// ALGORITHMS ---------------------------------------------------------------

// ALGORITHMS/SHAKE ---------------------------------------------------------
typedef enum
{
    MSS_INVALID = 0,
    MSS_WAITUP,  // Expecting to get out of the deadzone
    MSS_WAITDN   // Expecting to get back to the deadzone
} Pod_motionAlgShakeState_t;

static struct
{
    uint32_t    window;                     // Sliding window for acceleration vector length
    int32_t     windowDiff;                 // Sliding window diff
    uint16_t    nPeaks;                     // Number of consecutive peaks detected
    Pod_motionAlgShakeState_t state;        // FSM state
    bool        peakReported;               // Has the peak of this cycle been reported?
    uint32_t    timeFromLastPeak_ms;        // Time elapsed since the last peak
    bool        isShaking;                  // True between events STARTED and FINISHED
    //bool        is;
    uint32_t    timeFromThreshold_ms;
} s_Pod_motionAlgShakeData;

static void _Pod_motionAlgShakeReset(void)
{
    memset(&s_Pod_motionAlgShakeData, 0, sizeof(s_Pod_motionAlgShakeData));
    s_Pod_motionAlgShakeData.state = MSS_WAITUP;
}

static void _Pod_motionAlgShakeMoveWindow(uint32_t val)
{
    if(val > s_Pod_motionAlgShakeData.window + (MBT_CFG_MOTION_ALG_SHAKE_ACCEL_WINDOW_SIZE_mG >> 1))
    {
        s_Pod_motionAlgShakeData.window = val - (MBT_CFG_MOTION_ALG_SHAKE_ACCEL_WINDOW_SIZE_mG >> 1);
    } else if (val < s_Pod_motionAlgShakeData.window - (MBT_CFG_MOTION_ALG_SHAKE_ACCEL_WINDOW_SIZE_mG >> 1))
    {
        s_Pod_motionAlgShakeData.window = val + (MBT_CFG_MOTION_ALG_SHAKE_ACCEL_WINDOW_SIZE_mG >> 1);
    }
}

static void _Pod_motionAlgShakeTransitionToState(Pod_motionAlgShakeState_t nextState)
{
    if(nextState == s_Pod_motionAlgShakeData.state) return;

    //MBT_LOG("[SHAKE] %d -> %d\n", s_Pod_motionAlgShakeData.state, nextState);

    switch(s_Pod_motionAlgShakeData.state)
    {
        case MSS_WAITDN:
            s_Pod_motionAlgShakeData.timeFromThreshold_ms = 0; //
        break;
        case MSS_WAITUP:
            s_Pod_motionAlgShakeData.peakReported = false;
        break;
        default: break;
    }
    s_Pod_motionAlgShakeData.state = nextState;
}

#define MBT_CFG_MOTION_TAP_MIN_TIME_BETWEEN_PEAKS_MS (200)

static void _Pod_motionAlgShakeTick(void)
{
    bool peakFound = false;

    Pod_motionShakeState_t evtToSend = SHK_INVALID;

    char dbgPeakType = ' ';

    uint32_t winBefore = s_Pod_motionAlgShakeData.window;

    _Pod_motionAlgShakeMoveWindow(s_Pod_motionState.global.scaledLength);

    s_Pod_motionAlgShakeData.windowDiff = s_Pod_motionAlgShakeData.window - winBefore;

    s_Pod_motionAlgShakeData.timeFromLastPeak_ms += (s_Pod_motionState.accel.currentIntervalUs / 1000);

    switch(s_Pod_motionAlgShakeData.state)
    {
        case MSS_WAITDN:
            if(s_Pod_motionAlgShakeData.window < MBT_CFG_MOTION_ALG_SHAKE_DEADZONE_mG)
            {
                _Pod_motionAlgShakeTransitionToState(MSS_WAITUP);
            }

            if(s_Pod_motionAlgShakeData.windowDiff < 0)
            {
                // Peak found
                if(!s_Pod_motionAlgShakeData.peakReported)
                {                    
                    // No peaks reported in this cycle
                    ++s_Pod_motionAlgShakeData.nPeaks;                    
                    s_Pod_motionAlgShakeData.peakReported = true;                    
                    peakFound = true;
                }
            }

            s_Pod_motionAlgShakeData.timeFromThreshold_ms += (s_Pod_motionState.accel.currentIntervalUs / 1000);

            if(s_Pod_motionAlgShakeData.timeFromThreshold_ms < 22)
            {
                peakFound = false;
            }

        break;
        case MSS_WAITUP:
            if(s_Pod_motionAlgShakeData.window > MBT_CFG_MOTION_ALG_SHAKE_DEADZONE_mG)
            {
                _Pod_motionAlgShakeTransitionToState(MSS_WAITDN);                
            }
        break;
        default:
        break;
    }

    // Process the found peak
    if(peakFound)
    {
        // Is this peak enough separated in time to be accounted for?        
        if(s_Pod_motionAlgShakeData.timeFromLastPeak_ms >= MBT_CFG_MOTION_ALG_SHAKE_MIN_TIME_BETWEEN_PEAKS_MS)
        //if(s_Pod_motionAlgShakeData.timeFromLastPeak_ms <= 100)
        {
            dbgPeakType = 'O';
            // This peak should be accounted for
            if(!s_Pod_motionAlgShakeData.isShaking)
            {                
                if(s_Pod_motionAlgShakeData.nPeaks >= MBT_CFG_MOTION_ALG_SHAKE_NUMPEAKS)
                {   
                    // SHAKING STARTED
                    s_Pod_motionAlgShakeData.isShaking = true;
                    // START EVENT
                    evtToSend = SHK_STARTED;
                    Pod_powerSetActivity(PWR_ACT_SHK_ACTIVE);
                    MBT_FLAG_SET(s_Pod_motionState.busyAlgorithms, (1 << POD_MOTION_ALGORITHM_ID_SHAKE));
                    dbgPeakType = '>';
                }
            } else {
                // PEAK EVENT             
                evtToSend = SHK_PEAK;                
                dbgPeakType = '^';
            }

            
        } else {
            dbgPeakType = 'X';
        }

        // TAP
//        if(s_Pod_motionAlgShakeData.timeFromLastPeak_ms >= MBT_CFG_MOTION_TAP_MIN_TIME_BETWEEN_PEAKS_MS)
//        {
//            if(!s_Pod_motionAlgShakeData.isShaking)
//                MBT_LOG(">> TAP!\n");
//        }

        s_Pod_motionAlgShakeData.timeFromLastPeak_ms = 0;
    }

    if(s_Pod_motionAlgShakeData.timeFromLastPeak_ms >= MBT_CFG_MOTION_ALG_SHAKE_INACTIVITY_TIMEOUT_MS)
    {
        // Check if the shaking has timed out
        if(s_Pod_motionAlgShakeData.isShaking)
        {
            // STOP EVENT        
            evtToSend = SHK_STOPPED;
            Pod_powerClearActivity(PWR_ACT_SHK_ACTIVE);
            MBT_FLAG_CLEAR(s_Pod_motionState.busyAlgorithms, (1 << POD_MOTION_ALGORITHM_ID_SHAKE));
            dbgPeakType = '<';            
        }
        // Reset counters and state
        _Pod_motionAlgShakeReset(); // This also sets isShaking to false
    }


    if(evtToSend != SHK_INVALID)
    {
        const Pod_motionEventData_t e =
        {
            .type = MET_SHAKE,
            .data =
            {
                .shake = { evtToSend }
            }
        };
        Pod_motionEventHandler(&e);
    }

#define ABS(X) (X) < 0 ? -(X): (X)

#if 0
    {
        //int32_t meter = s_Pod_motionAlgShakeData.window >> 4;

        //int32_t disp = s_Pod_motionAlgShakeData.window;
        int32_t disp = ABS(s_Pod_motionState.global.scaledDiff.x) + ABS(s_Pod_motionState.global.scaledDiff.y) + ABS(s_Pod_motionState.global.scaledDiff.z);
        int32_t meter = disp >> 4;

        for(int32_t j = 1; j <=32; ++j)
        {
            if(j < meter)
            {
                MBT_LOG("=");
            } else {
                MBT_LOG(" ");
            }
        }
        HAL_accelSampleScaled_t uDiff = s_Pod_motionState.global.unfilteredScaledDiff;
        static HAL_accelSampleScaled_t lastuDiff;
        char cx, cy, cz;

        #define M(C, COORD)\
            {\
                bool d = false;\
                if(uDiff.COORD > 0 && lastuDiff.COORD < 0) d = true;\
                if(uDiff.COORD < 0 && lastuDiff.COORD > 0) d = true;\
                C = d ? '*' : (uDiff.COORD > 0 ? '>' : '<');\
            }
        M(cx, x)
        M(cy, y)
        M(cz, z)

        MBT_LOG("%d %c [%d, %d, %d]\t [%c, %c, %c] -- %d", disp, dbgPeakType, uDiff.x, uDiff.y, uDiff.z, cx, cy, cz, s_Pod_motionAlgShakeData.timeFromThreshold_ms);
//        if(peakFound)
//        {
//            MBT_LOG(" ****");
//        }
        MBT_LOG("\n");
        lastuDiff = uDiff;
    }
#endif

}

// ALGORITHMS/TAP ---------------------------------------------------------

typedef struct
{
    int16_t oldrange;

    bool result;
    uint8_t fsm;

    int16_t descentStart;
    int16_t dist;    
    uint16_t samples;
} Pod_motionAlgTapData_t;

typedef enum
{
    TS_IDLE = 0,
    FSM_REC_UP,
    FSM_REC_DN,
    FSM_WAIT,
    FSM_OK
} Pod_motionAlgTapState_t;

#define FSM_IDLE 0
#define FSM_REC_UP 1
#define FSM_REC_DN 2
#define FSM_WAIT 3
#define FSM_OK 4


static Pod_motionAlgTapData_t s_Pod_motionAlgTapData;

static void _Pod_motionAlgTapTick(void)
{
}

static void _Pod_motionAlgTapReset(void)
{
    memset(&s_Pod_motionAlgTapData, 0, sizeof(Pod_motionAlgTapData_t));
}

// ---------------------------------------------------------------------------

const Pod_motionAlgorithmControl_t s_podMotionAlgs[POD_MOTION_ALGORITHMS_NUM] =
{
    {
        .pReset = &_Pod_motionAlgShakeReset,
        .pUpdate = &_Pod_motionAlgShakeTick
    },
    {
        .pReset = &_Pod_motionAlgTapReset,
        .pUpdate = &_Pod_motionAlgTapTick
    }
};


// --------------------------------------------------------------------------

static void _Pod_motionAccelCmdSetRate(uint8_t rate)
{
    s_Pod_motionState.accel.mode.rate = rate;
    HAL_accelSetConfig(&s_Pod_motionState.accel.mode);
}

static void _Pod_motionAccelDoSetLevel(Pod_motionAccelLevel_t level)
{    
    if(s_Pod_motionState.level == level) return;

    if(level == ACCEL_LEVEL_INVALID) return;

    bool neededReporting = (s_Pod_motionState.level > ACCEL_LEVEL_IDLE);
    bool needsReporting = (level > ACCEL_LEVEL_IDLE);
    //bool needsReporting = true;

    uint8_t rate;
    uint32_t rateHz;
    
    MBT_ASSERT(level < ACCEL_LEVEL_COUNT);

    
    static const struct
    {
        uint8_t accelRate;
        uint16_t rateHz;
    } levels[ACCEL_LEVEL_COUNT - 1] =
    {
        { .accelRate = HAL_ACCEL_RATE_POWEROFF, .rateHz = 0     },
        { .accelRate = HAL_ACCEL_RATE_1HZ,      .rateHz = 1     },
        { .accelRate = HAL_ACCEL_RATE_10HZ,     .rateHz = 10    },
        { .accelRate = HAL_ACCEL_RATE_25HZ,     .rateHz = 25    },
        { .accelRate = HAL_ACCEL_RATE_400HZ,    .rateHz = 400   }
    };

    // TODO: When changing the rate, the prescalerCount should be scaled accordingly

    rate    = levels[level - 1].accelRate;
    rateHz  = levels[level - 1].rateHz;

    s_Pod_motionState.subStream.prescalerMax = POD_ACCEL_COMPUTE_PRESCALER(rateHz);
    s_Pod_motionState.accel.currentIntervalUs = (rateHz > 0) ? (1000000 / rateHz) : 0;

    _Pod_motionAccelCmdSetRate(rate);

    HAL_accelSetDataReporting(needsReporting);
    
    if((neededReporting != needsReporting) && needsReporting)
    {
        MBT_FLAG_SET(s_Pod_motionState.flags, POD_MOTION_FLAGS_FIRSTSAMPLE);
    }

    s_Pod_motionState.level = level;

    MBT_LOG("[MTN] Accel lvl = %d\n", level);
}

static void _Pod_motionAccelInit(void)
{
    const HAL_accelMode_t mode =
    {
        .rate       = HAL_ACCEL_RATE_400HZ,
        .axis       = HAL_ACCEL_X_ENABLE | HAL_ACCEL_Y_ENABLE | HAL_ACCEL_Z_ENABLE,
        .lowpower   = 0, //HAL_ACCEL_LOWPOWER,
        .range      = HAL_ACCEL_RANGE_2G
    };

    s_Pod_motionState.accel.mode = mode;

    _Pod_motionAccelDoSetLevel(ACCEL_LEVEL_IDLE);
    s_Pod_motionState.wantedLevel = ACCEL_LEVEL_IDLE;
    s_Pod_motionState.minLevel = ACCEL_LEVEL_DORMANT;

    const HAL_accelFilterConfig_t filter =
    {
        .target     = /*HAL_ACCEL_HPF_TARGET_CLICK |*/ HAL_ACCEL_HPF_TARGET_MOTION,
        .mode       = HAL_ACCEL_HPF_MODE_RESET_ON_REF_READ,
        .cutoff     = HAL_ACCEL_HPF_CUTOFF_0,
        .ref = 0
    };
    HAL_accelSetFilterConfig(&filter);

//    const HAL_accelClickDetectionConfig_t clickCfg =
//    {
//        .axis               = HAL_ACCEL_CLICK_AXIS_X_SINGLE /*| HAL_ACCEL_CLICK_AXIS_Y_SINGLE | HAL_ACCEL_CLICK_AXIS_Z_SINGLE*/,
//        .threshold_mg       = 200,
//        .time_limit_ms      = 45,
//        .time_latency_ms    = 150,
//        .time_window_ms     = 200
//    };
//    HAL_accelSetClickDetectionConfig(&clickCfg);

//    const HAL_accelMotionDetectionConfig_t act =
//    {
//        .axis               = HAL_ACCEL_ACTIVITY_X_HIGH | HAL_ACCEL_ACTIVITY_Y_HIGH | HAL_ACCEL_ACTIVITY_Z_HIGH,
//        .threshold_mg       = 40,
//        .duration_ms        = 40
//    };
//
//    HAL_accelSetMotionDetectionConfig(&act);
}


void Pod_motionInit(void)
{
    memset(&s_Pod_motionState, 0, sizeof(Pod_motionState_t));
    MBT_TIMER_DISABLE(s_Pod_motionState.activityTimer);
    MBT_TIMER_DISABLE(s_Pod_motionState.awareTimer);
    //MBT_TIMER_DISABLE(s_Pod_motionState.tapTimer);
    //MBT_TIMER_DISABLE(s_Pod_motionState.interTapTimer);
    _Pod_motionAccelInit();    
}

static void _Pod_motionSetWantedLevel(Pod_motionAccelLevel_t lvl)
{
    if(s_Pod_motionState.wantedLevel != lvl)
    {
        s_Pod_motionState.wantedLevel = lvl;
        HAL_sysSchedulePendingTick();
    }
}

static void _Pod_motionPwrMgmt(uint32_t ticks)
{
    Pod_motionAccelLevel_t want = ACCEL_LEVEL_2;

    if(s_Pod_motionState.busyAlgorithms)
    {
        want = ACCEL_LEVEL_2;
    } else if(s_Pod_motionState.subStream.enabled)
    {
        //want = ACCEL_LEVEL_1;        
        want = ACCEL_LEVEL_2;
    } else if(MBT_TIMER_ISENABLED(s_Pod_motionState.awareTimer)) {
        //want = ACCEL_LEVEL_IDLE;
        want = ACCEL_LEVEL_2;
    } else {
        want = MIN(ACCEL_LEVEL_DORMANT, s_Pod_motionState.minLevel);
    }
    
    _Pod_motionSetWantedLevel(want);

    Pod_motionAccelLevel_t setLevel =
        MAX(s_Pod_motionState.wantedLevel, s_Pod_motionState.minLevel);
    
    if(want > ACCEL_LEVEL_IDLE)
        Pod_powerSetActivity(PWR_ACT_MOTION);
    else
        Pod_powerClearActivity(PWR_ACT_MOTION);

    _Pod_motionAccelDoSetLevel(setLevel);
}

void Pod_motionSetMinimumLevel(Pod_motionAccelLevel_t lvl)
{
    s_Pod_motionState.minLevel = lvl;
    HAL_sysSchedulePendingTick();
}

void Pod_motionTick(uint32_t ticks)
{
    // Expired? -> Set accel level to IDLE
    const Pod_motionEventData_t e_act =
    {
        .type = MET_ACTIVITY,
        .data =
        {
            .activity = { false }
        }
    };

    const Pod_motionEventData_t e_aw =
    {
        .type = MET_AWARENESS,
        .data =
        {
            .awareness = { false }
        }
    };

    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_motionState.activityTimer, ticks,
        {
            Pod_motionEventHandler(&e_act);
        }
    );

    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_motionState.awareTimer, ticks,
        {
            MBT_LOG("[MTN] NOT AWARE\n");
            Pod_motionEventHandler(&e_aw);
        }
    );

    _Pod_motionPwrMgmt(ticks);

#if 0
    static uint32_t t = 0;
    static uint32_t lev = 0;
    t += ticks;
    if(t >= MBT_US_TO_TICKS(5000000))
    {
        if(++lev == ACCEL_LEVEL_COUNT)
        {
            lev = ACCEL_LEVEL_DORMANT;
        }
        MBT_LOG("LEV = %d\n", lev);
        t = 0;
        _Pod_motionAccelSetLevel(lev);
    }
#endif

    HAL_accelApplyConfig();
}

void Pod_motionEnableAlgorithm(uint8_t id)
{
    MBT_ASSERT(id < POD_MOTION_ALGORITHMS_NUM);
    MBT_FLAG_SET(s_Pod_motionState.algorithms, (1 << id));
    s_podMotionAlgs[id].pReset();
}

void Pod_motionDisableAlgorithm(uint8_t id)
{
    MBT_FLAG_CLEAR(s_Pod_motionState.algorithms, (1 << id));
}

bool Pod_motionIsActivity()
{
    return MBT_TIMER_ISENABLED(s_Pod_motionState.activityTimer);
}

static void _Pod_motionOnActivityInterrupt()
{
    if(!MBT_TIMER_ISENABLED(s_Pod_motionState.activityTimer))
    {
        const Pod_motionEventData_t e =
        {
            .type = MET_ACTIVITY,
            .data =
            {
                .activity = { true }
            }
        };
        
        Pod_motionEventHandler(&e);
    }

    MBT_TIMER_RESET_TIMEOUT(s_Pod_motionState.activityTimer, POD_MOTION_INACTIVITY_TIMEOUT_TICKS);

    if(!MBT_TIMER_ISENABLED(s_Pod_motionState.awareTimer))
    {
        const Pod_motionEventData_t e =
        {
            .type = MET_AWARENESS,
            .data =
            {
                .awareness = { true }
            }
        };
        
        Pod_motionEventHandler(&e);
        MBT_LOG("[MTN] AWARE\n");
    }

    MBT_TIMER_RESET_TIMEOUT(s_Pod_motionState.awareTimer, POD_MOTION_AWARE_TIMEOUT_TICKS);
}

bool Pod_motionIsAware(void)
{
    return MBT_TIMER_ISENABLED(s_Pod_motionState.awareTimer);
}

void Pod_motionMakeAware(void)
{
    _Pod_motionOnActivityInterrupt();
}

static void _Pod_motionFeedStream(const HAL_accelSample_t* buffer, uint16_t size_samples)
{
    HAL_accelSampleScaled_t diff = {0};    
    HAL_accelSampleScaled_t unfilteredDiff = {0};    
    
    for(uint16_t i = 0; i < size_samples; ++i)
    {        
        HAL_accelSample_t cur = buffer[i];

        HAL_accelSampleScaled_t scaled;
        HAL_accelSampleScaled_t unfilteredScaled; //
        HAL_accelScaleSample(&cur, &scaled);
        
        unfilteredScaled = scaled;

        // Smooth
        {
                s_Pod_motionState.global.scaledRawSmmpleBuffer[s_Pod_motionState.global.scaledRawSampleBufferIndex++ % POD_MOTION_AVG_WINDOW_LEN] = scaled;
        
                memset(&scaled, 0, sizeof(scaled)); // zero
                
                for(uint8_t j = 0; j < POD_MOTION_AVG_WINDOW_LEN; ++j)
                {
                    scaled.x += s_Pod_motionState.global.scaledRawSmmpleBuffer[j].x;
                    scaled.y += s_Pod_motionState.global.scaledRawSmmpleBuffer[j].y;
                    scaled.z += s_Pod_motionState.global.scaledRawSmmpleBuffer[j].z;
                }

                scaled.x /= POD_MOTION_AVG_WINDOW_LEN;
                scaled.y /= POD_MOTION_AVG_WINDOW_LEN;
                scaled.z /= POD_MOTION_AVG_WINDOW_LEN;
        }       

        // Push back the sample
        s_Pod_motionState.global.scaledSmmpleBuffer[s_Pod_motionState.global.scaledSampleBufferIndex] = scaled;
        ++ s_Pod_motionState.global.scaledSampleBufferIndex;
        s_Pod_motionState.global.scaledSampleBufferIndex %= POD_MOTION_SAMPLE_BUFFER_LEN;

        //MBT_LOG("%d %d %d\n", _c.y, min, max);
        
        if(MBT_FLAG_IS_SET(s_Pod_motionState.flags, POD_MOTION_FLAGS_FIRSTSAMPLE))
        {
            s_Pod_motionState.global.scaledLastSample = scaled;
            s_Pod_motionState.global.scaledLengthDiff = 0;

            s_Pod_motionState.global.unfilteredScaledLastSample = unfilteredScaled; //
            memset(&s_Pod_motionState.global.unfilteredScaledDiff, 0, sizeof(s_Pod_motionState.global.unfilteredScaledDiff)); //
                        
            //HAL_accelSampleScaled_t zero = {0};
            //s_Pod_motionState.global.filteredScaledSample = zero;
            
            MBT_FLAG_CLEAR(s_Pod_motionState.flags, POD_MOTION_FLAGS_FIRSTSAMPLE);
        } else {
            diff.x = scaled.x - s_Pod_motionState.global.scaledLastSample.x;
            diff.y = scaled.y - s_Pod_motionState.global.scaledLastSample.y;
            diff.z = scaled.z - s_Pod_motionState.global.scaledLastSample.z;

            s_Pod_motionState.global.scaledDiff = diff;

            unfilteredDiff.x = unfilteredScaled.x - s_Pod_motionState.global.unfilteredScaledLastSample.x; //
            unfilteredDiff.y = unfilteredScaled.y - s_Pod_motionState.global.unfilteredScaledLastSample.y; //
            unfilteredDiff.z = unfilteredScaled.z - s_Pod_motionState.global.unfilteredScaledLastSample.z; //

            s_Pod_motionState.global.unfilteredScaledDiff = unfilteredDiff; //

            uint32_t prevLength = s_Pod_motionState.global.scaledLength;
            s_Pod_motionState.global.scaledLength = Utils_getApproximatedDistance3D(diff.x, diff.y, diff.z);

            s_Pod_motionState.global.scaledLengthDiff = (s_Pod_motionState.global.scaledLength - prevLength);

            // Soft activity detection
            if(s_Pod_motionState.global.scaledLengthDiff >= MBT_CFG_MOTION_INACTIVITY_DEADZONE_mG)
            {
                _Pod_motionOnActivityInterrupt();
            }
#if 0
            {
                int32_t meter = s_Pod_motionState.global.scaledLength >> 5;
                //int32_t meter = diff.y >> 6;

                /*
                for(int32_t j = -32; j <=0; ++j)
                {
                    if(j < meter)
                    {
                        MBT_LOG(" ");
                    } else {
                        MBT_LOG("=");
                    }
                }*/

                for(int32_t j = 1; j <=32; ++j)
                {
                    if(j < meter)
                    {
                        MBT_LOG("=");
                    } else {
                        MBT_LOG(" ");
                    }
                }
                MBT_LOG("%d\n", s_Pod_motionState.global.scaledLength);
            }
#endif
            // Tick enabled motion detection algorithms
            for(uint8_t j = 0; j < POD_MOTION_ALGORITHMS_NUM; ++j)
            {
                if(MBT_FLAG_IS_SET(s_Pod_motionState.algorithms, (1 << j)))
                    s_podMotionAlgs[j].pUpdate();
            }

            s_Pod_motionState.global.scaledLastSample = scaled;
        }

        // Handle sub stream
        if(s_Pod_motionState.subStream.enabled)
        {
            if(++s_Pod_motionState.subStream.prescalerCount >= s_Pod_motionState.subStream.prescalerMax)
            {
                s_Pod_motionState.subStream.prescalerCount = 0;
                Pod_motionsubStreamHandler(&cur, 1);
            }
        }
    }
}

void Pod_motionHALEventHandler(const HAL_event_t *pEvent)
{
    switch(pEvent->data.accelEvent.type)
    {
        case HALACCELEVENTTYPE_DATA:
        {
            //MBT_LOG("*");
            HAL_accelSample_t sample;
            
            while(HAL_accelPullSample(&sample))
            {
                _Pod_motionFeedStream(&sample, 1);
            }            
            
            break;
        }
        case HALACCELEVENTTYPE_MOTION:
        {
            _Pod_motionOnActivityInterrupt();
            break;    
        }        
        case HALACCELEVENTTYPE_TAP:
        {
            // TODO: Fire tap event
//            if(!MBT_TIMER_ISENABLED(s_Pod_motionState.tapTimer))
//            {
//                MBT_TIMER_RESET_TIMEOUT(s_Pod_motionState.tapTimer, MBT_US_TO_TICKS(200*1000));
//                if(!s_Pod_motionAlgShakeData.isShaking)
//                    MBT_LOG(">> TAP!\n");
//            }

            break;    
        }
        case HALACCELEVENTTYPE_DBLTAP:
        {
            // TODO: Fire dbltap event
//            if(!s_Pod_motionAlgShakeData.isShaking)
//                MBT_LOG(">> DBLTAP!\n");

            break;    
        }
    }
}

void Pod_motionSetSubStreamEnabled(bool enabled)
{
    s_Pod_motionState.subStream.enabled = enabled;
    s_Pod_motionState.subStream.prescalerCount = (uint16_t)(-1);
}

