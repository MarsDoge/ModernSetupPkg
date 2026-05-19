<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Productization Feature Matrix

ModernSetup has two productization layers:

```text
Standard front-page App
  -> common dashboard, inventory, boot, security, diagnostics, and entry points

Native edk2 FormBrowser path
  -> platform/OEM HII pages, callbacks, validation, varstores, and policy
```

The App should provide a consistent first screen across x86, ARM, RISC-V, and
LoongArch products. It must not clone platform setup policy or parse IFR. When a
setting is platform-specific, the App should show a summary or entry point, then
open the owning HII form through `EFI_FORM_BROWSER2_PROTOCOL.SendForm()`.

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
| Power / Thermal | Power and cooling visibility. | ACPI table/protocol state, chassis thermal state, power supply record presence. | Platform fan, battery, thermal, and power-policy HII. | Basic read-only implemented. |
| Performance / Tuning | CPU/memory and tuning entry visibility. | Processor inventory, memory inventory, CPU I/O protocol, virtualization/RAS policy entry availability. | Platform performance, overclocking, NUMA/RAS, PCIe policy HII. | Basic read-only implemented. |
| PCIe Policy | PCIe inventory and policy-entry visibility. | Controller/root-bridge/endpoint counts, protocol presence, and read-only capability hints for ReBAR, Above 4G, SR-IOV, ASPM, bifurcation, hot-plug, ACS/ARI, and IOMMU. | Platform PCIe policy HII and native FormBrowser pages. | Basic read-only foundation implemented. |
| Exit | Session and shell control. | Continue, reset, native UiApp, language, theme, app/version info. | Native FormBrowser save/discard where needed. | Basic implemented. |

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

## Cross-Architecture Capability Targets

| Capability | x86 | ARM | RISC-V | LoongArch | App policy |
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
