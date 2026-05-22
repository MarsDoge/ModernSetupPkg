<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Productization Feature Matrix

Language: English | [简体中文](ProductizationFeatureMatrix.zh-CN.md)

See also: [ProductizationValidationMatrix.md](ProductizationValidationMatrix.md) for
the Phase30 evidence-backed validation matrix and smoke-gate contract.

ModernSetup has two XArch productization layers:

```text
Standard front-page App
  -> common dashboard, inventory, boot, security, diagnostics, and entry points

Native edk2 FormBrowser path
  -> platform/OEM HII pages, callbacks, validation, varstores, and policy
```

XArch is ModernSetupPkg's cross-architecture model for keeping one Setup UX, one
HII/FormBrowser ownership boundary, and one validation vocabulary across X64,
AARCH64, LOONGARCH64, and RISCV64 targets. XArch does not replace edk2 ARCH
values; build and product integration still use concrete architecture names.

The App should provide a consistent first screen across x86/X64, ARM/AARCH64,
RISC-V/RISCV64, and LoongArch/LOONGARCH64 products. It must not clone platform
setup policy or parse IFR. When a setting is platform-specific, the App should
show a summary or entry point, then open the owning HII form through
`EFI_FORM_BROWSER2_PROTOCOL.SendForm()`.

## Platform Classes

| Platform class | App goal | Examples |
| --- | --- | --- |
| Desktop / workstation | Fast boot, security, device, firmware update, and diagnostics overview. | OVMF/x86, ARM workstation, LoongArch desktop. |
| Server | Platform inventory, management, RAS, boot policy, security posture, remote lifecycle entry points. | x86 server, Arm server, LoongArch server, RISC-V server prototypes. |
| Embedded / industrial | Device inventory, boot source, secure state, update status, recovery entry points. | ARM/LoongArch/RISC-V boards. |
| Tablet / appliance | Minimal dashboard, boot/recovery, security, firmware update, input/display status. | ARM appliance-class products. |

## Standard App Pages

| Page | Common purpose | App should show | Complex settings owner | Current status |
| --- | --- | --- | --- | --- |
| Dashboard | First-glance platform state. | Firmware vendor/revision, architecture, form factor, boot mode, platform name, memory, display mode, boot count, Secure Boot state, HII/device count, provider availability. | App data providers. | Basic implemented. |
| Boot | Boot inventory and launch entry. | `BootOrder`, `Boot####`, active/hidden state, category, device-path summary, launch selected option. | Boot Maintenance HII pages for editing and advanced policy. | Basic implemented. |
| Devices / HII | Entry point to platform setup pages and device inventory. | HII formsets, driver/device path rows, Driver Health entry, inventory rows. | Each driver formset via FormBrowser2. | Basic implemented. |
| Security | Read-only security posture. | Secure Boot, Setup Mode, PK/KEK/db/dbx state, TPM/TCG/TCM presence when available. | Security HII pages and platform policy drivers. | Basic implemented. |
| Firmware Update | Firmware lifecycle entry point. | Capsule support, firmware version, recovery/update entry, last update state when available. | Capsule/update HII or platform update app. | Basic read-only implemented. |
| Diagnostics / Logs | Bring-up and service visibility. | POST/log summary, error count, ACPI/SMBIOS presence, memory map summary, test hooks. | Platform diagnostics HII or service app. | Basic read-only implemented. |
| Management | Server/remote management summary. | BMC/IPMI/Redfish presence, management NIC, host interface, remote update support. | BMC/IPMI/Redfish platform drivers. | Basic read-only implemented. |
| Power / Thermal | Power and cooling visibility. | ACPI table/protocol state, chassis thermal state, power supply record presence, and read-only demo Hardware Health temperature curves. | Platform fan, battery, thermal, and power-policy HII. | Basic read-only implemented; Hardware Health is demo data only. |
| Performance / Tuning | CPU/memory and tuning entry visibility. | Processor inventory, memory inventory, CPU I/O protocol, virtualization/RAS policy entry availability. | Platform performance, overclocking, NUMA/RAS, PCIe policy HII. | Basic read-only implemented. |
| PCIe Policy | PCIe inventory and policy-entry visibility. | Controller/root-bridge/endpoint counts, protocol presence, and read-only capability hints for ReBAR, Above 4G, SR-IOV, ASPM, bifurcation, hot-plug, ACS/ARI, and IOMMU. | Platform PCIe policy HII and native FormBrowser pages. | Basic read-only foundation implemented. |
| Exit | Session and shell control. | Continue, reset, native UiApp, language, theme, app/version info. | Native FormBrowser save/discard where needed. | Basic implemented. |

