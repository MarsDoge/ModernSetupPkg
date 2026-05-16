# ArmVirtQemu Manual Test

## Build

```sh
cd /Users/cy/github/edk2
ModernSetupPkg/Scripts/build-armvirt.sh
```

For English fallback verification:

```sh
MODERN_SETUP_LANGUAGE=en-US ModernSetupPkg/Scripts/build-armvirt.sh
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
- Default build shows Simplified Chinese UI strings for the header, tabs,
  page titles, footer hints, and status text.
- English fallback build shows Dashboard, Boot, Devices, Security, and Exit.
- `Left` and `Right` move between tabs while tab focus is active.
- `Down` or `Enter` moves focus into the page content area.
- `Left` or `Esc` moves focus back to the top tab bar.
- `Tab` toggles between tab focus and content focus.
- Boot, Devices, and Exit pages show visible row/action selection in content
  focus.
- Boot page `Enter` launches the selected `Boot####` option. If the target
  returns, the footer shows the returned EFI status.
- Dashboard, Boot, Devices, Security, and Exit render without overlapping text
  at 800x600 and 1024x768.
- Long Boot descriptions and Devices device paths are truncated inside the
  content panel rather than spilling past the right edge.
- Chinese, ASCII, numbers, `Boot####`, and device paths can appear on the same
  screen without missing-glyph boxes for built-in UI text.
