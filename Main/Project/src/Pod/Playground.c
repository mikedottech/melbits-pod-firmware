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

#include "Playground.h"
#include "HAL/HAL.h"
#include "HAL/Debug.h"

#include "Pod/BLE.h"
#include "Pod/UI.h"
#include "Pod/Vibration.h"
#include "Pod/Activity.h"
#include "Pod/BoxMode.h"
#include "Pod/Storage.h"
#include "Pod/FileServer.h"
#include "Pod/StreamServer.h"
#include "Pod/EventServer.h"
#include "Pod/FileSys.h"
#include "Pod/TestMode.h"

#include "Pod/RetainedRAM.h"

#include "utils.h"

//#define MBT_PG_DISABLE_ENCRYPTION

// SECRET encryption key. Must be the same in PodLib
// NOTE: This used to live in an external file.
//.      A dummy key has been hardcoded here.
//.      Pleas note this is not the key used in production devices, which is not disclosed in this codebase.
static const uint8_t k_ECBKey[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

typedef enum
{
    PGMS_IDLE = 0,
    PGMS_RX,     // Listening for requests
    PGMS_TX      // Ready to TX reply
} Pod_PGMainChFSM_t;

enum
{
    BP_MAIN = 1,
    BP_EVT  = 2,
    BP_GARBAGE = 0x80
};

typedef struct
{
    Pod_PGPIN_t         MLSessionPIN;
} Pod_PGStateRetained_t;

typedef struct
{
    uint8_t             seq;
    Pod_PGPDU_t         outBuffer;    
    size_t              outBufferSize;
    Pod_PGMainChFSM_t   mainFSM;
    bool                cmdInFlight;    
    bool                unlockedAPI;        
    MBT_TIMER_DEFINE    (mlSessionTimer);
    MBT_TIMER_DEFINE    (unlockAPITimer);
    bool                mlSessionAllowed;

    Pod_PGPIN_t         candidateLinkPIN;

    uint8_t             encryptedAPIState;  // 0 = not encrypted, 1 = nonce get ok pending, 2 = encrypted

    CommCtr_t            encRxCtr;
    CommCtr_t            encTxCtr;

    HAL_cryptCtx_t         encRxCtx;
    HAL_cryptCtx_t         encTxCtx;

    volatile Pod_PGPDU_t         inBufferMain;
    volatile Pod_PGPDU_t         inBufferEvt;
    volatile uint8_t             inBufferPendingMask;
} Pod_PGState_t; 


Pod_PGState_t s_Pod_PGState;
Pod_PGStateRetained_t s_Pod_PGStateRetained;

// ------------------------------------------------------------------------------

static void _ISR_PodPGHandleInvalidFrame()
{
    MBT_LOG(">>> GARBAGE!\n");
    MBT_FLAG_SET(s_Pod_PGState.inBufferPendingMask, BP_GARBAGE);
}

// ------------------------------------------------------------------------------

void Pod_PGClearLinkPIN()
{
    memset(&g_Pod_RetainedRAM.PGData.PGLinkPIN[0], 0, sizeof(g_Pod_RetainedRAM.PGData.PGLinkPIN));
    Pod_BLEClearAdvFlags(POD_BLE_ADV_FLAGS_HASLINKPIN);
}

// ------------------------------------------------------------------------------

void Pod_PGSetLinkPIN(uint64_t pin)
{
    g_Pod_RetainedRAM.PGData.PGLinkPIN[0] =
    g_Pod_RetainedRAM.PGData.PGLinkPIN[1] =
    g_Pod_RetainedRAM.PGData.PGLinkPIN[2] = pin;
    if(pin != 0)
    {
        Pod_BLESetAdvFlags(POD_BLE_ADV_FLAGS_HASLINKPIN);
    }
}

// ------------------------------------------------------------------------------

static bool _Pod_PGIsLinkPINValid(void)
{
    return (g_Pod_RetainedRAM.PGData.PGLinkPIN[0] == g_Pod_RetainedRAM.PGData.PGLinkPIN[1] &&
            g_Pod_RetainedRAM.PGData.PGLinkPIN[0] == g_Pod_RetainedRAM.PGData.PGLinkPIN[2] &&
            g_Pod_RetainedRAM.PGData.PGLinkPIN[0] == g_Pod_RetainedRAM.PGData.PGLinkPIN[3]);
}

// ------------------------------------------------------------------------------

static bool _Pod_PGOpcodeIsRestricted(uint8_t opcode)
{
    return (opcode < POD_PG_OPCODE_PINSET && opcode != POD_PG_OPCODE_VIBRATION
     && opcode != POD_PG_OPCODE_ENTERDFUMODE);
}

// ------------------------------------------------------------------------------

void Pod_PGInit(void)
{
    Pod_PGReset();
    memset(&s_Pod_PGStateRetained, 0, sizeof(s_Pod_PGStateRetained));
    if(!_Pod_PGIsLinkPINValid())
        Pod_PGClearLinkPIN();
}

// ------------------------------------------------------------------------------

void Pod_PGReset(void)
{
    
    memset(&s_Pod_PGState, 0 , sizeof(Pod_PGState_t));
    MBT_TIMER_DISABLE(s_Pod_PGState.mlSessionTimer);
    MBT_TIMER_DISABLE(s_Pod_PGState.unlockAPITimer);
}

// ------------------------------------------------------------------------------

static void Pod_PGFlushPendingTX(void)
{
    if(s_Pod_PGState.outBuffer.header.code != POD_PG_OPCODE_INVALID)
    {
        // Try to send s_Pod_PGState.outBuffer
        if(Pod_PGSend(&s_Pod_PGState.outBuffer, s_Pod_PGState.outBufferSize) == 0)
        {
            memset(&s_Pod_PGState.outBuffer, 0, s_Pod_PGState.outBufferSize);
            s_Pod_PGState.cmdInFlight = false;
            if(s_Pod_PGState.encryptedAPIState == 1) s_Pod_PGState.encryptedAPIState = 2;        
        } else {
            HAL_sysSchedulePendingTick();
        }
    }

    // We don't want to send unencrypted events, or any event while in garbage mode
    if(s_Pod_PGState.unlockedAPI && s_Pod_PGState.encryptedAPIState == 2 && !MBT_FLAG_IS_SET(s_Pod_PGState.inBufferPendingMask, BP_GARBAGE))
    {
        Pod_FSTXSlotHandler();    
        Pod_ESTXSlotHandler();
        Pod_SSTXSlotHandler();
    }
}

// ------------------------------------------------------------------------------

static void _Pod_PGSetResponseExtendedCommon(uint8_t ret, uint8_t *pRes, size_t *pResSize)
{
    if(ret != 0)
    {
        *pRes = POD_PG_RSCODE_EXTENDED;
        *pResSize += sizeof(Pod_PGRspExtendedCodePayload_t);
        s_Pod_PGState.outBuffer.payload.rsp.extendedCode.code = ret;
    }
}

// ------------------------------------------------------------------------------

static void _Pod_PGUnlockAPI(void)
{
    s_Pod_PGState.unlockedAPI = true;
    MBT_TIMER_DISABLE(s_Pod_PGState.unlockAPITimer);
    MBT_LOG("-API UNLOCKED\n");
}

// ------------------------------------------------------------------------------

static void Pod_PGCommandExecutor(Pod_PGPDU_t *pdu)
{    
    uint8_t res = POD_PG_RSCODE_OK;
    size_t resSize = sizeof(Pod_PGCommonHeader_t);

    if(s_Pod_PGState.cmdInFlight)
        return; // Ignore command

    if(s_Pod_PGState.encryptedAPIState != 2)
    {
        if(pdu->header.code != POD_PG_OPCODE_NONCEGET)
        {
            _ISR_PodPGHandleInvalidFrame();
            MBT_FLAG_SET(s_Pod_PGState.inBufferPendingMask, BP_MAIN);
            HAL_sysSchedulePendingTick();
            
            //res = POD_PG_RSCODE_RESTRICTED;
            //goto finish;
            return;
        }
    } else if(!s_Pod_PGState.unlockedAPI)
    {
        if(_Pod_PGOpcodeIsRestricted(pdu->header.code))
        {
            res = POD_PG_RSCODE_RESTRICTED;
            goto finish;
        }
    }

    switch(pdu->header.code)
    {
        // FILE XFER -----------------------------------
        case POD_PG_OPCODE_STOREARCHIVE:
        {
            uint8_t ret = Pod_FSStartArchiveXfer(pdu->payload.req.storeArchive.offset, pdu->payload.req.storeArchive.len);
            _Pod_PGSetResponseExtendedCommon(ret, &res, &resSize);
        }
        break;
        case POD_PG_OPCODE_ERASEPAGES:
        {
            uint8_t ret = Pod_FSErase();
            _Pod_PGSetResponseExtendedCommon(ret, &res, &resSize);
        }
        break;
        case POD_PG_OPCODE_ABORTXFER:
            Pod_FSAbortXfer();
        break;
        case POD_PG_OPCODE_GETFILE:         // All files
        {
            uint16_t size = 0;
            uint8_t ret = Pod_FSStartFileDownload(pdu->payload.req.getFile.id, &size);            
            if(ret != 0)
            {
                _Pod_PGSetResponseExtendedCommon(ret, &res, &resSize);
            } else {
                resSize += sizeof(Pod_PGRspFileInfoPayload_t);
                s_Pod_PGState.outBuffer.payload.rsp.fileInfo.size = size;
            }
        }
        break;
        case POD_PG_OPCODE_PUTFILE:         // VF files only
        {
            uint8_t ret = Pod_FSStartFileUpload(pdu->payload.req.putfile.id, pdu->payload.req.putfile.len);
            _Pod_PGSetResponseExtendedCommon(ret, &res, &resSize);
        }
        break;
        case POD_PG_OPCODE_DELFILE:                        
        break;
        case POD_PG_OPCODE_GETFILEINFO:      // All files
        {
            Pod_FSFileDesc_t desc = {0};
            Pod_FSysGetFile(pdu->payload.req.getFileInfo.id, &desc);
            if(desc.pData)
            {
                resSize += sizeof(Pod_PGRspFileInfoPayload_t);
                s_Pod_PGState.outBuffer.payload.rsp.fileInfo.size = (uint16_t)desc.size;
            } else {
                res = POD_PG_RSCODE_INVALID_PARAM;
            }
        }
        break;
        // UI ------------------------------------------
        case POD_PG_OPCODE_PLAYCLIP:
            if(pdu->payload.req.playClip.extra)
            {
                if(!Pod_UIPlayFilePrio(pdu->payload.req.playClip.id, pdu->payload.req.playClip.prio))
                    res = POD_PG_RSCODE_INVALID_PARAM;
            } else {
                if(pdu->payload.req.playClip.prio != 0)
                    Pod_UIPlaySystemClipPrio(pdu->payload.req.playClip.id, pdu->payload.req.playClip.prio);
                else
                    Pod_UIPlaySystemClip(pdu->payload.req.playClip.id);
            }
        break;
        case POD_PG_OPCODE_ABORTCLIP:
            Pod_UIStopCurrentClip();
        break;
        // VIBRATION ------------------------------------
        case POD_PG_OPCODE_VIBRATION:
            Pod_vibSetVibration(pdu->payload.req.vibration.level, pdu->payload.req.vibration.time_ms);
        break;
        // ACTIVITY -------------------------------------
        case POD_PG_OPCODE_START_ACTIVITY:
        {
            uint8_t ret = Pod_ActStart((Pod_ActType_t) pdu->payload.req.startActivity.id);

            if(ret != ACT_ERR_OK) 
            {
                res = POD_PG_RSCODE_EXTENDED;
                resSize += sizeof(Pod_PGRspExtendedCodePayload_t);
                s_Pod_PGState.outBuffer.payload.rsp.extendedCode.code = ret;
            }
        }
        break;
        case POD_PG_OPCODE_STOP_ACTIVITY:
            Pod_ActStop();
        break;
        case POD_PG_OPCODE_GET_ACTIVITY_STATE:
        {
            uint8_t id = (uint8_t)Pod_ActGetCurrentType();
            uint16_t state = Pod_ActGetState();
            resSize += sizeof(Pod_PGRspActivityStatePayload_t);
            s_Pod_PGState.outBuffer.payload.rsp.activityState.id = id;
            s_Pod_PGState.outBuffer.payload.rsp.activityState.state = state;
        }
        break;
        // STREAM ---------------------------------------
        case POD_PG_OPCODE_SETSTREAMITEMSMASK:
            Pod_SSSetStreamMask(pdu->payload.req.setStreamItemsMask.mask);
        break;
        // EVENTS ---------------------------------------
        case POD_PG_OPCODE_SETEVENTITEMSMASK:
            Pod_ESSetEventsMask(pdu->payload.req.setEventItemsMask.mask);
        break;
        // BOX ------------------------------------------
        case POD_PG_OPCODE_ENTERBOXMODE:
            Pod_BMEnter(BMR_NONE);
        break;
        // DFU ------------------------------------------
        case POD_PG_OPCODE_ENTERDFUMODE:
            HAL_sysEnterDFUMode();            
        break;
        // TEST -----------------------------------------
        case POD_PG_OPCODE_TESTCMD:
            Pod_TMRFTestSetMode((Pod_TMTestType_t)pdu->payload.req.testCommand.cmd);
        break;
        // PIN ------------------------------------------
        case POD_PG_OPCODE_PINSET:
        {        
            //__disable_irq();
            if((pdu->payload.req.pinSet.flags & POD_PG_PINSER_FLAGS_MLINK))
            {
                if(s_Pod_PGState.mlSessionAllowed)
                {
                    // The PIN is being set
                    s_Pod_PGStateRetained.MLSessionPIN = pdu->payload.req.pinSet.pin;
                    _Pod_PGUnlockAPI();
                    res = POD_PG_RSCODE_OK;
                } else {
                    // The PIN is being checked
                    if(pdu->payload.req.pinSet.pin == s_Pod_PGStateRetained.MLSessionPIN && s_Pod_PGStateRetained.MLSessionPIN != 0)
                    {
                        _Pod_PGUnlockAPI();
                        res = POD_PG_RSCODE_OK;
                    } else {
                        res = POD_PG_RSCODE_RESTRICTED;
                    }
                }
            } else 
            // Trying to set a new PIN?
            if((pdu->payload.req.pinSet.flags & POD_PG_PINSER_FLAGS_CLEARPIN) == 0)
            { 
                // If given PIN is 0, just return if there is a set linkPIN
                if(pdu->payload.req.pinSet.pin == 0)
                {
                    if(g_Pod_RetainedRAM.PGData.PGLinkPIN[0] != 0)
                    {
                        // linkPIN already set, need it to unlock
                        res = POD_PG_RSCODE_INVALID_STATE;
                    } else {
                        // linkPIN not set, setting a new PIN will ask user for confirmation
                        res = POD_PG_RSCODE_OK;
                    }
                } else 
                // Is the PIN clear?
                if(g_Pod_RetainedRAM.PGData.PGLinkPIN[0] == 0)
                {
                   // No previous PIN, accept new one
                   if(pdu->payload.req.pinSet.pin != 0) 
                   {
                        if(s_Pod_PGState.candidateLinkPIN == 0)
                        {
                            s_Pod_PGState.candidateLinkPIN = pdu->payload.req.pinSet.pin;
                            res = POD_PG_RSCODE_PENDING;
                            // Give 5 extra minutes to shake and confirm link
                            MBT_TIMER_RESET_TIMEOUT(s_Pod_PGState.unlockAPITimer, MBT_US_TO_TICKS(5 * 60 * 1000000));
                            //MBT_LOG("PIN: %x %x\n", *((uint32_t*)&s_Pod_PGState.candidateLinkPIN), *(((uint32_t*)&s_Pod_PGState.candidateLinkPIN)+1));
                        } else {
                            res = POD_PG_RSCODE_INVALID_STATE;
                        }                        
                   } else {
                        res = POD_PG_RSCODE_INVALID_PARAM;
                   }
                } else {
                    // Previous PIN set, check for match
                    if(pdu->payload.req.pinSet.pin == g_Pod_RetainedRAM.PGData.PGLinkPIN[0])
                    {
                        _Pod_PGUnlockAPI();
                        //s_Pod_PGState.MLSessionPIN = 0; // Connection using the link pin. Forget the ML Session PIN
                        res = POD_PG_RSCODE_OK;
                    } else {
                        res = POD_PG_RSCODE_RESTRICTED;                        
                    }
                }
            } else {
                // Trying to clear the PIN
                if(s_Pod_PGState.unlockedAPI && // API already unlocked
                    pdu->payload.req.pinSet.pin == g_Pod_RetainedRAM.PGData.PGLinkPIN[0] && // PINs match
                    g_Pod_RetainedRAM.PGData.PGLinkPIN[0] != 0) // A PIN is set
                {
                    Pod_PGClearLinkPIN();
                    res = POD_PG_RSCODE_OK;
                } else {
                    res = POD_PG_RSCODE_RESTRICTED;
                }
            }
        }
        break;
        case POD_PG_OPCODE_NONCEGET:
            if(s_Pod_PGState.encryptedAPIState == 0)
            {
                uint8_t encrNonceCtr[16];
                memset(&encrNonceCtr[0], 0, sizeof(encrNonceCtr));
                HAL_cryptNonceGenerate(&encrNonceCtr[0]);
                HAL_cryptCtrInit(&s_Pod_PGState.encRxCtx, &encrNonceCtr[0], &k_ECBKey[0]);
                HAL_cryptCtrInit(&s_Pod_PGState.encTxCtx, &encrNonceCtr[0], &k_ECBKey[0]);

//                static uint8_t test[10] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9 };
//                uint32_t ctr = 0;
//                HAL_cryptCtrEncrypt(&test[0], 10, &ctr);

                resSize += sizeof(Pod_PGRspNonceGetPayload_t);
                memcpy(&s_Pod_PGState.outBuffer.payload.rsp.encryptionNonce.nonce[0], &encrNonceCtr[4], sizeof(s_Pod_PGState.outBuffer.payload.rsp.encryptionNonce.nonce));                
                s_Pod_PGState.encryptedAPIState = 1;
            } else {
                res = POD_PG_RSCODE_RESTRICTED;
            }            
        break;
        default:
            res = POD_PG_RSCODE_NOTIMPLEMENTED;
        break;
    }

finish:
    s_Pod_PGState.outBuffer.header.code = res;
    s_Pod_PGState.outBufferSize = resSize;
    s_Pod_PGState.cmdInFlight = true;
    Pod_PGFlushPendingTX();
}

// ------------------------------------------------------------------------------

void Pod_PGNotifyPINAccepted(void)
{
    //MBT_LOG("PINSHAKE: %x %x\n", *((uint32_t*)&s_Pod_PGState.candidateLinkPIN), *(((uint32_t*)&s_Pod_PGState.candidateLinkPIN)+1));
    if(s_Pod_PGState.candidateLinkPIN != 0)
    {        
        Pod_PGSetLinkPIN(s_Pod_PGState.candidateLinkPIN);
        s_Pod_PGState.candidateLinkPIN = 0;
        _Pod_PGUnlockAPI();

        Pod_PGPDU_t pdu = {0};

        pdu.header.chan = POD_PG_CHANNEL_EVENT;
        pdu.header.code = POD_PG_EVCODE_PINACCEPTED;

        Pod_vibSetVibration(200, 200);

        const uint16_t pduSize = sizeof(Pod_PGCommonHeader_t);

        if(Pod_PGSend(&pdu, pduSize) != 0)  // WARNING: Do this through the event server
        {
            MBT_LOG("(E) PIN EVT\n");
            HAL_bleDisconnect();
        }
    }
}

// ------------------------------------------------------------------------------

void Pod_PGTick(uint32_t ticks)
{
    if(Pod_BLEGetState() == BLS_CONNECTED)
    {
        Pod_PGFlushPendingTX();
    }

    if(MBT_FLAG_IS_SET(s_Pod_PGState.inBufferPendingMask, BP_GARBAGE))
    {
        // Garbage mode: Respond every command in the main channel with garbage
        if(MBT_FLAG_IS_SET(s_Pod_PGState.inBufferPendingMask, BP_MAIN))
        {            
            // WARNING: Assumes that outBuffer is bigger than 16
            uint8_t garbage[16];
            HAL_cryptNonceGenerate(&garbage[0]);
            memcpy(&s_Pod_PGState.outBuffer, &garbage[4], 12);
            size_t obs = (garbage[4] & 0xF);
            obs = MIN(obs, 12);
            obs = MAX(obs, 2);
            s_Pod_PGState.outBufferSize = obs;

            // Make sure opcode is not POD_PG_OPCODE_INVALID. Otherwise it won't be sent
            s_Pod_PGState.outBuffer.header.code = MAX(POD_PG_OPCODE_INVALID + 1, s_Pod_PGState.outBuffer.header.code);
            // Make sure the channel is invalid, so PodLib can catch a bad frame
            s_Pod_PGState.outBuffer.header.chan = MAX(POD_PG_CHANNEL_MAX, s_Pod_PGState.outBuffer.header.chan);
            // Make sure the sequence number doesn't match the expected value
            s_Pod_PGState.outBuffer.header.seq = (s_Pod_PGState.outBuffer.header.seq == (s_Pod_PGState.encTxCtr & 0xF) ? (s_Pod_PGState.outBuffer.header.seq | 0xaa) : (s_Pod_PGState.outBuffer.header.seq));
        
            Pod_PGFlushPendingTX();
            MBT_FLAG_CLEAR(s_Pod_PGState.inBufferPendingMask, BP_MAIN);
        }
        return;
    }

    if(MBT_FLAG_IS_SET(s_Pod_PGState.inBufferPendingMask, BP_MAIN))
    {
        Pod_PGCommandExecutor((Pod_PGPDU_t *)&s_Pod_PGState.inBufferMain);
        MBT_FLAG_CLEAR(s_Pod_PGState.inBufferPendingMask, BP_MAIN);
    } else if(MBT_FLAG_IS_SET(s_Pod_PGState.inBufferPendingMask, BP_EVT))
    {
        Pod_FSEventPacketHandler((Pod_PGPDU_t *)&s_Pod_PGState.inBufferEvt);
        MBT_FLAG_CLEAR(s_Pod_PGState.inBufferPendingMask, BP_EVT);
    }

    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_PGState.mlSessionTimer, ticks, {
        MBT_LOG("-MLS TIMEOUT\n");
        Pod_BLEClearAdvFlags(POD_BLE_ADV_FLAGS_MLDETECTED);
    });

    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_PGState.unlockAPITimer, ticks, {
        MBT_LOG("-UNLOCK TIMEOUT\n");
        Pod_BLEDisconnect();
    });

    //MBT_TIMER_RESET_TIMEOUT(s_Pod_PGState.mlSessionTimer, MBT_US_TO_TICKS(1000));
}

