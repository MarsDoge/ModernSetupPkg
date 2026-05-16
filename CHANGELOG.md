# Changelog

All notable ModernSetupPkg changes should be recorded here. The project uses
this file as both a release log and a lightweight development progress record.

## Unreleased

### Added

- Private repository bootstrap under `MarsDoge/ModernSetupPkg`.
- BSD-2-Clause-Patent licensing.
- Standalone edk2 package metadata: `ModernSetupPkg.dec` and
  `ModernSetupPkg.dsc`.
- `ModernSetupApp` prototype with Dashboard, Boot, Devices, Security, and Exit
  pages.
- GOP-backed renderer library with rectangle, panel, text, and progress
  primitives.
- Input adapter library for keyboard events and optional absolute pointer
  events.
- Theme library with dark modern firmware UI tokens.
- ArmVirtQemu overlay build and run scripts for macOS Apple Silicon validation.
- Development guide covering function comments, multi-architecture boundaries,
  IBV-friendly extension points, and change discipline.
- `Tests/` documentation for manual ArmVirtQemu validation and planned smoke/unit
  test locations.
- A top-tab firmware UI layout with central content focus and bottom hotkey
  strip, moving away from the original left-rail prototype.
- Boot page launch support through `UefiBootManagerLib`, including a footer
  status message when a selected `Boot####` option returns.
- `ModernUiStringLib` with Simplified Chinese defaults and English fallback
  strings selected by `PcdModernSetupDefaultLanguage`.
- Minimal built-in CJK bitmap glyph fallback generated from Noto Sans CJK SC
  Regular, plus font source/license documentation and a regeneration script.
- `ModernUiHiiBridgeLib` first-stage DriverSample HII/IFR bridge demo.
- ArmVirt overlay switch `MODERN_SETUP_DEMO_DRIVER_SAMPLE`, enabled by default,
  to include edk2 `DriverSampleDxe` without modifying upstream ArmVirt files.
- HII page in `ModernSetupApp` that lists DriverSample formsets, forms, and IFR
  rows, with limited ConfigAccess write-back for safe checkbox, one-of, and
  numeric buffer-varstore questions.
- 18px anti-aliased built-in glyph generation from Noto Sans CJK SC, including
  ModernSetup strings and selected DriverSample `.uni` strings.
- Compatibility policy for keeping the classic UiApp/FormBrowser path alongside
  ModernSetup until the modern HII bridge can safely cover existing VFR data.
- Runtime language switching from the Exit page, persisted through the
  `ModernSetupLanguage` UEFI variable.
- Language switching now uses a drop-down selector with explicit Chinese and
  English options.
- Renderer initialization prefers a GOP mode of at least 1024x768 when the
  firmware exposes one, avoiding the cramped 800x600 default where possible.
- GitHub-facing visual showcase guidance for ArmVirt captures under
  `Assets/Screenshots/`.
- More premium firmware chrome styling: richer dark theme tokens, accent mode
  pill, bordered resolution status, raised tabs, setting rows, and an aligned
  language selector.
- DriverSample HII bridge now uses a scope-aware IFR parser with first-stage
  expression evaluation for `suppressif`, `grayoutif`, and `disableif`.
- HII rows now carry runtime visible/disabled/callback/read-only state, show
  clearer control types, support scrolling through longer forms, and run
  ConfigAccess callbacks for form open/close and callback actions.
- `ModernUiPageAdapterLib` static GUID adapter registry, establishing the
  `FormSetGuid -> page adapter -> generic HII fallback` engine layer used by
  IBV-style setup stacks.
- `ModernUiHiiBridgeLib` can now load an optional caller-provided formset GUID
  filter, moving the DriverSample demo selection out of the parser internals.
- ArmVirt overlay build now wires `ModernUiPageAdapterLib` alongside renderer,
  theme, input, string, and HII bridge libraries.
