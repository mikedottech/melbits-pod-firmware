# nRF Vendor Files

The Nordic Semiconductor MDK device support files (CMSIS device headers,
startup code, system files for the nRF51 / nRF52 / nRF91 families) were
removed from this portfolio publication. They are vendor code, available
directly from Nordic, and were never authored or modified by me.

The SEGGER Embedded Studio project (`Pod.emProject`) and the source code in
`src/` reference these headers (`nrf52810.h`, `nrf_peripherals.h`, etc.). To
restore a buildable tree:

1. Download the Nordic MDK from
   https://www.nordicsemi.com/Products/Development-tools/nrf5-sdk
   (use the version matching the SDK declared in `src/Pod/BLE.h`,
   originally `15.3.0`).
2. Copy `mdk/Device/Include/` and `mdk/Device/Source/` into this directory
   so the resulting layout is `Main/Project/src/nRF/Device/Include/...` and
   `Main/Project/src/nRF/Device/Source/...`.
3. Files are distributed by Nordic under the BSD-style license bundled with
   the SDK.

This shim README is intentional — it preserves the project structure so that
references in `Pod.emProject` remain meaningful, while keeping vendor code
out of the portfolio repository.