// ------------------------------------------------------------------------------

void HAL_ISR_onBLERX(const uint8_t *pData, uint16_t len)
{
//    if(isInterrupt())
//    {
//        MBT_LOG(">>>>>>>>>>>>>>>>> INTERRUPT!\n");
//    }

    if(len < 2) return;

    MBT_LOG("[PG] ");
    for(size_t i = 0; i < len; ++i)
    {
        MBT_LOG("%x ", pData[i]);
    }
    MBT_LOG("\n");
    
    Pod_PGPDU_t *pdu = (Pod_PGPDU_t *)pData;

    CommCtr_t oldRxCtr = s_Pod_PGState.encRxCtr;

    //MBT_LOG("RX: %d\n", oldRxCtr);

#ifndef MBT_PG_DISABLE_ENCRYPTION
    if(s_Pod_PGState.encryptedAPIState == 2)
    {
        HAL_cryptCtrDecrypt(&s_Pod_PGState.encRxCtx, (uint8_t*)pData, len, &s_Pod_PGState.encRxCtr);
        // Detect if the command is valid
        if((pdu->header.seq != (oldRxCtr & 0xF)))
        {
            // Invalid frame.
            goto err;
        }
    } else 
#else
    ++s_Pod_PGState.encRxCtr;
#endif
    if(pdu->header.chan != POD_PG_CHANNEL_MAIN)
    {
        goto err;
    }
    
    if(pdu->header.chan >= POD_PG_CHANNEL_MAX)
    {
        goto err;
    }

    if(pdu->header.chan == POD_PG_CHANNEL_MAIN)
    {
        if(len > sizeof(Pod_PGPDU_t))
            return;
        if(MBT_FLAG_IS_SET(s_Pod_PGState.inBufferPendingMask, BP_MAIN))
        {
            MBT_LOG("PANIC\n"); // Main channel Buffer overrun
            return;
        }
        memcpy((uint8_t *)&s_Pod_PGState.inBufferMain, pData, len);
        MBT_FLAG_SET(s_Pod_PGState.inBufferPendingMask, BP_MAIN);
    } else if(pdu->header.chan == POD_PG_CHANNEL_EVENT)
    {
        if(len > sizeof(Pod_PGPDU_t))
            return;
        if(MBT_FLAG_IS_SET(s_Pod_PGState.inBufferPendingMask, BP_EVT))
        {
            MBT_LOG("PANIC\n"); // Event channel Buffer overrun
            return;
        }
        memcpy((uint8_t *)&s_Pod_PGState.inBufferEvt, pData, len);
        MBT_FLAG_SET(s_Pod_PGState.inBufferPendingMask, BP_EVT);
    } else if(pdu->header.chan == POD_PG_CHANNEL_FILE)
    {
        Pod_ISR_FSDataPacketHandler(pData + 1, len - 1);
    }

    HAL_sysSchedulePendingTick();
    return;
err:
    _ISR_PodPGHandleInvalidFrame();
    MBT_FLAG_SET(s_Pod_PGState.inBufferPendingMask, BP_MAIN);
    HAL_sysSchedulePendingTick();
}


