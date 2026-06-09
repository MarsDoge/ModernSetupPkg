<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# ModernSetup App Feature Standard

Language: English | [简体中文](AppFeatureStandard.zh-CN.md)

This document is the **normative** specification for what the ModernSetup
standard front-page App exposes: its page set, its dashboard structure, the
quick-access category cards, and how those adapt per platform class. Where the
[IBV and Platform Setup Survey](IbvAndPlatformSetupSurvey.md) and the
[Productization Feature Matrix](ProductizationFeatureMatrix.md) are *reference*
material (what the broader firmware ecosystem does), this document is
*prescriptive* (what the App MUST/SHOULD do to conform). The reorg of
`Application/ModernSetupApp/` and the `Tests/Smoke/smoke_validate.py` guards
both track this standard.

Key words **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT**, and **MAY** are
used in the RFC 2119 sense.

## 1. Scope and the ownership boundary

The App is a read-only first screen plus a set of safe entry points. It is **not**
a second setup-policy engine.

- The App **MUST NOT** parse IFR/VFR, implement `ConfigAccess`, mutate HII
  forms, or write varstores. Every real configuration action either launches a
  boot option (through `UefiBootManagerLib`) or enters native edk2 FormBrowser
  via `EFI_FORM_BROWSER2_PROTOCOL.SendForm()`.
- The App **MUST** present platform-specific policy (CPU frequency/voltage,
  memory timing/profile/RAS, chipset/SoC straps, fan curves, PCIe resource
  policy, BMC networking, key/TPM management) as a *summary* and/or a native
  *entry point* only. These remain native-owned.
- Information the App shows **MUST** come from the read-only `ModernUi*DataLib`
  providers (see the Provider Roadmap in the Feature Matrix), never from
  hard-coded board assumptions.

This boundary is identical to the one enforced by smoke; this document does not
relax it.

## 2. Platform classes

The App standardizes around five platform classes. The class is derived from the
SMBIOS form factor and management-capability providers at runtime; it is a
*presentation* hint only and never gates a security or policy decision.

| Class | Code intent | Typical examples |
| --- | --- | --- |
| `Client-Desktop` | Desktop / workstation / AIO / NUC / mini PC. | OVMF X64 desktop, ARM/LoongArch desktop. |
| `Client-Mobile` | Laptop / 2-in-1 / tablet with a battery. | Notebook-class products. |
| `Server` | Rack/blade/server boards with management. | x86/Arm/LoongArch servers, RISC-V server prototypes. |
| `Embedded` | Industrial / appliance boards. | ARM/RISC-V/LoongArch boards. |
| `Unknown` | Form factor not reported (common in VMs). | QEMU/OVMF without SMBIOS chassis data. |

`Unknown` **MUST** behave as the most inclusive superset that is still safe to
show — i.e. it follows the `Client-Desktop` card set plus any card whose
provider reports live data. The App never hides a card *because* the class is
unknown; it only hides a card that is both class-inapplicable **and** backed by
an unavailable provider.

## 3. Canonical page set

The App's category pages are exactly the `SETUP_PAGE` enum and **MUST** stay in
this order. Adding a page is an additive API change (append before `PageMax`).

