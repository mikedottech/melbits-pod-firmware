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

#include "SC7A20.h"
#include "string.h"
#include "app_util_platform.h"
#include "mbt_config.h"
#include "HAL/HAL.h"
#include "HAL/Debug.h"
#include <nrf_queue.h>

NRF_QUEUE_DEF(SC7A20_sample_t, accelSampleQueue, 32, NRF_QUEUE_MODE_NO_OVERFLOW);
NRF_QUEUE_INTERFACE_DEF(SC7A20_sample_t, accelSampleQueue, &accelSampleQueue);

extern const nrf_twi_mngr_t *g_pnrfTwiMngr;

    //0x15 //(336 mG) @ +-2G
    //0x05 // 200 ms @25 Hz // 12 ms @ 400 Hz
    //0x34 // 2.08 s @25 Hz // 130 ms @400 Hz
    //0x7f // 5.08 s @25 Hz // 317 ms @400 Hz

static const SC7A20_reg_t s_initRegs[SC7A20_CACHED_REGS_SIZE] =
{
    { CTRL_REG1, 0 },                   { CTRL_REG2, 0 },
    { CTRL_REG3, 0 },                   { CTRL_REG4, 0 },
    { CTRL_REG5, CTRL_REG5_AOI1_LIR },  { CTRL_REG6, 0 },
    { REFERENCE, 0 },                   { FIFO_CTRL_REG, 0},
    { AOI1_CTRL_REG, 0 },               { AOI1_THS, 0 },
    { AOI1_DURATION, 0 },               { AOI2_CTRL_REG, 0 },
    { AOI2_THS, 0 },                    { AOI2_DURATION, 0 },
    { CLICK_CTRL_REG, 0 },              { CLICK_THS, 0},
    { TIME_LIMIT, 0 },                  { TIME_LATENCY, 0},        
    { TIME_WINDOW, 0}
};

#ifdef MBT_CFG_ACCEL_SC7A20_USE_INT1_FOR_MOTION
#   define MOTIONEVT_INT_REG   CACHE_OFFSET_REG3
#   define MOTIONEVT_INT_BIT   CTRL_REG3_I1_AOI1
#   define CLICKEVT_INT_BIT    CTRL_REG3_I1_CLICK
#else
#   define MOTIONEVT_INT_REG   CACHE_OFFSET_REG6
#   define MOTIONEVT_INT_BIT   CTRL_REG6_I2_AOI1
#   define CLICKEVT_INT_BIT    CTRL_REG6_I2_CLICK
#endif

static uint8_t readIntsRegAddrs[3] = 
{
    AOI1_SRC,
    CLICK_SRC,
    //STATUS_REG // FIFO?
};


static uint8_t readStatusRegAddr = STATUS_REG;
static uint8_t readXYZRegAddr = (OUT_X_L | 0x80);

static void _isr_sc7a20_read_ints_cb(ret_code_t result, void * p_user_data);
static void _isr_sc7a20_read_xyz_cb(ret_code_t result, void * p_user_data);
static void _isr_sc7a20_read_status_cb(ret_code_t result, void * p_user_data);

