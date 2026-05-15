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

#ifndef _SC7A20_H_
#define _SC7A20_H_

#include "nrf_twi_mngr.h"
#include "common.h"
#include "HAL/HAL.h"
#include <nrf_atomic.h>

// REGISTERS
#define OUT_TEMP_L      (0x0C)
#define OUT_TEMP_H      (0x0D)
#define WHO_AM_I        (0x0F)
#define USER_CAL        (0x13) //13-1A
#define NVM_WR          (0x1e)
#define TEMP_CFG        (0x1f)
#define CTRL_REG1       (0x20)
    #define CTRL_REG1_XEN   (0x1)
    #define CTRL_REG1_YEN   (0x2)
    #define CTRL_REG1_ZEN   (0x4)
    #define CTRL_REG1_LPEN  (0x8)
    
    #define CTRL_REG1_ODR_MASK  (0xff00)
    #define CTRL_REG1_ODR_POS   (4)
        #define CTRL_REG1_ODR_POWEROFF                  (0 << CTRL_REG1_ODR_POS)
        #define CTRL_REG1_ODR_NORMAL_LP_1HZ             (1 << CTRL_REG1_ODR_POS)
        #define CTRL_REG1_ODR_NORMAL_LP_10HZ            (2 << CTRL_REG1_ODR_POS)
        #define CTRL_REG1_ODR_NORMAL_LP_25HZ            (3 << CTRL_REG1_ODR_POS)
        #define CTRL_REG1_ODR_NORMAL_LP_50HZ            (4 << CTRL_REG1_ODR_POS)
        #define CTRL_REG1_ODR_NORMAL_LP_100HZ           (5 << CTRL_REG1_ODR_POS)
        #define CTRL_REG1_ODR_NORMAL_LP_200HZ           (6 << CTRL_REG1_ODR_POS)
        #define CTRL_REG1_ODR_NORMAL_LP_400HZ           (7 << CTRL_REG1_ODR_POS)
        #define CTRL_REG1_ODR_NORMAL_LP_1600HZ          (8 << CTRL_REG1_ODR_POS)
        #define CTRL_REG1_ODR_NORMAL_1250HZ_LP_5000HZ   (9 << CTRL_REG1_ODR_POS)

#define CTRL_REG2       (0x21)
    #define CTRL_REG2_HPIS1_EN (0x1)
    #define CTRL_REG2_HPIS2_EN (0x2)

    #define CTRL_REG2_HPCLICK (0x4)
    #define CTRL_REG2_FDS     (0x8)

    #define CTRL_REG2_HPCF_MASK  (0x30)
    #define CTRL_REG2_HPCF_POS   (4)

    #define CTRL_REG2_HPM_MASK  (0xC0)
    #define CTRL_REG2_HPM_POS   (6)
        #define CTRL_REG2_HPM_NORMAL_AUTO_RESET  (0x0 << CTRL_REG2_HPM_POS)
        #define CTRL_REG2_HPM_FILTER  (0x1 << CTRL_REG2_HPM_POS)
        #define CTRL_REG2_HPM_NORMAL  (0x2 << CTRL_REG2_HPM_POS)
        #define CTRL_REG2_HPM_AUTO_RESET  (0x3 << CTRL_REG2_HPM_POS)

#define CTRL_REG3       (0x22)
    #define CTRL_REG3_I1_OVERRUN    (0x2)
    #define CTRL_REG3_I1_WTM        (0x4)
    #define CTRL_REG3_I1_DRDY2      (0x8)
    #define CTRL_REG3_I1_DRDY1      (0x10)
    #define CTRL_REG3_I1_AOI2       (0x20)
    #define CTRL_REG3_I1_AOI1       (0x40)
    #define CTRL_REG3_I1_CLICK      (0x80)

