<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# IBV 与平台 Setup 调研

语言：[English](IbvAndPlatformSetupSurvey.md) | 简体中文

本文记录 ModernSetup 标准首页 App 的公开参考基线。App 应展示通用平台状态和安全入口；平台策略、VFR/IFR 解析、回调流程和变量写入仍由原生 edk2 FormBrowser 与各平台 HII 驱动拥有。

## 独立 UEFI 固件厂商

| 厂商 | 固件系列 | 常见覆盖范围 | ModernSetup 用途 |
| --- | --- | --- | --- |
| AMI / American Megatrends | Aptio | 客户端、工作站、嵌入式、服务器、OEM/ODM 主板。 | 仅作为视觉和信息架构参考。 |
| Insyde Software | InsydeH2O | 笔记本、平板、嵌入式、客户端、服务器平台。 | 仅作为视觉和信息架构参考。 |
| Phoenix Technologies | SecureCore、OmniCore、ServerCore | 客户端、嵌入式、服务器固件产品。 | 仅作为视觉和信息架构参考。 |
| 南京百敖 / Byosoft | ByoCore | 中国市场客户端、服务器、嵌入式和国产架构产品。 | 仅作为视觉和信息架构参考。 |
| TianoCore / edk2 | edk2 | 开源 UEFI 实现和参考代码库。 | 兼容性目标和实现基础。 |

ASUS、Gigabyte、MSI、ASRock、Dell、HP、Lenovo、HPE、Supermicro、Intel NUC 等 OEM、ODM 和主板厂商被视为产品 UI 与工作流参考，而不是核心独立 BIOS 厂商；它们的 Setup 表面通常位于 IBV 或 edk2 派生固件栈之上。

## 平台形态

| 形态 | 常见 Setup 信息 | 常见配置入口 | ModernSetup App 策略 |
| --- | --- | --- | --- |
| Desktop / workstation | 固件版本、CPU、内存、存储、PCIe、图形、风扇、Secure Boot、boot entries。 | Boot order、Secure Boot、TPM、virtualization、PCIe policy、fan/power profile、firmware update。 | 展示 dashboard、boot、devices、security、firmware、diagnostics、performance、power 摘要；策略页通过 FormBrowser 打开。 |
| Laptop / 2-in-1 | 固件版本、电池/适配器、显示、触控/输入、存储、无线、TPM、Secure Boot。 | Boot order、Secure Boot、TPM、virtualization、电池/电源行为、wake policy、camera/wireless toggles。 | 展示 power/thermal 和 security posture；设备特定开关留在 HII 页面。 |
| All-in-one / NUC / mini PC | 固件版本、CPU、内存、存储、显示、网络、风扇/散热、boot entries。 | Boot order、PXE、Secure Boot、TPM、thermal/acoustic profile、wake-on-LAN。 | 展示紧凑 dashboard，以及 boot、network/device、security、firmware update、thermal provider 状态。 |
| Server | 固件、CPU 拓扑、内存拓扑、PCIe/NVMe、BMC/IPMI/Redfish、RAS、TPM、Secure Boot。 | Boot policy、UEFI network boot、RAS、NUMA、PCIe bifurcation、SR-IOV、TPM、BMC/Redfish、system profile。 | 展示 inventory 和 management 能力摘要，以及只读 PCIe policy entry hints；RAS、BMC、PCIe、安全策略留在 HII/provider 页面。 |
| Embedded / industrial / tablet appliance | 固件版本、boot source、recovery state、display/input、storage、network、secure state。 | Boot source、recovery、firmware update、secure state、watchdog、serial/console、device enablement。 | 展示最小安全状态和 recovery/update 入口；板级策略留在平台 HII。 |

## 常见 Setup 表面

