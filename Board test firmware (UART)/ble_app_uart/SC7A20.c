#include "SC7A20.h"
#include "string.h"
#include "app_util_platform.h"

extern const nrf_twi_mngr_t *gp_nrf_twi_mngr;

//void __attribute__((weak)) _sch_accel_event_handler (void *p_event_data, uint16_t event_size)
//{}

//void __attribute__((weak)) __accel_event_handler (void *p_event_data, uint16_t event_size)
//{}

static const SC7A20_reg_t s_initRegs[SC7A20_CACHED_REGS_SIZE] =
    {
        { CTRL_REG1,
          CTRL_REG1_LPEN | CTRL_REG1_XEN | CTRL_REG1_YEN | CTRL_REG1_ZEN |
          CTRL_REG1_ODR_NORMAL_LP_400HZ
        },
        { CTRL_REG2,
          CTRL_REG2_HPCLICK | CTRL_REG2_HPIS1_EN
        },
        { CTRL_REG3,
          CTRL_REG3_I1_DRDY1
        },
        { CTRL_REG4,
          CTRL_REG4_DLPF_EN | CTRL_REG4_FS_2G | CTRL_REG4_BDU
        },
        { CTRL_REG5,
          CTRL_REG5_AOI1_LIR
        },
        { CTRL_REG6,
          CTRL_REG6_I2_AOI1 | CTRL_REG6_I2_CLICK
        },
        { REFERENCE,
          0
        },
        { FIFO_CTRL_REG,
          0
        },
        { AOI1_CTRL_REG,
          AOI1_CTRL_REG_XHIE  //?
        },
        { AOI1_THS,
          0x05
        },
        { AOI1_DURATION,
          0x04
        },
        {
          AOI2_CTRL_REG,
          0
        },
        {
          AOI2_THS,
          0
        },
        {
          AOI2_DURATION,
          0
        },
        {
          CLICK_CTRL_REG,
          CLICK_CTRL_REG_ZD | CLICK_CTRL_REG_YD | CLICK_CTRL_REG_XD
        },
        {
          CLICK_THS,
          0x15
          //MG_TO_THS(CTRL_REG4_FS_2G, 35)
        },
        {
          TIME_LIMIT,
          //MS_TO_DUR(CTRL_REG1_ODR_NORMAL_LP_400HZ, CTRL_REG1_LPEN, 84)          
          0x05
        },
        {
          TIME_LATENCY,
          0x34
        },        
        {
          TIME_WINDOW,
          0x7f
        }
    };

#ifdef SC7A20_USE_INT1_FOR_MOTION
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
                NRF_TWI_MNGR_READ(s->dev_addr, (&s->sampleBuffer), 6, 0),
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

void sc7a20_init(SC7A20_t* s, uint8_t dev_addr)
{
    memset(s, 0, sizeof(SC7A20_t));
    s->dev_addr = dev_addr;
    memcpy((void*)&(s->cachedRegs[0]), &(s_initRegs[0]), sizeof(s->cachedRegs));
    s->dirtyFlag = CACHE_DIRTY_ALL;     
    sc7a20_initStructs(s);
    while(sc7a20_commitWrites(s));    
    sc7a20_readOperation(s, SC7A20_READ_OP_INTS);
    sc7a20_readOperation(s, SC7A20_READ_OP_STATUS);
}


inline static void sc7a20_setDirty(SC7A20_t* s, uint8_t mask)
{
    s->dirtyFlag |= mask;    
}

static void sc7a20_updateActivityDependentParams(SC7A20_t* s)
{
    s->cachedRegs[CACHE_OFFSET_AOI1_THS].val        = SC7A20_MG_TO_THS(s->cached_fs, s->cached_act_threshold_mg);
    s->cachedRegs[CACHE_OFFSET_AOI1_DURATION].val   = SC7A20_MS_TO_DUR(s->cached_odr, s->cached_lp, s->cached_act_duration_ms);
    sc7a20_setDirty(s, (uint8_t)((1 << CACHE_OFFSET_AOI1_THS) | (1 << CACHE_OFFSET_AOI1_DURATION)));
}

