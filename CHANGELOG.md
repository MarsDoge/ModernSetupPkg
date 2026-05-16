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

### Current Status

- ArmVirt AARCH64 DEBUG_CLANGDWARF build has been validated locally.
- QEMU smoke test reaches `ModernSetupApp` as the boot manager menu app.
- Boot order editing, Devices, and Security pages are intentionally read-only in
  the first prototype.
- Simplified Chinese is the default UI language; English can be selected at
  build time through `MODERN_SETUP_LANGUAGE=en-US` for the ArmVirt overlay.
- The HII bridge is a DriverSample-focused demo, not a complete FormBrowser
  replacement. Unsupported opcodes, callback questions, EFI varstores, and
  string editing remain read-only/fallback work.
- The UI framework is still early: platform data providers, layout library, and
  full write-capable setup flows are planned but not implemented.

### Planned

- Add `ModernUiLayoutLib` for resolution-aware layout and safe-area handling.
- Split platform data access out of `ModernSetupApp` into provider libraries.
- Add PCDs for default page, feature visibility, fallback behavior, pointer
  support, and theme selection.
- Expand the HII bridge beyond DriverSample and cover more IFR opcodes,
  expressions, text editing, and safe write policies.
- Extend validation beyond ArmVirt to OVMF and LoongArch targets.
