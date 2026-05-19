<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# OVMF X64 QEMU Manual Test

This is a manual local QEMU validation path for the X64 OVMF overlay. It is not
CI-active yet. Use it to confirm the OVMF overlay can build both native edk2
DisplayEngine and ModernDisplayEngine firmware images, and that local QEMU can
enter native UiApp/FormBrowser with the expected graphics behavior.

## Build

Run from an edk2 workspace that contains `MdePkg`, `OvmfPkg`, and
`ModernSetupPkg` at the workspace root:

```sh
cd /path/to/edk2
ModernSetupPkg/Scripts/build-ovmf-x64.sh
```

The default build uses `MODERN_SETUP_DISPLAY_ENGINE=modern` and `TOOL_CHAIN_TAG=GCC`.
Override `TARGET`, `TOOL_CHAIN_TAG`, `JOBS`, or `MODERN_SETUP_THEME` as needed for
the local edk2 toolchain.

For before/after DisplayEngine comparison, build the same OVMF X64 overlay with
the native edk2 DisplayEngine first, then rebuild with the ModernDisplayEngine
path:

```sh
MODERN_SETUP_DISPLAY_ENGINE=native ModernSetupPkg/Scripts/build-ovmf-x64.sh
MODERN_SETUP_DISPLAY_ENGINE=modern ModernSetupPkg/Scripts/build-ovmf-x64.sh
```

Expected result:

- The script generates `Build/ModernSetupPkgOverlay/OvmfX64ModernSetup.dsc`.
- The script generates `Build/ModernSetupPkgOverlay/OvmfX64ModernSetup.fdf`.
- With `MODERN_SETUP_DISPLAY_ENGINE=native`, the overlay keeps upstream
  `MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf`.
- With `MODERN_SETUP_DISPLAY_ENGINE=modern`, the overlay replaces upstream
  `DisplayEngineDxe` with `ModernDisplayEngineDxe` and maps
  `CustomizedDisplayLib` to `ModernUiCustomizedDisplayLib`.
- The modern overlay includes `ModernUiEngineLib`, `ModernUiRendererLib`, and
  `ModernUiThemeLib` in the DisplayEngine library path.
- The generated OVMF overlay does not require `ModernSetupApp` or the
  experimental HII bridge/App path.
- `Build/OvmfX64/DEBUG_GCC/FV/OVMF_CODE.fd` exists, or the equivalent
  `Build/OvmfX64*/${TARGET}_${TOOL_CHAIN_TAG}/FV/OVMF_CODE.fd` path exists for
  the selected local build settings.
- `Build/OvmfX64/DEBUG_GCC/FV/OVMF_VARS.fd` exists, or the equivalent
  `Build/OvmfX64*/${TARGET}_${TOOL_CHAIN_TAG}/FV/OVMF_VARS.fd` path exists for
  the selected local build settings.

For overlay-only validation without compiling firmware:

```sh
GENERATE_ONLY=1 ModernSetupPkg/Scripts/build-ovmf-x64.sh
```

## Run

```sh
cd /path/to/edk2
GRAPHICS=1 RESET_VARS=1 ModernSetupPkg/Scripts/run-ovmf-x64.sh
```

If the host has multiple QEMU installs, select the intended X64 binary and
graphics backend explicitly:

```sh
QEMU_BIN=/usr/bin/qemu-system-x86_64 DISPLAY_BACKEND=gtk GRAPHICS=1 RESET_VARS=1 ModernSetupPkg/Scripts/run-ovmf-x64.sh
```

Supported `DISPLAY_BACKEND` values are `gtk`, `sdl`, `cocoa`, and `none`. The
script uses split pflash images, copies `OVMF_VARS.fd` to
`Build/ModernSetupPkgOverlay/OvmfX64_VARS.fd` when `RESET_VARS=1`, and boots Q35
with USB keyboard/tablet input.

For serial-only debugging:

```sh
GRAPHICS=0 RESET_VARS=1 ModernSetupPkg/Scripts/run-ovmf-x64.sh
```

Expected result:

- QEMU opens an X64 OVMF graphical window when `GRAPHICS=1`.
- Pressing `Esc` during BDS, or using the OVMF setup entry exposed by the boot
  menu, enters native `UiApp`.
- UiApp rendering is handled by `ModernDisplayEngineDxe` in the default modern
  overlay build.
- Device Manager, Boot Manager, Boot Maintenance Manager, and Driver Health
  Manager remain native edk2 FormBrowser surfaces.
- No ASSERT, exception, or GOP initialization failure is printed to serial.

## UI Checks

- UiApp front page renders without falling back to the old text-only
  DisplayEngine path in the modern build.
- Device Manager, Boot Manager, Boot Maintenance Manager, Driver Health Manager,
  and available platform HII formsets keep native FormBrowser behavior.
- `Up/Down`, `Left/Right`, `Enter`, `Esc`, `F9`, and `F10` behave like the
  upstream DisplayEngine path.
- Header, footer help, page title, selectable rows, highlighted rows, disabled
  rows, and popups are readable through the GOP-backed modern drawing path.
- Box drawing frames, arrows, triangles, and checkbox glyphs render as
  single-cell graphics rather than wide missing-glyph placeholders.
- OVMF variable persistence works across a run when `RESET_VARS=0` reuses
  `Build/ModernSetupPkgOverlay/OvmfX64_VARS.fd`.

## Target Viewports

Validate the modern OVMF path at these local QEMU viewports or GOP modes when the
host/QEMU combination exposes them:

- 800x600: setup remains usable with no required controls hidden behind chrome.
- 1024x768: primary compatibility target; header, content area, right rail, and
  footer do not overlap.
- 1280x800: widescreen target; right-rail/status reservations remain readable and
  statement text does not spill into the rail.

If QEMU does not expose all modes, record the modes that were available and keep
at least one 1024x768-or-larger pass for the X64 OVMF manual validation note.

## Before / After Checks

- Build and run with `MODERN_SETUP_DISPLAY_ENGINE=native`.
- Record the UiApp front page and at least one Device Manager or Boot Manager
  surface.
- Rebuild and run with `MODERN_SETUP_DISPLAY_ENGINE=modern`.
- Compare the same surfaces at 1024x768 or the nearest available mode.
- Confirm row titles, form titles, keyboard navigation, save/default/discard
  flows, and popup behavior match between builds; only the drawing style should
  differ.

This path is intended for local maintainer validation now. Promote it to CI only
after the repository has a scripted OVMF build environment and QEMU smoke harness
that can run reliably on the selected CI host.
