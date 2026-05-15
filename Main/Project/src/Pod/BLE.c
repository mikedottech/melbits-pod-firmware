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

#include "BLE.h"

#include "Pod/Power.h"
#include "Pod/UI.h"
#include "HAL/HAL.h"
#include "HAL/Debug.h"
#include "Misc/crc16.h"

typedef struct
{
    Pod_BLEFSM_t        fsm;
    uint8_t             flags;
    uint8_t             mlByte;
    uint8_t             inactiveAdvertisementRetries;    
} Pod_BLEState_t;


static Pod_BLEState_t s_Pod_BLEState;

static HAL_BroadcastPacket_t s_Pod_BLEAdvMDPkt;
static uint8_t s_Pod_BLEAdvMDXorBuffer[sizeof(HAL_BroadcastPacket_t)];

static void _Pod_BLEDriveAdvertising();

static void _Pod_BLETransitionToState(Pod_BLEFSM_t nextState)
{
    if(nextState == s_Pod_BLEState.fsm) return;

    MBT_LOG(">>> %d -> %d\n", s_Pod_BLEState.fsm, nextState);

    switch(nextState)
    {
        case BLS_OFF:
            // TODO: Check if previous state was connected to free up the involved resources
            Pod_powerClearActivity(PWR_ACT_BLE_ACTIVE);
            //Pod_UIStopSystemClip(SC_ML_CONNECT);
            s_Pod_BLEAdvMDPkt.obf.associationToken = s_Pod_BLEState.mlByte = 0;
            Pod_BLEUpdateAdvPacket();
        break;
        case BLS_ADVERTISING:
            Pod_powerSetActivity(PWR_ACT_BLE_ACTIVE);
            //Pod_UIPlaySystemClip(SC_ML_CONNECT);
        break;
        case BLS_CONNECTED:
            Pod_powerSetActivity(PWR_ACT_BLE_ACTIVE);
            
            //Pod_UIStopSystemClip(SC_ML_CONNECT);
            Pod_UIStopSystemClip(SC_ML_SYNC);
            Pod_BLEClearAdvFlags(POD_BLE_ADV_FLAGS_RECONNECTION);
        break;
    }

    s_Pod_BLEState.fsm = nextState;
}

static void _Pod_BLEEvalState(void)
{

    if(HAL_bleGetConnState() == HAL_BLE_CONN_STATE_CONNECTED)
    {
        _Pod_BLETransitionToState(BLS_CONNECTED);
        MBT_LOG("[BLE] CONNECTED\n");        
    } else {
        if(HAL_bleGetAdvState() != HAL_BLE_ADV_STATE_OFF)
        {
            _Pod_BLETransitionToState(BLS_ADVERTISING);
            MBT_LOG("[BLE] ADVERTISING\n");           
        } else {            
            _Pod_BLETransitionToState(BLS_OFF);            
            MBT_LOG("[BLE] OFF\n");
        }
    }
    //MBT_LOG("[BLE] CONN: %s\n", HAL_bleGetConnState() == HAL_BLE_CONN_STATE_CONNECTED ? "CONNECTED" : "DISCONNECTED");
    //MBT_LOG("[BLE] ADV: %s\n", HAL_bleGetAdvState() == HAL_BLE_ADV_STATE_OFF ? "IDLE" : "ADVERTISING");
}


void Pod_BLEInit(void)
{
    memset(&s_Pod_BLEState, 0, sizeof(s_Pod_BLEState));
    memset(&s_Pod_BLEAdvMDPkt, 0, sizeof(s_Pod_BLEAdvMDPkt));

    HAL_DeviceInfo_t *pDi = HAL_sysGetDevInfo();
    s_Pod_BLEAdvMDPkt.plain.packetVersion       = MBT_CFG_BLE_ADV_MANUFDATA_PACKET_VERSION;
    memcpy(&s_Pod_BLEAdvMDPkt.obf.productId, &pDi->factoryInfo.productId, sizeof(s_Pod_BLEAdvMDPkt.obf.productId));
    memcpy(&s_Pod_BLEAdvMDPkt.obf.fwVersionShort, &pDi->fwVersion, sizeof(s_Pod_BLEAdvMDPkt.obf.fwVersionShort));
    s_Pod_BLEAdvMDPkt.obf.fwFeatureLevel      = pDi->fwFeatureLevel;
    //s_Pod_BLEAdvMDPkt.advertisingFlags    = ... defined externally;
    s_Pod_BLEAdvMDPkt.obf.deviceId            = (uint32_t)(pDi->deviceId & 0xFFFFFFFF);
}

void Pod_BLETick(uint32_t ticks)
{    
}

