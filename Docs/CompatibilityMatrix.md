<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Compatibility Matrix

ModernSetupPkg's compatibility target is the native edk2 FormBrowser path:

```text
HII database -> UiApp / SetupBrowserDxe / FormBrowser2
             -> ModernDisplayEngineDxe
             -> ModernUiEngineLib / ModernUiRendererLib / GOP
```

ModernSetupPkg does not parse IFR, evaluate VFR conditions, call ConfigAccess,
or write varstores on the default setup path. Those semantics remain owned by
edk2. This matrix tracks whether the modern DisplayEngine path can draw and
operate the forms that edk2 has already prepared.

## Platform Coverage

| Target | Default path | App path | Status | Notes |
| --- | --- | --- | --- | --- |
| ArmVirtQemu AARCH64 | UiApp + ModernDisplayEngine | ESP `BOOTAA64.EFI` | Active | Primary macOS/Apple Silicon validation target. |
| LoongArchVirtQemu LOONGARCH64 | UiApp + ModernDisplayEngine | ESP `BOOTLOONGARCH64.EFI` | Active | QEMU may fall back to `-bios`; variable persistence depends on pflash support. |
| X64 / OVMF | UiApp + ModernDisplayEngine overlay | Not wired | Manual | Local `Scripts/build-ovmf-x64.sh` and `Scripts/run-ovmf-x64.sh` path supports native/modern DisplayEngine builds and manual QEMU validation; not CI-active yet. |
| Real boards | Not validated | Not validated | Planned | Needs GOP, input, FV space, HII, and boot-time validation. |

## FormBrowser Surface Coverage

| Surface | Expected behavior | Current status | Validation target |
| --- | --- | --- | --- |
| FrontPage | Native UiApp front page renders through ModernDisplayEngine. | Captured | `before/after-armvirt-frontpage.png` |
| Device Manager | Platform HII formsets enumerate through native FormBrowser. | Captured | `before/after-armvirt-device-manager.png` |
| Boot Manager | Boot options render and launch through native BootManager. | Basic | ArmVirt / LoongArch manual |
| Boot Maintenance Manager | Native navigation and forms remain available. | Basic | Manual |
| Driver Health Manager | Native list rendering and Esc behavior work. | Basic | Manual |
| DriverSample first page | Mixed DriverSample statements render through native FormBrowser. | Captured | `before/after-armvirt-driver-sample-first-page.png` |
| Pick-list popup | One-of popup content is readable and preserves native keys. | Captured | `before/after-armvirt-driver-sample-oneof-popup.png` |
| Popup / confirm dialog | Confirmation popup content is readable and preserves native keys. | Partial | DriverSample save/discard/default flows |
| Help / footer hotkeys | Native help text remains readable and does not overlap custom chrome. | Basic | Manual |

## IFR Question Coverage

| IFR / statement type | Semantic owner | ModernSetup responsibility | Current status | v0.5 acceptance |
| --- | --- | --- | --- | --- |
| `EFI_IFR_TEXT_OP` | FormBrowser | Prompt and secondary text are readable. | Captured | DriverSample first page shows prompt/help text. |
| `EFI_IFR_SUBTITLE_OP` | FormBrowser | Section-like row is visually distinct. | Captured | DriverSample subtitle appears in both engines. |
| `EFI_IFR_REF_OP` and REF variants | FormBrowser / ConfigAccess | Row looks actionable; Enter navigates or calls callback. | Captured | Browser Testcase Engine opens DriverSample first page. |
| `EFI_IFR_ACTION_OP` | FormBrowser / ConfigAccess | Action row is visually distinct. | Captured | DriverSample action rows are visible; callback behavior remains manual. |
| `EFI_IFR_CHECKBOX_OP` | FormBrowser | Value state is readable and selected row is clear. | Captured | DriverSample checkbox row is visible on first page. |
| `EFI_IFR_ONE_OF_OP` | FormBrowser | Current value and pick-list popup are readable. | Captured | One-of popup captured in before/after pair. |
| `EFI_IFR_NUMERIC_OP` | FormBrowser | Value field and edit popup are readable. | Partial | Edit flow works without stale graphics. |
| `EFI_IFR_STRING_OP` | FormBrowser | Input popup and committed value are readable. | Partial | Text entry returns without ASSERT. |
| `EFI_IFR_PASSWORD_OP` | FormBrowser | Password input stays masked and readable. | Partial | Masked input flow returns cleanly. |
| `EFI_IFR_ORDERED_LIST_OP` | FormBrowser | Ordered values and list popup are readable. | Captured | Ordered list row values appear on DriverSample first page; edit popup remains manual. |
| `EFI_IFR_DATE_OP` | FormBrowser | Date fields and edit behavior remain native. | Partial | DriverSample date fields operate. |
| `EFI_IFR_TIME_OP` | FormBrowser | Time fields and edit behavior remain native. | Partial | DriverSample time fields operate. |
| `EFI_IFR_RESET_BUTTON_OP` | FormBrowser | Reset button is actionable and visually distinct. | Basic | DriverSample reset path prompts/returns cleanly. |
| `EFI_IFR_SUPPRESS_IF_OP` | FormBrowser | Hidden rows remain hidden; no custom evaluation. | Basic | DriverSample suppress demo matches native. |
| `EFI_IFR_GRAY_OUT_IF_OP` | FormBrowser | Grayed rows are distinguishable and disabled. | Partial | DriverSample grayout demo matches native. |
| `EFI_IFR_DISABLE_IF_OP` | FormBrowser | Disabled rows are distinguishable and disabled. | Partial | DriverSample disable demo matches native. |
| Refresh / callback dynamic forms | FormBrowser / ConfigAccess | Redraw after database changes without stale surfaces. | Open | Needs focused DriverSample dynamic-page run. |

Status meanings:

- `Captured`: before/after screenshots exist from the scripted ArmVirt capture
  path and show the same native FormBrowser page or control.
- `Basic`: expected to work in the current manual path, but needs repeated
  before/after evidence.
- `Partial`: semantic path is native edk2, but ModernDisplayEngine drawing needs
  more focused visual validation.
- `Open`: known validation gap for v0.5 or later.

## v0.5 Exit Criteria

- Native and Modern DisplayEngine builds can be generated from the same overlay
  scripts with `MODERN_SETUP_DISPLAY_ENGINE=native|modern`.
- Before/after screenshots cover FrontPage, Device Manager, DriverSample, and
  at least one popup or pick-list.
- DriverSample question rows listed above do not ASSERT and do not corrupt the
  screen in 1024x768 ArmVirt.
- LoongArchVirt still boots the ModernSetupApp ESP and the native UiApp path.
- Known gaps remain documented here instead of being hidden as visual polish
  issues.
