<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Productization Status & Roadmap

A living, checklist-style read on how far ModernSetupPkg is from being shippable
on real platforms, and the ordered work remaining. It complements — and does not
duplicate — the capability inventories:

- `Docs/ProductizationFeatureMatrix.md` — where a given feature belongs.
- `Docs/ProductizationValidationMatrix.md` — what validation each target owes.
- `Docs/MODULE_BOUNDARIES.md` — the layer contract this work must not break.
- `Docs/BASELINE.md` — pinned edk2 baseline + the validation ladder.

This file answers a different question: *"what is done, what is left, how big, in
what order."* Percentages are an engineering judgment, not a measured metric.

Last updated: 2026-06-06.

## What "productized" means here

ModernSetupPkg is a **display-backend replacement** (plus an optional front-page
shell). edk2 keeps HII/IFR/FormBrowser/ConfigAccess ownership. "Productized"
therefore means, for each deliverable:

1. **Complete** — every interaction path the native DisplayEngine handles is
   rendered (no fall-through to retro text-grid output).
2. **Correct** — no truncation/clipping/seams; behavior matches native semantics.
3. **Localized** — CJK and other non-Latin strings render (the package is XArch
   and carries Chinese strings).
4. **Bounded** — fits a real platform's DXE memory/perf budget.
5. **Validated on hardware** — not only QEMU virtual targets.
6. **Regression-guarded** — covered beyond the current smoke gate.

## Snapshot

| Track | Estimate | One line |
| --- | --- | --- |
| Engine — GOP "modern" line | ~72% | Default backend; renders all forms; close to "usable product", not yet "modern-looking". |
| Engine — LVGL "beautiful" line | ~57% | Widgets + dialogs modernized; still gated `experimental/lvgl-spike`. **Priority line.** |
| Front-page app — ModernSetupApp | ~57% | Clean architecture/boundaries; owes real provider data + hardware validation. |

The two engine "lines" are the **same** `ModernDisplayEngineDxe` with a swapped
renderer **library** (`ModernUiRendererLib` GOP vs `ModernUiLvglRendererLib`),
selected by `MODERN_SETUP_DISPLAY_ENGINE=modern|lvgl`. There is no separate LVGL
DXE on the product path (`Experimental/.../LvglDisplayEngineDxe` is an orphaned
early probe — see "Cleanup backlog").

## Track A — Engine, GOP "modern" line (~72%)

Effort key: **S** ≈ <1 day, **M** ≈ days, **L** ≈ week+ / external dependency.

| ✓ | Item | Effort | Note |
| --- | --- | --- | --- |
| [x] | Full edk2 DisplayEngine fork (FormDisplay/Input/Popup/ProcessOptions) | — | Inherits all FormBrowser display semantics. |
| [x] | Modern chrome (header, tabs, right rail, themed surfaces) | — | |
| [x] | Themed value rendering for all value opcodes (GOP primitives) | — | |
| [x] | Popups: modern panel surface, no truncation, no box-draw seam | — | Shared with LVGL line. |
| [ ] | Aesthetic polish pass (row rhythm, selection bar, value alignment) | M | Shared with LVGL line. |
| [ ] | CJK / multilingual verification (uses HII font here) | M | Less acute than LVGL line. |
| [ ] | Multi-resolution + GOP-absent robustness | M | |
| [ ] | Hardware validation | L | Depends on real hardware. |

## Track B — Engine, LVGL "beautiful" line (~57%) — PRIORITY

This is the north-star look (mature graphics library). Critical path is ordered.

