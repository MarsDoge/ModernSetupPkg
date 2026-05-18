<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Before / After Capture Guide

The before/after comparison must use the same edk2 setup stack and the same HII
forms. Only the DisplayEngine changes.

```text
Before:
  UiApp + SetupBrowserDxe/FormBrowser2 + MdeModulePkg DisplayEngineDxe

After:
  UiApp + SetupBrowserDxe/FormBrowser2 + ModernDisplayEngineDxe
```

This proves ModernSetupPkg is a display engine replacement, not a second setup
browser.

## ArmVirt Before Build

```sh
cd /Users/cy/github/edk2
MODERN_SETUP_DISPLAY_ENGINE=native ModernSetupPkg/Scripts/build-armvirt.sh
GRAPHICS=1 RESET_VARS=1 ACCEL=hvf ModernSetupPkg/Scripts/run-armvirt.sh
```

Capture native edk2 DisplayEngine screenshots and save them under
`Assets/Screenshots/` with `before-` prefixes.

Recommended names:

- `before-armvirt-frontpage.png`
- `before-armvirt-device-manager.png`
- `before-armvirt-driver-sample.png`
- `before-armvirt-oneof-popup.png`

## ArmVirt After Build

```sh
cd /Users/cy/github/edk2
MODERN_SETUP_DISPLAY_ENGINE=modern ModernSetupPkg/Scripts/build-armvirt.sh
GRAPHICS=1 RESET_VARS=1 ACCEL=hvf ModernSetupPkg/Scripts/run-armvirt.sh
```

Capture ModernDisplayEngine screenshots with matching `after-` prefixes:

- `after-armvirt-frontpage.png`
- `after-armvirt-device-manager.png`
- `after-armvirt-driver-sample.png`
- `after-armvirt-oneof-popup.png`

## Scripted ArmVirt Capture

For repeatable baseline evidence, use the monitor-driven capture helper after
building the desired DisplayEngine path:

```sh
cd /Users/cy/github/edk2
CAPTURE_PREFIX=before-armvirt ModernSetupPkg/Scripts/capture-armvirt.sh
CAPTURE_PREFIX=after-armvirt ModernSetupPkg/Scripts/capture-armvirt.sh
```

The helper runs QEMU headless with `ramfb`, uses the QEMU monitor to send the
same key sequence, and writes PNG captures to `Assets/Screenshots/`.

Current scripted coverage:

- `frontpage`
- `frontpage-device-selected`
- `device-manager`
- `browser-testcase-selected`
- `driver-sample-first-page`
- `driver-sample-oneof-selected`
- `driver-sample-oneof-popup`

## LoongArch Before / After

Use the same switch on the LoongArch overlay:

```sh
cd ~/Desktop/github/edk2
MODERN_SETUP_DISPLAY_ENGINE=native ModernSetupPkg/Scripts/build-loongarchvirt.sh
GRAPHICS=1 RESET_VARS=1 ModernSetupPkg/Scripts/run-loongarchvirt.sh

MODERN_SETUP_DISPLAY_ENGINE=modern ModernSetupPkg/Scripts/build-loongarchvirt.sh
GRAPHICS=1 RESET_VARS=1 ModernSetupPkg/Scripts/run-loongarchvirt.sh
```

If the selected QEMU only supports `-bios`, the run still validates graphics,
but it does not validate variable persistence.

## Required Comparison Pages

- FrontPage: verifies native UiApp entry and page chrome.
- Device Manager: verifies platform HII formset enumeration.
- DriverSample first page: verifies mixed text, goto, checkbox, one-of,
  numeric, action, and ordered-list rows.
- A one-of popup or confirmation popup: verifies centered transient surfaces
  and native key handling.

## Review Checklist

- The same form title and row order appear in before and after captures.
- Native FormBrowser behavior is unchanged: Enter, Esc, F9, F10, and arrow keys
  keep their original meaning.
- The ModernDisplayEngine capture has stronger selection affordance without
  hiding disabled, grayed, or read-only state.
- There are no repeated missing-glyph warnings for box drawing, arrows,
  triangles, or checkbox symbols.
- Screenshots do not include commercial firmware artwork, logos, or copied
  vendor assets.
