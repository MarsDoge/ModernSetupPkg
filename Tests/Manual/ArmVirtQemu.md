# ArmVirtQemu Manual Test

## Build

```sh
cd /Users/cy/github/edk2
ModernSetupPkg/Scripts/build-armvirt.sh
```

For overlay verification without the DriverSample HII demo driver:

```sh
MODERN_SETUP_DEMO_DRIVER_SAMPLE=0 ModernSetupPkg/Scripts/build-armvirt.sh
```

Expected result:

- Build exits successfully.
- `Build/ArmVirtQemu-AArch64/DEBUG_CLANGDWARF/FV/QEMU_EFI.fd` exists.
- `Build/ArmVirtQemu-AArch64/DEBUG_CLANGDWARF/FV/QEMU_VARS.fd` exists.
- The default build firmware image contains `ModernDisplayEngineDxe`, native
  `UiApp`, and `DriverSample`.
- `ModernUiEngineLib` is included in the default DisplayEngine library path.
- The generated ArmVirt overlay does not reference `ModernSetupApp`,
  `ModernUiHiiBridgeLib`, `ModernUiPageAdapterLib`, or `ModernUiStringLib`.
- FVMAIN space is recorded after each architecture-level change. The current
  DisplayEngine build is expected to remain near full capacity.

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

Expected result:

- QEMU opens a graphical window.
- Pressing `Esc` or `F2` during BDS enters native `UiApp`, rendered through
  `ModernDisplayEngineDxe`.
- No ASSERT, exception, or GOP initialization failure is printed to serial.
- Serial output does not repeatedly print missing glyph warnings for box
  drawing, arrows, triangles, or checkbox glyphs.

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
