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

#include "Pod/FileServer.h"
#include "Pod/Playground.h"
#include "Pod/Storage.h"
#include "Pod/FileSys.h"

#include "HAL/Debug.h"

#include "utils.h"

#define POD_FS_PAYLOAD_BUFFER_LEN (248)

typedef enum
{
    FSS_IDLE = 0,
    FSS_UPLOAD_ARC,     // Upload archive to flash
    FSS_UPLOAD_VF,      // Upload special file
    FSS_DOWNLOAD,       // Download file
    FSS_ERASE           // Erase flash storage
} Pod_FSFSM_t;

typedef struct
{
    union
    {
        uint16_t    currentOffset;
        uint8_t     *pCurrent;
    };

    uint16_t    remainingSize;
    Pod_FSFSM_t fsm;
    struct
    {
        uint8_t     evtCode;        // Defined in Playground.h
        uint16_t    extendedCode;   // Defined in FileServer.h
    } evtBuffer;    
    bool                    flashOpInProgress;
    Pod_FSFileDesc_t        cachedFileDesc; // Descriptor of file being uploaded/downloaded
    uint16_t packetSize;
    bool pendingDataPacket;
} Pod_FSState_t;

static Pod_FSState_t s_Pod_FSState;

volatile uint8_t MBT_CFG_PLATFORM_ALIGNED(4) s_Pod_FSDataBuffer[POD_FS_PAYLOAD_BUFFER_LEN];

static size_t s_pktSize = 0;

// ------------------------------------------------------------------------------

static void Pod_setEvent(uint8_t evtCode, uint16_t extended)
{
    s_Pod_FSState.evtBuffer.evtCode = evtCode;
    s_Pod_FSState.evtBuffer.extendedCode = extended;    
    Pod_FSTXSlotHandler();
}

// ------------------------------------------------------------------------------

static bool Pod_FSTransitionToState(Pod_FSFSM_t nextState)
{
    if(s_Pod_FSState.fsm == nextState) return false;

    MBT_LOG("[FS] %d -> %d\n", (uint32_t)s_Pod_FSState.fsm, (uint32_t)nextState);

    s_Pod_FSState.fsm = nextState;
    return true;
}

// ------------------------------------------------------------------------------

static void _Pod_FSPrepareNextPacketDL(void)
{
    size_t sendSize = MIN(HAL_bleGetMTU() - 1, s_Pod_FSState.remainingSize);
    sendSize = MIN(sendSize, POD_FS_PAYLOAD_BUFFER_LEN - 1);

    struct
    {
        uint8_t     chan : 4;   // Channel ID (b)
        uint8_t     seq  : 4;   // Sequence number (a)
    } MBT_CFG_PLATFORM_PACKED basicHeader =
    {
        .chan = POD_PG_CHANNEL_FILE
    };

    memcpy((uint8_t *)&s_Pod_FSDataBuffer[0], (uint8_t *)&basicHeader, sizeof(basicHeader));
    memcpy((uint8_t *)&s_Pod_FSDataBuffer[sizeof(basicHeader)], s_Pod_FSState.pCurrent, sendSize);
    s_Pod_FSState.packetSize    = sendSize;
    s_Pod_FSState.remainingSize -= sendSize;
    s_Pod_FSState.pCurrent      += sendSize;
    s_Pod_FSState.pendingDataPacket = true;
}


// Called by Playground to flush pending file data or events from
// the File Server
void Pod_FSTXSlotHandler(void)
{
    if(s_Pod_FSState.fsm == FSS_DOWNLOAD)
    {
        if(s_Pod_FSState.pendingDataPacket)
        {
            if(Pod_PGSend((Pod_PGPDU_t *)&s_Pod_FSDataBuffer[0], s_Pod_FSState.packetSize + 1) == 0)
            {
                s_Pod_FSState.pendingDataPacket = false;
                if(s_Pod_FSState.remainingSize == 0)
                {
                    Pod_setEvent(POD_PG_EVCODE_FILESRV_OP_COMPLETE, 0);
                    Pod_FSTransitionToState(FSS_IDLE);
                } else {
                    _Pod_FSPrepareNextPacketDL();
                }
            }
        }
    }

    // Check if there are events to send
    if(s_Pod_FSState.evtBuffer.evtCode != 0)
    {
        MBT_LOG("[FS] Flsh evt %d, %d\n", (uint32_t)s_Pod_FSState.evtBuffer.evtCode, (uint32_t)s_Pod_FSState.evtBuffer.extendedCode);
        
        struct
        {
            Pod_PGCommonHeader_t            header;
            Pod_PGEvtExtendedCodePayload_t  extended;
        } data;

        data.header.chan = POD_PG_CHANNEL_EVENT;
        data.header.code = s_Pod_FSState.evtBuffer.evtCode;
        data.extended.code = s_Pod_FSState.evtBuffer.extendedCode;

        if(Pod_PGSend((Pod_PGPDU_t *)&data, sizeof(data)) == 0)
        {
            s_Pod_FSState.evtBuffer.evtCode = 0;
        }
    }
}