#define CTRL_REG4       (0x23)
    #define CTRL_REG4_SIM (0x1)
    #define CTRL_REG4_ST_POS (1)
        #define CTRL_REG4_ST_NORMAL (0 << CTRL_REG4_ST_POS)
        #define CTRL_REG4_ST_SELF_TEST1 (2 << CTRL_REG4_ST_POS)
    #define CTRL_REG4_DLPF_EN (0x8)
    
    #define CTRL_REG4_FS_POS (4)
        #define CTRL_REG4_FS_2G (0 << CTRL_REG4_FS_POS)
        #define CTRL_REG4_FS_4G (1 << CTRL_REG4_FS_POS)
        #define CTRL_REG4_FS_8G (2 << CTRL_REG4_FS_POS)
        #define CTRL_REG4_FS_16G (3 << CTRL_REG4_FS_POS)

    #define CTRL_REG4_BLE (0x40)
    #define CTRL_REG4_BDU (0x80)

#define CTRL_REG5       (0x24)
    #define CTRL_REG5_AOI2_D4D       (0x1)
    #define CTRL_REG5_AOI2_LIR       (0x2)
    #define CTRL_REG5_AOI1_D4D       (0x4)
    #define CTRL_REG5_AOI1_LIR       (0x8)
    #define CTRL_REG5_FIFO_EN        (0x40)
    #define CTRL_REG5_FIFO_BOOT      (0x80)

#define CTRL_REG6       (0x25)
    #define CTRL_REG6_H_LACTIVE     (0x2)
    #define CTRL_REG6_I2_BOOT       (0x10)
    #define CTRL_REG6_I2_AOI2       (0x20)
    #define CTRL_REG6_I2_AOI1       (0x40)
    #define CTRL_REG6_I2_CLICK      (0x80)

#define REFERENCE       (0x26)
#define STATUS_REG      (0x27)
    #define STATUS_REG_XDA      (0x1)
    #define STATUS_REG_YDA      (0x2)
    #define STATUS_REG_ZDA      (0x4)
    #define STATUS_REG_ZYXDA    (0x8)
    #define STATUS_REG_XOR      (0x10)
    #define STATUS_REG_YOR      (0x20)
    #define STATUS_REG_ZOR      (0x40)
    #define STATUS_REG_ZYXOR    (0x80)

#define OUT_X_L         (0x28)
#define OUT_X_H         (0x29)
#define OUT_Y_L         (0x2a)
#define OUT_Y_H         (0x2b)
#define OUT_Z_L         (0x2c)
#define OUT_Z_H         (0x2d)

#define FIFO_CTRL_REG   (0x2e)
    #define FIFO_CTRL_REG_FTH_POS   (0)
    #define FIFO_CTRL_REG_FTH_MASK  (0x1f)
        #define FIFO_CTRL_REG_FTH_THRESHOLD(X) ((X << FIFO_CTRL_REG_FTH_POS) & FIFO_CTRL_REG_FTH_MASK)

    #define FIFO_CTRL_REG_TR   (0x20)

    #define FIFO_CTRL_REG_FM_POS     (6)
    #define FIFO_CTRL_REG_FM_MASK    (0x0c)
    #define FIFO_CTRL_REG_FM_BYPASS  (0 << FIFO_CTRL_REG_FM_POS)
    #define FIFO_CTRL_REG_FM_FIFO    (1 << FIFO_CTRL_REG_FM_POS)
    #define FIFO_CTRL_REG_FM_STREAM  (2 << FIFO_CTRL_REG_FM_POS)
    #define FIFO_CTRL_REG_FM_TRIGGER (3 << FIFO_CTRL_REG_FM_POS)


#define FIFO_SRC_REG    (0x2f)
    #define FIFO_SRC_REG_FSS_POS    (0)
    #define FIFO_SRC_REG_FSS_MASK   (0x1f)
    #define FIFO_SRC_REG_FSS(X)   ((X) & FIFO_SRC_REG_FSS_MASK)


    #define FIFO_SRC_REG_EMPTY  (0x20)
    #define FIFO_SRC_REG_OVER   (0x40)
    #define FIFO_SRC_REG_WTM    (0x80)

#define AOI1_CTRL_REG   (0x30)
    #define AOI1_CTRL_REG_XLIE   (0x1)
    #define AOI1_CTRL_REG_XHIE   (0x2)
    #define AOI1_CTRL_REG_YLIE   (0x4)
    #define AOI1_CTRL_REG_YHIE   (0x8)
    #define AOI1_CTRL_REG_ZLIE   (0x10)
    #define AOI1_CTRL_REG_ZHIE   (0x20)        
    #define AOI1_CTRL_REG_6D     (0x40)        
    #define AOI1_CTRL_REG_AOI    (0x80)        

