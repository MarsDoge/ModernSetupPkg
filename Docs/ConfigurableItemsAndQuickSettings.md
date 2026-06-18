<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Configurable Items and Quick Settings (normative)

Language: English | [简体中文](ConfigurableItemsAndQuickSettings.zh-CN.md)

This document defines **how, and how far, ModernSetupPkg may make platform
configuration "UI-configurable"** in the modern shell. It surveys the
high-churn configurable items found across IBV and other platform
BIOS, and binds each to a sanctioned handling tier.

It is a companion to:

- [IbvAndPlatformSetupSurvey.md](IbvAndPlatformSetupSurvey.md) — the broader
  setup-surface taxonomy.
- [ProviderDataContract.md](ProviderDataContract.md) — read-only data sourcing.
- [MODULE_BOUNDARIES.md](MODULE_BOUNDARIES.md) — the layer contract.

> **One-line rule.** Editing of platform policy happens **only** in native
> FormBrowser through each driver's ConfigAccess; the modern shell **curates and
> deep-links** to those questions and **renders** them, but never writes a
> varstore itself. "Modern UI-configurable SR-IOV" means *a curated entry that
> opens the platform's real SR-IOV question, rendered by ModernDisplayEngine* —
> not a varstore write performed by `ModernSetupApp`.

## 1. What is even configurable in edk2

Not every PCD can be changed from a UI. The mechanism, not the wish, decides.

| PCD type | Runtime-mutable? | UI-configurable? |
| --- | --- | --- |
| `FixedAtBuild` / `PatchableInModule` / `FeatureFlag` | No (compile-time) | **No** — needs a rebuild / build switch, out of scope for any UI |
| `Dynamic` / `DynamicEx` | Yes (PCD database / HOB) | Only if the platform wires it to an HII question |
| **`DynamicHii`** | Yes — **bound to an NV variable** | **Yes — this is the real carrier of "UI-configurable"** |

Consequence: the user-configurable surface in edk2 is **HII questions bound to
NV varstores** (frequently surfaced as `DynamicHii` PCDs). On real platforms
SR-IOV / Above-4G / IOMMU / SATA-mode are typically exactly such questions,
authored in VFR by the silicon/platform code. If an item is `FixedAtBuild`, no
UI can change it at runtime.

## 2. Non-negotiable boundary (smoke-enforced)

`ModernSetupApp` MUST NOT parse IFR, call ConfigAccess, or write varstores.
`Tests/Smoke/smoke_validate.py` fails the build if app sources contain
`ExtractConfig`, `RouteConfig`, `SetVariable`, or `HiiSetBrowserData`. PCIe
policy (ReBAR / Above-4G / SR-IOV / ASPM / bifurcation / hot-plug / ACS / ARI /
IOMMU / BAR) stays native-owned.

Why a direct write is wrong (not merely disallowed): writing the raw variable
bypasses the platform's ConfigAccess callbacks — cross-question
`suppressif`/`grayoutif`/`disableif` logic, range/consistency validation,
default handling, and interactive warnings/reconfiguration. Setting a "SR-IOV
enabled" byte without that chain can silently mis-configure or brick a platform.
The dependency graph belongs to the platform; the modern shell does not own it.

## 3. Handling tiers

Every configurable item maps to exactly one tier.

| Tier | What the shell does | Who writes | Examples |
| --- | --- | --- | --- |
| **A — App-owned direct edit** | The shell reads and writes its own state. | `ModernSetupApp` (its own store) | Language, theme, EZ/Advanced mode, favorites, App preferences (`ModernUiPreferencesLib`). |
| **B — Curated quick-settings deep-link** | Locate a known high-churn HII question, present it grouped in a modern page; on activate, `SendForm()` into the owning formset/form/question. **Edit stays native; ModernDisplayEngine renders it.** | Native FormBrowser + platform ConfigAccess | Secure Boot, TPM enable, VT-d/IOMMU, SR-IOV, Above-4G, ReBAR, SATA mode, primary display, WoL, AC-loss restore, fast boot, TCM. |
| **C — Whole native page** | Open the entire formset via `SendForm()`. | Native FormBrowser + platform ConfigAccess | RAS, NUMA, memory timing, CPU voltage, BMC networking, multi-question flows. |

Tier B is the new productization work. It does **not** relax the boundary: the
only new code is *discovery + grouping + a deep-link `SendForm` target*. The
modern "switch" look comes from the DisplayEngine rendering the platform's
existing checkbox/oneof question, not from the App owning the value.

## 4. Configurable-item inventory