// ------------------------------------------------------------------------------

void Pod_FSReset(void)
{
    memset(&s_Pod_FSState, 0, sizeof(s_Pod_FSState));
}

// ------------------------------------------------------------------------------

void Pod_FSInit(void)
{
    Pod_FSReset();    
}

// ------------------------------------------------------------------------------

void Pod_FSTick(uint32_t ticks)
{
    if(s_pktSize > 0)
    {
        Pod_FSDataPacketHandler((const uint8_t *)&s_Pod_FSDataBuffer[0], s_pktSize);
        s_pktSize = 0;
    }
}

// ------------------------------------------------------------------------------

void Pod_FSAbortXfer(void)
{
    Pod_FSTransitionToState(FSS_IDLE);
    if(!s_Pod_FSState.flashOpInProgress)
    {
        Pod_FSStorageEventHandler(0); // Send OP_COMPLETE event
    }
}

// ------------------------------------------------------------------------------

uint8_t Pod_FSStartArchiveXfer(uint16_t offset, uint16_t size)
{    
    if(s_Pod_FSState.fsm == FSS_IDLE)
    {
        if(!Pod_storageIsEmpty())
            return FSRESULT_NOTEMPTY;

        if(size > Pod_storageGetCapacity())
            return FSRESULT_NOT_ENOUGH_SPACE;
            
        s_Pod_FSState.remainingSize = size;
        s_Pod_FSState.currentOffset = offset;
        Pod_FSTransitionToState(FSS_UPLOAD_ARC);
    } else {
        return FSRESULT_OP_IN_PROGRESS;
    }
    
    return FSRESULT_OK;
}

// ------------------------------------------------------------------------------

uint8_t _Pod_FSPrepareAccessCommon(uint16_t id, uint16_t *pSize, Pod_FSFSM_t nextState)
{
    if(s_Pod_FSState.fsm == FSS_IDLE)
    {
        // Only virtual files allowed

        Pod_FSFileDesc_t desc = {0};
        Pod_FSysGetFile(id, &desc);

        if(!desc.pData)
            return FSRESULT_BAD_ID;
        
        if(nextState == FSS_UPLOAD_VF)
        {
            if(!MBT_FLAG_IS_SET(desc.permissions, FS_PERM_W))
                return FSRESULT_ACCESS_DENIED;
            if(*pSize > desc.size)
                return FSRESULT_BAD_SIZE;
        } else if(nextState == FSS_DOWNLOAD)
        {
            *pSize = desc.size;
        }
        
        s_Pod_FSState.remainingSize = *pSize;
        s_Pod_FSState.pCurrent      = (uint8_t*)desc.pData;
        Pod_FSTransitionToState(nextState);

        s_Pod_FSState.cachedFileDesc = desc;
    } else {
        return FSRESULT_OP_IN_PROGRESS;
    }

    return FSRESULT_OK;
}

// ------------------------------------------------------------------------------

// Overwrite a virtual file with write permission
uint8_t Pod_FSStartFileUpload(uint16_t id, uint16_t size)
{
    return _Pod_FSPrepareAccessCommon(id, &size, FSS_UPLOAD_VF);    
}

// ------------------------------------------------------------------------------

// Download a virtual or flash file
uint8_t Pod_FSStartFileDownload(uint16_t id, uint16_t *pSize)
{
    uint8_t ret = _Pod_FSPrepareAccessCommon(id, pSize, FSS_DOWNLOAD);
    if(ret == 0)
    {
        _Pod_FSPrepareNextPacketDL();
    }
    return ret;
}

// ------------------------------------------------------------------------------



void Pod_ISR_FSDataPacketHandler(const uint8_t *pData, size_t size)
{
    if(s_pktSize > 0)
    {
        MBT_LOG("OVERFLOW!\n");
    }
    if(size > sizeof(s_Pod_FSDataBuffer))
    {
        MBT_LOG("BUFFER TOO BIG\n");
        return;
    }
    s_pktSize = size;
    memcpy((uint8_t *)&s_Pod_FSDataBuffer[0], pData, size);

    HAL_sysSchedulePendingTick();
}