static void sc7a20_updateClickDependentParams(SC7A20_t* s)
{    
    s->cachedRegs[CACHE_OFFSET_CLICK_THS].val       = SC7A20_MG_TO_THS(s->cached_fs, s->cached_click_threshold_mg);
    s->cachedRegs[CACHE_OFFSET_TIME_LIMIT].val      = SC7A20_MS_TO_DUR(s->cached_odr, s->cached_lp, s->cached_click_time_limit_ms);
    s->cachedRegs[CACHE_OFFSET_TIME_LATENCY].val    = SC7A20_MS_TO_DUR(s->cached_odr, s->cached_lp, s->cached_click_latency_ms);
    s->cachedRegs[CACHE_OFFSET_TIME_WINDOW].val     = SC7A20_MS_TO_DUR(s->cached_odr, s->cached_lp, s->cached_click_time_window_ms);

    sc7a20_setDirty(s, (uint8_t)((1 << CACHE_OFFSET_CLICK_THS) | (1 << CACHE_OFFSET_TIME_LIMIT) |
                (1 << CACHE_OFFSET_TIME_LATENCY) | (1 << CACHE_OFFSET_TIME_WINDOW)));
}

void sc7a20_stop(SC7A20_t* s)
{
    s->cachedRegs[CACHE_OFFSET_REG1].val &= ~CTRL_REG1_ODR_MASK;
    s->cachedRegs[CACHE_OFFSET_REG1].val |= CTRL_REG1_ODR_POWEROFF;    
    sc7a20_setDirty(s, (1 << CACHE_OFFSET_REG1));
}

void sc7a20_start(SC7A20_t* s, SC7A20_mode_t *mode)
{
    s->cachedRegs[CACHE_OFFSET_REG1].val = (mode->rate | mode->axis | mode->lowpower);
    s->cached_lp = mode->lowpower;
    s->cached_odr = mode->rate;
    sc7a20_updateActivityDependentParams(s);
    sc7a20_updateClickDependentParams(s);
    sc7a20_setDirty(s, (1 << CACHE_OFFSET_REG1));
}

void sc7a20_setHPF(SC7A20_t* s, SC7A20_filter_t *filter)
{   
    s->cachedRegs[CACHE_OFFSET_REG2].val = filter ? (filter->target | filter->mode | filter->cutoff | filter->ref) : 0;
    sc7a20_setDirty(s, (1 << CACHE_OFFSET_REG2));
}

void sc7a20_setRange(SC7A20_t* s, uint8_t range)
{
    s->cachedRegs[CACHE_OFFSET_REG4].val = (range | CTRL_REG4_BDU | CTRL_REG4_DLPF_EN);
    s->cached_fs = range;
    sc7a20_updateActivityDependentParams(s);
    sc7a20_updateClickDependentParams(s);
    sc7a20_setDirty(s, (1 << CACHE_OFFSET_REG4));
}

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

    sc7a20_setDirty(s, (1 << CACHE_OFFSET_REG3));
}

