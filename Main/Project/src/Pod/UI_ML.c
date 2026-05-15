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

#include "Pod/UI_ML.h"
#include "Pod/UI.h"
#include "Pod/BLE.h"

#include "HAL/Debug.h"

void Pod_UIMLHALEventHandler(const Pod_MLEventData_t *pData)
{
    if(pData->type == MLE_SIGNAL)
    {
        MBT_LOG("[ML] SIGNAL: %s\n", pData->data.signal.locked ? "LOCKED":"LOST");
        if(pData->data.signal.locked)
        {
            if(Pod_BLEGetState() != BLS_CONNECTED)
                Pod_UIPlaySystemClip(SC_ML_SYNC);
        } else {
            Pod_UIStopSystemClip(SC_ML_SYNC);
        }
    } else if(pData->type == MLE_DATA)
    {
        MBT_LOG("[ML] BYTE: 0x%x\n", pData->data.data);
    }
}