- `ModernDisplayEngineDxe`, an edk2 DisplayEngine-compatible DXE driver that
  produces `EDKII_FORM_DISPLAY_ENGINE_PROTOCOL` and `EFI_HII_POPUP_PROTOCOL`.
- `ModernUiCustomizedDisplayLib`, a GOP-backed replacement for edk2
  `CustomizedDisplayLib` used by `ModernDisplayEngineDxe`.
- ArmVirt overlay now replaces `MdeModulePkg/Universal/DisplayEngineDxe` with
  `ModernDisplayEngineDxe` while keeping the native UiApp/FormBrowser setup
  entry.

### Fixed

- Fixed rectangle fill rendering by using GOP `EfiBltVideoFill` instead of
  passing a one-line buffer as a multi-line BLT source.
- Fixed text rendering setup by providing the HII Font output dimensions and
  clipping flags expected by edk2's direct-to-screen `StringToImage()` path.
- Added an explicit navigation/content focus model so Right/Enter moves into the
  current page content area, Left/Esc returns to navigation, and content rows or
  actions have visible selection.
- Aligned top-tab navigation with horizontal firmware UI behavior: Left/Right
  switch tabs, Down/Enter enters page content, and Up no longer changes tabs.
- Refresh boot options before drawing setup pages so shell and device boot
  entries are available when firmware exposes them.
- Truncate long Boot and Devices rows before rendering so device paths do not
  spill outside their panels at 800x600.
- Use measured mixed-width text truncation so Chinese, ASCII, and device-path
  rows stay inside the content panels.
- Clear the GOP surface when the DisplayEngine path clears the logical setup
  page, preventing stale graphics from surviving a native FormBrowser redraw.

### Current Status

- ArmVirt AARCH64 DEBUG_CLANGDWARF build has been validated locally.
- The default ArmVirt setup path enters native `UiApp`; setup rendering is
  handled by `ModernDisplayEngineDxe` through edk2's
  `EDKII_FORM_DISPLAY_ENGINE_PROTOCOL`.
- Device Manager, DriverSample, Boot Manager, and Boot Maintenance now use
  native UiApp/FormBrowser behavior in the default firmware image.
- Simplified Chinese and English switching through `ModernUiStringLib` applies
  to the prototype `ModernSetupApp`; native UiApp language behavior remains
  owned by edk2 HII/FormBrowser.
- The custom HII bridge is retained as a DriverSample-focused prototype/debug
  path, but it is no longer the default setup compatibility route.
- Native `SetupBrowserDxe/FormBrowser2` owns HII/IFR/VFR parsing, GUID formset
  handling, ConfigAccess callback flow, condition evaluation, and variable
  write semantics in the default ArmVirt firmware.
- The page adapter registry is present but ships with no OEM-specific adapters
  in the default native FormBrowser path.
- The latest validated FVMAIN state is 8360320 bytes total, 8360288 bytes used,
  and 32 bytes free in the ArmVirt AARCH64 DEBUG_CLANGDWARF build.
- `ModernDisplayEngineDxe` currently preserves most edk2 DisplayEngine behavior
  and routes its low-level text-cell drawing through GOP. Further visual polish
  should happen in the DisplayEngine/customized display layer, not in a separate
  IFR parser.
- The UI framework is still early: the DisplayEngine path is compatibility-first
  and still needs substantial visual polish.

### Planned

- Add `ModernUiLayoutLib` for resolution-aware layout and safe-area handling.
- Decide whether the standalone `ModernSetupApp` remains a showcase/debug tool
  or gets removed after the DisplayEngine path is stable.
- Add PCDs for default page, feature visibility, fallback behavior, pointer
  support, and theme selection.
- Improve `ModernDisplayEngineDxe` visual styling for panels, highlight rows,
  popups, input boxes, and high-density setup pages.
- Keep `ModernUiHiiBridgeLib` as a debug/demo path or remove it after the
  DisplayEngine architecture is stable.
- Extend validation beyond ArmVirt to OVMF and LoongArch targets.