Churn = how often an end user changes it. Tier = §3 handling.

| Domain | High-churn items | IBV firmware | Other platforms | Churn | Tier |
| --- | --- | --- | --- | --- | --- |
| Boot | Boot order, Fast Boot, CSM/Legacy (x86), PXE/HTTP boot, timeout | All | Most (non-x86 platforms usually omit CSM) | High | B/C |
| Security | Secure Boot, TPM/PTT/fTPM enable, clear TPM, passwords | All | **+ TCM (trusted computing)**, Secure Boot certs | High | B/C |
| Virtualization / isolation | VT-x/SVM, **VT-d/IOMMU/SMMU**, **SR-IOV**, ACS/ARI/PASID | All | Server platforms; some Arm expose SMMU | Med-High | **B/C** |
| PCIe resource | **Above-4G, ReBAR, ASPM, link speed, bifurcation, hot-plug** | All | Server side | Med-High | B/C |
| CPU | SMT, C-states, Turbo/Boost, P-state/CPPC, core enable | All | Server platforms; partial elsewhere | Med | C |
| Memory | XMP/EXPO, frequency, **ECC, patrol scrub, NUMA/SNC/NPS, interleave** | Desktop XMP; server RAS | Server RAS/NUMA | Med | C |
| Storage | **SATA mode (AHCI/RAID)**, VMD, NVMe RAID, Opal | x86 all | Platform-dependent | Med | B/C |
| Graphics | Primary display (iGPU/dGPU/Auto), UMA size, hybrid/mux | Desktop/laptop | iGPU platforms | Med | B/C |
| Power | **ErP/Deep S5, Wake-on-LAN, AC-loss restore**, RTC wake; (laptop) charge threshold, lid behavior | All | Most | High | B/C |
| Thermal | Fan mode (silent/standard/perf), fan curves | Desktop/server | Server/industrial | Med | C |
| Network | Onboard LAN enable, WoL, network stack, MAC passthrough | All | Most | Med | B/C |
| Management (server) | BMC network (DHCP/static), IPMI over LAN, Redfish enable | Server | Server platforms | Med | C |
| Trusted computing | **Trusted Cryptography Module (TCM), compatibility mode, kernel integrity measurement** | — | Some platforms | Med | B/C |
| Self / branding | Language, EZ/Advanced, favorites, theme, date/time | All | All | High | **A** |

## 5. Quick Settings (Tier B) design

A new modern page (`PageQuickSettings`, opt-in) that:

1. **Discovers** known high-churn questions across installed HII formsets using
   the existing keyword-probe approach (`HasHiiFormsetKeyword`-style scanning in
   a provider), producing a typed read-only list of *(group, label, owning
   formset/form/question coordinates, present?)*. No IFR mutation, no ConfigAccess.
2. **Groups** them (Security / Virtualization / PCIe / Power / Boot) and
   renders each as a modern row with the platform-reported current value when it
   can be read read-only (else "Configure ›").
3. On activate, calls `EFI_FORM_BROWSER2_PROTOCOL.SendForm()` targeting the
   owning formset (optionally a specific `FormId`/`QuestionId` jump). The native
   browser performs the edit; `ModernDisplayEngineDxe` renders it modern.

Constraints (carried into review and smoke):

- The discovery provider is **read-only**: it MAY read HII string/formset
  metadata; it MUST NOT call `ExtractConfig`/`RouteConfig`/`SetVariable`/
  `HiiSetBrowserData`, and MUST NOT use `EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL`
  set paths.
- Quick Settings rows are **entry points**, not editors. The only state the App
  writes is its own favorites/ordering (Tier A).
- Items whose owning question is absent are hidden (graceful degradation), never
  fabricated.

### Out of scope (needs separate architecture review)

True inline single-question editing without re-entering FormBrowser — via
`EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL` (x-UEFI keyword Get/SetData) or by driving
ConfigAccess directly — is **explicitly out of scope**. It bypasses
FormBrowser's `suppressif`/`grayoutif`, validation, defaults, and interactive
callbacks, is currently smoke-blocked, and would require its own design review
(a "FormBrowser-backed inline question host" that still drives the full
validation chain).

## 6. References

- UEFI PI/UEFI spec — HII, ConfigAccess, ConfigKeywordHandler, FormBrowser2.
- edk2 `MdeModulePkg` — `SetupBrowserDxe`, `DisplayEngine`, PCD database, HII.
- Mainstream IBV setup
  references (visual/IA only; see IbvAndPlatformSetupSurvey.md).
- Other platform UEFI setup references; trusted-computing (TPM/TCM) documentation.
