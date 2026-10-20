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

## Native HII presentation

Native forms retain FormBrowser editing and validation. The header clock refreshes
once per second in the idle and key-entry waits, repainting only its text rectangle;
it is not a form refresh and must not restart the form-exit timeout. Clearing the
page disables clock painting until chrome is drawn again.

Full-screen graphical forms at 720px height or above use a private 32px-target row
grid without changing the console mode. Explicit caller screen bounds retain their
original grid. Message popups use the same viewport. Native forms show their real
title, graphite selection/value surfaces and contextual help; duplicate hardware
telemetry remains on the front page rather than the configuration form.

## Secure Boot native configuration

Advanced > Runtime opens Quick Settings through both keyboard and pointer routing,
including when Dashboard cards are filtered. Power remains reachable from Dashboard.

Quick Settings Secure Boot (row 4) and Security's selectable **Secure Boot
configuration** entry match `5daf50a5-ea81-4de2-8f9b-cabda9cf5c14` from pinned
edk2 `SecurityPkg/Include/Guid/SecureBootConfigHii.h`. Discovery uses all cached
DeviceData entries, not translated titles or the visible Devices row cap.
`Open setup` / `Unavailable` / `Discovery error` describe entry discovery,
independently of the read-only SecureBoot enabled state. Activation refreshes
discovery, then uses `ModernUiDeviceDataOpenEntry()` and FormBrowser2; the
configured ModernDisplayEngine/LVGL remains the renderer. Missing forms return
`EFI_NOT_FOUND`, never UiApp/BootManagerMenuApp or unrelated HII. Native callbacks
own keys and policy; no new IFR parsing, ConfigAccess or variable writes are added.
The shared handoff invalidates boot/device/provider caches even on error, so
security summaries refresh lazily on return. TPM remains a presence summary,
not a configuration entry. Keyboard and pointer use the same activation path;
Quick Settings retains select-then-activate clicks with shared grouped-row geometry.

Host coverage: `python3 Tests/Smoke/boot_handoff_test.py` and smoke guards.
Firmware builds and LVGL visual/callback validation remain separate requirements.

## 1. Audience model and ownership boundary

The App standard serves two audiences, and the UI **MUST NOT** confuse them:

| Audience | What they care about | Where it belongs |
| --- | --- | --- |
| Interface user | UI category/display implementation: which setup category a field belongs to, how it is grouped, what label/value/status is visible, what row is selectable, and what native entry point the UI exposes. | Visible UI, page/category layout, labels, row states, entry-point affordances, screenshots. |
| Developer / contributor | Interface-flow implementation: how the value moves from SMBIOS/ACPI/UEFI services/PI protocols/native HII/app NV into a provider, how the app consumes it, who owns writes, what fallback/status means, and what validation guards the flow. | `ProviderDataContract.md`, issues such as #45, code comments, smoke guards, provider headers, and implementation docs. |

Visible pages should therefore focus on category, label, value, state, and safe
entry points (for example, "Native Security setup owns keys and TPM policy").
The formal source/access/provider/native-owner/fallback/status flow belongs in
developer-facing docs and issues.

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

## 3. Canonical page set and IBV product IA

The C implementation still exposes the `SETUP_PAGE` enum, and enum additions
**MUST** remain append-only before `PageMax`. The product-facing information
architecture, however, should read like an IBV setup utility: **Main, Advanced,
Chipset / Platform, Boot, Security, Server Management, Power & Thermal,
Diagnostics, Preferences, Save & Exit**. The table below is the normative mapping
from that IBV product IA to the current enum names.

| Product IA | Current page | Purpose | App shows | Native owner |
| --- | --- | --- | --- | --- |
| Main | `PageDashboard` + `PageSystemInfo` | First-glance platform state and detailed read-only system specification. | BIOS/firmware version, build/release date, product/board identity, CPU, memory, architecture, boot mode, display mode, provider health, quick-category grid. | — (SMBIOS/UEFI/provider read-only). |
| Boot | `PageBoot` | Boot inventory + launch. | `Boot####` active/hidden/category/path, BootNext/BootOrder affordances, native boot tools fallback, source/owner/access/fallback prompt. | Boot Maintenance HII. |
| Advanced | `PageDevices` + `PageQuickSettings` + selected `PagePerformance` rows | Curated entry points to high-churn platform setup without becoming the policy owner. | HII formsets, device-path rows, Driver Health, Quick Settings native-owner hints, CPU/memory/tuning entry availability. | Each driver formset through FormBrowser2. |
| Chipset / Platform | `PageDevices` + `PageServerInventory` + provider subsections | Platform fabric and onboard-device visibility. | HII/device entries, PCIe/root-bridge inventory, resource-entry hints, storage/network/device inventory when providers expose it. | Platform chipset/SoC/PCIe/storage HII. |
| Security | `PageSecurity` | Security posture. | Secure Boot, Setup Mode, PK/KEK/db/dbx, TPM/TCG/TCM presence, native security ownership prompt. | SecurityPkg / platform HII. |
| Firmware Update / Recovery | `PageFirmware` | Firmware lifecycle. | Capsule support, humanized firmware revision, recovery/update entry. | Capsule/update HII or app. |
| Server Management | `PageManagement` + `PageServerInventory` | Server / remote management and asset rollup. | BMC/IPMI/Redfish presence, host interface, management inventory, PCIe policy-entry hints. | BMC/Redfish/platform management HII. |
| Power & Thermal | `PagePower` | Power / thermal visibility. | ACPI state, chassis thermal state, power-supply presence, sensor provider summary, source/owner/access/fallback prompt. | Platform power/thermal HII, EC, BMC, or service app. |
| Performance / Tuning | `PagePerformance` | CPU/memory and tuning visibility. | Processor/memory inventory, CPU I/O protocol, virtualization/RAS/native tuning entry hints. | Platform performance/tuning/RAS HII. |
| Diagnostics | `PageDiagnostics` | Bring-up / service visibility. | ACPI/SMBIOS presence, memory-map/handle/table counts, provider health, first degraded provider. | Platform diagnostics HII or service app. |
| Preferences / UX | `PagePreferences` | App-local UX preferences. | Theme, density, language, OEM watermark toggle. | — (App-owned, no platform state). |
| Save & Exit | `PageExit` | Session / shell control. | Continue, reset, native UiApp fallback, language. | Native FormBrowser save/discard/default workflows. |

