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

#include "Storage.h"
#include "HAL/HAL.h"
#include "HAL/Debug.h"
#include "fstest.h"
#include "Misc/crc16.h"
//#include "Pod/BoxMode.h"

typedef enum
{
    SS_INVALID = 0,
    SS_IDLE,
    SS_ERASING,
    SS_WRITING
} Pod_storageFSM_t;

typedef struct
{
    uint8_t *pBase;
    uint16_t storageSize;
    Pod_storageFD_t *fdFirst;
    Pod_storageFD_t *fdLast;
    Pod_storageFSM_t fsm;
    bool mounted;
} Pod_storageState_t;

static Pod_storageState_t s_Pod_storageState;

// ------------------------------------------------------------------------------

static void Pod_storageTransitionToState(Pod_storageFSM_t nextState)
{
    if(s_Pod_storageState.fsm == nextState) return;

    switch(nextState)
    {
        case SS_ERASING:            
            s_Pod_storageState.fdFirst = s_Pod_storageState.fdLast = NULL;
        break;
        case SS_WRITING:
        break;
    }

    Pod_storageEventHandler(nextState != SS_IDLE ? STGEVENT_BUSY : STGEVENT_IDLE);    

    s_Pod_storageState.fsm = nextState;
}

// ------------------------------------------------------------------------------

static bool Pod_storageValidateOffset(uint16_t offset)
{
    return (offset < s_Pod_storageState.storageSize);
}

// ------------------------------------------------------------------------------

bool Pod_storageIsEmpty(void)
{
    return (s_Pod_storageState.fdFirst == NULL) && (s_Pod_storageState.fdLast == NULL);
}

// ------------------------------------------------------------------------------

uint16_t Pod_storageGetCapacity(void)
{
    return (s_Pod_storageState.storageSize);
}

// ------------------------------------------------------------------------------

static bool Pod_storageCheckFiles(uint8_t *pCur, Pod_storageFD_t **fdFirst, Pod_storageFD_t **fdLast)
{
    Pod_storageFD_t *pFD = (Pod_storageFD_t *)pCur;

    *fdFirst = pFD;

    uint16_t i = 0;

    for(; i < MBT_CFG_POD_STORAGE_MAX_FILES; ++i)
    {
        pFD = (Pod_storageFD_t *)pCur;
        MBT_LOG("[FS] ID = 0x%x\n", (uint32_t)pFD->id);

        // Validate self offset + size
        if(!Pod_storageValidateOffset((uint16_t)(pCur - s_Pod_storageState.pBase) + sizeof(Pod_storageFD_t) + pFD->size))
        {
            return false;
        }
    
        uint16_t poly = POD_STORAGE_FS_CRC16_POLY;
        uint16_t crc = crc16_compute(pCur + 2, pFD->size + sizeof(Pod_storageFD_t) - 2, &poly);
        if(crc != pFD->crc16)
        {
            MBT_LOG("[FS] WRONG CRC\n");
            return false;
        }
        if(pFD->offsetNext != 0xFFFF)
        {
            if(!Pod_storageValidateOffset(pFD->offsetNext))
            {
                return false;
            }
            pCur = s_Pod_storageState.pBase + pFD->offsetNext;
        } else {
            *fdLast = pFD;
            break;
        }                
    } 

    if(i >= MBT_CFG_POD_STORAGE_MAX_FILES) return false;

    return true;
}

// ------------------------------------------------------------------------------

static bool Pod_storageFSInit(void)
{
    Pod_storageHeader_t *pFsh = NULL;
    Pod_storageFD_t *fdFirst = 0;
    Pod_storageFD_t *fdLast = 0;

    uint8_t *pCur = s_Pod_storageState.pBase;
    for(uint16_t i = 0; i < s_Pod_storageState.storageSize - sizeof(Pod_storageHeader_t) - sizeof(Pod_storageFD_t); i += 2, pCur += 2)
    {
        if((*(uint16_t*)pCur) == POD_STORAGE_FS_HEADER_MAGIC)
        {
            pFsh = (Pod_storageHeader_t *)pCur;

            if(pFsh->version != POD_STORAGE_FS_HEADER_VERSION)
            {
                MBT_LOG("[FS] WRONG VERSION\n");
                continue;
            }

            // Iterate files
            if(Pod_storageCheckFiles(pCur + sizeof(Pod_storageHeader_t), &fdFirst, &fdLast))
            {
                s_Pod_storageState.fdFirst  = fdFirst;
                s_Pod_storageState.fdLast   = fdLast;
                return true;
            }
        }
    }
    return false;
}