| Page | Purpose | App shows | Native owner |
| --- | --- | --- | --- |
| `PageDashboard` | First-glance platform state. | System Information + Platform Health panels, quick-category grid. | — |
| `PageSystemInfo` | Read-only system specification detail. | Real platform identity (SMBIOS Type 1), CPU (Type 4), memory type/speed (Type 17), architecture, form factor, boot mode, firmware vendor/revision. A deeper companion to the dashboard System Information panel. | — (SMBIOS/UEFI read-only). |
| `PageBoot` | Boot inventory + launch. | `Boot####` active/hidden/category/path; Enter launches the selected entry. | Boot Maintenance HII. |
| `PageDevices` | Device / HII entry inventory. | HII formsets, device-path rows, Driver Health. | Each driver formset. |
| `PageSecurity` | Security posture. | Secure Boot, Setup Mode, PK/KEK/db/dbx, TPM/TCG/TCM presence. | SecurityPkg / platform HII. |
| `PageFirmware` | Firmware lifecycle. | Capsule support, humanized firmware revision, recovery/update entry. | Capsule/update HII or app. |
| `PageDiagnostics` | Bring-up / service visibility. | ACPI/SMBIOS presence, memory-map/handle/table counts, provider health. | Platform diagnostics HII. |
| `PageManagement` | Server / remote management. | BMC/IPMI/Redfish presence, host interface. | BMC/Redfish HII. |
| `PagePower` | Power / thermal visibility. | ACPI state, chassis thermal state, power-supply presence, demo health trend. | Platform power/thermal HII. |
| `PagePerformance` | CPU/memory + tuning entry. | Processor/memory inventory, CPU I/O protocol, virtualization/RAS entry hints. | Platform tuning/RAS/PCIe HII. |
| `PageServerInventory` | Server asset/management rollup + PCIe policy hints. | Management + PCIe capability summary; ReBAR/4G/SR-IOV entry hints. | Platform management/PCIe HII. |
| `PagePreferences` | App-local UX preferences. | Theme, density, language, OEM watermark toggle. | — (App-owned, no platform state). |
| `PageExit` | Session / shell control. | Continue, reset, native UiApp fallback, language. | Native FormBrowser save/discard. |

PCIe policy is surfaced through `PageServerInventory` / `PagePerformance` entry
hints; it does **not** get its own top-level page in the App, and the App
**MUST NOT** expose writable PCIe controls.

## 4. Dashboard structure

The dashboard **MUST** be three zones, top to bottom:

1. **System Information panel** — read-only identity/inventory: firmware vendor,
   humanized firmware revision (`major.minor (0xhex)`), platform, form factor,
   boot mode, memory, display mode. Rows that resolve to `N/A`/`Unknown`/`Limited
   data` **SHOULD** be collapsed (the row flows up) rather than shown as a dead
   placeholder.
2. **Platform Health panel** — architecture, provider health summary, coverage,
   first issue. Present when horizontal space allows; otherwise the System panel
   spans full width.
3. **Quick-category grid** — the standardized navigation cards in §5.

Status that already appears in panels 1–2 **MUST NOT** be the *sole* purpose of a
quick card: every quick card is a navigation entry first (Enter routes to its
page) and a one-line status second.

## 5. Standardized quick-category cards

The quick grid is an ordered catalog. Each card is a navigation entry: `Title` is
its category, `Value`/`Detail` are a one-line live status, and Enter routes to the
mapped page/focus. The canonical catalog:

| # | Card | Routes to | Group | One-line status |
| --- | --- | --- | --- | --- |
| 0 | Continue boot | `PageExit` / content | Exit | "Same as native Continue". |
| 1 | Boot options | `PageBoot` / content | Boot & Devices | Boot entry count + mode/secure hint. |
| 2 | Devices | `PageDevices` / content | Boot & Devices | HII handle / table count. |
| 3 | Provider status | `PageDiagnostics` / nav | Platform Health | Provider health + coverage. |
| 4 | Firmware | `PageFirmware` / nav | Platform Health | Vendor + humanized revision; capsule presence. |
| 5 | Power / Thermal | `PagePower` / nav | Power & Performance | Chassis thermal / sensor or ACPI+SMBIOS presence. |
| 6 | Performance | `PagePerformance` / nav | Power & Performance | CPU/Memory/PCIe readiness. |
| 7 | Server inventory | `PageServerInventory` / nav | Management | Management presence + PCIe root count. |

### 5.1 Per-platform-class applicability

Each card is `Always` (shown on every class), or class-scoped. A class-scoped
card is shown when its class matches **or** its backing provider reports live
data; otherwise it is hidden and the grid reflows.

