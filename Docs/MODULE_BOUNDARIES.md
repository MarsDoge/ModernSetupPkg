<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Module Boundaries

Language: English | [简体中文](MODULE_BOUNDARIES.zh-CN.md)

ModernSetupPkg is easiest to maintain when each layer keeps a small, concrete contract. Internal implementation can change freely, but public headers, DEC entries, and cross-layer behavior should remain stable unless the affected owners review the change.

## Stable contracts

- Public headers: `Include/ModernUi/*.h`.
- Package metadata: `ModernSetupPkg.dec`.
- Public `LibraryClass` names, GUIDs, and PCDs.
- Provider data-model semantics, renderer/theme tokens, engine draw models, and DisplayEngine compatibility behavior.

## Layer rules

| Layer | May do | Must not do |
| --- | --- | --- |
| Renderer/theme | Draw primitives, text/glyphs, colors, theme tokens, safe backend abstraction | Parse HII/IFR, enumerate providers, own app navigation, own FormBrowser policy |
| UI engine/layout/input | Convert typed models into reusable layout, rows, popups, footers, and UI events | Read Boot#### directly, write varstores, hard-code platform policy |
| DisplayEngine path | Preserve edk2 DisplayEngine/FormBrowser behavior while using modern rendering pieces | Replace FormBrowser semantics, bypass ConfigAccess/callbacks, invent app-only policy |
| App shell | Own front-page navigation, dashboard composition, language/theme state, and `SendForm()` entry points | Parse IFR, evaluate VFR conditions, call ConfigAccess directly, write HII varstores |
| Providers | Expose typed summaries and FormBrowser entry points from platform/firmware data | Draw UI, choose layout/theme, own app navigation policy |
| HII bridge | Stay isolated as experimental parser/adapter research | Become the default compatibility path, force writes, treat unsupported opcodes as safe |
| Platform/CI | Build scripts, overlays, QEMU/manual validation, release packaging | Hide behavior changes inside scripts or docs-only PRs |

## Renderer backends

`ModernUiRendererLib` is a class with two interchangeable implementations selected
by the build (`MODERN_SETUP_DISPLAY_ENGINE`); both expose the identical
`Include/ModernUi/ModernUiRenderer.h` API:

- **GOP backend** (`Library/ModernUiRendererLib`) — the default. Draws straight to
  `EFI_GRAPHICS_OUTPUT_PROTOCOL` (BLT fills, HII-font text, built-in bitmap glyphs).
- **LVGL backend** (`Library/ModernUiLvglRendererLib`, `=lvgl`, experimental) — every
  primitive is composited by LVGL's software renderer into a persistent full-screen
  XRGB8888 **shadow canvas**, then only the touched region is BLT'd to GOP. Geometry
  uses `lv_draw_rect`; ASCII text uses `lv_draw_label` (Montserrat); non-ASCII (CJK)
  runs use the package's embedded bitmap glyphs (`ModernUiGlyphs.c`), with the
  firmware HII font as a secondary fallback (LVGL ships no CJK font).

Both backends share, verbatim, the backend-agnostic surface
`Library/ModernUiRendererLib/ModernUiRendererCommon.c` (geometry compositions, text
measurement, themed widgets, the `ModernUiFillTriangle` shape primitive) and the
glyph table `ModernUiGlyphs.c`. A backend provides only three primitives —
`ModernUiRendererInit`, `ModernUiFillRect`, `ModernUiDrawText` — declared with the
shared helpers in `ModernUiRendererInternal.h`. Keep the 8 px-cell /
fixed-CJK-cell measurement model identical across backends so caller layouts are
backend-stable.

## Control-affordance vocabulary

The per-control cue shapes (checkbox box, one-of `▼`, ordered-list up/down,
numeric `+`, date/time segment ticks, password dots, string caret, action `▶`)
are defined **once**, in `ModernUiEngineDrawControlCue` (`ModernUiEngineLib`),
keyed on `MODERN_UI_VALUE_TYPE` and built only from renderer primitives
(`ModernUiFillRect` / `ModernUiStrokeRect` / `ModernUiFillTriangle`). Both
surfaces that show controls delegate to it, so a given control type reads
identically in each:

- **Front-page App value lane** — `ModernUiEngineDrawValue` paints the cue just
  left of the value box.
- **In-setup DisplayEngine row** — `ModernDisplayDrawStatementRowCue` maps its
  `MODERN_DISPLAY_FORM_ROW_KIND` to the value type via
  `ModernDisplayKindToValueType` and draws the cue at the row's right edge,
  **after** native FormBrowser prints the row text (so it composites on top
  instead of being overpainted). It classifies already-materialized statement
  data only and never touches HII/config/storage; read-only and disabled rows
  get no cue.

Do not add a second copy of these shapes in either consumer — extend the shared
vocabulary and add the value-type mapping instead.

The text-input **edit caret** is likewise renderer-drawn: `ReadString` suppresses
the native `EFI_SIMPLE_TEXT_OUTPUT` cursor and calls `ModernDisplayDrawTextCaret`,
so no backend paints a cursor straight to the framebuffer. This keeps the full
FormBrowser interaction set (rows, popups, input editing) on the renderer, which
is what lets the off-screen LVGL backend composite it end-to-end. Any new
interactive surface must route its cursor/caret through the renderer the same
way rather than calling `gST->ConOut->EnableCursor`.

## What not to do

- App code must not parse IFR or write HII varstores. Real setup pages should enter native FormBrowser with `EFI_FORM_BROWSER2_PROTOCOL.SendForm()`.
- Providers must not draw UI. They return data and entry points; renderer/engine/app decide presentation.
- Renderer code must not own FormBrowser policy. It draws what higher layers ask it to draw.
- DisplayEngine changes must preserve native HII semantics unless the compatibility impact is explicitly reviewed.
- HII bridge code is experimental-only. Unsupported constructs should fail closed or render read-only/fallback rows.
- Platform-specific policy belongs in LibraryClass instances, PCDs, or overlays, not shared UI core.

## Contract-first workflow

1. If a change touches `Include/ModernUi/*.h` or `ModernSetupPkg.dec`, describe the public contract before wiring consumers to it.
2. Use append-only changes for shared structs/enums whenever possible.
3. Request core-api review for public contracts and affected owner review for implementations.
4. Add validation notes that match the changed layer; QEMU unavailable is acceptable if the reason is stated.
5. Update `CHANGELOG.md` when behavior, build flow, public API, platform support, or compatibility changes.
