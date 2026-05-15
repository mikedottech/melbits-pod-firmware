# Build Setup

> **Note:** this repository is published as a portfolio archive. It is not a
> turn-key buildable tree — vendor SDKs, signing keys, audio assets, and
> tool binaries were removed before publication. The notes below describe
> what would be required to reconstruct a buildable environment for
> reference.

## Targets

| Project | Path | Toolchain |
|---|---|---|
| Main application firmware | `Main/Project/Pod.emProject` | SEGGER Embedded Studio for ARM 4.16+ |
| Secure DFU bootloader | `Secure Bootloader/secure_bootloader.eww` | SEGGER Embedded Studio for ARM 4.16+ |
| Board bring-up / QA firmware (UART) | `Board test firmware (UART)/ble_app_uart/ble_app_uart.eww` | SEGGER Embedded Studio for ARM 4.16+ |
| Radio certification firmware | `Certification/radio_test/radio_test.eww` | SEGGER Embedded Studio for ARM 4.16+ |

Target MCU: **Nordic nRF52810** (Cortex-M4, no FPU, 192 KB flash / 24 KB RAM)
with **SoftDevice S112** v6.1.1 (peripheral-only BLE stack).

## External dependencies

These were removed from the publication; obtain them from their original
sources before attempting to build.

### Nordic nRF5 SDK 15.3.0
Download from
[nordicsemi.com](https://www.nordicsemi.com/Products/Development-tools/nrf5-sdk).
The SEGGER project files (`*.emProject`) reference paths under a top-level
`NRF5SDK/` directory next to this folder.

### Nordic CMSIS device files
Place `mdk/Device/Include/` and `mdk/Device/Source/` from the SDK into
`Main/Project/src/nRF/Device/`. See `Main/Project/src/nRF/README.md`.

### Nordic command-line tools (host side)
- `nrfjprog` — flashing via J-Link
- `nrfutil` (`pip install nrfutil`) — generating DFU update packages

### Audio pipeline (host side)
- `ffmpeg`, `sox` — install via your package manager
- See `Main/Project/Resources/README.md` for the conversion workflow

### DFU signing key
The original ECDSA private key used to sign production DFU images was
**not published**. The corresponding public key (`Secure Bootloader/public_key.c`)
is included so the verification path can be inspected. To sign new images
you would generate a fresh key pair with `nrfutil keys generate` and
update the public key blob accordingly.

## Building (reference)

Once the dependencies are in place:

1. Open the appropriate `.emProject` or `.eww` file in SEGGER Embedded Studio.
2. Select a build configuration (e.g. `Pod_Production`, `Pod_Debug`,
   `Pod_QA_DFU` — see the `.jlink` flash scripts in `Main/Project/` for
   the matching flash plan).
3. Build and flash via J-Link. The `.jlink` files in `Main/Project/`
   automate the multi-image flash sequence (SoftDevice + bootloader +
   application + bootloader settings).
4. For OTA updates, packages are generated with `nrfutil pkg generate`
   using the build artifacts and the signing key.

## Layout overview

```
.
├── Main/                          Production firmware
│   ├── Project/
│   │   ├── Pod.emProject          SEGGER ES project
│   │   ├── src/                   ← Application source
│   │   ├── Buildscripts/          DFU package + factory image build scripts
│   │   ├── Resources/             Audio asset pipeline (assets removed)
│   │   └── *.jlink                Multi-image flash scripts
│   ├── config/
│   │   └── sdk_config.h           Nordic SDK component configuration
│   └── Feedbacks/                 Renoise (.xrni) instrument designs
├── Secure Bootloader/             Signed-DFU capable bootloader
├── Board test firmware (UART)/    Manufacturing/bring-up firmware
├── Certification/radio_test/      RF certification firmware (FCC/CE)
├── DFU/keys/public_key.c          Public half of the DFU signing keypair
└── Tools/
    ├── LZSS/                      Host LZSS encoder/decoder (PD, Okumura)
    └── ModConverter/              Audio/song asset converter (DuinoTune-based)
```
