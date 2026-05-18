<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# ArmVirtQemu Manual Test

## Build

```sh
cd /Users/cy/github/edk2
ModernSetupPkg/Scripts/build-armvirt.sh
```

Build the optional standard front-page app ESP:

```sh
cd /Users/cy/github/edk2
ModernSetupPkg/Scripts/build-modern-app.sh
```

For overlay verification without the DriverSample HII demo driver:

```sh
MODERN_SETUP_DEMO_DRIVER_SAMPLE=0 ModernSetupPkg/Scripts/build-armvirt.sh
```

For before/after comparison, build the same ArmVirt overlay with the native edk2
DisplayEngine first, then rebuild with the ModernDisplayEngine path:

```sh
MODERN_SETUP_DISPLAY_ENGINE=native ModernSetupPkg/Scripts/build-armvirt.sh
MODERN_SETUP_DISPLAY_ENGINE=modern ModernSetupPkg/Scripts/build-armvirt.sh
```

After each build, capture the repeatable baseline pages:

```sh
CAPTURE_PREFIX=before-armvirt ModernSetupPkg/Scripts/capture-armvirt.sh
CAPTURE_PREFIX=after-armvirt ModernSetupPkg/Scripts/capture-armvirt.sh
```

Expected result:

- Build exits successfully.
- `Build/ArmVirtQemu-AArch64/DEBUG_CLANGDWARF/FV/QEMU_EFI.fd` exists.
- `Build/ArmVirtQemu-AArch64/DEBUG_CLANGDWARF/FV/QEMU_VARS.fd` exists.
- The default build firmware image contains `ModernDisplayEngineDxe`, native
  `UiApp`, and `DriverSample`.
- With `MODERN_SETUP_DISPLAY_ENGINE=native`, the generated overlay keeps
  `MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf` and does not
  inject `ModernDisplayEngineDxe`.
- With `MODERN_SETUP_DISPLAY_ENGINE=modern`, the generated overlay replaces
  edk2 `DisplayEngineDxe` with `ModernDisplayEngineDxe`.
- `ModernUiEngineLib` is included in the default DisplayEngine library path.
- The generated ArmVirt overlay does not reference `ModernSetupApp`,
  `ModernUiHiiBridgeLib`, `ModernUiPageAdapterLib`, or `ModernUiStringLib`.
- FVMAIN space is recorded after each architecture-level change. The current
  DisplayEngine build is expected to remain near full capacity.
- `Build/ModernSetupAppEsp/EFI/BOOT/BOOTAA64.EFI` exists after
  `build-modern-app.sh`.

## Run

```sh
cd /Users/cy/github/edk2
GRAPHICS=1 RESET_VARS=1 ACCEL=hvf ModernSetupPkg/Scripts/run-armvirt.sh
```

The run script defaults HVF to `gic-version=2`; QEMU HVF with ArmVirt
`gic-version=3` can stop advancing during the BDS wait countdown on macOS.
Use `GIC_VERSION=3` only when explicitly validating that combination.

If HVF is unavailable:

```sh
GRAPHICS=1 RESET_VARS=1 ACCEL=tcg ModernSetupPkg/Scripts/run-armvirt.sh
```

Attach the optional ModernSetupApp ESP while preserving native UiApp in firmware:

```sh
GRAPHICS=1 RESET_VARS=1 ACCEL=hvf DUAL_APP=1 ModernSetupPkg/Scripts/run-armvirt.sh
```

Expected result:

- QEMU opens a graphical window.
- Pressing `Esc` or `F2` during BDS enters native `UiApp`, rendered through
  `ModernDisplayEngineDxe`.
- No ASSERT, exception, or GOP initialization failure is printed to serial.
- Serial output does not repeatedly print missing glyph warnings for box
  drawing, arrows, triangles, or checkbox glyphs.
- With `DUAL_APP=1`, Boot Manager can see the removable ESP path for
  `ModernSetupApp` while native UiApp remains available.