## Broad Setup Taxonomy to App IA / Provider Mapping

`ModernSetupApp` maps broad IBV/platform setup surfaces to standard category
landing pages and read-only provider summaries. It is not a second setup policy
engine: CPU frequency/voltage, memory timing/profile, chipset/SoC controls, fan
curves, PCIe resource policy, and similar board policy stay native
HII/FormBrowser-owned unless a later allowlisted design explicitly adds a safe
App-owned control.

| Domain / subsystem | Standard App IA | Provider / data source direction | App display / entry behavior | Native owner / non-goal |
| --- | --- | --- | --- | --- |
| System overview / Main | Dashboard; Exit for language/session affordances. | `ModernUiPlatformDataLib`, UEFI system table, SMBIOS when available. | Show firmware, architecture, form factor, platform name, memory/display summary, language/session hints. | Date/time, password, defaults, and save/discard workflows remain native. |
| Boot manager / boot policy | Boot; Dashboard quick category. | `ModernUiBootDataLib`, Boot#### variables, Boot Manager services. | List boot entries, active/hidden/category/path summary, and launch selected option. | Boot order editing, one-time boot policy, fast boot, PXE/HTTP policy stay Boot Maintenance/platform HII. |
| Device / HII formset inventory | Devices / HII; Dashboard quick category. | `ModernUiDeviceDataLib`, HII database, device paths, Driver Health. | List native setup/device entries and open them via `EFI_FORM_BROWSER2_PROTOCOL.SendForm()`. | No IFR parsing, ConfigAccess implementation, form mutation, or varstore writes in App. |
| Security / identity | Security summary and Dashboard status. | `ModernUiSecurityDataLib`, Secure Boot variables, TCG/TCM/TPM protocol probes. | Show Secure Boot, Setup Mode, key presence, and TPM/TCM availability as posture. | Key enrollment, password, physical presence, measured boot policy, chassis policy remain native. |
| Firmware update / recovery | Firmware Update category; Dashboard lifecycle card. | `ModernUiFirmwareDataLib`, capsule runtime/protocol/report probes, platform update entry hints. | Show capsule/update/recovery availability and route to native updater when exposed. | No capsule construction, flash programming, rollback policy, or recovery writes in App. |
| Diagnostics / logs | Diagnostics / Logs category. | `ModernUiDiagnosticsDataLib`, ACPI/SMBIOS/config table/handle/memory-map summaries. | Show table/log/provider health and basic platform evidence for service/debug. | POST log management, error clearing, vendor diagnostics, and repair flows remain native/service app. |
| Management / BMC / remote lifecycle | Management category, especially on server products. | `ModernUiManagementDataLib`, IPMI/Redfish/SMBIOS Type 38/42 probes. | Show BMC/IPMI/Redfish presence and route to native management pages. | BMC networking, users, KVM/media, SEL policy, and remote update configuration remain BMC/native owned. |
| Power management / battery | Power / Thermal category. | `ModernUiPowerDataLib`, ACPI table/protocol state, SMBIOS chassis/power supply records. | Show power/thermal capability and battery/power-supply presence where available. | Wake policy, charge thresholds, adapter behavior, ErP, and power-restore writes remain native. |
| Thermal / fan / acoustics | Power / Thermal category. | `ModernUiPowerDataLib` plus `ModernUiHardwareHealthDataLib` demo provider. | Show thermal state and demo temperature trend UX without claiming platform readings. | Fan curves, pump headers, thermal trip points, acoustic profiles stay native platform HII/FormBrowser-owned. |
| CPU topology / processor inventory | Performance / Tuning category; Dashboard performance card. | `ModernUiPerformanceDataLib`, SMBIOS/ACPI/CPU I/O or platform inventory probes. | Show coarse processor inventory and performance-provider readiness. | Detailed CPU feature policy pages remain native. |
| CPU frequency / voltage / overclocking | Performance / Tuning category as entry hint only. | No writable App provider. Optional future read-only current-state provider only. | Show native tuning entry availability if detected. | Multipliers, BCLK, voltage offsets/overrides, turbo limits, undervolting, P-state/CPPC policy remain native only. |
| Memory inventory / topology | Performance / Tuning category. | `ModernUiPerformanceDataLib`, UEFI memory map, SMBIOS/ACPI when available. | Show memory capacity/topology summary when reliable. | Training, channel mode, detailed DIMM policy remain native. |
| Memory timing / profile / RAS | Performance / Tuning category as entry/status only; Management/Diagnostics may show health. | Read-only memory/RAS entry hints through existing or future provider. | Show memory/RAS availability and health signals, not individual timing controls. | XMP/EXPO, DRAM ratios/timings/voltage, scrub, mirroring, sparing, interleave policy remain native only. |
| Chipset / SoC configuration | Devices / HII; Power / Thermal; Diagnostics depending on surfaced formset. | HII entry enumeration plus platform/device inventory; no generic chipset writer. | Expose native setup entry and summarize device/protocol presence. | PCH/SoC straps, GPIO/I2C/SPI/UART, watchdog, SATA/USB enablement, and board muxes remain native only. |
| Storage / NVMe / RAID | Devices / HII; Boot; Diagnostics. | Device path inventory, future read-only storage health provider if portable. | Show bootable/storage device presence and route to native storage/RAID tools. | RAID/VMD/RST, Opal, sanitize, hot-plug, and storage security operations remain native/vendor utility. |
| PCIe resource / fabric policy | PCIe Policy category; Performance / Tuning cross-link. | `ModernUiPcieDataLib`, PCIe inventory/capability hints. | Show controller/root-bridge/endpoint counts and read-only hints for ReBAR, Above 4G, SR-IOV, ASPM, bifurcation, hot-plug, ACS/ARI, IOMMU. | All resource allocation and policy changes remain native PCIe policy HII/FormBrowser. |
| Graphics / display | Dashboard; Devices / HII. | `ModernUiPlatformDataLib` for GOP mode plus device inventory. | Show current GOP resolution/renderer state and display-device entries. | iGPU/dGPU mux, hybrid graphics, UMA, panel/backlight, and OpROM policy remain native. |
| Network / connectivity | Boot; Devices / HII; Management for server NIC path. | Boot entries, device paths, optional management provider probes. | Show network boot entries, NIC/device presence, management host hints. | PXE/HTTP stack policy, VLAN/iSCSI, Wi-Fi/Bluetooth toggles, MAC policy, WoL settings remain native. |
| RAS / reliability / serviceability | Diagnostics / Logs; Management; Performance / Tuning. | Diagnostics, management, performance, and PCIe providers expose read-only presence/health. | Show RAS/log/provider readiness and route to native RAS pages. | ECC/scrub/poison/AER/NMI behavior, log clearing, and service policy remain native. |
| Virtualization / isolation | Security; Performance / Tuning; PCIe Policy. | Performance/security/PCIe provider hints where protocols or inventory expose capability. | Show virtualization/IOMMU/SR-IOV capability presence and native entry availability. | Enable/disable controls, isolation mode, confidential-computing policy remain native. |
| Server profile / workload tuning | Performance / Tuning; Management on server. | Future read-only profile summary only if platform exposes a stable source. | Show current profile name/status when safe, else route to native page. | Workload profile selection, NUMA/SNC/NPS, C-state/turbo policy remain native only. |
| Embedded / industrial controls | Firmware Update; Devices / HII; Diagnostics. | HII entries, firmware/recovery/security summaries, optional board-specific read-only provider. | Show recovery/update/security posture and native board-control entries. | Watchdog, GPIO defaults, serial policy, provisioning, boot-pin behavior remain platform HII/native. |
| Accelerator / CXL / AI device policy | Devices / HII; Diagnostics; PCIe Policy. | Device inventory, PCIe provider, possible future read-only accelerator health provider. | Show device presence/health/resource-entry hints when discoverable. | CXL modes, persistent memory policy, accelerator enablement, MMIO windows, and vendor telemetry controls remain native. |
| OEM customization / branding | Dashboard category landing, theme/layout layer. | App-private IA constants and theme tokens; no cloned IBV pages. | Provide consistent category labels and OEM-themable visual shell. | Vendor-specific setup workflows, favorites/search/manufacturing pages are future native/allowlisted design items. |

