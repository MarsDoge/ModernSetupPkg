<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Tests

ModernSetupPkg tests are split by verification level. Early firmware UI work is
still mostly QEMU/manual validation, but the layout below keeps the path clear
for build smoke tests and provider-level unit tests.

```text
Tests
|
+-- Manual
|   |
|   +-- ArmVirtQemu.md       Manual graphics, input, and navigation checks.
|   +-- LoongArchVirtQemu.md Manual LoongArchVirt build and graphics checks.
|
+-- Smoke
|   |
|   +-- smoke_validate.py  Lightweight host-side repository invariant checks.
|
+-- Unit                   Planned host-testable provider and layout tests.
```

## Current Coverage

- ArmVirtQemu AARCH64 build validation through `Scripts/build-armvirt.sh`.
- LoongArchVirtQemu LOONGARCH64 overlay generation through
  `Scripts/build-loongarchvirt.sh`; full compile requires an external
  LoongArch GCC/binutils cross toolchain.
- Manual QEMU graphics validation through `Scripts/run-armvirt.sh`.
- Manual native UiApp/FormBrowser navigation validation through
  `ModernDisplayEngineDxe`.
- Manual DriverSample validation through edk2 Device Manager/FormBrowser in the
  ArmVirt overlay.
- Manual `ModernSetupApp` front-page validation from the ESP path, including
  dashboard, dynamic boot entries, HII formset entry enumeration, security
  summary, and FormBrowser2 handoff.
- Manual `ModernSetupApp` productization checks against
  `Docs/ProductizationFeatureMatrix.md`, currently covering dashboard, boot,
  devices/HII, security, firmware update, diagnostics/logs, management, and
  power/thermal, performance/tuning, and exit. The firmware, diagnostics,
  management, power, and performance pages are read-only provider summaries in
  this phase.
- Static overlay validation that the default path does not reference
  `ModernSetupApp` or custom HII bridge libraries.
- Static LoongArchVirt overlay validation that the default path uses native
  UiApp plus `ModernDisplayEngineDxe`.
- Lightweight scripted smoke validation through
  `Tests/Smoke/smoke_validate.py`, covering shell syntax, overlay generation
  dry runs, native/modern overlay separation, default overlay exclusion of
  `ModernSetupApp` and experimental HII bridge paths, `ModernSetupApp.inf`
  source coverage, app-internal Dashboard/FormBrowser boundary checks, the
  app-private provider snapshot boundary for read-only summary pages, and derived
  Dashboard/Diagnostics provider health summary coverage without edk2 or QEMU.
- Manual before/after validation by rebuilding the same overlay with
  `MODERN_SETUP_DISPLAY_ENGINE=native` and `MODERN_SETUP_DISPLAY_ENGINE=modern`.
- Scripted ArmVirt before/after capture through
  `Scripts/capture-armvirt.sh`, currently covering FrontPage, Device Manager,
  DriverSample first page, and a DriverSample one-of popup.

## Compatibility Documentation

- `Docs/CompatibilityMatrix.md` tracks platform, FormBrowser surface, and IFR
  question coverage.
- `Docs/BeforeAfter.md` describes how to capture native edk2 DisplayEngine
  screenshots and matching ModernDisplayEngine screenshots from the same HII
  pages.
- `Docs/ProductizationFeatureMatrix.md` tracks standard App feature coverage
  across x86, ARM, RISC-V, and LoongArch product classes.
- `Docs/IbvAndPlatformSetupSurvey.md` records the public IBV/OEM/form-factor
  survey used to decide common App surfaces.

## Lightweight Smoke Validation

Run the host-side smoke harness from the repository root before opening PRs that
touch scripts, overlay generation, governance docs, or default/native/modern
path separation:

```sh
python3 Tests/Smoke/smoke_validate.py
```

The harness uses tiny synthetic edk2 source fixtures and `GENERATE_ONLY=1` to
validate overlay behavior. It does not require an edk2 checkout, firmware
toolchains, or QEMU.

## Planned Coverage

- Scripted QEMU serial-log smoke checks for native `UiApp` launch with
  `ModernDisplayEngineDxe` installed.
- Layout geometry tests once `ModernUiLayoutLib` exists.
- Host-side tests for renderer, theme, font measurement, and any provider
  libraries that remain outside the native FormBrowser path.
