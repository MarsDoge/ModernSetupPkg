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

### Fixed

- Fixed rectangle fill rendering by using GOP `EfiBltVideoFill` instead of
  passing a one-line buffer as a multi-line BLT source.
- Fixed text rendering setup by providing the HII Font output dimensions and
  clipping flags expected by edk2's direct-to-screen `StringToImage()` path.
- Added an explicit navigation/content focus model so Right/Enter moves into the
  current page content area, Left/Esc returns to navigation, and content rows or
  actions have visible selection.

### Current Status

- ArmVirt AARCH64 DEBUG_CLANGDWARF build has been validated locally.
- QEMU smoke test reaches `ModernSetupApp` as the boot manager menu app.
- Boot, Devices, and Security pages are intentionally read-only in the first
  prototype.
- The UI framework is still early: platform data providers, layout library, HII
  bridge, and write-capable setup flows are planned but not implemented.

### Planned

- Add `ModernUiLayoutLib` for resolution-aware layout and safe-area handling.
- Split platform data access out of `ModernSetupApp` into provider libraries.
- Add PCDs for default page, feature visibility, fallback behavior, pointer
  support, and theme selection.
- Add HII bridge experiments for reading existing setup data and strings.
- Extend validation beyond ArmVirt to OVMF and LoongArch targets.