#define AOI1_SRC        (0x31)
    #define AOI1_SRC_XL        (0x1)
    #define AOI1_SRC_XH        (0x2)
    #define AOI1_SRC_YL        (0x4)
    #define AOI1_SRC_YH        (0x8)
    #define AOI1_SRC_ZL        (0x10)
    #define AOI1_SRC_ZH        (0x20)
    #define AOI1_SRC_IA        (0x40)

#define AOI1_THS        (0x32)
#define AOI1_DURATION   (0x33)
#define AOI2_CTRL_REG   (0x34)
    #define AOI2_CTRL_REG_XLIE   (0x1)
    #define AOI2_CTRL_REG_XHIE   (0x2)
    #define AOI2_CTRL_REG_YLIE   (0x4)
    #define AOI2_CTRL_REG_YHIE   (0x8)
    #define AOI2_CTRL_REG_ZLIE   (0x10)
    #define AOI2_CTRL_REG_ZHIE   (0x20)        
    #define AOI2_CTRL_REG_6D     (0x40)        
    #define AOI2_CTRL_REG_AOI    (0x80)  

#define AOI2_SRC        (0x35)
    #define AOI2_SRC_XL        (0x1)
    #define AOI2_SRC_XH        (0x2)
    #define AOI2_SRC_YL        (0x4)
    #define AOI2_SRC_YH        (0x8)
    #define AOI2_SRC_ZL        (0x10)
    #define AOI2_SRC_ZH        (0x20)
    #define AOI2_SRC_IA        (0x40)

#define AOI2_THS        (0x36)
#define AOI2_DURATION   (0x37)

#define CLICK_CTRL_REG  (0x38)
    #define CLICK_CTRL_REG_XS   (0x1)
    #define CLICK_CTRL_REG_XD   (0x2)
    #define CLICK_CTRL_REG_YS   (0x4)
    #define CLICK_CTRL_REG_YD   (0x8)
    #define CLICK_CTRL_REG_ZS   (0x10)
    #define CLICK_CTRL_REG_ZD   (0x20)

#define CLICK_SRC       (0x39)
    #define CLICK_SRC_X       (0x1)
    #define CLICK_SRC_Y       (0x2)
    #define CLICK_SRC_Z       (0x4)
    #define CLICK_SRC_SIGN    (0x8)
    #define CLICK_SRC_SCLICK  (0x10)
    #define CLICK_SRC_DCLICK  (0x20)
    #define CLICK_SRC_IA      (0x40)

#define CLICK_THS       (0x3a)
#define TIME_LIMIT      (0x3b)
#define TIME_LATENCY    (0x3c)
#define TIME_WINDOW     (0x3d)
#define DIG_CTRL        (0x57)

#define SC7A20_CACHED_REGS_SIZE         ((19) + (1)) // Account for the extra reg write if ODR flush is needed

#define CACHE_OFFSET_REG1               (0)
#define CACHE_OFFSET_REG2               (1)
#define CACHE_OFFSET_REG3               (2)
#define CACHE_OFFSET_REG4               (3)
#define CACHE_OFFSET_REG5               (4)
#define CACHE_OFFSET_REG6               (5)
#define CACHE_OFFSET_REFERENCE          (6)
#define CACHE_OFFSET_FIFO_CTRL_REG      (7)
#define CACHE_OFFSET_AOI1_CTRL_REG      (8)
#define CACHE_OFFSET_AOI1_THS           (9)
#define CACHE_OFFSET_AOI1_DURATION      (10)
#define CACHE_OFFSET_AOI2_CTRL_REG      (11)
#define CACHE_OFFSET_AOI2_THS           (12)
#define CACHE_OFFSET_AOI2_DURATION      (13)
#define CACHE_OFFSET_CLICK_CTRL_REG     (14)
#define CACHE_OFFSET_CLICK_THS          (15)
#define CACHE_OFFSET_TIME_LIMIT         (16)
#define CACHE_OFFSET_TIME_LATENCY       (17)
#define CACHE_OFFSET_TIME_WINDOW        (18)


#define CACHE_DIRTY_ALL                 (0x0003ffff)

