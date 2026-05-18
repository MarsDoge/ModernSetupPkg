<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

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
- `Experimental/ModernSetupApp.dsc` for explicitly building the legacy
  ModernSetupApp/HII bridge prototype outside the default DisplayEngine path.
- First IBV-style DisplayEngine chrome skeleton: procedural dark pattern bands,
  top advanced-mode navigation chrome, right-side status rail, content area
  reservation, and bottom action strip drawn in the GOP-backed customized
  display path without vendor artwork or a custom HII parser.
- AORUS-like black/orange theme tokens for DisplayEngine rendering, including
  header pattern, orange/yellow accents, selected bands, popup border, warning
  text, and telemetry text colors.
- Shared layout helper for DisplayEngine chrome and native statement area
  reservation, with the right telemetry rail enabled only on sufficiently wide
  text grids.
- Project-provided logo asset under `Assets/Brand/` for repository showcase
  use. It is intentionally not included in the default firmware image.
- Build-time DisplayEngine theme selection through
  `PcdModernSetupTheme`/`MODERN_SETUP_THEME`, with black/orange as the default
  and black/deep-red/yellow retained as an experimental option.
- `ModernUiEngineLib`, a shared visual model and drawing layer for page chrome,
  tabs, statement rows, value controls, popups, footers, help surfaces, and the
  right-side telemetry rail.
- `ModernUiPlatformDataLib`, `ModernUiBootDataLib`, `ModernUiDeviceDataLib`,
  and `ModernUiSecurityDataLib` as the standard front-page app data layer.
- Dual-entry ArmVirt run mode through `DUAL_APP=1`, attaching the
  `ModernSetupApp` ESP while preserving native UiApp in firmware.
- LoongArchVirtQemu overlay build and run scripts for LOONGARCH64 validation
  with native UiApp plus `ModernDisplayEngineDxe`.
- LoongArchVirtQemu manual validation guide covering GCC toolchain checks,
  graphics boot, Device Manager, DriverSample, and FormBrowser compatibility.
- v0.5 compatibility documentation, including `Docs/CompatibilityMatrix.md`
  and `Docs/BeforeAfter.md`.
- Scripted ArmVirt before/after screenshot capture helper for the native edk2
  DisplayEngine and ModernDisplayEngine paths.
- v0.5 before/after ArmVirt screenshot set covering FrontPage, Device Manager,
  DriverSample first page, and DriverSample one-of popup.

### Changed

- The main package DSC now builds only the DisplayEngine path:
  `ModernDisplayEngineDxe`, `ModernUiCustomizedDisplayLib`,
  `ModernUiEngineLib`, `ModernUiRendererLib`, and `ModernUiThemeLib`.
- The ArmVirt overlay no longer injects prototype-only `ModernUiInputLib`,
  `ModernUiStringLib`, `ModernUiHiiBridgeLib`, or `ModernUiPageAdapterLib`
  library mappings.
- `ModernSetupApp`, the custom HII bridge, and the GUID page adapter registry
  are now treated as experimental prototype code instead of setup compatibility
  infrastructure.
- Shared visual primitives from the experimental app are now promoted into
  `ModernUiRendererLib`, so both `ModernDisplayEngineDxe` and the legacy app use
  the same public renderer interfaces for color blending, measured text
  truncation, and selectable row surfaces.
- `Scripts/build-modern-app.sh` now builds the opt-in `ModernSetupApp`
  and prepares a bootable ArmVirt ESP at `Build/ModernSetupAppEsp`.
- `Scripts/run-armvirt.sh` now supports `APP=1` to boot the opt-in app
  from `\EFI\BOOT\BOOTAA64.EFI` instead of entering the native UiApp path.
- Added GitHub showcase screenshots for the experimental ModernSetupApp
  dashboard and English/Simplified Chinese exit pages.
- Migrated more experimental app visual helpers into `ModernUiRendererLib`:
  formatted text, focus frames, info cards, selectable row background
  calculation, selectable row borders, value selector boxes, and drop-down
  frames.
- Native FormBrowser statement layout now reserves a right status rail on wide
  GOP modes so DisplayEngine pages can evolve toward high-density IBV-style
  setup layouts without letting HII statement text overlap the telemetry area.
- DisplayEngine chrome now avoids drawing custom footer buttons over native
  FormBrowser hotkey help, suppresses duplicate form-title text in the top tab
  area, and keeps the procedural pattern limited to the header instead of the
  main content surface.
- DisplayEngine rows, value boxes, drop-down frames, popups, and pick lists now
  use black/orange AORUS-like surfaces while preserving native FormBrowser
  control semantics.
- Native FormBrowser selected rows now keep the highlight attribute across the
  full statement field and use a brighter orange/yellow selection treatment.
- Restored the black/orange DisplayEngine theme as the default after the
  black/deep-red/yellow experiment proved too harsh in FrontPage rendering.