| 表面 | App 直接显示 | App 仅提供入口 | 原生 FormBrowser 归属 |
| --- | --- | --- | --- |
| System / Dashboard | Firmware vendor/revision、architecture、form factor、boot mode、memory、display mode、Secure Boot、provider availability。 | 详细平台 inventory。 | Platform inventory HII 或 SMBIOS/ACPI-specific pages。 |
| Boot | Boot#### number、active/hidden state、category、description、device path summary。ModernSetupApp 中条目可启动；回车通过 UefiBootManagerLib 尝试启动所选 Boot####。 | 直接启动 Boot####、Boot order editing 和 boot policy。 | 原生 Boot Manager、Boot Maintenance Manager 和 platform boot HII。 |
| Devices | HII formsets、device path inventory、capability providers。 | Driver/device setup pages。 | Device Manager、Driver Health 和各 driver formset。 |
| Security | Secure Boot、Setup Mode、PK/KEK/db/dbx presence、TPM/TCG/TCM protocol presence。 | Key management、TPM physical presence、measured boot policy。 | SecurityPkg/platform security HII。 |
| Firmware Update | Capsule runtime support、capsule protocol、capsule report presence、firmware revision。 | Capsule/update/recovery application。 | Capsule/update HII 或 platform update application。 |
| Diagnostics / Logs | ACPI/SMBIOS presence、memory map count、handle count、configuration table count。 | Event logs、POST logs、hardware diagnostics。 | Platform diagnostics/log HII 或 service app。 |
| Management | IPMI、Redfish Discover、SMBIOS Type 38/42 presence。 | BMC/IPMI/Redfish configuration。 | BMC、Redfish 或 server management HII。 |
| Power / Thermal | ACPI table/protocol presence、SMBIOS chassis thermal state、power supply record presence。 | Fan curves、acoustic profile、battery behavior、power policy。 | Platform power/thermal HII。 |
| Performance / Tuning | CPU/memory inventory presence、CPU I/O protocol presence、virtualization/RAS policy entry availability。 | Overclocking、CPU policy、memory timing、NUMA/RAS、PCIe policy。 | Platform performance/tuning HII。 |
| PCIe Policy | PCIe controller/root-bridge/endpoint inventory、protocol presence，以及 ReBAR、Above 4G、SR-IOV、ASPM、bifurcation、hot-plug、ACS/ARI、IOMMU 能力提示。 | Native PCIe policy formset entry。 | Platform PCIe policy HII through FormBrowser；实际 policy changes 保持原生所有。 |
| Exit | Continue、reset、language、native UiApp fallback。 | Save/discard where native browser owns state。 | Native FormBrowser save/discard/default handling。 |

## 广义 IBV / 平台 Setup 功能分类

本分类覆盖 IBV、OEM、ODM 以及服务器、工作站、桌面、笔记本和嵌入式产品中常见的 Setup 页面族，范围有意大于当前 App 已实现的能力。边界必须保持清楚：`ModernSetupApp` 只提供只读摘要和进入原生 HII/FormBrowser 页面的入口壳层。CPU 频率/电压、内存时序/Profile、Chipset/SoC、风扇曲线、PCIe 资源策略，以及同类主板策略仍归原生 HII/FormBrowser 所有；任何可写策略都必须进入独立架构/设计评审，不属于本分类工作的授权范围。

优先级说明：`P0` 表示常见 App 摘要或入口需求，`P1` 表示产品化时有价值的覆盖项，`P2` 表示更偏平台定制的后续覆盖项。