## Form-Factor Feature Matrix

Legend: `Display` means App can show a read-only status directly; `Entry` means
App should expose a native FormBrowser/HII entry; `Native` means the feature is
owned by platform HII and should not be implemented in the App.

| Capability | Desktop / workstation | Laptop / 2-in-1 | AIO / NUC / mini PC | Server | Embedded / tablet |
| --- | --- | --- | --- | --- | --- |
| Firmware and platform summary | Display | Display | Display | Display | Display |
| Boot inventory and launch | Display | Display | Display | Display | Display |
| Boot order editing | Entry | Entry | Entry | Entry | Entry |
| Device and HII entries | Display + Entry | Display + Entry | Display + Entry | Display + Entry | Display + Entry |
| Secure Boot and TPM posture | Display + Entry | Display + Entry | Display + Entry | Display + Entry | Display + Entry |
| Key management and TPM physical presence | Native | Native | Native | Native | Native |
| Capsule/update/recovery | Display + Entry | Display + Entry | Display + Entry | Display + Entry | Display + Entry |
| Diagnostics/log summary | Display + Entry | Display + Entry | Display + Entry | Display + Entry | Display + Entry |
| BMC/IPMI/Redfish management | N/A | N/A | N/A | Display + Entry | Platform-dependent |
| Power/thermal status | Display + Entry | Display + Entry | Display + Entry | Display + Entry | Display + Entry |
| Battery and adapter policy | N/A | Native | N/A | N/A | Platform-dependent |
| Performance/tuning policy | Entry | Entry | Entry | Entry | Platform-dependent |
| RAS/NUMA/PCIe policy | Platform-dependent | N/A | Platform-dependent | Native | Platform-dependent |
| PCIe capability summary and native policy entry hints | Display + Entry | Platform-dependent | Display + Entry | Display + Entry | Platform-dependent |