| Card | Client-Desktop | Client-Mobile | Server | Embedded | Driver |
| --- | --- | --- | --- | --- | --- |
| Continue boot | Always | Always | Always | Always | — |
| Boot options | Always | Always | Always | Always | `ModernUiBootDataLib` |
| Devices | Always | Always | Always | Always | `ModernUiDeviceDataLib` |
| Provider status | Always | Always | Always | Always | diagnostics rollup |
| Firmware | Always | Always | Always | Always | `ModernUiFirmwareDataLib` |
| Power / Thermal | Always | Always (battery emphasis) | Always | Show if provider live | `ModernUiPowerDataLib` |
| Performance | Always | Always | Always | Show if provider live | `ModernUiPerformanceDataLib` |
| **Server inventory** | **Hidden** | **Hidden** | **Always** | Show if mgmt/PCIe live | `ModernUiManagementDataLib` / `ModernUiPcieDataLib` |

The only card that is hard class-scoped today is **Server inventory**: it is
server-class content (BMC/IPMI/Redfish + PCIe root policy) and **MUST** be hidden
on `Client-Desktop`/`Client-Mobile` **unless** a management or PCIe provider
reports live data (so a managed workstation or a desktop with discoverable PCIe
policy still surfaces it). On `Unknown`, it follows the live-provider rule.

Future class-scoped additions (e.g. a Battery card for `Client-Mobile`, a
Recovery card for `Embedded`) **SHOULD** extend this table rather than branch ad
hoc in drawing code.

### 5.2 Known gaps (non-blocking, tracked here)

- **Security has no quick card.** Security posture is reachable through the nav
  rail (`PageSecurity`) and summarized in the dashboard, but a first-class
  Security quick card is a recommended future addition (it is a P0 surface in the
  survey). Adding it is an additive change to this catalog.
- **Battery / Recovery cards** are not yet implemented for `Client-Mobile` /
  `Embedded`; the applicability table reserves their slots.

## 6. Conformance and enforcement

- The visible quick-card count is **variable** by platform class. Smoke
  **MUST** assert the *catalog* count (the array length) and the
  *route-table* length agree, and that every catalog card maps to a valid
  `SETUP_PAGE`. Smoke **MUST NOT** assert a fixed *visible* count, because that
  is now class-dependent.
- Card hiding **MUST** be data/class driven (a single applicability predicate),
  not a per-card `if` scattered through `ModernSetupDrawDashboard`.
- Every card route **MUST** resolve to a real page; a hidden card **MUST NOT**
  be focusable or Enter-activatable (keyboard navigation skips hidden cards).
- Localized card text **MUST** use only glyphs present in the embedded
  Noto Sans CJK SC subset (`Library/ModernUiRendererLib/ModernUiGlyphs.c`); when
  a Simplified-Chinese term is not covered, the English term is the
  graceful fallback (per the CJK strategy in
  [LvglProductizationPlan.md](LvglProductizationPlan.md)).

## 7. XArch (per-architecture) notes

The card *catalog* and *applicability* are architecture-neutral — the same App
build runs on X64, AARCH64, LOONGARCH64, and RISCV64. Architecture only affects
which providers report live data:

- `Server inventory` typically shows on x86/Arm servers; it is provider-gated on
  LoongArch/RISC-V server prototypes and hidden on all client/VM targets unless a
  provider is live.
- `Power / Thermal` and `Performance` degrade to presence/`N/A` on targets whose
  ACPI/SMBIOS/inventory providers are thin (common on RISC-V/LoongArch VMs).
- No card is gated on a hard-coded `ARCH` value; gating is by provider liveness
  and platform class only.

## 8. Change control

Changes to the canonical page set (§3) or the card catalog (§5) are user-visible
and **MUST** be recorded in `CHANGELOG.md` and reflected in both this standard
and the smoke guards in the same PR. The Chinese mirror
([AppFeatureStandard.zh-CN.md](AppFeatureStandard.zh-CN.md)) **MUST** be updated
alongside the English source.