| 领域 / 子系统 | 示例选项 | 适用产品 | 架构说明 | ModernSetup 处理方式 | 优先级 |
| --- | --- | --- | --- | --- | --- |
| System overview / Main | Firmware vendor/version、build date、board SKU、serial、UUID、language、date/time、administrator/user password 状态。 | 服务器、工作站、桌面、笔记本、嵌入式。 | 架构中立；SMBIOS 在 x86/Arm 服务器常见，在 RISC-V/LoongArch 上可选；RTC 和 language 是 UEFI 标准界面。 | 展示只读身份/状态；存在原生 date/time、language、password 页面时只提供入口。 | P0 |
| Boot manager / boot policy | Boot order、one-time boot、支持时的 UEFI/legacy mode、PXE、HTTP boot、USB boot、network stack、boot timeout、fast boot。 | 所有产品类型；网络启动在服务器、工作站、嵌入式上尤其常见。 | UEFI Boot#### 架构中立；CSM/legacy 主要是 x86 客户端遗留项；HTTP/PXE 取决于 NIC 栈。 | 展示 Boot#### 清单，并通过 UefiBootManagerLib 启动所选条目；编辑和策略仍归 Boot Maintenance/平台 HII。 | P0 |
| Security / identity | Secure Boot、Setup Mode、key databases、TPM/PTT/fTPM/TCM、measured boot、physical presence、chassis intrusion、passwords。 | 所有产品类型；服务器、工作站、笔记本最常见。 | Secure Boot 变量是 UEFI 标准；TPM/TCG 在 x86/Arm 常见，在 RISC-V/LoongArch 可选；面向中国市场的平台可能出现 TCM。 | 展示安全态势和协议/key 存在性；key 管理、password、physical-presence 流程和 measured-boot 策略保持原生。 | P0 |
| Firmware update / recovery | Capsule support、BIOS flash utility、recovery capsule、rollback prevention、dual-bank image、update log。 | 所有产品类型；嵌入式和服务器更强调恢复与生命周期。 | UEFI capsule 接口架构中立；恢复介质和 flash layout 与主板相关。 | 展示能力/状态，并打开原生 update/recovery app 或 HII；App 不执行 flash 写入。 | P0 |
| Device / HII formset inventory | Device Manager、Driver Health、storage controller pages、NIC pages、USB controller pages、option ROM pages。 | 所有产品类型。 | 通过 HII database 和 device path 保持架构中立；总线可用性取决于平台。 | 枚举入口，并通过 FormBrowser2 打开归属 formset；App 不解析 IFR、不实现 ConfigAccess。 | P0 |
| CPU topology / processor inventory | Socket/core/thread 数、microcode/firmware revision、cache、feature flags、current frequency、efficiency/performance core 提示。 | 服务器、工作站、桌面、笔记本；嵌入式在暴露 CPU 信息时适用。 | x86 常用 CPUID/SMBIOS；Arm 常用 MPIDR/SMBIOS/ACPI PPTT；RISC-V 和 LoongArch 多为平台特定拓扑/特性数据。 | 有 provider 数据时展示粗粒度清单；详细 CPU 策略页面保持原生。 | P0 |
| CPU frequency / voltage / overclocking | Ratio/multiplier、BCLK、voltage override/offset、P-state/CPPC policy、turbo limits、AVX offset、undervolt guard。 | 发烧级桌面、工作站、部分笔记本；服务器通常锁定或以 Profile 形式提供。 | 强依赖 CPU 厂商和主板供电；x86 最常见，Arm/RISC-V/LoongArch 通常是 SoC firmware/PMIC 特定实现。 | 仅原生处理。App 最多提示 performance/tuning 入口存在，绝不暴露可写频率/电压控制。 | P1 |
| Memory inventory / topology | DIMM slots、capacity、speed、ECC、rank、channel、interleave、training state、SPD summary。 | 服务器、工作站、桌面、笔记本；嵌入式差异较大。 | SMBIOS/ACPI 在 x86/Arm 服务器常见；RISC-V/LoongArch 平台数据可能较少；板载 LPDDR 通常插槽信息有限。 | 数据可靠时展示摘要/拓扑；training 和详细 memory setup 保持原生。 | P0 |
| Memory timing / profile / RAS | XMP/EXPO/JEDEC profile、DRAM frequency、primary/secondary timings、voltage、ECC scrub、patrol scrub、spare/rank sparing、mirroring、interleave。 | 服务器/工作站偏 RAS；发烧级桌面偏 timing/profile；笔记本通常受限；嵌入式取决于平台。 | x86 桌面/服务器最常见；Arm 服务器有 RAS/NUMA 控制；RISC-V/LoongArch 取决于内存控制器固件暴露程度。 | timing/profile/RAS 策略仅原生处理；App 可展示只读内存信息和 RAS 入口可用性。 | P1 |
| Chipset / SoC configuration | PCH/SoC straps、SATA mode、USB policy、integrated audio/camera/wireless、GPIO、I2C/SPI/UART、eMMC/SD、watchdog、serial console。 | 桌面、笔记本、嵌入式、工作站；服务器常见于 serial/watchdog/SoC IO。 | x86 PCH 术语不同于 Arm/RISC-V/LoongArch SoC fabric；嵌入式强依赖 device tree、ACPI 或板级 straps。 | enablement/policy 仅原生处理；App 可摘要设备存在性并提供 HII 入口。 | P1 |
| Storage / NVMe / RAID | SATA mode、NVMe info、VMD/RAID、RST、Opal/security、sanitize、hot-plug、bootable storage policy。 | 服务器、工作站、桌面、笔记本、嵌入式。 | NVMe/SCSI/ATA 协议架构中立；Intel VMD/RST 偏 x86；Arm/RISC-V/LoongArch 的 SoC storage policy 差异较大。 | 安全时展示清单和健康提示；RAID/VMD/Opal/sanitize 策略保持原生或厂商工具所有。 | P1 |
| PCIe resource / fabric policy | Above 4G decoding、ReBAR、SR-IOV、ASPM、bifurcation、link speed、hot-plug、ACS/ARI、IOMMU、option ROM、MMIO aperture。 | 服务器、工作站、桌面；嵌入式和笔记本取决于平台。 | PCIe 覆盖 x86/Arm 以及部分 RISC-V/LoongArch 平台；root-complex 拓扑和 IOMMU 命名按架构不同。 | 只展示只读能力和原生策略入口提示；实际资源分配和策略修改保持原生 HII/FormBrowser。 | P0 |
| Graphics / display | Primary display、iGPU/dGPU selection、hybrid graphics、GOP/OpROM、UMA frame buffer、panel/backlight、multi-monitor boot display。 | 桌面、工作站、笔记本、AIO/NUC、带显示的嵌入式产品。 | GOP 是 UEFI 标准；mux/UMA/backlight 与主板或 SoC 相关；独显选项多见于 x86/工作站但不限于此。 | 展示 GOP mode/当前 renderer；graphics mux、UMA、panel 策略保持原生。 | P1 |
| Network / connectivity | Onboard LAN、PXE/HTTP boot、MAC display、Wi-Fi/Bluetooth toggles、WoL、VLAN/iSCSI、management NIC selection。 | 所有产品类型；服务器和嵌入式更强调远程/网络启动。 | UEFI network 协议架构中立；无线开关常为笔记本/OEM 特定；management NIC 多见于服务器。 | 展示设备/协议存在性和 boot entries；network stack、VLAN/iSCSI、无线开关保持原生。 | P1 |
| Power management / battery | ACPI S-states、ErP、wake-on-LAN/USB/RTC、lid behavior、battery charge thresholds、adapter warnings、power restore policy。 | 笔记本偏 battery；桌面/工作站/AIO 偏 wake/ErP；服务器偏 restore policy；嵌入式偏掉电恢复。 | ACPI 在 x86/Arm 服务器常见，在 RISC-V/LoongArch 上也可能存在；device tree 平台标准 Setup 控制可能更少。 | 展示电源能力/状态；battery、wake 和 power-restore 策略保持原生。 | P1 |
| Thermal / fan / acoustics | Fan curves、pump header、thermal trip points、acoustic/performance mode、chassis thermal sensors、dust/fan diagnostics。 | 桌面、工作站、服务器、笔记本、主动散热嵌入式。 | 传感器/风扇控制强依赖 EC/BMC/Super I/O/PMIC；Arm/RISC-V/LoongArch 嵌入式差异很大。 | 只展示 thermal/fan 存在性和状态；fan curves 和 trip points 保持原生 HII、EC/BMC 或厂商工具所有。 | P1 |
| Platform management / BMC | IPMI、Redfish、BMC network、KVM/media、SEL、host interface、watchdog、power restore、remote update。 | 主要是服务器；部分工作站/嵌入式 appliance 也会使用。 | x86/Arm 服务器常见；RISC-V/LoongArch 服务器产品可选；协议暴露可能是 IPMI、Redfish 或 SMBIOS Type 38/42。 | 展示 management 能力/状态并打开原生 BMC/Redfish 页面；App 不配置 BMC 网络。 | P0 |
| RAS / reliability / serviceability | ECC mode、memory patrol scrub、MCA/SEA handling、poison、PCIe AER、SMI/NMI behavior、error logs、spare devices。 | 服务器和工作站；高可靠嵌入式。 | x86 服务器 RAS 成熟；Arm 服务器通过 ACPI/APEI/SEA；RISC-V/LoongArch 支持取决于产品成熟度。 | 展示健康/日志可用性和 RAS 入口提示；策略与日志清除保持原生。 | P1 |
| Virtualization / isolation | VT-x/AMD-V、SVM、VT-d/IOMMU/SMMU、SR-IOV、ATS/PRI/PASID、CXL isolation、confidential computing capabilities。 | 服务器、工作站、桌面；笔记本常见基础虚拟化；嵌入式取决于平台。 | x86 名称不同于 Arm SMMU/Realm/CCA 和 RISC-V/LoongArch IOMMU 扩展；可用性取决于平台。 | provider 能检测时仅展示只读能力存在性；开关状态、isolation mode、confidential-computing 策略保持原生 HII/FormBrowser 所有。 | P1 |
| Server profile / workload tuning | Balanced/performance/power saver、deterministic performance、NUMA、SNC/NPS、C-states、turbo policy、memory interleave、accelerator profile。 | 服务器、工作站/HPC。 | x86 和 Arm 服务器厂商暴露的旋钮不同；RISC-V/LoongArch 服务器 Profile 仍在演进且与产品相关。 | Profile 选择仅原生处理；若平台有稳定只读来源，App 可摘要当前 Profile。 | P1 |
| Embedded / industrial controls | Watchdog、serial console redirection、GPIO defaults、boot source pins、recovery mode、secure provisioning、field-update channel。 | 嵌入式、工业、appliance、类平板产品。 | 跨 Arm/RISC-V/LoongArch/x86 都强烈依赖板级实现，常绑定 device tree、ACPI tables、EC 或制造变量。 | 展示安全的 recovery/update/security 状态并提供原生入口；板级控制保持原生。 | P2 |
| Accelerator / CXL / AI device policy | CXL memory mode、persistent memory、accelerator enablement、firmware level、MMIO windows、RAS、telemetry。 | 服务器、工作站、部分边缘 AI 嵌入式。 | CXL 当前主要在 x86/Arm 服务器；RISC-V/LoongArch 取决于平台生态；accelerator 控制通常厂商特定。 | 可发现时展示清单/健康；mode/resource 修改保持原生。 | P2 |
| OEM customization / branding | EZ/Advanced mode、favorites、search、language packs、help text、screenshots、custom update tools、manufacturing pages。 | 所有 OEM 产品都有需求。 | UI 层架构中立；内容仍映射到产品特定 HII/formsets。 | 仅作为 theme/navigation 参考；App 可提供标准分类，但不能复制厂商策略页面。 | P2 |

## 参考链接

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