// ------------------------------------------------------------------------------

uint32_t Pod_PGSend(Pod_PGPDU_t *ppdu, size_t len)
{
    static uint8_t encBuffer[MBT_CFG_BLE_MAX_PAYLOAD_LEN];

    // Take care here of encryption stuff
        
    ppdu->header.seq = (s_Pod_PGState.encTxCtr & 0xF);

    memcpy(&encBuffer[0], ppdu, MIN(len, sizeof(encBuffer)));

    MBT_ASSERT(len == MIN(len, sizeof(encBuffer)));

    uint32_t oldCtr = s_Pod_PGState.encTxCtr;
    uint32_t res = 0;

    HAL_cryptCtx_t _back1 = s_Pod_PGState.encTxCtx;
    uint32_t _back2 = oldCtr;

    //MBT_LOG("TX: %d\n", oldCtr);

#ifndef MBT_PG_DISABLE_ENCRYPTION
    if(s_Pod_PGState.encryptedAPIState == 2)
    {
#ifdef MBT_PG_PROTOCOL_DEBUGGING
        for(uint8_t i = 16; i < 32; ++i)
        {
            MBT_LOG("%02x", s_Pod_PGState.encTxCtx.data[i]);
        }
        MBT_LOG("-");
#endif
        res = HAL_cryptCtrEncrypt(&s_Pod_PGState.encTxCtx, (uint8_t*)&encBuffer[0], len, &s_Pod_PGState.encTxCtr);
#ifdef MBT_PG_PROTOCOL_DEBUGGING
        for(uint8_t i = 32; i < 48; ++i)
        {
            MBT_LOG("%02x", s_Pod_PGState.encTxCtx.data[i]);
        }
        MBT_LOG("\n");
#endif

        //MBT_LOG("> %u b. Sq %u OLD %d\n", len, s_Pod_PGState.encTxCtr, oldCtr);
        if(res != 0)
        {
            MBT_LOG("> ENCR ERROR\n");
            s_Pod_PGState.encTxCtr = oldCtr;
            return res;
        }
    }
#else
    if(s_Pod_PGState.encryptedAPIState == 2)
        ++s_Pod_PGState.encTxCtr;
#endif

    uint16_t _len = (uint16_t)len;
    res = HAL_bleEnqueueSendNotification((const uint8_t *)&encBuffer[0], &_len, false);

    // Only increment counter if send was successful    
    if((res != 0 || len != _len) && s_Pod_PGState.encryptedAPIState == 2)
    {
        MBT_LOG("**RS: %d %d %d\n", res, len, _len);
        s_Pod_PGState.encTxCtr = oldCtr;
    }
    #ifdef MBT_PG_PROTOCOL_DEBUGGING_TESTENCRYPTIONTX
    else {
        if(s_Pod_PGState.encryptedAPIState == 2)
        {
            res = HAL_cryptCtrDecrypt(&_back1, (uint8_t*)&encBuffer[0], len, &_back2);
            if(memcmp(&encBuffer[0], ppdu, len) != 0)
            {
                MBT_LOG("||| ERROR |||\n");
                MBT_ASSERT(false);                
            }
        }        
    }
    #endif

    return res;
}