void sc7a20_initStructs(SC7A20_t* s)
{
    {
        SC7A20_READ_STORAGE_IMP(readInts) =
        {
            .xfer = {
                NRF_TWI_MNGR_WRITE(s->dev_addr, (&readIntsRegAddrs[0]), 1, NRF_TWI_MNGR_NO_STOP),
                NRF_TWI_MNGR_READ(s->dev_addr, (&s->readRegs.aoi1), 1, 0),
                NRF_TWI_MNGR_WRITE(s->dev_addr, (&readIntsRegAddrs[1]), 1, NRF_TWI_MNGR_NO_STOP),
                NRF_TWI_MNGR_READ(s->dev_addr, (&s->readRegs.click), 1, 0),
//                NRF_TWI_MNGR_WRITE(s->dev_addr, (&readIntsRegAddrs[2]), 1, NRF_TWI_MNGR_NO_STOP),
//                NRF_TWI_MNGR_READ(s->dev_addr, (&s->readRegs.status), 1, 0)
            },
            .xaction = {
                .p_user_data = (void*)s,
                .callback = _isr_sc7a20_read_ints_cb,
                .number_of_transfers = 4//6
            }
        };
    
        s->readInts = readInts;
        s->readInts.xaction.p_transfers = &s->readInts.xfer[0];        
    }

    {
        SC7A20_READ_STORAGE_IMP(readXYZ) =
        {
            .xfer = {
                NRF_TWI_MNGR_WRITE(s->dev_addr, (&readXYZRegAddr), 1, NRF_TWI_MNGR_NO_STOP),
                NRF_TWI_MNGR_READ(s->dev_addr, (&s->sampleBufferBack), 6, 0),
            },
            .xaction = {
                .p_user_data = (void*)s,
                .callback = _isr_sc7a20_read_xyz_cb,
                .number_of_transfers = 2
            }
        };
    
        s->readXYZ = readXYZ;
        s->readXYZ.xaction.p_transfers = &s->readXYZ.xfer[0];
    }

    {
        SC7A20_READ_STORAGE_IMP(readStatus) =
        {
            .xfer = {
                NRF_TWI_MNGR_WRITE(s->dev_addr, (&readStatusRegAddr), 1, NRF_TWI_MNGR_NO_STOP),
                NRF_TWI_MNGR_READ(s->dev_addr, (&s->readRegs.status), 1, 0),
            },
            .xaction = {
                .p_user_data = (void*)s,
                .callback = _isr_sc7a20_read_status_cb,
                .number_of_transfers = 2
            }
        };
    
        s->readStatus = readStatus;
        s->readStatus.xaction.p_transfers = &s->readStatus.xfer[0];
    }
}

void sc7a20_forceReset(SC7A20_t *s)
{
    memcpy((void*)&(s->cachedRegs[0]), &(s_initRegs[0]), sizeof(s->cachedRegs));
    s->dirtyFlag            = CACHE_DIRTY_ALL;
    s->outRateMultiplier    = 1;
    sc7a20_initStructs(s);
    sc7a20_commitWrites(s);
    while(s->isBufferLocked);
}

uint32_t sc7a20_init(SC7A20_t* s, uint8_t dev_addr)
{
    memset(s, 0, sizeof(SC7A20_t));
    s->dev_addr = dev_addr;
    sc7a20_forceReset(s);
    uint32_t err = 0;
    static const uint8_t k_startReadOps[] = {SC7A20_READ_OP_INTS, SC7A20_READ_OP_STATUS};
    for(uint8_t i = 0; i < sizeof(k_startReadOps) / sizeof(uint8_t); ++i)
    {
        err = sc7a20_readOperation(s, k_startReadOps[i]);
        if(err != 0) return err;
    }

    return 0;
}


inline static void sc7a20_setDirty(SC7A20_t* s, uint32_t mask)
{
    s->dirtyFlag |= mask;
    //s->dirtyFlag |= CACHE_DIRTY_ALL;
}

static void sc7a20_updateActivityDependentParams(SC7A20_t* s)
{
    s->cachedRegs[CACHE_OFFSET_AOI1_THS].val        = SC7A20_MG_TO_THS(s->cached_fs, s->cached_act_threshold_mg);
    s->cachedRegs[CACHE_OFFSET_AOI1_DURATION].val   = SC7A20_MS_TO_DUR(s->cached_odr, s->cached_lp, s->cached_act_duration_ms);
    sc7a20_setDirty(s, (uint32_t)((1UL << CACHE_OFFSET_AOI1_THS) | (1UL << CACHE_OFFSET_AOI1_DURATION)));
}

static void sc7a20_updateClickDependentParams(SC7A20_t* s)
{    
    s->cachedRegs[CACHE_OFFSET_CLICK_THS].val       = SC7A20_MG_TO_THS(s->cached_fs, s->cached_click_threshold_mg);
    s->cachedRegs[CACHE_OFFSET_TIME_LIMIT].val      = SC7A20_MS_TO_DUR(s->cached_odr, s->cached_lp, s->cached_click_time_limit_ms);
    s->cachedRegs[CACHE_OFFSET_TIME_LATENCY].val    = SC7A20_MS_TO_DUR(s->cached_odr, s->cached_lp, s->cached_click_latency_ms);
    s->cachedRegs[CACHE_OFFSET_TIME_WINDOW].val     = SC7A20_MS_TO_DUR(s->cached_odr, s->cached_lp, s->cached_click_time_window_ms);

    sc7a20_setDirty(s, (uint32_t)((1UL << CACHE_OFFSET_CLICK_THS) | (1UL << CACHE_OFFSET_TIME_LIMIT) |
                (1UL << CACHE_OFFSET_TIME_LATENCY) | (1UL << CACHE_OFFSET_TIME_WINDOW)));
}