- `ModernUiCustomizedDisplayLib` now converts DisplayEngine text-cell state
  into `ModernUiEngineLib` draw models for page chrome, rows, and popups instead
  of owning those visual surfaces locally.
- The experimental `ModernSetupApp` now uses `ModernUiEngineLib` for header,
  tabs, footer, selectable rows, value selector rows, and language drop-down
  surfaces while keeping only demo data and navigation state in the app.
- `ModernSetupApp` is now a standard front-page shell instead of a self-owned
  HII bridge demo. It displays dynamic Boot#### options, HII formset/device
  entries, platform summary, and Secure Boot state through provider libraries.
- The Devices page opens selected HII formsets through
  `EFI_FORM_BROWSER2_PROTOCOL.SendForm()`, keeping VFR/IFR parsing,
  ConfigAccess callback flow, conditions, validation, and variable writes in
  native edk2 FormBrowser.
- The app build no longer links `ModernUiHiiBridgeLib` or
  `ModernUiPageAdapterLib`; those libraries remain source-level
  experimental/debug code only.
- `Scripts/run-armvirt.sh` can attach the ModernSetupApp ESP with `DUAL_APP=1`
  without forcing the VM to boot the app directly.
- `ModernSetupApp` dashboard now uses a system overview layout with hardware
  monitor and selectable Quick Access cards for Boot, Devices/HII, and Secure
  Boot.
- Added the `setup-v0.4-dashboard.png` GitHub showcase capture.
- `ModernUiEngineLib` right-rail platform strings now follow the build
  architecture, including LOONGARCH64, instead of hardcoding ArmVirt labels.
- ArmVirt and LoongArchVirt overlay builds now support
  `MODERN_SETUP_DISPLAY_ENGINE=native|modern` so native edk2 DisplayEngine and
  ModernDisplayEngine can be compared from the same HII pages.

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
- Render UEFI text-mode box drawing, arrow, triangle, and checkbox characters
  as narrow GOP primitives so native UiApp/FormBrowser frames do not appear as
  missing-glyph boxes.
- Moved statement row surface styling and measured text truncation into the
  DisplayEngine/customized display path so native FormBrowser rows can use the
  modern GOP visual treatment without the experimental app/HII bridge.

### Current Status

- ArmVirt AARCH64 DEBUG_CLANGDWARF build has been validated locally.
- LoongArchVirtQemu overlay generation has been validated locally; full
  LOONGARCH64 firmware compilation still requires installing/providing a
  LoongArch GCC/binutils cross toolchain on this macOS host.
- The default ArmVirt setup path enters native `UiApp`; setup rendering is
  handled by `ModernDisplayEngineDxe` through edk2's
  `EDKII_FORM_DISPLAY_ENGINE_PROTOCOL`.
- Device Manager, DriverSample, Boot Manager, and Boot Maintenance now use
  native UiApp/FormBrowser behavior in the default firmware image.
- Simplified Chinese and English switching through `ModernUiStringLib` applies
  only to the experimental `ModernSetupApp`; native UiApp language behavior
  remains owned by edk2 HII/FormBrowser.
- The custom HII bridge is retained only as prototype/debug code and is not in
  the default package DSC, ArmVirt overlay path, or standard ModernSetupApp
  build.
- Native `SetupBrowserDxe/FormBrowser2` owns HII/IFR/VFR parsing, GUID formset
  handling, ConfigAccess callback flow, condition evaluation, and variable
  write semantics in the default ArmVirt firmware.
- The page adapter registry is experimental and not in the default native
  FormBrowser path.
- The latest validated FVMAIN state is 8372608 bytes total, 8372576 bytes used,
  and 32 bytes free in the ArmVirt AARCH64 DEBUG_CLANGDWARF build.
- `ModernDisplayEngineDxe` currently preserves most edk2 DisplayEngine behavior
  and routes its low-level text-cell drawing through GOP. Further visual polish
  should happen in the DisplayEngine/customized display layer, not in a separate
  IFR parser.
- The UI framework is still early: the DisplayEngine path is compatibility-first
  and still needs substantial visual polish.

### Planned

- Add DisplayEngine/customized display layout helpers for resolution-aware
  layout and safe-area handling.
- Continue filling out `ModernSetupApp` as a portable standard front-page shell
  while keeping all real setup pages on native FormBrowser.
- Add PCDs for default page, feature visibility, fallback behavior, pointer
  support, and theme selection.
- Improve `ModernDisplayEngineDxe` visual styling for panels, highlight rows,
  popups, input boxes, and high-density setup pages.
- Keep `ModernUiHiiBridgeLib` experimental or remove it after the DisplayEngine
  architecture is stable.
- Extend validation beyond ArmVirt to OVMF and LoongArch targets.
