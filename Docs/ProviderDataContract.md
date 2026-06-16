<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Provider Data Contract

Language: English | [简体中文](ProviderDataContract.zh-CN.md)

This document is the **normative data-source contract** for ModernSetupPkg: for
every value the App and DisplayEngine show, it pins **which standard edk2
interface that value comes from** (SMBIOS structure type, ACPI table, or an edk2
protocol), the **read-only boundary**, the **source precedence / fallback**, and
the **evolution rule**. It is the data-side companion to the presentation-side
[App Feature Standard](AppFeatureStandard.md) and the reference
[IBV and Platform Setup Survey](IbvAndPlatformSetupSurvey.md).

Key words **MUST / MUST NOT / SHOULD / MAY** are RFC 2119.

## 1. Principles

1. **Standard sources only.** A provider field **MUST** be derived from a
   standard firmware interface (SMBIOS / ACPI / a UEFI or edk2 protocol), never
   from hard-coded board assumptions or a vendor-private back channel.
2. **Read-only.** Providers **MUST NOT** write variables, program hardware, or
   change policy. They observe; native FormBrowser/HII owns every mutation.
3. **Graceful absence.** A field whose source is absent **MUST** degrade to a
   localized `N/A`/`Unknown` (or a hidden row), never tofu, garbage, or a fake
   value.
4. **Architecture-neutral gating.** A field is gated by *source liveness*, not by
   a hard-coded `ARCH`. The same App build runs on X64 / AARCH64 / LOONGARCH64 /
   RISCV64; only which sources answer differs (see §6).
5. **Additive evolution.** Summary structs in `Include/ModernUi/*Data.h` grow by
   appending fields at the end (per `API_COMPATIBILITY.md`); never reorder.
6. **One access layer.** Structured-table access (SMBIOS walk + string/UUID
   extraction, ACPI table lookup) **SHOULD** go through a single shared helper
   library, not be re-implemented per provider (see §5).

## 2. Reference: what IBV / 信创 setups show

Information-architecture reference only (no asset/string reuse). Mainstream IBV
setup utilities (AMI Aptio, Insyde H2O, Phoenix SecureCore) and China-market /
信创 platforms (龙芯 Loongson, 飞腾 Phytium, 鲲鹏 Kunpeng, 海光 Hygon, 兆芯
Zhaoxin; firmware often edk2-derived or ByoCore/昆仑) converge on a similar
**System Information / Main** surface, plus deeper hardware pages:

| Common info-page item | Typical setup label | Standard source it maps to |
| --- | --- | --- |
| Firmware/BIOS version + date | "BIOS Version", "Build Date" | SMBIOS Type 0 |
| System identity | "Product Name", "Serial", "UUID" | SMBIOS Type 1 |
| Baseboard | "Motherboard", "Board Serial" | SMBIOS Type 2 |
| Processor | "Processor Type", "Speed", "Count" | SMBIOS Type 4 (+ MP Services for live count) |
| Processor cache | "L1/L2/L3 Cache" | SMBIOS Type 7 (or ACPI PPTT on Arm) |
| Total + per-DIMM memory | "Total Memory", "DIMM #, Size, Speed, Type" | SMBIOS Type 16/17 (+ UEFI memory map for total) |
| Memory slots | "Slot population" | SMBIOS Type 17 (one record/slot) |
| Storage devices | "SATA/NVMe device list" | BlockIo / DiskInfo / device paths |
| PCIe/expansion slots | "Slot occupancy, link" | SMBIOS Type 9 + PciIo (per-device) |
| Network | "MAC address", "NIC" | SimpleNetwork / device paths |
| Security posture | "Secure Boot", "TPM/TCM" | UEFI vars + TCG2 protocol |
| 信创-specific | "可信计算 TCM (国密)", "国产化平台标识" | TCG2/vendor protocol + SMBIOS identity |

信创 nuance: domestic platforms emphasize **可信计算 / 国密 TCM** alongside or
instead of TPM, and a **platform/vendor identity** ("国产化标识"). Both map to
existing read-only sources (TCG2 presence, SMBIOS Type 1/2) — no new policy
surface is implied.

## 3. Data-source map (domain → field → edk2 source → status)

Status: **Done** = surfaced today; **Gap** = a named field with a known source
not yet wired; **Roadmap** = larger follow-up.

### Platform / System identity — `ModernUiPlatformDataLib`

| Field | Standard source | Status |
| --- | --- | --- |
| Firmware vendor | `gST->FirmwareVendor` | Done |
| Firmware revision (humanized) | `gST->FirmwareRevision` | Done |
| BIOS version / release date | **SMBIOS Type 0** (`BiosVersion`, `BiosReleaseDate`) | Done |
| System product / manufacturer | **SMBIOS Type 1** | Done |
| Serial / UUID | **SMBIOS Type 1** (`SerialNumber`, `Uuid`) | Done |
| Baseboard | **SMBIOS Type 2** | Done |
| Form factor | **SMBIOS Type 3** (chassis type) | Done |
| Architecture | compile-time `MDE_CPU_*` | Done |
| Memory total (MiB) | **UEFI memory map** (`GetMemoryMap`) | Done |
| Memory type/speed/DIMM count | **SMBIOS Type 17** (aggregate) | Done |
| Display mode | **GraphicsOutput** active mode | Done |

