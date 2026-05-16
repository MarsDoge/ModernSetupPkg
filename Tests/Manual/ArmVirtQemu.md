# ArmVirtQemu Manual Test

## Build

```sh
cd /Users/cy/github/edk2
ModernSetupPkg/Scripts/build-armvirt.sh
```

Expected result:

- Build exits successfully.
- `Build/ArmVirtQemu-AArch64/DEBUG_CLANGDWARF/FV/QEMU_EFI.fd` exists.
- `Build/ArmVirtQemu-AArch64/DEBUG_CLANGDWARF/FV/QEMU_VARS.fd` exists.

## Run

```sh
cd /Users/cy/github/edk2
GRAPHICS=1 RESET_VARS=1 ACCEL=hvf ModernSetupPkg/Scripts/run-armvirt.sh
```

If HVF is unavailable:

```sh
GRAPHICS=1 RESET_VARS=1 ACCEL=tcg ModernSetupPkg/Scripts/run-armvirt.sh
```

Expected result:

- QEMU opens a graphical window.
- Pressing `Esc` or `F2` during BDS enters `ModernSetupApp`.
- No ASSERT, exception, or GOP initialization failure is printed to serial.

## UI Checks

- Header shows firmware utility name, mode, architecture, and resolution.
- Top tab bar shows Dashboard, Boot, Devices, Security, and Exit.
- `Up` and `Down` move between tabs while tab focus is active.
- `Right` or `Enter` moves focus into the page content area.
- `Left` or `Esc` moves focus back to the top tab bar.
- `Tab` toggles between tab focus and content focus.
- Boot, Devices, and Exit pages show visible row/action selection in content
  focus.
- Dashboard, Boot, Devices, Security, and Exit render without overlapping text
  at 800x600 and 1024x768.
