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
- Manual keyboard navigation validation for tab focus and content focus.
- Manual HII bridge validation using edk2 DriverSampleDxe in the ArmVirt
  overlay.

## Planned Coverage

- Scripted QEMU serial-log smoke checks for `ModernSetupApp` launch.
- Layout geometry tests once `ModernUiLayoutLib` exists.
- Mock provider tests for boot, device, security, platform, and HII data
  libraries once those libraries are split from `ModernSetupApp`.
