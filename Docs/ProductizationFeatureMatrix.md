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
| Dashboard | First-glance platform state. | Firmware vendor/revision, architecture, platform name, memory, display mode, boot count, Secure Boot state, HII/device count. | App data providers. | Basic implemented. |
| Boot | Boot inventory and launch entry. | `BootOrder`, visible `Boot####`, active/hidden state, launch selected option. | Boot Maintenance HII pages for editing and advanced policy. | Basic implemented. |
| Devices / HII | Entry point to platform setup pages and device inventory. | HII formsets, driver/device path rows, Driver Health entry, inventory rows. | Each driver formset via FormBrowser2. | Basic implemented. |
| Security | Read-only security posture. | Secure Boot, Setup Mode, PK/KEK/db/dbx state, TPM/TCG/TCM presence when available. | Security HII pages and platform policy drivers. | Secure Boot implemented; TPM pending. |
| Firmware Update | Firmware lifecycle entry point. | Capsule support, firmware version, recovery/update entry, last update state when available. | Capsule/update HII or platform update app. | Planned. |
| Diagnostics / Logs | Bring-up and service visibility. | POST/log summary, error count, ACPI/SMBIOS presence, memory map summary, test hooks. | Platform diagnostics HII or service app. | Planned. |
| Management | Server/remote management summary. | BMC/IPMI/Redfish presence, management NIC, host interface, remote update support. | BMC/IPMI/Redfish platform drivers. | Planned. |
| Exit | Session and shell control. | Continue, reset, native UiApp, language, theme, app/version info. | Native FormBrowser save/discard where needed. | Basic implemented. |

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
| SMBIOS summary | Common | Common on server | Optional | Optional | Planned provider; display `N/A` when absent. |
| ACPI / device tree | ACPI common | ACPI or DT | ACPI or DT | ACPI or DT | Planned provider; summarize presence only. |
| PCI / USB / NVMe inventory | Common | Platform-dependent | Platform-dependent | Platform-dependent | Use handle/device-path inventory; do not hard-code buses. |
| BMC / IPMI / Redfish | Server common | Server common | Optional | Server/product dependent | Planned provider; hide or `N/A` on client platforms. |
| Capsule update | Common | Platform-dependent | Platform-dependent | Platform-dependent | Detect capsule/update support; hand off to native page/app. |
| RAS / NUMA / PCIe policy | Server/workstation | Server | Emerging | Server/product dependent | Never implement policy in App; open owning HII formset. |

## Provider Roadmap

| Provider | Responsibility | Minimum v1 behavior | Failure behavior |
| --- | --- | --- | --- |
| `ModernUiPlatformDataLib` | Firmware, architecture, memory, display, platform name. | Fill dashboard strings and memory summary. | Show `Unknown` or `N/A`; never ASSERT. |
| `ModernUiBootDataLib` | Boot option enumeration and launch. | Show visible `Boot####`; launch selected option. | Show empty state or returned `EFI_STATUS`. |
| `ModernUiDeviceDataLib` | HII formset/device entry discovery. | List HII entries and open forms through FormBrowser2. | Keep row read-only and show `EFI_STATUS`. |
| `ModernUiSecurityDataLib` | Secure Boot and key database state. | Read standard variables as read-only. | Show `Unknown`; no writes. |
| `ModernUiFirmwareDataLib` | Capsule/update/recovery status. | Detect capsule/update capability and firmware version metadata. | Show `N/A`; open native update path when present. |
| `ModernUiDiagnosticsDataLib` | POST/log/platform health summary. | Count available log/diagnostic providers and expose entry points. | Show `N/A`; no persistent changes. |
| `ModernUiManagementDataLib` | BMC/IPMI/Redfish/server management summary. | Detect management protocols and show presence/state. | Hide on non-server platforms or show `N/A`. |

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
