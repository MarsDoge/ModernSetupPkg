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

- DisplayEngine text-input edit caret is now drawn by the Modern renderer
  (`ModernDisplayDrawTextCaret`) instead of the native `EFI_SIMPLE_TEXT_OUTPUT`
  cursor. `ReadString` suppresses the native cursor (which draws straight to the
  GraphicsConsole framebuffer and is invisible/misplaced behind an off-screen
  canvas) and paints a thin accent caret at the edit cell each keystroke, so the
  string/password editor's cursor renders identically on the GOP and LVGL
  backends. This closes the last interaction surface that bypassed the renderer:
  the LVGL backend now composites the full FormBrowser interaction set
  (rows, one-of/dialog popups, and input editing) end-to-end. App-local, no HII
  or value semantics touched. Verified by an OVMF X64 lvgl-mode screendump of the
  DriverSample string editor showing the caret, plus GOP/lvgl builds and smoke.
- DisplayEngine per-opcode control affordances: each editable FormBrowser
  statement now shows a distinct, non-semantic cue glyph keyed on its control
  kind — checkbox box, numeric `+`, one-of/choice `▼`, ordered-list up/down,
  string caret, date/time segment ticks, and reference/action `▶`. Cues are
  drawn purely from renderer primitives, so they render identically through the
  GOP and LVGL backends, and are composited by a post-text overlay
  (`ModernDisplayDrawStatementRowCue`, called at the end of `DisplayOneMenu`) so
  the native highlight text background no longer overpaints them. Read-only and
  grayed/disabled rows intentionally get no cue. edk2 keeps all HII/value/storage
  ownership; the cue helpers are smoke-guarded against
  `ConfigAccess`/`RouteConfig`/`ExtractConfig`/`SetVariable`/`HiiGetString`.
- Unified control-affordance vocabulary across the front-page App and the
  in-setup DisplayEngine: the per-control cue shapes now live in one shared
  function, `ModernUiEngineDrawControlCue` (`ModernUiEngineLib`), keyed on
  `MODERN_UI_VALUE_TYPE` and built from a new shared `ModernUiFillTriangle`
  renderer primitive plus `ModernUiFillRect`/`ModernUiStrokeRect`. The App value
  lane (`ModernUiEngineDrawValue`) now paints the matching cue just left of the
  value box, and the DisplayEngine row overlay maps its row kind to the value
  type (`ModernDisplayKindToValueType`) and delegates to the same function — so a
  checkbox/drop-down/numeric/date-time/password/string/ordered-list/action
  control reads identically in both surfaces. The DisplayEngine's former private
  `ModernDisplayDrawKindCue`/`ModernDisplayCueTriangle` shape copies are removed.
- `build-ovmf-x64.sh` gains `MODERN_SETUP_DEMO_DRIVER_SAMPLE=0|1` (default `0`):
  when `1`, edk2 `DriverSampleDxe` is added to the OVMF overlay (reachable via
  Device Manager) as a control-rich VFR for exercising the DisplayEngine control
  affordances. Off by default; never in a shipped overlay.
- Experimental LVGL renderer-swap mode (`experimental/lvgl-spike` branch only):
  `MODERN_SETUP_DISPLAY_ENGINE=lvgl` now keeps the existing
  `ModernDisplayEngineDxe` (all FormBrowser/HII/interaction ownership unchanged)
  but resolves the `ModernUiRendererLib` class to a new LVGL-backed
  implementation, `Library/ModernUiLvglRendererLib`. Every primitive composites
  through LVGL's software draw pipeline into a persistent full-screen XRGB8888
  shadow canvas, then BLTs only the touched region to GOP: geometry (fills,
  borders, panels, rows, cards, progress, value boxes, drop-downs) via
  `lv_draw_rect`, ASCII text via `lv_draw_label` (Montserrat), and non-ASCII
  (CJK) runs via the firmware HII font composited into the same canvas (LVGL
  bundles no CJK coverage). The library keeps the exact `ModernUiRenderer.h` API
  and the original 8 px-cell text-measurement model, so layouts in the display
  engine and `ModernSetupApp` are unchanged. Verified on OVMF X64: the live UiApp
  FormBrowser front page renders end-to-end through LVGL. `LvglCoreLib` +
  `IntrinsicLib` are force-linked only into the lvgl-mode overlay; never in a
  default overlay. The earlier standalone `LvglDisplayEngineDxe` remains as a
  separate from-scratch reference and is no longer wired into the OVMF overlay.
  CJK is handled by compositing the package's embedded bitmap glyphs
  (`ModernUiGlyphs.c`) into the LVGL canvas, with the firmware HII font as a
  secondary fallback for code points without an embedded glyph. `lvgl` mode also
  composes with `MODERN_SETUP_REPLACE_UIAPP=1`: the `ModernSetupApp` front-page
  shell then renders through the same LVGL pipeline (IntrinsicLib is force-linked
  into the app component in lvgl mode). Verified on OVMF X64 — both the UiApp
  FormBrowser front page and the full ModernSetupApp dashboard (zh labels +
  English values + all chrome) render end-to-end through LVGL.