`PageQuickSettings` is a Tier-B curated entry surface, not a writable policy
page: rows are selectable so Enter can report the native owner or handoff status,
but edits still happen only after entering the owning FormBrowser page. PCIe
policy is therefore surfaced through Advanced / Chipset / Server Management entry
hints; it does **not** get its own independent writable top-level page in the App,
and the App **MUST NOT** expose writable PCIe controls.

### Boot configuration handoff

The Boot page's **Native Boot Tools** row refreshes DeviceData discovery and
matches the installed edk2 Boot Maintenance formset GUID
`642237c7-35d4-472d-8365-12e0ccf27a22` across all entries, not just visible
Devices rows. `ModernUiDeviceDataOpenEntry()` enters FormBrowser2; the configured
DisplayEngine, including LVGL, renders the native form. The App does not parse
IFR, invoke ConfigAccess, or add policy writes. Existing BootNext/BootOrder
provider wrappers remain unchanged; those affordances are not read-only.

Replace-UiApp overlays retain upstream `BootMaintenanceManagerUiLib` as a NULL
library on the replacement component. Its constructor/destructor owns HII
registration and cleanup; native callbacks own configuration. Without this
dependency, replacing UiApp also removes its Boot Maintenance owner.
Standalone app builds rely on an already installed formset or fallback.

If no usable matching formset exists, the existing fallback remains: UiApp
normally, BootManagerMenuApp in replace-UiApp images to prevent self-recursion.
**BootManagerMenuApp is a boot picker, not Boot Maintenance.** Discovery errors
and matched-handoff errors are returned, not hidden by opening another UI after
a possible edit. Returning from any Devices FormBrowser handoff or native
fallback `StartImage()` invalidates boot options, device entries and provider
snapshots even on error; the next access rebuilds them lazily. Exit's native
fallback remains separate from the Boot configuration row.

Host validation: `python3 Tests/Smoke/boot_handoff_test.py` compiles the real
routing functions with mocked provider/cache APIs; smoke guards the routing and
overlay contracts. Firmware validation must additionally open Boot → Native
Boot Tools, save a native boot setting, return, and inspect refreshed summaries.
Host tests alone do not prove native callbacks or LVGL rendering.

## 4. Dashboard structure

The dashboard **MUST** be three zones, top to bottom:

1. **System Information panel** — read-only identity/inventory: firmware vendor,
   humanized firmware revision (`major.minor (0xhex)`), platform, form factor,
   boot mode, CPU identity, memory, display mode. Rows that resolve to
   `N/A`/`Unknown`/`Limited data` **SHOULD** be collapsed (the row flows up)
   rather than shown as a dead placeholder. The detailed `PageSystemInfo` view
   **SHOULD** group the same Main data as System Identity, Firmware Identity,
   Processor, Memory, and Runtime, and may include optional SMBIOS/UEFI detail
   rows such as serial number, UUID, BIOS version/date, processor speed, cache,
   and logical processor count when providers report them.
2. **Platform Health panel** — architecture, provider health summary, coverage,
   first issue. Present when horizontal space allows; otherwise the System panel
   spans full width.
3. **Quick-category grid** — the standardized navigation cards in §5.

Status that already appears in panels 1–2 **MUST NOT** be the *sole* purpose of a
quick card: every quick card is a navigation entry first (Enter routes to its
page) and a one-line status second.

## 5. Standardized quick-category cards and top navigation

Top navigation **MUST** expose only the reduced top-level product IA, not every
concrete page. The horizontal strip is limited to the primary categories:
`Main`, `Advanced`, `Boot`, `Security`, and `Exit`. Detailed pages such as
Firmware, Diagnostics, Management, Power, Performance, Quick Settings, Assets,
and Preferences are second-level destinations reached through the Dashboard
quick-card directory or their owning category.

Second-level placement is shown as a fixed-visible, clickable vertical rail in
the left-side content area, not pinned to the far-left screen edge and not as
another all-pages horizontal dump. The Dashboard remains a full-width entry
directory and does not show this rail. Detail/category pages show a rail that
only lists groups for the selected top-level category, for example Advanced
shows `Platform`, `Runtime`, `Service`, and `UX`; Security shows `Posture`,
`Secure Boot`, and `TPM`; Boot shows `Order` and `Native Tools`. Clicking a
second-level group routes to its representative third-level page, while
third-level placement stays in the page title hierarchy, e.g.
`Advanced > Runtime > Power` or `Main > System > Inventory`, and in the content
rows/cards.

The quick grid is an ordered directory. Each card is a navigation entry: `Title`
is the category, `Value`/`Detail` is one live status line, and Enter routes to the
mapped page/focus. Normative catalog:

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