void sc7a20_stop(SC7A20_t* s)
{
    s->cachedRegs[CACHE_OFFSET_REG1].val &= ~CTRL_REG1_ODR_MASK;
    s->cachedRegs[CACHE_OFFSET_REG1].val |= CTRL_REG1_ODR_POWEROFF;    
    sc7a20_setDirty(s, (uint32_t)(1UL << CACHE_OFFSET_REG1));
}

void sc7a20_start(SC7A20_t* s, const SC7A20_mode_t *mode)
{
    s->cachedRegs[CACHE_OFFSET_REG1].val = (mode->rate | mode->axis | mode->lowpower);
    s->cachedRegs[CACHE_OFFSET_REG4].val = (mode->range | CTRL_REG4_BDU | CTRL_REG4_DLPF_EN);
    s->cached_lp = mode->lowpower;
    if(mode->rate > s->cached_odr) s->needsODRFlush = true;
    s->cached_odr = mode->rate;
    if(mode->rate >= CTRL_REG1_ODR_NORMAL_LP_100HZ)
        nrf_atomic_flag_set(&s->useDelayedDataEvents);
    else
        nrf_atomic_flag_clear(&s->useDelayedDataEvents);
    s->cached_fs = mode->range;
    sc7a20_updateActivityDependentParams(s);
    sc7a20_updateClickDependentParams(s);
    sc7a20_setDirty(s, (uint32_t)((1UL << CACHE_OFFSET_REG1) | (1UL << CACHE_OFFSET_REG4)));
}

void sc7a20_setHPF(SC7A20_t* s, const SC7A20_filter_t *filter)
{   
    s->cachedRegs[CACHE_OFFSET_REG2].val = filter ? (filter->target | filter->mode | filter->cutoff | filter->ref) : 0;
    sc7a20_setDirty(s, (uint32_t)(1UL << CACHE_OFFSET_REG2));
}

//void sc7a20_setRange(SC7A20_t* s, uint8_t range)
//{
//    s->cachedRegs[CACHE_OFFSET_REG4].val = (range | CTRL_REG4_BDU | CTRL_REG4_DLPF_EN);
//    s->cached_fs = range;
//    sc7a20_updateActivityDependentParams(s);
//    sc7a20_updateClickDependentParams(s);
//    sc7a20_setDirty(s, (uint32_t)(1UL << CACHE_OFFSET_REG4));
//}

void sc7a20_setDataReporting(SC7A20_t* s, bool enabled)
{
    uint8_t v = s->cachedRegs[CACHE_OFFSET_REG3].val;
    if(enabled)
    {
        v |= CTRL_REG3_I1_DRDY1;
    } else {
        v &= ~(CTRL_REG3_I1_DRDY1);
    }
    s->cachedRegs[CACHE_OFFSET_REG3].val = v;

    sc7a20_setDirty(s, (uint32_t)(1UL << CACHE_OFFSET_REG3));
}

