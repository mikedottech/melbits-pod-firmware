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

#include "Pod/FileSys.h"
#include "Pod/Storage.h"
#include "Pod/FileIds.h"
#include "Pod/VirtualFiles.h"

#include "utils.h"

extern const Pod_VFDesc_t g_Pod_VFList[POD_VIRTUALFILES_COUNT];

// ------------------------------------------------------------------------------

static const Pod_VFDesc_t *_Pod_FSys_SearchVF(uint16_t id)
{
    for(uint16_t i = 0; i < POD_VIRTUALFILES_COUNT; ++i)
    {
        const Pod_VFDesc_t *pCur = &g_Pod_VFList[i];
        if(pCur->id == id) return pCur;
    }
    return NULL;
}

// ------------------------------------------------------------------------------

void Pod_FSysGetFile(uint16_t id, Pod_FSFileDesc_t *pDesc)
{
    const Pod_VFDesc_t *pvf = _Pod_FSys_SearchVF(id);
    bool searchInFlash = true;

    memset(pDesc, 0, sizeof(Pod_FSFileDesc_t));

    if(pvf)
    {
        // No need to search in flash
        pDesc->pData        = pvf->pData;
        pDesc->size         = MIN(pvf->size, (pvf->cb ? pvf->cb(id, VFE_GETREALSIZE) : (uint16_t)-1));
        pDesc->permissions  = pvf->permissions;
        pDesc->flags        = FS_FLAG_ISVIRTUAL;
        pDesc->pvf          = pvf;
        if(!MBT_FLAG_IS_SET(pvf->permissions, FS_PERM_FLASHOVERLAY))
        {
            searchInFlash = false;
        }
    }
    
    if(searchInFlash)
    {
        const Pod_storageFD_t * pff = Pod_storageGetFD(id);
        if(pff)
        {
            pDesc->pData = Pod_storageGetPtr(pff);
            pDesc->size = pff->size;
            pDesc->permissions = FS_PERM_R;
        }
    }
}