#define SC7A20_READ_OP_XYZ      (0)
#define SC7A20_READ_OP_INTS     (1)
#define SC7A20_READ_OP_STATUS   (2)

#define SC7A20_EVTFLAG_DATA     (0x1)   // New data has arrived
#define SC7A20_EVTFLAG_MOTION   (0x2)   // Motion event detected
#define SC7A20_EVTFLAG_CLICK    (0x4)   // Click event detected
#define SC7A20_EVTFLAG_DBLCLICK (0x8)   // DblClick event detected

#define SC7A20_MAX_BUS_TRANSFERS (19)

// Expands to the sampling rate in HZ from an ODR value as a macro
#define SC7A20_ODRV_TO_HZ_CONST(X, LP) ((X) == 1 ? 1 : ((X) == 2 ? 10 : ((X) == 3 ? 25 : ((X) == 4 ? 50 : ((X) == 5 ? 100 : ((X) == 6 ? 200 : ((X) == 7 ? 400 : ((X) == 8 ? 1000 : ((X) == 9 ? (LP ? 5000 : 1250) : 0xFF)))))))))

#define SC7A20_MG_TO_THS(FS, MG) (uint8_t)((MG) / (/*float*/uint32_t)(((1 << (FS >> CTRL_REG4_FS_POS)) + 1) << 3)) // MG / ((FS to G (2/4/8/1&)) * 8)

#define SC7A20_MS_TO_DUR(ODR, LP, MS) (uint8_t)((MS * SC7A20_ODRV_TO_HZ_CONST(ODR >> CTRL_REG1_ODR_POS, LP)) / 1000 /*.0*/)

typedef struct
{
    uint8_t reg;
    uint8_t val;
} __attribute__((packed)) SC7A20_reg_t;

typedef enum
{
    SC7A20_ST_IDLE,
    SC7A20_ST_R,
    SC7A20_ST_W
} SC7A20_state_t;

#define HAL_ACCEL_X_ENABLE   CTRL_REG1_XEN
#define HAL_ACCEL_Y_ENABLE   CTRL_REG1_YEN
#define HAL_ACCEL_Z_ENABLE   CTRL_REG1_ZEN
#define HAL_ACCEL_LOWPOWER   CTRL_REG1_LPEN

#define HAL_ACCEL_RATE_POWEROFF            CTRL_REG1_ODR_POWEROFF
#define HAL_ACCEL_RATE_1HZ                 CTRL_REG1_ODR_NORMAL_LP_1HZ
#define HAL_ACCEL_RATE_10HZ                CTRL_REG1_ODR_NORMAL_LP_10HZ
#define HAL_ACCEL_RATE_25HZ                CTRL_REG1_ODR_NORMAL_LP_25HZ
#define HAL_ACCEL_RATE_50HZ                CTRL_REG1_ODR_NORMAL_LP_50HZ
#define HAL_ACCEL_RATE_100HZ               CTRL_REG1_ODR_NORMAL_LP_100HZ
#define HAL_ACCEL_RATE_200HZ               CTRL_REG1_ODR_NORMAL_LP_200HZ
#define HAL_ACCEL_RATE_400HZ               CTRL_REG1_ODR_NORMAL_LP_400HZ
#define HAL_ACCEL_RATE_1600HZ              CTRL_REG1_ODR_NORMAL_LP_1600HZ
#define HAL_ACCEL_RATE_1250HZ_LP_5000HZ    CTRL_REG1_ODR_NORMAL_1250HZ_LP_5000HZ

#define HAL_ACCEL_RANGE_2G  CTRL_REG4_FS_2G
#define HAL_ACCEL_RANGE_4G  CTRL_REG4_FS_4G
#define HAL_ACCEL_RANGE_8G  CTRL_REG4_FS_8G
#define HAL_ACCEL_RANGE_16G CTRL_REG4_FS_16G

#define HAL_ACCEL_EVENT_ERROR      (0)
#define HAL_ACCEL_EVENT_DRDY       (1)
#define HAL_ACCEL_EVENT_MOTION     (2)
#define HAL_ACCEL_EVENT_CLICK      (3)