void Pod_BLEHALEventHandler(const HAL_event_t *pEventData)
{    
    switch(pEventData->data.bleEvent.type)
    {
        case HALBLEEVENTTYPE_CONNECTION:               
            if(HAL_bleGetConnState() == HAL_BLE_CONN_STATE_DISCONNECTED)
                _Pod_BLEDriveAdvertising(); // Trigger slow advertising if disconnected
        case HALBLEEVENTTYPE_ADVERTISEMENT:
            _Pod_BLEEvalState();            
            if(s_Pod_BLEState.inactiveAdvertisementRetries > 0 && HAL_bleGetAdvState() == HAL_BLE_ADV_STATE_OFF)
            {
                _Pod_BLEDriveAdvertising();
                --s_Pod_BLEState.inactiveAdvertisementRetries;
            }
        break;
        case HALBLEEVENTTYPE_DATARX:
        {
//            size_t sz = HAL_bleGetAvailableReadBytes();
//            MBT_LOG("[BLE] DATARX (%d bytes:\n", sz);
//            uint8_t *pData;
//            HAL_bleReadBytes(&pData, &sz);
//            for(size_t i = 0; i < sz; ++i)
//            {
//                MBT_LOG("%x ", pData[i]);
//            }
//            HAL_bleFreeBytes(sz);
//            MBT_LOG("\n");
        }
        break;
        case HALBLEEVENTTYPE_RX_OVERFLOW:
            //MBT_LOG("[BLE] RX OVERFLOW\n");
        break;
        case HALBLEEVENTTYPE_TX_READY:
            //MBT_LOG("[BLE] TX READY\n");
        break;      
    } 
}

//HAL_BroadcastPacket_t* Pod_BLEGetAdvMDPacket(void)
//{
//    return &s_Pod_BLEAdvMDPkt;
//}