void sc7a20_setActivityInterrupt(SC7A20_t* s, const SC7A20_activityDetectionConfig_t *a)
{
    // USE AOI1 with INT2
    if(!a)
    {
        s->cachedRegs[MOTIONEVT_INT_REG].val &= ~(MOTIONEVT_INT_BIT);
        s->cachedRegs[CACHE_OFFSET_AOI1_CTRL_REG].val = 0;
        sc7a20_setDirty(s, (uint32_t)((1UL << MOTIONEVT_INT_REG) | (1UL << CACHE_OFFSET_AOI1_CTRL_REG)));
    } else {
        s->cached_act_duration_ms   = a->duration_ms;
        s->cached_act_threshold_mg  = a->threshold_mg;

        s->cachedRegs[MOTIONEVT_INT_REG].val |= MOTIONEVT_INT_BIT;
        
        // Set AOI1 event config        
        s->cachedRegs[CACHE_OFFSET_AOI1_CTRL_REG].val &= ~(AOI1_CTRL_REG_AOI | AOI1_CTRL_REG_XLIE | AOI1_CTRL_REG_XHIE | AOI1_CTRL_REG_YLIE | AOI1_CTRL_REG_YHIE | AOI1_CTRL_REG_ZLIE | AOI1_CTRL_REG_ZHIE);
        s->cachedRegs[CACHE_OFFSET_AOI1_CTRL_REG].val |= a->axis;
        sc7a20_updateActivityDependentParams(s);
        
        sc7a20_setDirty(s, (uint32_t)((1UL << MOTIONEVT_INT_REG) | (1UL << CACHE_OFFSET_AOI1_CTRL_REG)));
    }
}

void sc7a20_setClickInterrupt(SC7A20_t* s, const SC7A20_clickDetectionConfig_t* c)
{
    // USE INT2
    if(!c)
    {
        s->cachedRegs[MOTIONEVT_INT_REG].val &= ~(CLICKEVT_INT_BIT);        
        s->cachedRegs[CACHE_OFFSET_CLICK_CTRL_REG].val = 0;
        sc7a20_setDirty(s, (uint32_t)((1UL << MOTIONEVT_INT_REG) | (1UL << CACHE_OFFSET_CLICK_CTRL_REG)));
    } else {
        s->cachedRegs[MOTIONEVT_INT_REG].val |= CLICKEVT_INT_BIT;
        
        // Set click event config        
        s->cachedRegs[CACHE_OFFSET_CLICK_CTRL_REG].val  = c->axis;
        s->cached_click_threshold_mg  = c->threshold_mg;
        s->cached_click_time_limit_ms = c->time_limit_ms;
        s->cached_click_latency_ms    = c->time_latency_ms;
        s->cached_click_time_window_ms= c->time_window_ms;
        sc7a20_updateClickDependentParams(s);
        sc7a20_setDirty(s, (uint32_t)((1UL << MOTIONEVT_INT_REG) | (1UL << CACHE_OFFSET_CLICK_CTRL_REG)));
    }
}

// Simple stack allocator
static uint8_t *sc7a20_allocFromBuffer(SC7A20_t* s, uint8_t size)
{
    uint8_t newIndex = s->allocIndex + size;
    if(newIndex < SC7A20_BUFFER_SIZE)
    {
        uint8_t *ret = &(s->buffer[s->allocIndex]);
        s->allocIndex = newIndex;
        return ret;
    }
    return NULL;
}

static void sc7a20_allocFreeAll(SC7A20_t* s)
{
    s->allocIndex = 0;
}

static void sc7a20_freeAllTransferSlots(SC7A20_t* s)
{
    s->transferIdx = 0;
}

static nrf_twi_mngr_transfer_t* sc7a20_getNewTransferSlot(SC7A20_t* s)
{
    if(s->transferIdx < SC7A20_BUFFER_SIZE)
    {
        return &(s->transfers[s->transferIdx ++]);
        
    }
    return NULL;
}

static void write_all_cb(ret_code_t result, void * p_user_data)
{
    SC7A20_t *s = (SC7A20_t*)p_user_data;
    if (result != NRF_SUCCESS)
    {
        //printf("WRITE - error: %d", (int)result);
        return;
    }    
    nrf_atomic_flag_clear_fetch(&s->isBufferLocked);
    //printf("WRITE OK!\n");
}