#define HAL_ACCEL_HPF_MODE_RESET_ON_REF_READ  (0)
#define HAL_ACCEL_HPF_MODE_DIFF_WITH_REF      (1)
#define HAL_ACCEL_HPF_MODE_NORMAL             (2)
#define HAL_ACCEL_HPF_MODE_RESET_ON_INTERRUPT (3)

#define HAL_ACCEL_HPF_CUTOFF_0 (0)
#define HAL_ACCEL_HPF_CUTOFF_1 (1)
#define HAL_ACCEL_HPF_CUTOFF_2 (2)
#define HAL_ACCEL_HPF_CUTOFF_3 (3)

#define HAL_ACCEL_ACTIVITY_X_LOW      AOI1_CTRL_REG_XLIE
#define HAL_ACCEL_ACTIVITY_X_HIGH     AOI1_CTRL_REG_XHIE
#define HAL_ACCEL_ACTIVITY_Y_LOW      AOI1_CTRL_REG_YLIE
#define HAL_ACCEL_ACTIVITY_Y_HIGH     AOI1_CTRL_REG_YHIE
#define HAL_ACCEL_ACTIVITY_Z_LOW      AOI1_CTRL_REG_ZLIE
#define HAL_ACCEL_ACTIVITY_Z_HIGH     AOI1_CTRL_REG_ZHIE
#define HAL_ACCEL_ACTIVITY_BOOL_AND   AOI1_CTRL_REG_AOI

#define HAL_ACCEL_HPF_TARGET_AOI1 CTRL_REG2_HPIS1_EN
#define HAL_ACCEL_HPF_TARGET_AOI2 CTRL_REG2_HPIS2_EN

#define HAL_ACCEL_HPF_TARGET_MOTION CTRL_REG2_HPIS1_EN
#define HAL_ACCEL_HPF_TARGET_CLICK CTRL_REG2_HPCLICK
#define HAL_ACCEL_HPF_TARGET_OUTPUT CTRL_REG2_FDS

#define HAL_ACCEL_CLICK_AXIS_X_SINGLE CLICK_CTRL_REG_XS
#define HAL_ACCEL_CLICK_AXIS_X_DOUBLE CLICK_CTRL_REG_XD
#define HAL_ACCEL_CLICK_AXIS_Y_SINGLE CLICK_CTRL_REG_YS
#define HAL_ACCEL_CLICK_AXIS_Y_DOUBLE CLICK_CTRL_REG_YD
#define HAL_ACCEL_CLICK_AXIS_Z_SINGLE CLICK_CTRL_REG_ZS
#define HAL_ACCEL_CLICK_AXIS_Z_DOUBLE CLICK_CTRL_REG_ZD

#define SC7A20_BUFFER_SIZE (sizeof(SC7A20_reg_t) * SC7A20_CACHED_REGS_SIZE)

#define SC7A20_IRQ_INT1 (1)
#define SC7A20_IRQ_INT2 (2)

#define MBT_CFG_ACCEL_RING_SIZE_BYTES (128) // Must be a power of two

typedef struct
{
    uint8_t rate;
    uint8_t axis;
    uint8_t lowpower;
    uint8_t range;
} SC7A20_mode_t;

typedef struct
{
    uint8_t target;
    uint8_t mode;
    uint8_t cutoff;
    uint8_t ref;
} SC7A20_filter_t;

typedef struct
{
    uint8_t axis;
    uint16_t threshold_mg;
    uint16_t duration_ms;
} SC7A20_activityDetectionConfig_t;

typedef struct
{
    uint8_t axis;
    uint16_t threshold_mg;
    uint16_t time_limit_ms;
    uint16_t time_latency_ms;
    uint16_t time_window_ms;
} SC7A20_clickDetectionConfig_t;

typedef struct
{
    int16_t x, y, z;    // Acceleration sample reading
} __attribute__((packed)) SC7A20_sample_t;

typedef struct
{
    uint8_t regAddr[3];
    uint8_t *readTarget[3];
    nrf_twi_mngr_transfer_t xfer[6];
    nrf_twi_mngr_transaction_t xaction;
} readIntsXaction;

#define SC7A20_READ_STORAGE_DEC(NAME, NREGS)\
typedef struct \
{\
    uint8_t *readTarget[NREGS];\
    nrf_twi_mngr_transfer_t xfer[NREGS*2];\
    nrf_twi_mngr_transaction_t xaction;\
} NAME##_t