// ------------------------------------------------------------------------------

bool Pod_storageIsBusy(void)
{
    return (s_Pod_storageState.fsm != SS_IDLE);
}

// ------------------------------------------------------------------------------

bool Pod_storageErase(void)
{
    if(!Pod_storageIsBusy())
    {
        Pod_storageTransitionToState(SS_ERASING);
        return HAL_flashErasePage(0);        
    }
    return false;
}

// ------------------------------------------------------------------------------

void Pod_storageInit(void)
{
    memset(&s_Pod_storageState, 0, sizeof(s_Pod_storageState));

    HAL_flashGetStorageAbsoluteAddr(&s_Pod_storageState.pBase, &s_Pod_storageState.storageSize);

    Pod_storageTransitionToState(SS_IDLE);

    Pod_storageMountFS();
}

// ------------------------------------------------------------------------------

const Pod_storageFD_t *Pod_storageGetFD(uint16_t id)
{
    if(Pod_storageIsEmpty() || !s_Pod_storageState.mounted) 
        return NULL;

    Pod_storageFD_t *i = NULL;
    uint16_t n = 0;
    uint16_t offsetNext = ((uint8_t *)s_Pod_storageState.fdFirst - s_Pod_storageState.pBase);
    do
    {
        i = (Pod_storageFD_t*) (s_Pod_storageState.pBase + offsetNext);
        if(i->id == id)
        {
            return i;
        }        
        offsetNext = i->offsetNext;
    } while(i != s_Pod_storageState.fdLast && n++ < MBT_CFG_POD_STORAGE_MAX_FILES);
    return NULL;
}

// ------------------------------------------------------------------------------

const uint8_t* Pod_storageGetPtr(const Pod_storageFD_t *pFD)
{
    return ((uint8_t *)pFD + sizeof(Pod_storageFD_t));
}

// ------------------------------------------------------------------------------

void Pod_storageHALEventHandler(const HAL_event_t *pEventData)
{
    uint8_t evtOffset = 0;
    if (pEventData->data.flashEvent.success)
    {
        MBT_LOG("[FLASH] SUCCESS\n");        
    } else
    {
        MBT_LOG("[FLASH] ERROR\n");
        evtOffset = 1;        
    }

    if(s_Pod_storageState.fsm == SS_ERASING)
    {
        evtOffset += STGEVENT_ERASE_BASE;
    } else {
        evtOffset += STGEVENT_WRITE_BASE;
    }

    Pod_storageTransitionToState(SS_IDLE);
    
    Pod_storageEventHandler(evtOffset);    
}

// ------------------------------------------------------------------------------

static bool Pod_storageCheckAlignment(uint16_t d)
{
    return !(d & 0x3);
}

// ------------------------------------------------------------------------------

bool Pod_storageWrite(uint16_t offset, const uint8_t *pData, uint16_t len)
{
    MBT_LOG("STGW: OFF = %d, PDATA = 0x%x, LEN = %d\n", (uint32_t)offset, (uint32_t)pData, (uint32_t)len);
    if(Pod_storageIsBusy())
    {
        MBT_LOG("STORAGE BUSY\n");
        return false;
    }
    
    if(!Pod_storageValidateOffset(offset + len))
    {
        // Outside bounds
        MBT_LOG("OUTSIDE BOUNDS\n");
        return false;
    }

    if(!Pod_storageCheckAlignment(offset) || !Pod_storageCheckAlignment(len))
    {
        MBT_LOG("NOT ALIGNED\n");
        // Not aligned to 32-bit
        return false;
    }

    s_Pod_storageState.mounted = false;

    bool op = HAL_flashWrite((const uint32_t*) pData, offset, len);

    if(op)
    {
        Pod_storageTransitionToState(SS_WRITING);
    }

    return op;
}

// ------------------------------------------------------------------------------

bool Pod_storageMountFS(void)    
{
    if(Pod_storageIsBusy()) return false;

    bool ret = Pod_storageFSInit();
    
    s_Pod_storageState.mounted = ret;

    if(!ret)
    {
        MBT_LOG("[FS] CORRUPT/EMPTY. ERASING\n");        
        Pod_storageErase();
    }
    return ret;
}
