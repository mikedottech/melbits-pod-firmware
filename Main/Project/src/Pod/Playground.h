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

#ifndef _POD_PLAYGROUND_H_
#define _POD_PLAYGROUND_H_

#include "common.h"

#include "HAL/HAL.h"

#include "Pod/Playground_Defs.h"

#include "mbt_config.h"
#include "Pod/MLINK.h"

void Pod_PGInit(void);
void Pod_PGReset(void);
void Pod_PGTick(uint32_t ticks);
uint32_t Pod_PGSend(Pod_PGPDU_t *ppdu, size_t len);

void Pod_PGHALEventHandler(const HAL_event_t *pEventData);

void Pod_PGClearLinkPIN();
void Pod_PGNotifyPINAccepted(void);
void Pod_PGMLHALEventHandler(const Pod_MLEventData_t *pData);

typedef struct
{
    Pod_PGPIN_t PGLinkPIN[3];
} Pod_PGRetainedData;

#endif