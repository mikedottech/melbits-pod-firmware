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

#include "EventServer.h"
#include "Pod/Playground_Defs.h"
#include "Pod/Playground.h"

#include "utils.h"

enum
{
    ESS_MLINK,
    ESS_POWER,
    ESS_MOTION,
};

typedef struct
{
    uint32_t eventSource;
//    struct
//    {
//        Pod_PGCommonHeader_t        header;
//        Pod_PGEvtEventSvrPayload_t  events;
//    } eventBuffer;
} Pod_ESState_t;

static Pod_ESState_t s_Pod_ESState;

void Pod_ESInit(void)
{
    Pod_ESReset();
}

void Pod_ESReset(void)
{
    memset(&s_Pod_ESState, 0, sizeof(Pod_ESState_t));
}

void Pod_ESTick(uint32_t ticks)
{
    //printf("%08x\n", s_Pod_ESState.eventSource);
}

void Pod_ESSetEventsMask(uint32_t mask)
{

}
//Pod_ESNotifyEventSource(ESE_ACT_STATECHANGED)
void Pod_ESNotifyEventSource(uint32_t mask)
{
    MBT_FLAG_SET(s_Pod_ESState.eventSource, mask);
    HAL_sysSchedulePendingTick();
}

void Pod_ESTXSlotHandler(void)
{    
    if(s_Pod_ESState.eventSource != 0)
    {        
        Pod_PGPDU_t pdu = {0};

        pdu.header.chan = POD_PG_CHANNEL_EVENT;
        pdu.header.code = POD_PG_EVCODE_EVTSRV;

        pdu.payload.evt.events.masked = s_Pod_ESState.eventSource;

        const uint16_t pduSize = sizeof(Pod_PGCommonHeader_t) + sizeof(Pod_PGEvtEventSvrPayload_t);

        if(Pod_PGSend(&pdu, pduSize) == 0)
        {
            s_Pod_ESState.eventSource = 0;
        }
    }

/*
    // Try to tx pending packet
    s_Pod_ESState.eventBuffer.header.chan = POD_PG_CHANNEL_EVENT;

    if(MBT_FLAG_IS_SET(s_Pod_ESState.eventSource, ESS_MLINK))
    {
    }

    if(s_Pod_ESState.eventBuffer.events.activity ||
        s_Pod_ESState.eventBuffer.events.masked)
    {
        if(Pod_PGSend((Pod_PGPDU_t*)&s_Pod_ESState.eventBuffer, sizeof(s_Pod_ESState.eventBuffer)) == 0)
        {
            memset(&s_Pod_ESState.eventBuffer.events, 0, sizeof(s_Pod_ESState.eventBuffer.events));
        }
    }
    */
}

void Pod_ESMLEventHandler(const Pod_MLEventData_t *pData)
{
    if(pData->type == MLE_SIGNAL)
    {       
        Pod_ESNotifyEventSource(pData->data.signal.locked ? ESE_ML_LOCKED : ESE_ML_LOST);
    }
}

void Pod_ESpowerEventHandler(const Pod_powerEventData_t *pData)
{
    uint32_t msk = 0;
    switch(pData->type)
    {
        case PET_BATTSTATUS:
        {
            if(pData->data.battStatus.status == BS_CHARGING)
            {
                msk = ESE_PWR_BATTCHARGING;
            } else if(pData->data.battStatus.status == BS_CHARGED)
            {
                msk = ESE_PWR_BATTCHARGED;
            }
        }
        break;
        case PET_DEADBATT:
            msk = ESE_PWR_BATTDEAD;
        break;
        case PET_LOWBATT:
            msk = ESE_PWR_BATTLOW;
        break;
        case PET_EXTPOWER_ATTACHED:
            msk = ESE_PWR_EXTPOWERATTACHED;            
        break;
        case PET_EXTPOWER_REMOVED:
            msk = ESE_PWR_EXTPOWERREMOVED;
        break;
    }
    Pod_ESNotifyEventSource(msk);
}
