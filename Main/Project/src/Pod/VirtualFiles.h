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

#ifndef _POD_VIRTUALFILES_H_
#define _POD_VIRTUALFILES_H_

#include "common.h"


typedef enum
{
    VFE_GETREALSIZE,
    VFE_FINISHWRITE
} Pod_VFEventType_t;

typedef struct
{
    uint16_t        id;
    const uint8_t*  pData;
    size_t          size;
    uint8_t         permissions;
    uint16_t        (*cb)(uint16_t id, Pod_VFEventType_t event);
} Pod_VFDesc_t;

#endif