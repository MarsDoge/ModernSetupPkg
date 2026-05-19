<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# IBV and Platform Setup Survey

This document records the public reference baseline used to shape the
ModernSetup standard front-page App. The App should expose common platform
state and safe entry points. Platform policy, VFR/IFR parsing, callback flow,
and variable writes remain owned by native edk2 FormBrowser and each platform
HII driver.

## Independent UEFI Firmware Vendors

| Vendor | Firmware family | Typical coverage | ModernSetup use |
| --- | --- | --- | --- |
| AMI / American Megatrends | Aptio | Client, workstation, embedded, server, OEM/ODM boards. | Visual and information architecture reference only. |
| Insyde Software | InsydeH2O | Notebook, tablet, embedded, client, server platforms. | Visual and information architecture reference only. |
| Phoenix Technologies | SecureCore, OmniCore, ServerCore | Client, embedded, server-oriented firmware products. | Visual and information architecture reference only. |
| Nanjing Byosoft | ByoCore | China-market client, server, embedded, and domestic architecture products. | Visual and information architecture reference only. |
| TianoCore / edk2 | edk2 | Open source UEFI implementation and reference codebase. | Compatibility target and implementation base. |

OEM, ODM, and motherboard vendors such as ASUS, Gigabyte, MSI, ASRock, Dell,
HP, Lenovo, HPE, Supermicro, and Intel NUC vendors are treated as product UI
and workflow references. They are not counted here as core independent BIOS
vendors, because their setup surfaces commonly sit above an IBV or edk2-derived
firmware stack.

## Platform Form Factors

| Form factor | Common setup information | Common configuration entries | ModernSetup App policy |
| --- | --- | --- | --- |
| Desktop / workstation | Firmware version, CPU, memory, storage, PCIe, graphics, fans, Secure Boot, boot entries. | Boot order, Secure Boot, TPM, virtualization, PCIe policy, fan/power profile, firmware update. | Show dashboard, boot, devices, security, firmware, diagnostics, performance, and power summary. Open policy pages through FormBrowser. |
| Laptop / 2-in-1 | Firmware version, battery/adapter, display, touch/input, storage, wireless, TPM, Secure Boot. | Boot order, Secure Boot, TPM, virtualization, battery/power behavior, wake policy, camera/wireless toggles. | Show power/thermal and security posture. Device-specific toggles stay in HII pages. |
| All-in-one / NUC / mini PC | Firmware version, CPU, memory, storage, display, network, fan/thermal, boot entries. | Boot order, PXE, Secure Boot, TPM, thermal/acoustic profile, wake-on-LAN. | Show compact dashboard plus boot, network/device, security, firmware update, and thermal provider state. |
| Server | Firmware, CPU topology, memory topology, PCIe/NVMe, BMC/IPMI/Redfish, RAS, TPM, Secure Boot. | Boot policy, UEFI network boot, RAS, NUMA, PCIe bifurcation, SR-IOV, TPM, BMC/Redfish, system profile. | Show inventory and management capability summaries plus read-only PCIe policy entry hints. RAS, BMC, PCIe, and security policy stay in HII/provider pages. |
| Embedded / industrial / tablet appliance | Firmware version, boot source, recovery state, display/input, storage, network, secure state. | Boot source, recovery, firmware update, secure state, watchdog, serial/console, device enablement. | Show minimal safe state and recovery/update entry points. Board policy stays in platform HII. |

## Common Setup Surfaces

| Surface | Display directly in App | App entry point only | Native FormBrowser owner |
| --- | --- | --- | --- |
| System / Dashboard | Firmware vendor/revision, architecture, form factor, boot mode, memory, display mode, Secure Boot, provider availability. | Detailed platform inventory. | Platform inventory HII or SMBIOS/ACPI-specific pages. |
| Boot | Boot#### number, active/hidden state, category, description, device path summary, launch selected option. | Boot order editing and boot policy. | Boot Maintenance Manager and platform boot HII. |
| Devices | HII formsets, device path inventory, capability providers. | Driver/device setup pages. | Device Manager, Driver Health, and each driver formset. |
| Security | Secure Boot, Setup Mode, PK/KEK/db/dbx presence, TPM/TCG/TCM protocol presence. | Key management, TPM physical presence, measured boot policy. | SecurityPkg/platform security HII. |
| Firmware Update | Capsule runtime support, capsule protocol, capsule report presence, firmware revision. | Capsule/update/recovery application. | Capsule/update HII or platform update application. |
| Diagnostics / Logs | ACPI/SMBIOS presence, memory map count, handle count, configuration table count. | Event logs, POST logs, hardware diagnostics. | Platform diagnostics/log HII or service app. |
| Management | IPMI, Redfish Discover, SMBIOS Type 38/42 presence. | BMC/IPMI/Redfish configuration. | BMC, Redfish, or server management HII. |
| Power / Thermal | ACPI table/protocol presence, SMBIOS chassis thermal state, power supply record presence. | Fan curves, acoustic profile, battery behavior, power policy. | Platform power/thermal HII. |
| Performance / Tuning | CPU/memory inventory presence, CPU I/O protocol presence, virtualization/RAS policy entry availability. | Overclocking, CPU policy, memory timing, NUMA/RAS, PCIe policy. | Platform performance/tuning HII. |
| PCIe Policy | PCIe controller/root-bridge/endpoint inventory, protocol presence, and capability hints for ReBAR, Above 4G, SR-IOV, ASPM, bifurcation, hot-plug, ACS/ARI, and IOMMU. | Native PCIe policy formset entry. | Platform PCIe policy HII through FormBrowser; actual policy changes remain native owned. |
| Exit | Continue, reset, language, native UiApp fallback. | Save/discard where the native browser owns state. | Native FormBrowser save/discard/default handling. |

## Reference Links

- UEFI Forum members: https://uefi.org/members
- UEFI firmware ecosystem overview: https://uefi.org/blog/understanding-uefi-and-firmware-ecosystem
- AMI Aptio: https://www.ami.com/aptio/
- InsydeH2O: https://www.insyde.com/products/insydeh2o/
- Phoenix SecureCore: https://phoenixtech.com/phoenix-securecore/
- HPE ProLiant UEFI/RBSU: https://support.hpe.com/hpesc/public/docDisplay?docId=a00112581en_usen_us&page=GUID-D7147C7F-2016-0901-0A72-000000000511.html
- Lenovo ThinkSystem UEFI Setup: https://pubs.lenovo.com/lxpm-v4/UEFI_setup
- Dell PowerEdge BIOS/UEFI Reference: https://www.dell.com/support/manuals/en-us/poweredge-r550/per550_bios_ism_pub/system-security?guid=guid-24e63dd7-d56c-4146-a871-217d22f1faab&lang=en-us
- ASUS UEFI BIOS EZ Mode: https://www.asus.com/global/support/faq/1044236/
- HP Notebook BIOS menu options: https://support.hp.com/us-en/document/ish_3900499-3190557-16/1000
- Intel NUC BIOS boot options: https://www.intel.com/content/www/us/en/support/articles/000054990/intel-nuc/intel-nuc-kits.html
