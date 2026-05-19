<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Smoke Validation

`smoke_validate.py` is a lightweight host-side validation harness for
multi-agent maintenance. It does not build edk2, launch QEMU, or require a
firmware toolchain.

Run from the repository root:

```sh
python3 Tests/Smoke/smoke_validate.py
```

The smoke harness currently checks:

- `bash -n` syntax for `Scripts/*.sh` when `bash` is available.
- ArmVirt and LoongArchVirt overlay generation in `GENERATE_ONLY=1` mode against
  small synthetic edk2 source fixtures.
- Overlay writes remain under `Build/ModernSetupPkgOverlay`.
- `MODERN_SETUP_DISPLAY_ENGINE=native` output stays on the native edk2
  DisplayEngine/UiApp path.
- `MODERN_SETUP_DISPLAY_ENGINE=modern` output adds the ModernDisplayEngine and
  shared Modern UI libraries.
- Default firmware overlay outputs do not reference `ModernSetupApp`,
  `ModernUiHiiBridgeLib`, or `ModernUiPageAdapterLib`.
- `Application/ModernSetupApp/ModernSetupApp*.c` files are covered by
  `Application/ModernSetupApp/ModernSetupApp.inf` `[Sources]`, and every listed
  app `.c` source exists.
- `ModernSetupApp` keeps Dashboard drawing in `ModernSetupAppDashboard.c`, calls
  it from `ModernSetupAppPages.c`, and avoids direct experimental HII bridge,
  page adapter, or ConfigAccess coupling in app sources.
- Dashboard/provider summary pages consume read-only provider data through the
  app-private `ModernSetupAppProvider.c` snapshot instead of calling provider
  summary LibraryClasses directly from presentation modules.
- Provider health/readiness is derived in `ModernSetupAppProvider.c`, rendered by
  Dashboard, and included in Diagnostics without adding public API or coupling to
  experimental HII bridge/page adapter paths.
- The expanded Dashboard card set uses one app-private selectable-card count and
  remains backed by the normalized provider snapshot for firmware, diagnostics,
  power/thermal, and performance details.

Use this as the first validation for docs, ownership, script, and static overlay
changes. It complements, but does not replace, manual QEMU checks for firmware UI
behavior.