| ✓ | Item | Effort | Note |
| --- | --- | --- | --- |
| [x] | LVGL UEFI bridge + shadow-canvas BLT pipeline | — | |
| [x] | Real widgets: dropdown / checkbox / textarea / ordered-list / date-time | — | `lv_dropdown`/`lv_checkbox`/`lv_textarea`. |
| [x] | In-setup edit vs widget conflict fixes | — | |
| [x] | OEM watermark slot (original art, A8 composite) | — | |
| [x] | Confirm/error dialogs: clean modern panel (no truncation / box-draw) | — | 2026-06-06. |
| [ ] | **Backend graduation decision** — promote `ModernUiLvglRendererLib` out of `experimental/lvgl-spike` so it can enter a default overlay | S (decision) | **Gates everything below.** |
| [ ] | Selectable-option popup + password + multi-string help verified/cleaned | S–M | Same machinery as the confirm dialog; verify each. |
| [ ] | Dialog visual elevation (accent title, Y/N affordances) — optional | S–M | Gilding; current dialogs already clean. |
| [~] | **CJK / multilingual font** — demand-driven Noto Sans CJK SC subset (182 glyphs, 18px A8) + HII fallback; app fully renders zh-Hans. Chrome localized 2026-06-06. Remaining: arbitrary-platform coverage strategy + single-size. | M–L | Foundation strong; gate is arbitrary HII coverage. |
| [ ] | **Memory / perf budget** — DXEFV near-full with DEBUG; per-refresh snapshot allocations | M | Measure, then trim. |
| [ ] | Aesthetic polish pass | M | Shared with Track A. |
| [ ] | **Hardware + multi-resolution validation** | L | Currently 0 hardware coverage. |
| [x] | Watermark confined to genuine whitespace (suppressed on dense forms) | — | 2026-06-06. |

## Track C — Front-page app, ModernSetupApp (~57%)

Architecturally complete and boundary-guarded; the gap is "filling in", not design.

| ✓ | Item | Effort | Note |
| --- | --- | --- | --- |
| [x] | Front-page dashboard, tabs, footer/status chrome | — | |
| [x] | `SendForm()` handoff to native FormBrowser; boot launch; language select | — | |
| [x] | 11× typed read-only provider DataLibs + app-private snapshot | — | Boundaries smoke-enforced. |
| [ ] | **Real provider data** — Platform/Security/Firmware/Power etc. expose N/A / placeholders | M–L | Wire real SMBIOS/ACPI/sensor sources. |
| [ ] | Boot-manager completeness (boot-option types, Secure Boot state, one-time boot) | M | |
| [ ] | Localization of app strings | M | |
| [ ] | Default-on decision (currently opt-in `MODERN_SETUP_REPLACE_UIAPP`) | S (decision) | |
| [ ] | Hardware validation | L | |

## Cross-cutting (blocks "product" for both engine and app)

| ✓ | Item | Effort | Note |
| --- | --- | --- | --- |
| [ ] | **Real-hardware validation** across the XArch targets | L | Today everything is QEMU virtual. |
| [ ] | **CJK / i18n font story** | M–L | |
| [ ] | Regression suite beyond smoke (per-driver HII render checks) | M | Smoke is the only gate today. |

## Known issues / regressions

- ~~**Watermark on dense forms**~~ — fixed 2026-06-06: the overlay now receives
  the first empty row and confines the mark to the whitespace band below the last
  menu row, so it is suppressed when the rows fill the content area.
- See `Docs/ISSUE_BACKLOG.md` for the broader list.

## Cleanup backlog (no rush)

- `Experimental/LvglSpikePkg/LvglDisplayEngineDxe` — an early ~10 KB probe that
  renders prompt labels and waits for ESC (no editing/popups/widgets). The
  product LVGL path is `ModernDisplayEngineDxe` + `ModernUiLvglRendererLib`; the
  probe is wired into no product overlay (only the standalone
  `LvglSpikeLoongArch.dsc`). Can be retired with its references
  (`LvglSpikeLoongArch.dsc`, `Tests/Smoke/smoke_validate.py` token, README /
  REFERENCES / CHANGELOG mentions) when convenient.

## Recommended near-term sequence

Small, stable steps first; the two "decisions" unblock the most:

1. Fix the watermark-on-dense-form regression (**S**, clears a self-inflicted regression).
2. Verify + clean the remaining popups — selectable-option, password, multi-string help (**S–M**).
3. Make the **LVGL backend graduation decision** (**S**); it gates real productization of the look.
4. Stand up the **CJK font** story (**M–L**) — the first true product gate.
5. **Memory/perf budget** measurement + trim (**M**).
6. First **hardware** bring-up on one target (**L**).

The first three are days of work and visibly move the LVGL line; items 4–6 are the
real thresholds between "great-looking sample" and "shippable product".