void sc7a20_setActivityInterrupt(SC7A20_t* s, SC7A20_activityDetectionConfig_t *a)
{
    // USE AOI1 with INT2
    if(!a)
    {
        s->cachedRegs[MOTIONEVT_INT_REG].val &= ~(MOTIONEVT_INT_BIT);
        s->cachedRegs[CACHE_OFFSET_AOI1_CTRL_REG].val = 0;
        sc7a20_setDirty(s, (uint8_t)((1 << MOTIONEVT_INT_REG) | (1 << CACHE_OFFSET_AOI1_CTRL_REG)));
    } else {
        s->cached_act_duration_ms   = a->duration_ms;
        s->cached_act_threshold_mg  = a->threshold_mg;

        s->cachedRegs[MOTIONEVT_INT_REG].val |= MOTIONEVT_INT_BIT;
        
        // Set AOI1 event config        
        s->cachedRegs[CACHE_OFFSET_AOI1_CTRL_REG].val &= ~(AOI1_CTRL_REG_AOI | AOI1_CTRL_REG_XLIE | AOI1_CTRL_REG_XHIE | AOI1_CTRL_REG_YLIE | AOI1_CTRL_REG_YHIE | AOI1_CTRL_REG_ZLIE | AOI1_CTRL_REG_ZHIE);
        s->cachedRegs[CACHE_OFFSET_AOI1_CTRL_REG].val |= a->axis;
        sc7a20_updateActivityDependentParams(s);
        
        sc7a20_setDirty(s, (uint8_t)((1 << MOTIONEVT_INT_REG) | (1 << CACHE_OFFSET_AOI1_CTRL_REG)));
    }
}

void sc7a20_setClickInterrupt(SC7A20_t* s, SC7A20_clickDetectionConfig_t* c)
{
    // USE INT2
    if(!c)
    {
        s->cachedRegs[MOTIONEVT_INT_REG].val &= ~(CLICKEVT_INT_BIT);        
        s->cachedRegs[CACHE_OFFSET_CLICK_CTRL_REG].val = 0;
        sc7a20_setDirty(s, (uint8_t)((1 << MOTIONEVT_INT_REG) | (1 << CACHE_OFFSET_CLICK_CTRL_REG)));
    } else {
        s->cachedRegs[MOTIONEVT_INT_REG].val |= CLICKEVT_INT_BIT;
        
        // Set click event config        
        s->cachedRegs[CACHE_OFFSET_CLICK_CTRL_REG].val  = c->axis;
        s->cached_click_threshold_mg  = c->threshold_mg;
        s->cached_click_time_limit_ms = c->time_limit_ms;
        s->cached_click_latency_ms    = c->time_latency_ms;
        s->cached_click_time_window_ms= c->time_window_ms;
        sc7a20_updateClickDependentParams(s);
        sc7a20_setDirty(s, (uint8_t)((1 << MOTIONEVT_INT_REG) | (1 << CACHE_OFFSET_CLICK_CTRL_REG) |
                        (1 << CACHE_OFFSET_CLICK_THS) | (1 << CACHE_OFFSET_TIME_LIMIT) |
                        (1 << CACHE_OFFSET_TIME_LATENCY) | (1 << CACHE_OFFSET_TIME_WINDOW)));
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
        printf("WRITE - error: %d", (int)result);
        return;
    }
    nrfx_atomic_flag_clear_fetch(&s->isBufferLocked);
    printf("WRITE OK!\n");
}

