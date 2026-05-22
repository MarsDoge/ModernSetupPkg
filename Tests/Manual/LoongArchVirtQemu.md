<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# LoongArchVirtQemu Manual Test

## Build

LoongArchVirt follows upstream edk2's GCC toolchain path. Provide a LoongArch64
cross compiler before building:

```sh
cd /Users/cy/github/edk2
export GCC_LOONGARCH64_PREFIX=loongarch64-unknown-linux-gnu-
ModernSetupPkg/Scripts/build-loongarchvirt.sh
```

For before/after DisplayEngine comparison:

```sh
MODERN_SETUP_DISPLAY_ENGINE=native ModernSetupPkg/Scripts/build-loongarchvirt.sh
MODERN_SETUP_DISPLAY_ENGINE=modern ModernSetupPkg/Scripts/build-loongarchvirt.sh
```

Expected result:

- The script generates
  `Build/ModernSetupPkgOverlay/LoongArchVirtQemuModernSetup.dsc`.
- The script generates
  `Build/ModernSetupPkgOverlay/LoongArchVirtQemuModernSetup.fdf`.
- The overlay replaces upstream `DisplayEngineDxe` with
  `ModernDisplayEngineDxe`.
- With `MODERN_SETUP_DISPLAY_ENGINE=native`, the overlay keeps upstream
  `DisplayEngineDxe` for before/after comparison.
- The overlay keeps native `UiApp` as the setup entry.
- For the opt-in architecture-validation image,
  `MODERN_SETUP_REPLACE_UIAPP=1` keeps LoongArchVirt's original
  `PcdBootManagerMenuFile` pointing at the UiApp file GUID, replaces that FV
  file with `ModernSetupApp`, and adds native `BootManagerMenuApp` to the FV so
  ModernSetupApp's native fallback does not recurse into itself.
- `DriverSampleDxe` is included by default unless
  `MODERN_SETUP_DEMO_DRIVER_SAMPLE=0` is set.
- `Build/LoongArchVirtQemu/DEBUG_GCC/FV/QEMU_EFI.fd` exists.
- `Build/LoongArchVirtQemu/DEBUG_GCC/FV/QEMU_VARS.fd` exists.

If `${GCC_LOONGARCH64_PREFIX}gcc` or `${GCC_LOONGARCH64_PREFIX}objcopy` is not
available, the script should still be able to generate the overlay with
`GENERATE_ONLY=1` and should fail the real compile path with a clear toolchain
message.

## Run

```sh
cd /Users/cy/github/edk2
GRAPHICS=1 RESET_VARS=1 ModernSetupPkg/Scripts/run-loongarchvirt.sh
```

If the host has multiple QEMU installs, prefer one with `gtk` or `sdl` display
support:

```sh
QEMU_BIN=/usr/bin/qemu-system-loongarch64 GRAPHICS=1 RESET_VARS=1 ModernSetupPkg/Scripts/run-loongarchvirt.sh
```

The script automatically uses split pflash when the selected QEMU supports it.
If the selected QEMU only supports `-bios`, the firmware should still boot, but
variable persistence is not covered by that run.

Expected result:

- QEMU opens a LoongArch64 graphical window.
- Pressing `Esc` or `F2` during BDS enters native `UiApp`.
- UiApp rendering is handled by `ModernDisplayEngineDxe`.
- With `MODERN_SETUP_REPLACE_UIAPP=1`, the same firmware setup entry enters
  `ModernSetupApp`; use the app's native fallback action to launch
  `BootManagerMenuApp` for the native Boot#### launch path.
- Device Manager can enumerate platform HII formsets.
- DriverSample appears automatically when the demo driver is enabled.
- No ASSERT, exception, or GOP initialization failure is printed to serial.

For serial-only debugging:

```sh
GRAPHICS=0 RESET_VARS=1 ModernSetupPkg/Scripts/run-loongarchvirt.sh
```

## UI Checks

- Header architecture/status text should identify the LoongArch64 target rather
  than showing ArmVirt-only strings.
- Device Manager, Boot Manager, Boot Maintenance Manager, and DriverSample keep
  native FormBrowser behavior.
- `Up/Down`, `Left/Right`, `Enter`, `Esc`, `F9`, and `F10` behave like the
  upstream DisplayEngine path.
- Box drawing frames, arrows, triangles, and checkbox glyphs render as
  single-cell graphics rather than missing-glyph placeholders.
- 1024x768 and the default QEMU graphics mode do not show header, content,
  right rail, or footer overlap.
