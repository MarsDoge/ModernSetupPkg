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
|
+-- Smoke                  Planned scripted build/boot smoke tests.
|
+-- Unit                   Planned host-testable provider and layout tests.
```

## Current Coverage

- ArmVirtQemu AARCH64 build validation through `Scripts/build-armvirt.sh`.
- Manual QEMU graphics validation through `Scripts/run-armvirt.sh`.
- Manual native UiApp/FormBrowser navigation validation through
  `ModernDisplayEngineDxe`.
- Manual DriverSample validation through edk2 Device Manager/FormBrowser in the
  ArmVirt overlay.

## Planned Coverage

- Scripted QEMU serial-log smoke checks for native `UiApp` launch with
  `ModernDisplayEngineDxe` installed.
- Layout geometry tests once `ModernUiLayoutLib` exists.
- Host-side tests for renderer, theme, font measurement, and any provider
  libraries that remain outside the native FormBrowser path.