#define SC7A20_READ_STORAGE_IMP(NAME)\
NAME##_t NAME

SC7A20_READ_STORAGE_DEC(readStatus, 1);
SC7A20_READ_STORAGE_DEC(readInts, 2);
SC7A20_READ_STORAGE_DEC(readXYZ, 1);

typedef struct
{
    // Full array of cached registers
    uint8_t                     buffer[SC7A20_BUFFER_SIZE];
    SC7A20_reg_t                cachedRegs[SC7A20_CACHED_REGS_SIZE];
    uint8_t                     allocIndex;

    nrf_twi_mngr_transfer_t     transfers[SC7A20_MAX_BUS_TRANSFERS];
    uint8_t                     transferIdx;

    nrf_atomic_flag_t          isBufferLocked;
    nrf_atomic_u32_t           readOps;

    volatile SC7A20_sample_t             sampleBuffer;
    volatile SC7A20_sample_t             sampleBufferBack;    
    //HALAccelerometerData        cachedScaledSample;

    SC7A20_READ_STORAGE_IMP(readStatus);
    SC7A20_READ_STORAGE_IMP(readInts);
    SC7A20_READ_STORAGE_IMP(readXYZ);

    struct
    {
        uint8_t status;
        uint8_t aoi1;
        uint8_t fifo;
        uint8_t click;
    } readRegs;

    uint32_t dirtyFlag;
    uint32_t rDirtyFlag;
    
    // I2C device address
    uint8_t dev_addr;

    // Cached parameters to update automatically the required registers
    // when changing low power mode, sampling rate or scale
    uint8_t cached_fs;
    uint8_t cached_odr;
    uint8_t cached_lp;

    uint16_t cached_click_threshold_mg;
    uint16_t cached_click_time_limit_ms;
    uint16_t cached_click_latency_ms;
    uint16_t cached_click_time_window_ms;    
    uint16_t cached_act_duration_ms;
    uint16_t cached_act_threshold_mg;

    nrfx_atomic_u32_t eventsFlag;

    volatile uint8_t outRateMultiplier;
    volatile uint8_t nSamples;
    nrfx_atomic_flag_t useDelayedDataEvents;
    
    bool needsODRFlush;
} SC7A20_t;

void sc7a20_forceReset(SC7A20_t *s);
uint32_t sc7a20_init(SC7A20_t *s, uint8_t dev_addr);
void sc7a20_start(SC7A20_t *s, const SC7A20_mode_t *mode);
void sc7a20_stop(SC7A20_t *s);
//void sc7a20_setOutputRateMultiplier(SC7A20_t *s, uint8_t m);
void sc7a20_setHPF(SC7A20_t *s, const SC7A20_filter_t *filter);
//void sc7a20_setRange(SC7A20_t *s, uint8_t range);
void sc7a20_setActivityInterrupt(SC7A20_t *s, const SC7A20_activityDetectionConfig_t *a);
void sc7a20_setClickInterrupt(SC7A20_t *s, const SC7A20_clickDetectionConfig_t* c);

uint32_t sc7a20_readOperation(SC7A20_t *s, uint8_t id);
uint32_t sc7a20_commitWrites(SC7A20_t* s);

bool sc7a20_pullSample(SC7A20_t* s, SC7A20_sample_t* smp);
//void sc7a20_getLastScaledSample(SC7A20_t* s, HAL_accelSampleScaled_t* smp);
void sc7a20_scaleSample(SC7A20_t* s, const SC7A20_sample_t* smp, HAL_accelSampleScaled_t* dat);

void sc7a20_setDataReporting(SC7A20_t* s, bool enabled);

uint32_t sc7a20_getEventsFlags(SC7A20_t* s);

#define MG_TO_REF(ODR, LP, MG) (ODRV_TO_HZ_CONST(ODR, LP) << 3)

// [NO] SetFIFO(enabled, mode, wtm)
// CalibrateOffset() // TEST MODE

// ====== ISR HANDLERS =======
// IRQ interrupt lines event handlers
void _isr_sc7a20_io_event_handler(SC7A20_t *s, uint8_t irqs);

#endif