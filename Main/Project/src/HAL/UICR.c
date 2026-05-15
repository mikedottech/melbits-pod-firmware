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

#include "common.h"
#include "mbt_config.h"
#include "HAL/HAL.h"

// UICR SETTINGS
#define MBT_FACTORY_STRUCT_VERSION (1)
#define MBT_FACTORY_PRODUCT_ID PRODUCT_ID   // Defined in project config preprocessor macros


#if defined(FACTORY)
#   if defined(DEV) || defined(DEBUG) || !defined(PRODUCTION) || defined(DFU)
#       error INCONSISTENT BUILD PREPROCESSOR MACROS!
#   endif
#endif


const MBT_FactoryInfo_t sk_FactoryInfo __attribute__((used, section(".uicr_customer"), aligned(4))) =
{
    .structVersion      = MBT_FACTORY_STRUCT_VERSION,
    .productCode        = MBT_CFG_FACTORY_PRODUCT_CODE,
    .productId          = MBT_FACTORY_PRODUCT_ID,
    .hwRevision         = MBT_CFG_FACTORY_HW_REVISION,
    .reserved           = 0x12345678,
};

#if ((defined(PRODUCTION) && defined(FACTORY)) /*|| (defined(DEV) && defined(DFU)*/ || defined(DFU))
// 0x208
// Enable debug port protection
const uint32_t __attribute__((used)) __attribute__((section(".uicr_appprotect"))) UICR_ADDR_0x208  __attribute__ ((aligned (4))) = 0x00000000;
#endif

const uint64_t __attribute__((used)) __attribute__((section(".uicr_pinreset"))) UICR_ADDR_0x200  __attribute__ ((aligned (4))) = 0xFFFFFFFFFFFFFFFF;