uint32_t sc7a20_commitWrites(SC7A20_t* s)
{
    if(s->dirtyFlag == 0)
    {
        return 0;
    }

    if(nrfx_atomic_flag_set_fetch(&s->isBufferLocked))
    {
        return 1;
    }

    s->isBufferLocked = true;

    // Alloc writes in buffer

    sc7a20_allocFreeAll(s);
    sc7a20_freeAllTransferSlots(s);
    uint8_t nRegs = 0;

    for(uint32_t i = 0; i < SC7A20_CACHED_REGS_SIZE; ++i)
    {
        if(!!(s->dirtyFlag & (1 << i)))
        {
            uint8_t *ptr = sc7a20_allocFromBuffer(s, sizeof(SC7A20_reg_t));
            
            if(!ptr)
            {
                return 1;
            }
            memcpy((void*)ptr, (void*)&(s->cachedRegs[i]), sizeof(SC7A20_reg_t));

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

    return nrf_twi_mngr_schedule(gp_nrf_twi_mngr, &transaction);    
}

void sc7a20_read(SC7A20_t* s, uint8_t regOffset, uint8_t len)
{
    s->rDirtyFlag |= (1 << regOffset);
}

static void _isr_sc7a20_read_xyz_cb(ret_code_t result, void * p_user_data)
{
    SC7A20_t *s = (SC7A20_t*)p_user_data;

    nrfx_atomic_u32_and(&(s->readOps), ~(1 << SC7A20_READ_OP_XYZ));
    
    if (result != NRF_SUCCESS)
    {
        printf("READ - error: %d", (int)result);
        return;
    }

    //app_sched_event_put((void const*)(&s->sampleBuffer), sizeof(SC7A20_sample_t), _sch_accel_event_handler);
    nrfx_atomic_u32_or(&(s->eventsFlag), SC7A20_EVTFLAG_DATA);
}

static void _isr_sc7a20_read_ints_cb(ret_code_t result, void * p_user_data)
{
    SC7A20_t *s = (SC7A20_t*)p_user_data;

    nrfx_atomic_u32_and(&(s->readOps), ~(1 << SC7A20_READ_OP_INTS));
    
    if (result != NRF_SUCCESS)
    {
        printf("READ - error: %d", (int)result);
        return;
    }

    uint32_t ormask = 0;

    // Check the interrupt source
    if(s->readRegs.aoi1 & AOI1_SRC_XH)
    {
        printf("MOTION\n");
        ormask |= SC7A20_EVTFLAG_MOTION;
    }
    if(s->readRegs.click & CLICK_SRC_SCLICK)
    {
        printf("CLICK\n");
        ormask |= SC7A20_EVTFLAG_CLICK;
    }
    if(s->readRegs.click & CLICK_SRC_DCLICK)
    {
        printf("DBLCLICK\n");
        ormask |= SC7A20_EVTFLAG_DBLCLICK;        
    }
    nrfx_atomic_u32_or(&(s->eventsFlag), ormask);
}

static void _isr_sc7a20_read_status_cb(ret_code_t result, void *p_user_data)
{
    SC7A20_t *s = (SC7A20_t*)p_user_data;

    nrfx_atomic_u32_and(&(s->readOps), ~(1 << SC7A20_READ_OP_STATUS));
    
    if (result != NRF_SUCCESS)
    {
        printf("READ - error: %d", (int)result);
        return;
    }

    // Check the interrupt source
    if(s->readRegs.status & STATUS_REG_ZYXDA)
    {
        printf("DATA INTERRUPT\n");
        sc7a20_readOperation(s, SC7A20_READ_OP_XYZ);
    }
}

uint32_t sc7a20_readOperation(SC7A20_t* s, uint8_t id)
{
    uint8_t msk = (1 << id);

    if(nrfx_atomic_u32_fetch_or(&(s->readOps), msk) & msk)
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

    return nrf_twi_mngr_schedule(gp_nrf_twi_mngr, xaction);
}

void sc7a20_getLastSample(SC7A20_t* s, SC7A20_sample_t* smp)
{    
    CRITICAL_REGION_ENTER();
    *smp = s->sampleBuffer;
    CRITICAL_REGION_EXIT();
}

uint32_t sc7a20_getEventsFlags(SC7A20_t* s)
{
    return nrfx_atomic_u32_fetch_store(&(s->eventsFlag), 0);
}

static void sc7a20_readWHOAMI(SC7A20_t* s)
{
}

void _isr_sc7a20_io_event_handler(SC7A20_t *s, uint8_t irqs)
{

    ret_code_t r = 0;
    if(irqs & SC7A20_IRQ_INT1)
    {
        //printf("INT1\n");
        r = sc7a20_readOperation(s, SC7A20_READ_OP_STATUS);
        //APP_ERROR_CHECK(r);
    }

    if(irqs & SC7A20_IRQ_INT2)
    {
        //printf("INT2\n");
        sc7a20_readOperation(s, SC7A20_READ_OP_INTS);
        //APP_ERROR_CHECK(r);
    }
    

//    sc7a20_readOperation(s, SC7A20_READ_OP_STATUS);
//    sc7a20_readOperation(s, SC7A20_READ_OP_INTS);
}