### Processor — `ModernUiPlatformDataLib` + `ModernUiPerformanceDataLib`

| Field | Standard source | Status |
| --- | --- | --- |
| Processor version / model | **SMBIOS Type 4** (`ProcessorVersion`) | Done |
| Core / thread count | **SMBIOS Type 4** (`CoreCount`/`ThreadCount` + `*2`) | Done |
| Live enabled core/thread count | **MP Services** (`EFI_MP_SERVICES_PROTOCOL`) | Done |
| Current / max speed (MHz) | **SMBIOS Type 4** (`CurrentSpeed`/`MaxSpeed`) | Done |
| **L1 / L2 / L3 cache** | **SMBIOS Type 7** (Cache Information); **ACPI PPTT** on Arm | Done (Type 7) |
| Processor inventory present | SMBIOS Type 4 presence | Done (boolean) |

> Note: CPU data is currently split — identity in Platform, presence booleans in
> Performance. The contract target is a single coherent processor summary fed by
> Type 4 + Type 7 (+ MP Services for the live count), with Performance keeping
> only the tuning/RAS *entry-availability* hints.

### Memory (per-DIMM) — `ModernUiPlatformDataLib`

| Field | Standard source | Status |
| --- | --- | --- |
| Aggregate type / speed / DIMM count | SMBIOS Type 17 (first populated) | Done |
| Per-slot: locator, size, speed, type, rank | **SMBIOS Type 17** (one record/slot) | Gap |
| Array max capacity / slot count | **SMBIOS Type 16** | Gap |

### PCIe — `ModernUiPcieDataLib`

| Field | Standard source | Status |
| --- | --- | --- |
| Controller / root-bridge / endpoint / bridge counts | **PciIo / PciRootBridgeIo** enumeration | Done |
| Policy-entry presence hints (ReBAR/4G/SR-IOV/ASPM/…) | protocol presence probes | Done (read-only hints) |
| Per-device vendor/device ID, class | **PciIo** config space `0x00`/`0x09` | Done |
| Per-device link speed / width | **PciIo** PCIe capability (`0x10` cap) config reads | Done |
| Physical slot occupancy | **SMBIOS Type 9** (System Slots) | Gap |

> Boundary (unchanged, smoke-enforced): PCIe **policy** — ReBAR, Above-4G,
> SR-IOV, ASPM, bifurcation, hot-plug, ACS/ARI, IOMMU, BAR/resource allocation —
> **stays native HII/FormBrowser**. PciIo here is **read-only enumeration for
> display**; the App MUST NOT call `SetBarAttributes` or mutate config space.

### Storage — `ModernUiInventoryDataLib`

| Field | Standard source | Status |
| --- | --- | --- |
| Bootable storage presence | device-path inventory (via Devices) | Done (indirect) |
| Device bus type (NVMe/SATA/…) + capacity | **DiskInfo** (`EFI_DISK_INFO_PROTOCOL`) + BlockIo | Done |
| Device model string | DiskInfo Identify/Inquiry parse | Roadmap |

### Network — `ModernUiInventoryDataLib` + `ModernUiManagementDataLib` (server)

| Field | Standard source | Status |
| --- | --- | --- |
| Management host interface | **SMBIOS Type 38/42** (IPMI/Redfish) | Done (presence) |
| IPMI / Redfish protocol presence | protocol probes | Done |
| NIC MAC / link state | **SimpleNetwork** (`Mode->CurrentAddress`/`MediaPresent`) | Done |

### Diagnostics / ACPI — `ModernUiDiagnosticsDataLib`

| Field | Standard source | Status |
| --- | --- | --- |
| ACPI table presence | **ACPI** (RSDP/XSDT via config table or `EFI_ACPI_SDT_PROTOCOL`) | Done |
| SMBIOS table presence | SMBIOS protocol | Done |
| Memory map / handle / config-table counts | UEFI boot services | Done |
| NUMA topology (nodes, distance) | **ACPI SRAT / SLIT** | Roadmap (server) |

### Power / Thermal — `ModernUiPowerDataLib` + `ModernUiHardwareHealthDataLib`

| Field | Standard source | Status |
| --- | --- | --- |
| ACPI table/protocol state | ACPI presence | Done |
| Chassis thermal state, power supply | **SMBIOS Type 3 / Type 39** | Done (presence) |
| Real sensor temperatures | platform sensor source (none standard in UEFI) | Demo-only |

### Security — `ModernUiSecurityDataLib`

| Field | Standard source | Status |
| --- | --- | --- |
| Secure Boot / Setup Mode | **UEFI variables** (`SecureBoot`, `SetupMode`) | Done |
| PK/KEK/db/dbx presence | UEFI variables | Done |
| TPM / TCG presence | **TCG2** (`EFI_TCG2_PROTOCOL`) | Done |
| TCM / 国密 trusted computing | vendor/TCG protocol presence | Roadmap (信创) |

