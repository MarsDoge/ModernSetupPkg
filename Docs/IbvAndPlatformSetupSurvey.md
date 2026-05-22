<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# IBV and Platform Setup Survey

Language: English | [简体中文](IbvAndPlatformSetupSurvey.zh-CN.md)

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
| Boot | Boot#### number, active/hidden state, category, description, and device path summary. Entries are launchable in ModernSetupApp; Enter attempts the selected Boot#### through UefiBootManagerLib. | Direct Boot#### launch, boot order editing, and boot policy. | Native Boot Manager, Boot Maintenance Manager, and platform boot HII. |
| Devices | HII formsets, device path inventory, capability providers. | Driver/device setup pages. | Device Manager, Driver Health, and each driver formset. |
| Security | Secure Boot, Setup Mode, PK/KEK/db/dbx presence, TPM/TCG/TCM protocol presence. | Key management, TPM physical presence, measured boot policy. | SecurityPkg/platform security HII. |
| Firmware Update | Capsule runtime support, capsule protocol, capsule report presence, firmware revision. | Capsule/update/recovery application. | Capsule/update HII or platform update application. |
| Diagnostics / Logs | ACPI/SMBIOS presence, memory map count, handle count, configuration table count. | Event logs, POST logs, hardware diagnostics. | Platform diagnostics/log HII or service app. |
| Management | IPMI, Redfish Discover, SMBIOS Type 38/42 presence. | BMC/IPMI/Redfish configuration. | BMC, Redfish, or server management HII. |
| Power / Thermal | ACPI table/protocol presence, SMBIOS chassis thermal state, power supply record presence. | Fan curves, acoustic profile, battery behavior, power policy. | Platform power/thermal HII. |
| Performance / Tuning | CPU/memory inventory presence, CPU I/O protocol presence, virtualization/RAS policy entry availability. | Overclocking, CPU policy, memory timing, NUMA/RAS, PCIe policy. | Platform performance/tuning HII. |
| PCIe Policy | PCIe controller/root-bridge/endpoint inventory, protocol presence, and capability hints for ReBAR, Above 4G, SR-IOV, ASPM, bifurcation, hot-plug, ACS/ARI, and IOMMU. | Native PCIe policy formset entry. | Platform PCIe policy HII through FormBrowser; actual policy changes remain native owned. |
| Exit | Continue, reset, language, native UiApp fallback. | Save/discard where the native browser owns state. | Native FormBrowser save/discard/default handling. |

## Broad IBV / Platform Setup Taxonomy

This taxonomy covers common setup page families across IBVs, OEMs, ODMs,
servers, workstations, desktops, notebooks, and embedded products. Its scope is
intentionally broader than the current App implementation. The boundary remains
strict: `ModernSetupApp` only provides read-only summaries and entry points into
native HII/FormBrowser pages. CPU frequency/voltage, memory timing/profile,
Chipset/SoC controls, fan curves, PCIe resource policy, and similar board policy
remain native HII/FormBrowser-owned. Any writable policy requires a separate
architecture/design review and is not authorized by this taxonomy.

Priority: `P0` means a common App summary or entry need, `P1` means valuable
productization coverage, and `P2` means more platform-specific follow-up scope.