- Experimental LVGL rendering-backend spike (`experimental/lvgl-spike` branch only;
  `Experimental/LvglSpikePkg`, never in a default overlay or `ModernSetupApp`).
  Pins `External/lvgl` at the upstream commit `0b1ea312d` (`v9.5.0-273`), which
  is upstream lvgl/lvgl master carrying the merged LoongArch64/RISC-V64 UEFI
  build support (PR #10221). Validates that LVGL core + software renderer + its
  upstream UEFI port build under edk2 GCC on a hard architecture:
  `LvglSpikeProbe.efi` compiles for LoongArch64 and was run on real LoongArch
  hardware, drawing an LVGL UI straight to GOP (standalone-app path, not via any
  DisplayEngine). The submodule is consumed pristine (no patch to `External/lvgl`
  itself), with two edk2-side accommodations for unrelated upstream churn between
  v9.5.0 and this commit: (1) a documented empty `efi.h` shim under
  `Experimental/LvglSpikePkg/Library/LvglLib/` that satisfies an unconditional
  `#include <efi.h>` introduced by the header reorg PR #10041 (the EDK2 framework
  path is selected via `LV_USE_UEFI_INCLUDE`, so the shim pulls in no gnu-efi
  types); and (2) our spike sources include the canonical `<lvgl/lvgl.h>` umbrella
  instead of the now-deprecated `lvgl/src/drivers/uefi/*` public headers. Both are
  removable once upstream gates the gnu-efi include behind the framework
  selection. See `Experimental/LvglSpikePkg/README.md`.
- ModernSetupApp header clock now updates live while the front page is idle. The
  app loop arms a one-second periodic timer and waits on it alongside the
  keyboard/pointer sources, repainting only the header clock text in place on
  each tick (no full-frame redraw, so no flicker on the direct-to-GOP renderer).
  Setting the date/time is unchanged — that still belongs to the native setup
  forms reached via `SendForm()`; this only keeps the displayed clock from
  freezing between keystrokes. The change is entirely app-local (no public API
  or PCD changes). Verified by app X64 CLANGDWARF build and smoke.
- Modern UI header chrome: the shared header band (used by both `ModernSetupApp`
  and the modern DisplayEngine) now fades `HeaderPattern` down to the background
  instead of the older hard top-half/bottom-half split, and the product name,
  mode label, and clock are laid out by measured width (left-anchored,
  centred, right-aligned) instead of fixed column offsets — keeping the header
  seam-free and collision-free from the 1024x768 floor through 1280x800
  captures. Verified by app X64 CLANGDWARF build, smoke, and a 1280x800 OVMF
  X64 screendump.
- Phase49 ModernSetupApp: the Boot page now ends with a native boot-tools entry
  row; pressing Enter on it opens the native Boot Manager / Boot Maintenance via
  `SendForm()` instead of launching a boot option, and the per-entry BootNext /
  BootOrder keys (N/C/+/-) are suppressed while that fallback row is selected.
- Phase36 architecture guard: documented the PEI -> DXE services ->
  ModernSetupApp/DisplayEngine dynamic data/configuration pipeline in English and
  zh-CN, with smoke coverage that keeps DisplayEngine focused on rendering state
  rather than owning hardware probing or config mutation.
- Phase36 UX iteration: Modern footer hotkey help now reserves left-side text
  columns for the GOP status chip, preventing key-help text from colliding with
  live/unsaved/reboot-required state indicators.
- Phase36 UX iteration: highlighted interactive rows now draw a subtle right-side
  value lane, making prompt/value separation clearer without taking ownership of
  FormBrowser text or value semantics.
- Phase36 UX iteration: the Modern UI header time now includes seconds, making
  redraw/refresh activity visible without adding a new timer path or changing
  FormBrowser event ownership.
