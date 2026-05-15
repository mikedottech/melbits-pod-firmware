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

#include "UI.h"
#include "BuiltinAssets/Mods/BuiltinClips.h"

const Pod_UISystemClipEntry_t k_PodUIClipPriorities[SC_COUNT] =
{
   {
        900,
        &send_seed
    },
    {
        500,
        &send_seed_error
    },
    {
        900,
        &receive_seed
    },
    {
        500,
        &shake_start
    },
    {
        450,
        &shake_start_error
    },
    {
        450,
        &shake_start_success
    },
    {
        450,
        &incubation_complete
    },
    {
        500,
        &shake_incubation
    },
    {
        10,
        &battery_alarm
    },
    {
        1000,
        NULL        //*SC_ML_SYNC
    },
    {
        50,
        NULL        //*SC_ML_CONNECT
    },
    {
        460,
        NULL        //*SC_INCUBATION_REPORTPROGRESSAUTO
    },
    {
        1400,
        &incubation_ar  //*SC_INCUBATION_REPORTPROGRESS
    },        
    {
        50,
        NULL        //*SC_EMPTY
    },
    {
        0,
        NULL        //*SC_HWPANIC
    },
    {
        0,
        NULL        //*SC_LEDSON
    },
    {               
        0,
        NULL        //*SC_LIGHTSENSORTEST
    },
    {
        0,
        &charge_start //SC_WELCOME
    },
    {
        50,
        NULL        //*SC_FILE
    }
};