## UI Checks

- UiApp front page, Device Manager, Boot Manager, Boot Maintenance Manager, and
  Driver Health Manager render without falling back to the old text-only
  DisplayEngine path.
- Header, footer help, page title, selectable rows, highlighted rows, disabled
  rows, and popups are drawn through the shared `ModernUiEngineLib` plus the
  GOP-backed renderer.
- `Up/Down`, `Left/Right`, `Enter`, `Esc`, `F9`, and `F10` keep native
  FormBrowser behavior.
- Boot Manager can launch a selected `Boot####` option. If the target returns,
  UiApp remains responsive.
- Header resolution should prefer at least 1024x768 when the platform GOP
  exposes that mode; if only 800x600 is exposed, native setup remains usable.
- UiApp, Device Manager, and DriverSample render without overlapping text at
  800x600 and 1024x768.
- Chinese, ASCII, numbers, `Boot####`, and device paths can appear on the same
  screen without missing-glyph boxes for strings covered by the built-in font
  subset or platform HII font.
- Box drawing frames, arrows, triangles, and checkbox glyphs render as
  single-cell graphics rather than wide missing-glyph placeholders.

## Native FormBrowser / DriverSample Checks

- Open `Device Manager`.
- DriverSample main setup and Inventory formsets appear automatically when
  `MODERN_SETUP_DEMO_DRIVER_SAMPLE` is enabled. ModernSetup must not maintain a
  separate DriverSample GUID filter in the default setup path.
- `Enter` opens a DriverSample formset, then opens forms and rows using native
  FormBrowser navigation.
- DriverSample form rows render text, goto/ref, checkbox, one-of, numeric,
  string/password, ordered list, date, time, action, and reset button controls
  without crashing or spilling past the content panel.
- Long DriverSample forms scroll with the same behavior as native
  `DisplayEngineDxe`.
- On the first DriverSample setup page, changing the suppress/grayout selector
  updates dependent rows through native FormBrowser condition evaluation.
- Text rows show both prompt and secondary text when DriverSample provides both
  HII strings.
- Goto/ref rows navigate to local target forms. Callback-driven refs/actions
  invoke DriverSample ConfigAccess callback flow.
- Checkbox, one-of, numeric, string/password, date/time, ordered list, action,
  reset, default, submit, discard, and confirm popups behave the same as edk2
  `DisplayEngineDxe`.
- F9/F10 default/save flows and Esc discard/exit confirmation work without
  ASSERTs or stale graphics.

## Before / After Checks

- Build and capture FrontPage with `MODERN_SETUP_DISPLAY_ENGINE=native`.
- Rebuild and capture the same FrontPage with `MODERN_SETUP_DISPLAY_ENGINE=modern`.
- Repeat for Device Manager, DriverSample first page, and a one-of or confirm
  popup.
- Confirm row titles, form titles, and navigation behavior match between the two
  builds; only the drawing style should differ.
- Current scripted capture covers FrontPage, Device Manager, DriverSample first
  page, and DriverSample one-of popup. Confirmation popup and edit-input flows
  remain manual checks.

## Standard Front-Page App Checks

- Boot `ModernSetupApp` from the ESP with `APP=1` or through Boot Manager with
  `DUAL_APP=1`.
- Dashboard shows firmware vendor/revision, display mode, boot option count,
  architecture, memory size, and Secure Boot summary without ASSERTs.
- Boot page lists dynamic visible `Boot####` entries from `BootOrder` and can
  launch the selected option.
- Devices page lists HII formset entries. Selecting an entry opens native
  FormBrowser via `EFI_FORM_BROWSER2_PROTOCOL.SendForm()`; the app must not
  show the old self-parsed HII bridge page.
- Security page shows Secure Boot, Setup Mode, PK, KEK, db, and dbx as
  read-only state.
- Exit page language switching and fallback to classic UiApp still work.