// ------------------------------------------------------------------------------
uint8_t Pod_FSDataPacketHandler(const uint8_t *pData, size_t size)
{
    //MBT_LOG("SIZE: %u\nREM: %u\n", size, s_Pod_FSState.remainingSize);
    if(size > s_Pod_FSState.remainingSize)
    {
        MBT_LOG("BAD SIZE\n");
        Pod_setEvent(POD_PG_EVCODE_FILESRV_EXTENDED, FSRESULT_BAD_SIZE);
        return FSRESULT_BAD_SIZE;
    }

    uint8_t result = FSRESULT_OK;

    // Store in the intermediate buffer
    
    if(s_Pod_FSState.fsm == FSS_UPLOAD_VF)
    {
        memcpy(s_Pod_FSState.pCurrent, pData, size);
        s_Pod_FSState.pCurrent      += size;        
        s_Pod_FSState.remainingSize -= size;
        if(s_Pod_FSState.remainingSize == 0)
        {
            Pod_setEvent(POD_PG_EVCODE_FILESRV_OP_COMPLETE, 0);
            
            // Call the VF write complete handler
            if(s_Pod_FSState.cachedFileDesc.pvf && s_Pod_FSState.cachedFileDesc.pvf->cb)
                s_Pod_FSState.cachedFileDesc.pvf->cb(s_Pod_FSState.cachedFileDesc.pvf->id, VFE_FINISHWRITE);
            
            Pod_FSTransitionToState(FSS_IDLE);
        } else {
            Pod_setEvent(POD_PG_EVCODE_FILESRV_FLOW_CTRL, 0);
        }
    } else if(s_Pod_FSState.fsm == FSS_UPLOAD_ARC)
    {
        if(s_Pod_FSState.flashOpInProgress) 
        {
            MBT_LOG("FLASH OP IN PROGRESS\n");
            Pod_setEvent(POD_PG_EVCODE_FILESRV_EXTENDED, FSRESULT_OP_IN_PROGRESS);
            return FSRESULT_OP_IN_PROGRESS;
        }

        //memcpy(&s_Pod_FSDataBuffer[0], pData, size);

        bool res = Pod_storageWrite(s_Pod_FSState.currentOffset, (const uint8_t*)&s_Pod_FSDataBuffer[0], size);
        if(!res)
        {
            MBT_LOG("[FS] STG WR ERROR\n");
            Pod_FSTransitionToState(FSS_IDLE);
            Pod_setEvent(POD_PG_EVCODE_FILESRV_EXTENDED, FSRESULT_INTERNAL_ERROR);
            return FSRESULT_INTERNAL_ERROR;
        } else {
            MBT_LOG("[FS] STG WR %d bytes\n", (uint32_t)size);
            s_Pod_FSState.currentOffset += size;
            s_Pod_FSState.remainingSize -= size;
        }
        s_Pod_FSState.flashOpInProgress = true;
    } else {
        Pod_setEvent(POD_PG_EVCODE_FILESRV_EXTENDED, FSRESULT_INVALID_STATE);
        return FSRESULT_INVALID_STATE;
    }

    return FSRESULT_OK;
}

// ------------------------------------------------------------------------------

uint8_t Pod_FSErase(void)
{
    if(s_Pod_FSState.fsm != FSS_IDLE)
    {
        return FSRESULT_OP_IN_PROGRESS;
    }
    if(Pod_storageErase())
    {
        Pod_FSTransitionToState(FSS_ERASE);
    } else {
        return FSRESULT_INTERNAL_ERROR;
    }
    return FSRESULT_OK;
}

// ------------------------------------------------------------------------------

void Pod_FSStorageEventHandler(Pod_storageEventType_t evt)
{
    Pod_FSFSM_t nextState = s_Pod_FSState.fsm;
    
    uint8_t evtCode = POD_PG_EVCODE_FILESRV_OP_COMPLETE;
    uint16_t extCode = 0;

    s_Pod_FSState.flashOpInProgress = false;
    switch(evt)
    {
        case STGEVENT_IDLE:
        case STGEVENT_BUSY:
            return;
        case STGEVENT_WRITE_OK:
            if(s_Pod_FSState.fsm == FSS_UPLOAD_ARC)
            {
                if(s_Pod_FSState.remainingSize == 0)
                {
                    nextState = FSS_IDLE;
                    if(Pod_storageMountFS())
                    {
                        // File OP complete event
                        evtCode = POD_PG_EVCODE_FILESRV_OP_COMPLETE;
                    } else {
                        evtCode = POD_PG_EVCODE_FILESRV_EXTENDED;
                        extCode = FSRESULT_CANT_MOUNT;
                    }
                } else {
                    // Flow control event
                    evtCode = POD_PG_EVCODE_FILESRV_FLOW_CTRL;
                }
            }
        break;
        case STGEVENT_WRITE_ERROR:
            if(s_Pod_FSState.fsm == FSS_UPLOAD_ARC)
            {
                nextState = FSS_IDLE;
                // Error event
                evtCode = POD_PG_EVCODE_FILESRV_EXTENDED;
                extCode = FSRESULT_INTERNAL_ERROR;
            }
        break;
        case STGEVENT_ERASE_OK:
            if(s_Pod_FSState.fsm == FSS_ERASE)
            {
                nextState = FSS_IDLE;
                // FILE OP FINISHED Event
                evtCode = POD_PG_EVCODE_FILESRV_OP_COMPLETE;
            }
        break;
        case STGEVENT_ERASE_ERROR:
            if(s_Pod_FSState.fsm == FSS_ERASE)
            {
                nextState = FSS_IDLE;
                // Error event
                evtCode = POD_PG_EVCODE_FILESRV_EXTENDED;
                extCode = FSRESULT_INTERNAL_ERROR;
            }
        break;
    }        
    Pod_FSTransitionToState(nextState);
    Pod_setEvent(evtCode, extCode);
}

// ------------------------------------------------------------------------------

void Pod_FSEventPacketHandler(Pod_PGPDU_t *pdu)
{
    // Get flow control event from host when downloading files
}