## XArch Product Target Capability Matrix

The rows below keep the concrete architecture family names because product teams,
edk2 build scripts, and platform packages still use them. The table is the XArch
view of common App/provider behavior, not a request to hide `ARCH=X64`,
`ARCH=AARCH64`, `ARCH=RISCV64`, or `ARCH=LOONGARCH64` behind a new build name.

| Capability | x86 / X64 | ARM / AARCH64 | RISC-V / RISCV64 | LoongArch / LOONGARCH64 | App policy |
| --- | --- | --- | --- | --- | --- |
| Architecture string | Yes | Yes | Yes | Yes | Show from build/runtime architecture. |
| Firmware vendor/revision | Yes | Yes | Yes | Yes | Show from `gST->FirmwareVendor` and revision. |
| Memory summary | Yes | Yes | Yes | Yes | Show total usable memory from UEFI memory map. |
| GOP display mode | Yes | Yes | Yes | Yes | Show current resolution and renderer state. |
| Boot#### inventory | Yes | Yes | Yes | Yes | Enumerate through UEFI variables / Boot Manager library. |
| HII formset entries | Yes | Yes | Yes | Yes | Enumerate HII handles and open with FormBrowser2. |
| Secure Boot state | Yes | Yes | Yes | Yes when implemented | Read standard UEFI variables only. |
| TPM / TCG / TCM | Common on PC/server | Platform-dependent | Platform-dependent | Platform-dependent | Detect protocol/presence; display `N/A` when absent. |
| SMBIOS summary | Common | Common on server | Optional | Optional | Basic provider support; display `N/A` when absent. |
| ACPI / device tree | ACPI common | ACPI or DT | ACPI or DT | ACPI or DT | Basic ACPI presence summary; device-tree detail remains future work. |
| PCI / USB / NVMe inventory | Common | Platform-dependent | Platform-dependent | Platform-dependent | Use handle/device-path inventory; do not hard-code buses. |
| BMC / IPMI / Redfish | Server common | Server common | Optional | Server/product dependent | Basic provider support; hide or `N/A` on client platforms. |
| Capsule update | Common | Platform-dependent | Platform-dependent | Platform-dependent | Detect capsule/update support; hand off to native page/app. |
| RAS / NUMA / PCIe policy | Server/workstation | Server | Emerging | Server/product dependent | Never implement policy in App; open owning HII formset. |
| PCIe inventory and policy-entry hints | Common | Platform-dependent | Emerging | Server/product dependent | Show read-only capability summary from `ModernUiPcieDataLib`; actual PCIe policy changes remain native HII/FormBrowser-owned. |