void Pod_BLEUpdateAdvPacket(void)
{
    // TODO: Update here the advertising flags
    // - ML detected
    // - Has linkPIN  -> Because it will be reset to 0 between broadcast sessions even if a link pin is present

    // Re-hash
    //s_Pod_BLEAdvMDPkt.hash = Utils_adler32((uint8_t*)&s_Pod_BLEAdvMDPkt.productId, sizeof(HAL_BroadcastPacket_t)-((uint32_t)&s_Pod_BLEAdvMDPkt.productId - (uint32_t)&s_Pod_BLEAdvMDPkt));
    // Re-XOR the packet
    // NOTE: This is a hardcoded dummy key. Not the production one.
    // Not hard to guess since it's just XOR obfuscation
    static const uint8_t xorKey[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    memcpy(&s_Pod_BLEAdvMDXorBuffer[sizeof(HAL_AdvPacketVersion_t)], (uint8_t*)&s_Pod_BLEAdvMDPkt + sizeof(HAL_AdvPacketVersion_t), sizeof(s_Pod_BLEAdvMDXorBuffer) - sizeof(HAL_AdvPacketVersion_t));
    Utils_xorBuffer(&s_Pod_BLEAdvMDXorBuffer[sizeof(s_Pod_BLEAdvMDPkt.plain)], sizeof(s_Pod_BLEAdvMDXorBuffer) - sizeof(s_Pod_BLEAdvMDPkt.plain), &xorKey[0], sizeof(xorKey));
}


static void _Pod_BLEDriveAdvertising()
{
    bool fast = false;
    bool advertise = false;

    static bool lastFast = false;


    // MLINK takes precedence
    if(s_Pod_BLEState.mlByte != 0)
    {
        s_Pod_BLEAdvMDPkt.obf.associationToken = s_Pod_BLEState.mlByte;
        s_Pod_BLEState.mlByte = 0;
        fast = true;
        advertise = true;
    } else if(/*justDisconnected && MBT_FLAG_IS_SET(s_Pod_BLEAdvMDPkt.obf.advertisingFlags, POD_BLE_ADV_FLAGS_RECONNECT)*/ 1)
    {        
        if(HAL_bleGetConnState() == HAL_BLE_CONN_STATE_CONNECTED)
            return;
        // Trigger reconnect
        fast = false;
        advertise = true;
    }

    Pod_BLEUpdateAdvPacket();

    // ------------------------------ TODO: MLLESS

    //Pod_BLEFSM_t bleState = Pod_BLEGetState();
    
    if(advertise)
    {
        if(!lastFast && fast)
        {
            HAL_bleStopAdvertising();
        }
        

        if((!lastFast && fast) || HAL_bleGetAdvState() == HAL_BLE_ADV_STATE_OFF)
        {
            MBT_LOG(">>> ");
            if(!HAL_bleIsAdvertising())
            {
                MBT_LOG("Start");
            } else {
                MBT_LOG("Update");
            }
            
            MBT_LOG(" advertise: %s\n", fast ? "FAST" : "SLOW");
            
            HAL_bleAdvertisingConfig_t cfg =
            {
                .initialAdvertisingData = 
                {
                    .pData = &s_Pod_BLEAdvMDXorBuffer[0],
                    .len = sizeof(s_Pod_BLEAdvMDXorBuffer)
                },
                .fast = fast
            };                
            HAL_bleStopAdvertising();
            HAL_bleStartAdvertising(&cfg);
            HAL_bleUpdateAdvertisingData(&cfg.initialAdvertisingData);
            lastFast = fast;            
        } else {        
            HAL_smallBuffer_t bfr = {
                .pData = &s_Pod_BLEAdvMDXorBuffer[0],
                .len = sizeof(s_Pod_BLEAdvMDXorBuffer)
            };
            HAL_bleUpdateAdvertisingData(&bfr);
        }
    }
}

// MagicLink event
void Pod_BLEMLHALEventHandler(const Pod_MLEventData_t *pData)
{
    switch(pData->type)
    {   
        case MLE_SIGNAL:
            //s_Pod_BLEState.mlByte = 0xFF;

            if(pData->data.signal.locked)                 
                Pod_BLESetAdvFlags(POD_BLE_ADV_FLAGS_MLDETECTED);
            else 
                Pod_BLEClearAdvFlags(POD_BLE_ADV_FLAGS_MLDETECTED);

            //_Pod_BLEDriveAdvertising();
            break;
        case MLE_DATA:
            s_Pod_BLEState.mlByte = pData->data.data.byte;
            Pod_BLESetAdvFlags(POD_BLE_ADV_FLAGS_MLDETECTED);
            _Pod_BLEDriveAdvertising();
        break;        
    }

    

//    Pod_BLEFSM_t bleState = Pod_BLEGetState();  
//   
//    if(pData->type == MLE_DATA)
//    {
//        mlByte = pData->data.byte;
//
//        if(bleState <= BLS_CONNECTING)
//        {
//            s_Pod_BLEAdvMDPkt.obf.associationToken = pData->data.data.byte;
//            Pod_BLEUpdateAdvPacket();
//        }
//
//        switch(bleState)
//        {
//            case BLS_OFF:        
//            {
//                HAL_bleAdvertisingConfig_t cfg =
//                {
//                    .initialAdvertisingData = 
//                    {
//                        .pData = &s_Pod_BLEAdvMDXorBuffer[0],
//                        .len = sizeof(s_Pod_BLEAdvMDXorBuffer)
//                    },
//                    .fast = true    // TODO: Check this
//                };
//
//                HAL_bleStartAdvertising(&cfg);
//            }
//            break;
//            case BLS_ADVERTISING:
//            {
//                HAL_smallBuffer_t bfr = {
//                    .pData = &s_Pod_BLEAdvMDXorBuffer[0],
//                    .len = sizeof(s_Pod_BLEAdvMDXorBuffer)
//                };
//                HAL_bleUpdateAdvertisingData(&bfr);
//            }
//            break;
//        }
//    }
}

Pod_BLEFSM_t Pod_BLEGetState(void)
{
    return s_Pod_BLEState.fsm;
}

void Pod_BLESetInactiveAdvertisementRetries(uint8_t retries)
{
/*
    if(enable)
    {
        MBT_FLAG_SET(s_Pod_BLEAdvMDPkt.obf.advertisingFlags,    POD_BLE_ADV_FLAGS_RECONNECT);
    } else {
        MBT_FLAG_CLEAR(s_Pod_BLEAdvMDPkt.obf.advertisingFlags,  POD_BLE_ADV_FLAGS_RECONNECT);
    }
*/
    s_Pod_BLEState.inactiveAdvertisementRetries = retries;
    if(retries > 0)
        _Pod_BLEDriveAdvertising();
    

    /*
    Pod_USUserSettingsFile_t settings = *(Pod_USGetUserSettings());
    MBT_FLAG_SET(settings.binSettings, POD_US_BIN_SETTTINGS_BYPASSML);
    Pod_USSetUserSettings(&settings);
    */
}

void Pod_BLESetMLL(bool enable)
{
    
}

void Pod_BLEAdvertisingLogic(void)
{
    _Pod_BLEDriveAdvertising();
}

void Pod_BLESetAdvFlags(uint8_t flags)
{
//    if(flags == POD_BLE_ADV_FLAGS_MLDETECTED)
//    {
//        MBT_LOG("ADVF SET\n");
//    }
    uint8_t oldFlags = s_Pod_BLEAdvMDPkt.obf.advertisingFlags;
    s_Pod_BLEAdvMDPkt.obf.advertisingFlags |= flags;
    if(oldFlags != s_Pod_BLEAdvMDPkt.obf.advertisingFlags)
        _Pod_BLEDriveAdvertising();
}

void Pod_BLEClearAdvFlags(uint8_t flags)
{
//    if(flags == POD_BLE_ADV_FLAGS_MLDETECTED)
//    {
//        MBT_LOG("ADVF CLEAR\n");
//    }
    uint8_t oldFlags = s_Pod_BLEAdvMDPkt.obf.advertisingFlags;
    s_Pod_BLEAdvMDPkt.obf.advertisingFlags &= ~flags;
    if(oldFlags != s_Pod_BLEAdvMDPkt.obf.advertisingFlags)
        _Pod_BLEDriveAdvertising();
}
 

void Pod_BLEDisconnect(void)
{
    if(Pod_BLEGetState() >= BLS_CONNECTING)
        HAL_bleDisconnect();
}