- Phase36 UX iteration: DisplayEngine page status now normalizes through a
  private page-state enum with an explicit future `REBOOT REQUIRED` state, so
  reboot-after-save policy can be surfaced later without conflating it with
  generic unsaved changes.
- Phase36 UX iteration: the shared Modern UI footer now renders page status as a
  compact color-coded chip, giving live/refresh/unsaved/modal states a durable
  visual slot for future PEI/DXE/App data handoff and reboot-required flows.
- Phase36 UX iteration: DisplayEngine page chrome now surfaces FormBrowser-owned
  page state in the footer (`LIVE VIEW`, `LIVE REFRESH`, `UNSAVED CHANGES`, or
  `MODAL VIEW`), establishing a presentation slot for future PEI/DXE/App data
  handoff and dynamic refresh without adding renderer-owned policy/storage logic.
- Phase36 UX iteration: the modern DisplayEngine page chrome now adds a subtle
  right-rail divider, clarifying the split between the actionable statement list
  and contextual help without changing FormBrowser help text placement.
- Phase36 UX iteration: native FormBrowser prompt/value glyphs printed inside
  the modern statement list now receive a small GOP-only inset so text no longer
  crowds the accent rail or rounded row surface while preserving text-mode cursor
  accounting and FormBrowser ownership.
- Phase36 DisplayEngine row visual polish: FormModel-driven row surfaces now add
  conservative GOP accents for editable/action rows, changed settings,
  invalid/warning feedback, and disabled/read-only states without changing
  FormBrowser text/value rendering or HII/config ownership.
- Phase35 native-vs-modern DisplayEngine visual validation foundation for OVMF
  X64: `Scripts/capture-displayengine-ovmf-x64.sh` creates separated native and
  modern overlay/build/capture artifact paths under a safe TMPDIR default,
  `Tests/Manual/DisplayEngineOvmfX64Visual.md` documents evidence levels and
  limitations, and smoke checks enforce the script/doc contract without claiming
  visual verification.
- Phase34 DisplayEngine row rendering hardening: statement row GOP surfaces now
  classify highlighted/selected, disabled/locked, read-only, changed, invalid,
  modal, and action/text affordance state through the private FormModel helpers,
  keeping native FormBrowser HII semantics intact while reducing scattered visual
  role decisions.
- Phase33 DisplayEngine form view-model foundation: `ModernUiCustomizedDisplayLib`
  now has a private `ModernDisplayFormModel` layer that classifies borrowed
  FormBrowser DisplayEngine form/statement data into Modern UI row metadata,
  centralizes page layout handoff in `DisplayPageFrame`, and routes key-help UX
  decisions through the new row helper without promoting `ModernUiHiiBridgeLib`
  or taking HII/config ownership.
- phase32(app): apply DashboardDensity to Boot/Devices/Provider pages.
  `Docs/ProductizationValidationMatrix.md` (and zh-CN) now records the responsive
  page-layout evidence: the renderer pins the App to a 1024x768 floor via
  `SelectPreferredGopMode`, so sub-1024 modes such as 800x600 are not reached
  when a qualifying mode exists, and OVMF X64 Boot/Devices/Firmware-provider pages
  were screendump-inspected at the firmware default 1280x800 (modern-App-only,
  not a native-vs-modern maintainer Visual reviewed sign-off).
- Phase30 XArch/productization validation docs and smoke gate coverage through
  `Docs/ProductizationValidationMatrix.md`, its zh-CN counterpart, doc-index
  links, and host-side checks for evidence wording, concrete ARCH values,
  ModernSetupApp/native HII boundaries, Hardware Health demo-only/read-only
  status, app-owned preferences, PCIe policy ownership, and xarch dry-run target
  metadata. This is documentation/test coverage only; runtime behavior is
  unchanged.
- Phase 29 Dashboard density layout: the app-owned `DashboardDensity`
  preference now feeds the Dashboard grid helper, so `Compact` reduces the top
  summary area and quick-card spacing while navigation uses the same density-aware
  geometry as rendering.
- Phase 28 app-owned runtime theme switching: the `Theme` preference now resolves
  the active `ModernSetupApp` palette during redraw, adds the generic premium
  `Graphite Gold` dark graphite/champagne palette, and exposes the additive
  `ModernUiGetThemeForPreference()` resolver with smoke coverage.
- Phase 10 `ModernSetupApp` Dashboard category landing: the former Quick Access
  section is now labeled Setup Categories/设置分类, routes through an app-private
  card-to-page helper, and shows a small localized Open/Enter affordance while
  preserving Boot/Devices content focus and overview-page navigation focus.