## 4. Source precedence and fallback

When more than one standard source can answer a field, providers **MUST** prefer
in this order and fall through on absence:

1. **Live/dynamic protocol** when it reflects the *running* state (e.g. MP
   Services for the actually-enabled core count; PciIo for present devices).
2. **SMBIOS** for static identity/inventory (vendor-populated; tends to exist on
   x86/Arm servers, optional on RISC-V/LoongArch).
3. **ACPI** for topology where SMBIOS is thin (e.g. ACPI PPTT for Arm cache/CPU
   topology, SRAT/SLIT for NUMA).
4. **UEFI core services** (memory map, handle database) as the architecture-
   neutral floor.
5. **`N/A` / hidden row** when none answer.

Placeholder strings ("To Be Filled By O.E.M.", "Not Specified", …) **MUST** be
treated as absent (already done for SMBIOS identity).

## 5. Shared access layer (the "connect" work)

Previously four providers each `LocateProtocol(gEfiSmbiosProtocolGuid)` and
walked tables independently, and SMBIOS string/UUID extraction was
re-implemented per provider — duplication, and the place strict-alignment bugs
hid (the AArch64 packed-UUID fault).

**Status: implemented.** **`ModernUiPlatformTablesLib`**
(`Include/ModernUi/ModernUiPlatformTables.h`) is the single table-access layer:

- `ModernUiSmbiosFindStructure(type, index)`, `ModernUiSmbiosTypePresent(type)`,
  NUL-safe `ModernUiSmbiosGetString()`, and `ModernUiSmbiosIsPlaceholder()`.
- ACPI RSDP/XSDT (RSDT fallback) walk via `ModernUiAcpiFindTable(signature)` /
  `ModernUiAcpiTablePresent(signature)`.
- `ModernUiPlatformDataLib` consumes it (its private SMBIOS string/placeholder
  helpers and per-reader `LocateProtocol`+`GetNext` prologues were removed).

Remaining: migrate the other SMBIOS/ACPI-reading providers
(`ModernUiPerformanceDataLib`, `ModernUiManagementDataLib`,
`ModernUiPowerDataLib`, `ModernUiDiagnosticsDataLib`) onto it, then add a smoke
guard that a provider `LocateProtocol`ing SMBIOS or walking ACPI directly
(outside the shared lib) is a finding.

This is a pure refactor behind the existing `*Data.h` contract — no public API
change — and is the prerequisite that makes the optional protocol step (§7)
clean.

## 6. Architecture coverage (XArch)

| Source | X64 | AARCH64 | LOONGARCH64 | RISCV64 |
| --- | --- | --- | --- | --- |
| SMBIOS (Type 0–17, 38/42) | Common | Common (server) | Optional | Optional |
| ACPI (PPTT/SRAT/SLIT/MADT) | Common | Common | ACPI or DT | ACPI or DT |
| PciIo / PciRootBridgeIo | Common | Platform-dep. | Platform-dep. | Emerging |
| MP Services | Common | Common | Platform-dep. | Platform-dep. |
| TCG2 | Common | Platform-dep. | Platform-dep. (incl. 国密 TCM) | Platform-dep. |
| UEFI memory map / GOP | Yes | Yes | Yes | Yes |

No field is gated on a hard-coded ARCH; thin-SMBIOS targets (RISC-V/LoongArch
VMs) simply fall to ACPI/UEFI/`N/A` per §4. Device-tree-only platforms are a
documented follow-up (no standard UEFI DT consumer surface yet).

## 7. Optional: provider protocol (deferred, ABI-gated)

The 11 `*Data.h` summaries are currently a **link-time (LibraryClass) contract**.
Promoting them to a **runtime protocol** (`EFI_MODERN_SETUP_*_PROVIDER_PROTOCOL`,
GUID in `ModernSetupPkg.dec`) would let a platform/OEM driver *install* a richer
provider (real BMC/sensor/RAS data) that the App picks up at runtime, with the
built-in providers as fallback. `ModernSetupAppProvider.c` (the single place
providers are called) is the natural seam: `LocateProtocol` first, built-in
fallback.

This is a **public-ABI commitment** (binary compatibility forever; the summary
structs gain a `Size`/`Revision` field) and **MUST** go through `core-api`
review per `API_COMPATIBILITY.md`. It **SHOULD** be deferred until a concrete
consumer (a platform that wants to inject data) exists — premature
protocol-ization adds ABI burden without a beneficiary. §5 (shared access) and
the gap fills in §3 deliver the "unify the data sources" value with zero ABI
cost in the meantime.

## 8. Change control

Adding a field or a source mapping is user-visible: record it in `CHANGELOG.md`,
update this contract and its [简体中文 mirror](ProviderDataContract.zh-CN.md) in
the same PR, and keep the summary-struct change additive per
`API_COMPATIBILITY.md`. The presentation side (which page shows the field) is
governed by [AppFeatureStandard.md](AppFeatureStandard.md).