uint32_t sc7a20_commitWrites(SC7A20_t* s)
{
    if(s->dirtyFlag == 0)
    {
        return 0;
    }

    if(nrf_atomic_flag_set_fetch(&s->isBufferLocked))
    {
        return 1;
    }

    s->isBufferLocked = true;

    // Alloc writes in buffer

    sc7a20_allocFreeAll(s);
    sc7a20_freeAllTransferSlots(s);
    uint8_t nRegs = 0;



    void *regDataSrc = NULL;
    int32_t iStart = s->needsODRFlush ? -1 : 0;
    s->needsODRFlush = false;

    for(int32_t i = iStart; i < SC7A20_CACHED_REGS_SIZE; ++i)
    {
        if(i == -1 || !!(s->dirtyFlag & (1UL << i)))
        {
            // Low to high ODR?
            if(i == -1)
            {
                static const SC7A20_reg_t kODRFlush =
                {
                    .reg = CTRL_REG1,
                    .val = 0x97
                };
                regDataSrc = (void *)&kODRFlush;
            } else {
                regDataSrc = &(s->cachedRegs[i]);
            }

            uint8_t *ptr = sc7a20_allocFromBuffer(s, sizeof(SC7A20_reg_t));
        
            if(!ptr)
            {
                return 1;
            }

            memcpy((void*)ptr, regDataSrc, sizeof(SC7A20_reg_t));

            nrf_twi_mngr_transfer_t *t1 = sc7a20_getNewTransferSlot(s);
            if(t1 == NULL) return 1;
            nrf_twi_mngr_transfer_t tt1 = NRF_TWI_MNGR_WRITE(s->dev_addr, ptr, 2, 0);
                    
            *t1 = tt1;
            ++nRegs;  
        }
    }

    s->dirtyFlag = 0;

    static nrf_twi_mngr_transaction_t NRF_TWI_MNGR_BUFFER_LOC_IND transaction =
    {
        .callback            = write_all_cb,
    };

    transaction.p_user_data         = s;
    transaction.p_transfers         = &(s->transfers[0]);
    transaction.number_of_transfers = nRegs;

    return nrf_twi_mngr_schedule(g_pnrfTwiMngr, &transaction);    
}

void sc7a20_read(SC7A20_t* s, uint8_t regOffset, uint8_t len)
{
    s->rDirtyFlag |= (1UL << regOffset);
}

void sc7a20_scaleSample(SC7A20_t* s, const SC7A20_sample_t* smp, HAL_accelSampleScaled_t* dat)
{
    static const int32_t scaleFactors[4] = {65536 >> 2,  65536 >> 3, 65536 >> 4, 65536 >> 5};
    int32_t scaleFactor = scaleFactors[(s->cached_fs >> CTRL_REG4_FS_POS)];
    
    dat->x = (int32_t)((smp->x * 1000) / scaleFactor);
    dat->y = (int32_t)((smp->y * 1000) / scaleFactor);
    dat->z = (int32_t)((smp->z * 1000) / scaleFactor);
}

static void _isr_sc7a20_read_xyz_cb(ret_code_t result, void * p_user_data)
{
    SC7A20_t *s = (SC7A20_t*)p_user_data;
    
    HAL_sysSchedulePendingTick();

    nrf_atomic_u32_and(&(s->readOps), ~(1UL << SC7A20_READ_OP_XYZ));
    
    if (result != NRF_SUCCESS)
    {
        //printf("READ - error: %d", (int)result);
        return;
    }

    size_t sz = sizeof(SC7A20_sample_t);
    

    
    ret_code_t err = accelSampleQueue_push((SC7A20_sample_t *)&s->sampleBufferBack);
    if(err != 0)
    {
    }
    MBT_ASSERT(err == 0);

//    if((++s->nSamples % s->outRateMultiplier) != 0)
//    {
//        return;
//    }
        
    if(s->useDelayedDataEvents)
    {
        if(accelSampleQueue_utilization_get() < 16)
            return;
    }

    nrf_atomic_u32_or(&(s->eventsFlag), SC7A20_EVTFLAG_DATA);
}

static void _isr_sc7a20_read_ints_cb(ret_code_t result, void * p_user_data)
{
    SC7A20_t *s = (SC7A20_t*)p_user_data;

    HAL_sysSchedulePendingTick();

    nrf_atomic_u32_and(&(s->readOps), ~(1UL << SC7A20_READ_OP_INTS));
    
    if (result != NRF_SUCCESS)
    {
        //printf("READ - error: %d", (int)result);
        return;
    }

    uint32_t ormask = 0;

    // Check the interrupt source
    if(s->readRegs.aoi1 & (AOI1_SRC_XH | AOI1_SRC_YH | AOI1_SRC_ZH))
    {
        //printf("MOTION\n");
        ormask |= SC7A20_EVTFLAG_MOTION;
    }
    if(s->readRegs.click & CLICK_SRC_SCLICK)
    {
        //printf("CLICK\n");
        ormask |= SC7A20_EVTFLAG_CLICK;
    }
    if(s->readRegs.click & CLICK_SRC_DCLICK)
    {
        //printf("DBLCLICK\n");
        ormask |= SC7A20_EVTFLAG_DBLCLICK;        
    }
    nrf_atomic_u32_or(&(s->eventsFlag), ormask);
}

