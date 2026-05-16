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

For overlay verification without the DriverSample HII demo:

```sh
MODERN_SETUP_DEMO_DRIVER_SAMPLE=0 ModernSetupPkg/Scripts/build-armvirt.sh
```

Expected result:

- Build exits successfully.
- `Build/ArmVirtQemu-AArch64/DEBUG_CLANGDWARF/FV/QEMU_EFI.fd` exists.
- `Build/ArmVirtQemu-AArch64/DEBUG_CLANGDWARF/FV/QEMU_VARS.fd` exists.
- The default build firmware image contains both `ModernSetupApp` and
  `DriverSample`.
- FVMAIN space is recorded after each architecture-level change. The current
  adapter-engine build is expected to remain near full capacity.

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
- English fallback build shows Dashboard, Boot, Devices, Security, HII, and
  Exit.
- `Left` and `Right` move between tabs while tab focus is active.
- `Down` or `Enter` moves focus into the page content area.
- `Left` or `Esc` moves focus back to the top tab bar.
- `Tab` toggles between tab focus and content focus.
- Boot, Devices, and Exit pages show visible row/action selection in content
  focus.
- Boot page `Enter` launches the selected `Boot####` option. If the target
  returns, the footer shows the returned EFI status.
- Exit page includes a language row. Pressing `Enter` opens a language
  drop-down, `Up/Down` selects Simplified Chinese or English, and `Enter`
  applies the highlighted language.
- Language changes redraw the current screen immediately and keep the selection
  after reboot when `RESET_VARS=0`.
- Header resolution should prefer at least 1024x768 when the platform GOP
  exposes that mode; if only 800x600 is exposed, the UI remains usable there.
- Dashboard, Boot, Devices, Security, and Exit render without overlapping text
  at 800x600 and 1024x768.
- Long Boot descriptions and Devices device paths are truncated inside the
  content panel rather than spilling past the right edge.
- Chinese, ASCII, numbers, `Boot####`, and device paths can appear on the same
  screen without missing-glyph boxes for built-in UI text.

## HII Bridge Checks

- Open the HII or `高级设置` tab.
- The formset list shows DriverSample main setup and Inventory formsets when
  `MODERN_SETUP_DEMO_DRIVER_SAMPLE` is enabled.
- DriverSample is loaded through the app-provided formset GUID filter; the HII
  parser must not depend on DriverSample GUIDs internally.
- With the default package registry, DriverSample has no custom page adapter and
  therefore uses the generic HII renderer fallback.
- `Enter` opens a formset, then opens a form, then selects a row.
- DriverSample form rows render text, goto/ref, checkbox, one-of, numeric, and
  string questions without crashing or spilling past the content panel.
- Long DriverSample forms can be scrolled with `Up/Down`; selection is not
  limited to the first visible nine rows.
- On the first DriverSample setup page, changing the suppress/grayout selector
  updates dependent rows: suppressed rows disappear, grayed rows stay visible
  but cannot be edited, and unsupported conditions are marked disabled.
- Text rows show both prompt and secondary text when DriverSample provides both
  HII strings.
- Goto/ref rows navigate to local target forms. Callback-driven refs/actions
  invoke ConfigAccess callback flow and refresh the HII model afterward.
- Supported checkbox, one-of, and numeric rows that are buffer-varstore backed,
  not read-only, and not callback-driven can be advanced with `Enter`.
- Returning to the same form after a successful write shows the refreshed value.
- Callback, EFI varstore, name/value varstore, string, password, ordered list,
  date, and time rows remain read-only or disabled rather than being force
  written by ModernSetup.
- Action rows show a callback/action status instead of a generic unsupported
  row, and callback action requests are shown in the footer status line.
- Exit page fallback to classic UiApp still works, and DriverSample remains
  available there for comparison with the native FormBrowser.