## Provider Roadmap

| Provider | Responsibility | Minimum v1 behavior | Failure behavior |
| --- | --- | --- | --- |
| `ModernUiPlatformDataLib` | Firmware, architecture, memory, display, platform name. | Fill dashboard strings and memory summary. | Show `Unknown` or `N/A`; never ASSERT. |
| `ModernUiBootDataLib` | Boot option enumeration and launch. | Show `Boot####` active/hidden/category/path summary; launch selected option. | Show empty state or returned `EFI_STATUS`. |
| `ModernUiDeviceDataLib` | HII formset/device entry discovery. | List HII entries and open forms through FormBrowser2. | Keep row read-only and show `EFI_STATUS`. |
| `ModernUiSecurityDataLib` | Secure Boot, key database, and TCG protocol state. | Read standard variables and protocol presence as read-only. | Show `Unknown`; no writes. |
| `ModernUiFirmwareDataLib` | Capsule/update/recovery status. | Detect capsule runtime services, capsule architectural protocol, and capsule report presence. | Show `N/A`; open native update path when present. |
| `ModernUiDiagnosticsDataLib` | POST/log/platform health summary. | Show ACPI/SMBIOS presence, memory map count, handle count, and configuration table count. | Show `N/A`; no persistent changes. |
| `ModernUiManagementDataLib` | BMC/IPMI/Redfish/server management summary. | Detect IPMI protocol, Redfish discover protocol, and SMBIOS management host interface. | Hide on non-server platforms or show `N/A`. |
| `ModernUiPowerDataLib` | Power and thermal capability summary. | Detect ACPI table/protocol state, SMBIOS chassis thermal state, and power supply record presence. | Show `N/A`; no persistent changes. |
| `ModernUiHardwareHealthDataLib` | Read-only demo Hardware Health summary. | Supplies deterministic demo temperature sensors and sample history for the Power / Thermal trend-sparkline UI. | Demo provider only; real thermal policy, fan controls, trip points, and platform ownership remain native HII/FormBrowser-owned. |
| `ModernUiPerformanceDataLib` | Performance/tuning capability summary. | Detect CPU/memory inventory, CPU I/O protocol, virtualization policy entry availability, and RAS entry availability. | Show `N/A`; no persistent changes. |
| `ModernUiPcieDataLib` | PCIe capability summary and native policy entry hints. | Detect controller/root-bridge/endpoint inventory, PCIe protocol presence, and read-only hints for ReBAR, Above 4G, SR-IOV, ASPM, bifurcation, hot-plug, ACS/ARI, and IOMMU policy entries. | Show `N/A`; actual PCIe policy changes remain platform HII/FormBrowser-owned. |

## Completion Criteria

- App pages are architecture-neutral and driven by providers, not hard-coded
  board assumptions.
- Every real setup/configuration action either launches a boot option or enters
  native FormBrowser/HII ownership.
- Missing platform features are represented as `N/A`, `Unknown`, hidden rows, or
  read-only entry points; they do not crash or create fake settings.
- The same App build can run on ArmVirt and LoongArchVirt, with x86/OVMF and
  RISC-V planned as future platform overlays.
- DisplayEngine compatibility checks continue separately through
  `Docs/CompatibilityMatrix.md`.