static void _isr_sc7a20_read_status_cb(ret_code_t result, void *p_user_data)
{
    SC7A20_t *s = (SC7A20_t*)p_user_data;

    HAL_sysSchedulePendingTick();

    nrf_atomic_u32_and(&(s->readOps), ~(1UL << SC7A20_READ_OP_STATUS));
    
    if (result != NRF_SUCCESS)
    {
        //printf("READ - error: %d", (int)result);
        return;
    }

    // Check the interrupt source
    if(s->readRegs.status & STATUS_REG_ZYXDA)
    {
        //printf("DATA INTERRUPT\n");
        sc7a20_readOperation(s, SC7A20_READ_OP_XYZ);
    }
}

uint32_t sc7a20_readOperation(SC7A20_t* s, uint8_t id)
{
    uint32_t msk = (1UL << id);

    if(nrf_atomic_u32_fetch_or(&(s->readOps), msk) & msk)
    {
        return 1;
    }
    
    nrf_twi_mngr_transaction_t  *xaction;
    
    switch(id)
    {
        case SC7A20_READ_OP_XYZ:
            xaction = &(s->readXYZ.xaction);
        break;
        case SC7A20_READ_OP_INTS:
            xaction = &(s->readInts.xaction);
        break;
        case SC7A20_READ_OP_STATUS:
            xaction = &(s->readStatus.xaction);
        break;
    }

    return nrf_twi_mngr_schedule(g_pnrfTwiMngr, xaction);
}

bool sc7a20_pullSample(SC7A20_t* s, SC7A20_sample_t* smp)
{    

    return (accelSampleQueue_pop(smp) == 0);
/*
    SC7A20_sample_t ret = {0};
    size_t size = sizeof(SC7A20_sample_t);
    uint8_t* pData;
    uint8_t* pRet = (uint8_t*)&ret;
    size_t missingSize = size;
    do
    {        
        ret_code_t r = nrf_ringbuf_get(&s->sampleRing, &pData, &size, true);
        MBT_ASSERT(r == 0);
        if(size == 0)
            return false;
        memcpy(pRet, pData, size);
        nrf_ringbuf_free(&s->sampleRing, size);
        missingSize -= size;
        size = missingSize;
        pRet += size;
    } while(missingSize > 0);

    *smp = ret;
    return true;
    */
}

//void sc7a20_getLastScaledSample(SC7A20_t* s, HAL_accelSampleScaled_t* smp)
//{
//    SC7A20_sample_t tmp;
//    sc7a20_getLastSample(s, &tmp);    
//    sc7a20_scaleSample(s, &tmp, smp);
//}

uint32_t sc7a20_getEventsFlags(SC7A20_t* s)
{
    return nrf_atomic_u32_fetch_store(&(s->eventsFlag), 0);
}

static void sc7a20_readWHOAMI(SC7A20_t* s)
{
}

void sc7a20_setOutputRateMultiplier(SC7A20_t *s, uint8_t m)
{
    s->outRateMultiplier = m;
}

void _isr_sc7a20_io_event_handler(SC7A20_t *s, uint8_t irqs)
{
    if(irqs & SC7A20_IRQ_INT1)
    {
        //printf("INT1\n");
        sc7a20_readOperation(s, SC7A20_READ_OP_STATUS);
#ifdef MBT_CFG_ACCEL_SC7A20_USE_INT1_FOR_MOTION
        sc7a20_readOperation(s, SC7A20_READ_OP_INTS);
#endif
    }

#ifndef MBT_CFG_ACCEL_SC7A20_USE_INT1_FOR_MOTION
    if(irqs & SC7A20_IRQ_INT2)
    {
        //printf("INT2\n");
        sc7a20_readOperation(s, SC7A20_READ_OP_INTS);
    }
#endif
}