| Domain / subsystem | Example options | Product fit | Architecture notes | ModernSetup handling | Priority |
| --- | --- | --- | --- | --- | --- |
| System overview / Main | Firmware vendor/version, build date, board SKU, serial, UUID, language, date/time, administrator/user password status. | Server, workstation, desktop, notebook, embedded. | Architecture-neutral; SMBIOS is common on x86/Arm servers and optional on RISC-V/LoongArch; RTC and language are standard UEFI surfaces. | Show read-only identity/status; provide native date/time, language, or password entry points when present. | P0 |
| Boot manager / boot policy | Boot order, one-time boot, UEFI/legacy mode when supported, PXE, HTTP boot, USB boot, network stack, boot timeout, fast boot. | All product classes; network boot is especially common on servers, workstations, and embedded products. | UEFI Boot#### is architecture-neutral; CSM/legacy is mainly an x86 client legacy item; HTTP/PXE depends on the NIC stack. | Show the Boot#### list and launch the selected entry through UefiBootManagerLib; editing and policy remain Boot Maintenance/platform HII-owned. | P0 |
| Security / identity | Secure Boot, Setup Mode, key databases, TPM/PTT/fTPM/TCM, measured boot, physical presence, chassis intrusion, passwords. | All product classes; most common on servers, workstations, and notebooks. | Secure Boot variables are UEFI-standard; TPM/TCG is common on x86/Arm and optional on RISC-V/LoongArch; China-market platforms may expose TCM. | Show security posture and protocol/key presence; key management, passwords, physical-presence flows, and measured-boot policy stay native. | P0 |
| Firmware update / recovery | Capsule support, BIOS flash utility, recovery capsule, rollback prevention, dual-bank image, update log. | All product classes; embedded and server products emphasize recovery and lifecycle. | UEFI capsule interfaces are architecture-neutral; recovery media and flash layout are board-specific. | Show capability/status and open native update/recovery app or HII; the App does not perform flash writes. | P0 |
| Device / HII formset inventory | Device Manager, Driver Health, storage controller pages, NIC pages, USB controller pages, option ROM pages. | All product classes. | HII database and device paths keep this architecture-neutral; bus availability depends on platform. | Enumerate entries and open owning formsets through FormBrowser2; the App does not parse IFR or implement ConfigAccess. | P0 |
| CPU topology / processor inventory | Socket/core/thread count, microcode/firmware revision, cache, feature flags, current frequency, efficiency/performance core hints. | Server, workstation, desktop, notebook; embedded when CPU information is exposed. | x86 commonly uses CPUID/SMBIOS; Arm uses MPIDR/SMBIOS/ACPI PPTT; RISC-V and LoongArch are often platform-specific. | Show coarse inventory when provider data is available; detailed CPU policy pages stay native. | P0 |
| CPU frequency / voltage / overclocking | Ratio/multiplier, BCLK, voltage override/offset, P-state/CPPC policy, turbo limits, AVX offset, undervolt guard. | Enthusiast desktop, workstation, some notebooks; servers are usually locked or profile-based. | Strongly CPU-vendor and board-power dependent; most common on x86, while Arm/RISC-V/LoongArch usually expose SoC firmware or PMIC-specific controls. | Native only. The App may hint that a performance/tuning entry exists, but must never expose writable frequency/voltage controls. | P1 |
| Memory inventory / topology | DIMM slots, capacity, speed, ECC, rank, channel, interleave, training state, SPD summary. | Server, workstation, desktop, notebook; embedded varies widely. | SMBIOS/ACPI are common on x86/Arm servers; RISC-V/LoongArch may expose less platform data; soldered LPDDR often has limited slot information. | Show summary/topology when reliable; training and detailed memory setup stay native. | P0 |
| Memory timing / profile / RAS | XMP/EXPO/JEDEC profile, DRAM frequency, primary/secondary timings, voltage, ECC scrub, patrol scrub, spare/rank sparing, mirroring, interleave. | Server/workstation for RAS; enthusiast desktop for timing/profile; notebooks usually limited; embedded depends on platform. | Most common on x86 desktop/server; Arm servers expose RAS/NUMA controls; RISC-V/LoongArch depend on memory-controller firmware exposure. | Timing/profile/RAS policy is native only; the App may show read-only memory information and RAS entry availability. | P1 |
| Chipset / SoC configuration | PCH/SoC straps, SATA mode, USB policy, integrated audio/camera/wireless, GPIO, I2C/SPI/UART, eMMC/SD, watchdog, serial console. | Desktop, notebook, embedded, workstation; server often exposes serial/watchdog/SoC I/O. | x86 PCH terminology differs from Arm/RISC-V/LoongArch SoC fabric; embedded relies heavily on device tree, ACPI, or board straps. | Enablement/policy is native only; the App may summarize device presence and provide HII entries. | P1 |
| Storage / NVMe / RAID | SATA mode, NVMe info, VMD/RAID, RST, Opal/security, sanitize, hot-plug, bootable storage policy. | Server, workstation, desktop, notebook, embedded. | NVMe/SCSI/ATA are architecture-neutral; Intel VMD/RST is x86-oriented; SoC storage policy varies on Arm/RISC-V/LoongArch. | Show inventory and health hints when safe; RAID/VMD/Opal/sanitize policy stays native or vendor-utility owned. | P1 |
| PCIe resource / fabric policy | Above 4G decoding, ReBAR, SR-IOV, ASPM, bifurcation, link speed, hot-plug, ACS/ARI, IOMMU, option ROM, MMIO aperture. | Server, workstation, desktop; embedded and notebook depend on platform. | PCIe spans x86/Arm and some RISC-V/LoongArch platforms; root-complex topology and IOMMU naming vary by architecture. | Show read-only capabilities and native policy entry hints; actual resource allocation and policy mutation remain native HII/FormBrowser-owned. | P0 |
| Graphics / display | Primary display, iGPU/dGPU selection, hybrid graphics, GOP/OpROM, UMA frame buffer, panel/backlight, multi-monitor boot display. | Desktop, workstation, notebook, AIO/NUC, display-capable embedded products. | GOP is UEFI-standard; mux/UMA/backlight are board or SoC-specific; dGPU options are common on x86/workstation but not exclusive. | Show GOP mode/current renderer; graphics mux, UMA, and panel policy stay native. | P1 |
| Network / connectivity | Onboard LAN, PXE/HTTP boot, MAC display, Wi-Fi/Bluetooth toggles, WoL, VLAN/iSCSI, management NIC selection. | All product classes; servers and embedded emphasize remote/network boot. | UEFI network protocols are architecture-neutral; wireless toggles are often notebook/OEM-specific; management NICs are common on servers. | Show device/protocol presence and boot entries; network stack, VLAN/iSCSI, and wireless toggles stay native. | P1 |
| Power management / battery | ACPI S-states, ErP, wake-on-LAN/USB/RTC, lid behavior, battery charge thresholds, adapter warnings, power restore policy. | Notebook for battery; desktop/workstation/AIO for wake/ErP; server for restore policy; embedded for power-loss recovery. | ACPI is common on x86/Arm servers and may exist on RISC-V/LoongArch; device-tree platforms may expose fewer standard setup controls. | Show power capability/status; battery, wake, and power-restore policy stay native. | P1 |
| Thermal / fan / acoustics | Fan curves, pump header, thermal trip points, acoustic/performance mode, chassis thermal sensors, dust/fan diagnostics. | Desktop, workstation, server, notebook, actively cooled embedded. | Sensors and fan control depend on EC/BMC/Super I/O/PMIC; Arm/RISC-V/LoongArch embedded products vary widely. | Show thermal/fan presence and status only; fan curves and trip points stay native HII, EC/BMC, or vendor-tool owned. | P1 |
| Platform management / BMC | IPMI, Redfish, BMC network, KVM/media, SEL, host interface, watchdog, power restore, remote update. | Mainly server; some workstation/embedded appliances. | Common on x86/Arm servers; optional on RISC-V/LoongArch server products; exposure may be IPMI, Redfish, or SMBIOS Type 38/42. | Show management capability/status and open native BMC/Redfish pages; the App does not configure BMC networking. | P0 |
| RAS / reliability / serviceability | ECC mode, memory patrol scrub, MCA/SEA handling, poison, PCIe AER, SMI/NMI behavior, error logs, spare devices. | Server and workstation; high-reliability embedded. | x86 server RAS is mature; Arm server uses ACPI/APEI/SEA; RISC-V/LoongArch support depends on product maturity. | Show health/log availability and RAS entry hints; policy and log clearing stay native. | P1 |
| Virtualization / isolation | VT-x/AMD-V, SVM, VT-d/IOMMU/SMMU, SR-IOV, ATS/PRI/PASID, CXL isolation, confidential computing capabilities. | Server, workstation, desktop; notebooks commonly expose baseline virtualization; embedded depends on platform. | x86 names differ from Arm SMMU/Realm/CCA and RISC-V/LoongArch IOMMU extensions; availability is platform-dependent. | Show read-only capability presence when providers can detect it; switches, isolation mode, and confidential-computing policy remain native. | P1 |
| Server profile / workload tuning | Balanced/performance/power saver, deterministic performance, NUMA, SNC/NPS, C-states, turbo policy, memory interleave, accelerator profile. | Server, workstation/HPC. | x86 and Arm server knobs differ by vendor; RISC-V/LoongArch server profiles are still evolving and product-specific. | Profile selection is native only; the App may summarize the current profile if a stable read-only source exists. | P1 |
| Embedded / industrial controls | Watchdog, serial console redirection, GPIO defaults, boot source pins, recovery mode, secure provisioning, field-update channel. | Embedded, industrial, appliance, tablet-like products. | Strongly board-specific across Arm/RISC-V/LoongArch/x86, often tied to device tree, ACPI tables, EC, or manufacturing variables. | Show safe recovery/update/security state and provide native entries; board controls stay native. | P2 |
| Accelerator / CXL / AI device policy | CXL memory mode, persistent memory, accelerator enablement, firmware level, MMIO windows, RAS, telemetry. | Server, workstation, some edge-AI embedded. | CXL is currently mostly x86/Arm server; RISC-V/LoongArch depend on platform ecosystem; accelerator controls are usually vendor-specific. | Show inventory/health when discoverable; mode/resource mutation stays native. | P2 |
| OEM customization / branding | EZ/Advanced mode, favorites, search, language packs, help text, screenshots, custom update tools, manufacturing pages. | All OEM products. | UI layer is architecture-neutral; content still maps to product-specific HII/formsets. | Theme/navigation reference only; the App may provide standard categories but must not copy vendor policy pages. | P2 |

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