- `ModernUiPcieDataLib` read-only PCIe capability and native policy-entry
  summary foundation for controller/root-bridge inventory, protocol presence,
  ReBAR, Above 4G, SR-IOV, ASPM, bifurcation, hot-plug, ACS/ARI, and IOMMU
  hints, with smoke validation for provider wiring and mutation-token exclusion.
- RiscVVirtQemu RISCV64 overlay build-validation path through
  `Scripts/build-riscvvirt.sh`, with native/modern DisplayEngine generation,
  smoke dry-run coverage, and manual build-validation documentation.
- Phase 4 `ModernSetupApp` Dashboard card expansion with six provider-backed
  Quick Access/status cards for boot readiness, device visibility, provider
  health, firmware lifecycle, power/thermal, and performance inventory, plus
  smoke validation for the expanded card set.
- Phase 3 `ModernSetupApp` provider health summary derived from the app-private
  provider snapshot, with Dashboard health/coverage rendering, Diagnostics first
  issue details, and smoke validation for the health boundary.
- Phase 2 `ModernSetupApp` provider snapshot boundary for Dashboard/provider
  summary pages, with smoke validation that presentation modules do not bypass
  the app-private read-only provider contract.
- Phase 1 `ModernSetupApp` ownership readiness docs and smoke validation for app
  INF source coverage plus Dashboard module boundary checks.
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
- High-contrast black/orange theme tokens for DisplayEngine rendering, including
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
- `Docs/ProductizationFeatureMatrix.md`, defining the cross-architecture
  standard front-page App roadmap for desktop, workstation, server, embedded,
  appliance, x86, ARM, RISC-V, and LoongArch targets.
- `ModernUiFirmwareDataLib`, `ModernUiDiagnosticsDataLib`, and
  `ModernUiManagementDataLib` read-only providers for the standard front-page
  App productization track.
- `Docs/IbvAndPlatformSetupSurvey.md`, documenting public IBV/IFV vendors,
  OEM/ODM reference boundaries, form-factor setup surfaces, and common
  front-page/App capability areas.
- `ModernUiPowerDataLib` and `ModernUiPerformanceDataLib` read-only providers
  for power/thermal and performance/tuning capability summaries.
- X64 OVMF overlay build and local QEMU validation path through
  `Scripts/build-ovmf-x64.sh`, `Scripts/run-ovmf-x64.sh`, and
  `Tests/Manual/OvmfX64Qemu.md`, including native/modern DisplayEngine rebuilds
  for manual before/after checks. This path is not CI-active yet.

### Changed

- Modern UI engine: added a `ModernUiEngineDrawStatusPill` primitive (second entry
  in the base shape vocabulary) and refactored the footer status badge onto it.
  The badge is now sized to comfortably contain one text line (height = line + 6)
  with the label vertically centred via the shared `ModernUiBoxTextY` helper,
  fixing the cramped/clipped look of the old fixed 20px chip with text pinned at
  +11. Verified clean in the app footer (status message centred and legible). The
  in-setup DisplayEngine badge still shows a separate draw-order overdraw clipping
  its top — that is tracked for the DisplayEngine row/badge integration step, not a
  pill-geometry issue. App-local; no public API or PCD change. Verified by smoke,
  OVMF X64 + app X64 CLANGDWARF builds.
- Modern UI engine: began a base graphics-primitive vocabulary. Introduced named
  metric tokens (`MODERN_UI_TEXT_LINE_HEIGHT`, `MODERN_UI_BOX_TEXT_INSET`) plus a
  `ModernUiBoxTextY` vertical-centring helper, and extracted the per-row drawing
  in `ModernUiEngineDrawRows` into a single `ModernUiEngineDrawStatementRow`
  primitive that `ModernUiEngineDrawValue` shares. Behaviour-preserving (no pixel
  change; verified by app X64 capture of the dashboard and Boot page), this
  replaces scattered raw pixel offsets so row/box geometry is defined and polished
  in one place. Groundwork for moving the DisplayEngine's menu rows off edk2's
  text-grid column truncation onto graphical primitives. App-local; no public API
  or PCD change. Verified by smoke + app X64 CLANGDWARF build.
- ModernSetupApp Dashboard Quick Access polish: cards now carry a subtle raised
  depth (a faint inner top highlight plus a bottom shadow hairline), and the
  category headers are grid-aware -- a category that wraps across a grid row now
  repeats its header at the wrapped row start instead of leaving a header-less
  card stranded under a neighbouring column.