// ------------------------------------------------------------------------------

static void Pod_PGClearOutBuffer(void)
{
    s_Pod_PGState.outBuffer.header.code = POD_PG_OPCODE_INVALID;
    s_Pod_PGState.outBufferSize = 0;
    s_Pod_PGState.cmdInFlight = false;
}

static void Pod_PGResetServers(void)
{
    Pod_PGClearOutBuffer(); // Discard pending TX packet
    // Reset servers
    Pod_FSReset(); // File server
    Pod_SSReset(); // Stream server
    Pod_ESReset(); // Event server
}

// ------------------------------------------------------------------------------

void Pod_PGHALEventHandler(const HAL_event_t *pEventData)
{
    switch(pEventData->data.bleEvent.type)
    {
//        case HALBLEEVENTTYPE_DATARX:
//        {
//            size_t sz = HAL_bleGetAvailableReadBytes();
//            const uint8_t *pData;
//            HAL_bleReadBytes(&pData, &sz);
//            Pod_PGRXHandler(pData, sz);
//            HAL_bleFreeBytes(sz);
//        }        
//        break;
        case HALBLEEVENTTYPE_CONNECTION:
            if(HAL_bleGetConnState() == HAL_BLE_CONN_STATE_DISCONNECTED)
            {
                Pod_TMRFTestSetMode(TT_NONE); // Finish radio session and freq sweep
                Pod_PGResetServers();
                Pod_PGReset();
            } else if(HAL_bleGetConnState() == HAL_BLE_CONN_STATE_CONNECTED)
            {
                Pod_PGResetServers();
                //Pod_PGReset();
                
                s_Pod_PGState.encRxCtr = 0;
                s_Pod_PGState.encTxCtr = 0;
                s_Pod_PGState.encryptedAPIState = 0;
                s_Pod_PGState.unlockedAPI = false;
                s_Pod_PGState.candidateLinkPIN = 0;

                if(MBT_TIMER_ISENABLED(s_Pod_PGState.mlSessionTimer))
                {
                    MBT_LOG("-MLS ALLOWED\n");
                    s_Pod_PGState.mlSessionAllowed = true;
                    MBT_TIMER_DISABLE(s_Pod_PGState.mlSessionTimer);
                }
                MBT_TIMER_RESET_TIMEOUT(s_Pod_PGState.unlockAPITimer, MBT_US_TO_TICKS(30000000));
            }
        break;
    }
}

// ------------------------------------------------------------------------------

void Pod_PGMLHALEventHandler(const Pod_MLEventData_t *pData)
{
    if(HAL_bleGetConnState() == HAL_BLE_CONN_STATE_CONNECTED) return;

    switch(pData->type)
    {        
        case MLE_DATA:
            // Timer
            MBT_TIMER_RESET_TIMEOUT(s_Pod_PGState.mlSessionTimer, MBT_US_TO_TICKS(10000000));
        break;
        case MLE_SIGNAL:
            // Timer
            if(pData->data.signal.locked)
            {
                MBT_TIMER_RESET_TIMEOUT(s_Pod_PGState.mlSessionTimer, MBT_US_TO_TICKS(20000000));
            }
        break;
    }
}