- ModernSetupApp default theme is now Graphite Gold (previously the
  system/orange default), so fresh installs open with the warmer graphite base
  and gold accent palette on first launch.
- The in-setup DisplayEngine now defaults to Graphite Gold as well, closing the
  theme seam where the Graphite Gold front page jumped to orange after
  `SendForm()` entered a real setup form. `PcdModernSetupTheme` gains value
  `2 = graphite/gold` and its default changes from `0x00` to `0x02`;
  `ModernUiGetTheme()` maps the new value, and the `MODERN_SETUP_THEME` build
  switch gains `graphite-gold`/`graphite` (now the default across the OVMF X64,
  ArmVirt, LoongArchVirt, and RiscVVirt scripts). `orange`/`red` remain
  selectable for integrators who want the older palette. **This is a public PCD
  default + semantics change and requires `core-api` review before the batch PR
  merges.** Verified by smoke (overlay dry run now asserts `|0x02`).
- Modern UI chrome refresh: the header status band is now a flat shelf (a single
  faint top sheen plus a baseline hairline) instead of vertical "vent" bars and
  horizontal striations, and the top tabs mark the active tab with a bright
  underline and a soft background tint instead of a boxed outline plus left
  accent bar. This affects both `ModernSetupApp` and the shared DisplayEngine
  chrome via `ModernUiEngineLib`.
- ModernSetupApp Simplified Chinese: the "Form Factor", "Boot Mode", "Category",
  and "Device Path" labels are now localized (外形规格 / 启动模式 / 类别 /
  设备路径), and the Devices count reuses the shared "%u 项" count format. Six
  CJK glyphs (别外径格规路) were regenerated into the built-in glyph table to
  cover the new strings.
- Phase48 ModernSetupApp Boot membership: the Boot page option filter now
  mirrors native `BootManagerMenuApp` `IgnoreBootOption()` semantics — it hides
  the running app and any HIDDEN/INACTIVE entries while preserving the Boot
  Manager Menu exception, so the list matches the native menu without
  reimplementing boot semantics.
- Phase50 ModernSetupApp: Boot selection and BootNext/BootOrder move actions are
  now clamped to the visible row count (`MIN(OptionCount, MAX_BOOT_ROWS)`), so
  keyboard actions can no longer target boot entries scrolled off screen.
- ModernSetupApp visual/copy polish: refined setup-page affordances and wording,
  the Dashboard first screen, tab windowing, and the highlighted boot value lane
  for clearer prompt/value separation.
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
  use black/orange advanced-mode surfaces while preserving native FormBrowser
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
- `ModernSetupApp` now includes Firmware, Diagnostics, and Management pages
  backed by provider libraries while keeping real setup policy in native
  FormBrowser/HII pages.
- `ModernSetupApp` now includes Power and Performance pages, extends Dashboard
  with form-factor, boot-mode, and provider availability fields, shows
  Boot#### category/hidden/device-path summaries, and reports TCG2/TrEE
  protocol presence on the Security page.
- DisplayEngine visual treatment now uses a darker selected-row band, softer
  help text, a unified black footer strip, and a subtle content-frame accent to
  reduce the raw text-mode feel of native FormBrowser pages.
- Added the `setup-v0.4-dashboard.png` GitHub showcase capture.
- `ModernUiEngineLib` right-rail platform strings now follow the build
  architecture, including LOONGARCH64, instead of hardcoding ArmVirt labels.
- ArmVirt and LoongArchVirt overlay builds now support
  `MODERN_SETUP_DISPLAY_ENGINE=native|modern` so native edk2 DisplayEngine and
  ModernDisplayEngine can be compared from the same HII pages.

### Fixed

- ModernSetupApp Dashboard no longer echoes the generic platform name as the
  form factor: when SMBIOS Type 3 is unavailable the platform provider now
  reports an empty form factor and the app surfaces its localized Unknown/N/A
  text instead of duplicating the "UEFI platform" value.
- Fixed seeded ModernSetupApp Preferences input editing: the first printable key
  now replaces the pre-filled current value (e.g. the default boot timeout "5")
  instead of appending to it, preventing accidental out-of-range values like
  "51".
- Changed ModernSetupApp Boot page Enter behavior to launch the selected
  visible Boot#### entry through UefiBootManagerLib; native Boot Manager remains
  available as the Exit-page fallback.
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
