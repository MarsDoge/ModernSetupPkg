<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Provider 数据契约

语言：[English](ProviderDataContract.md) | 简体中文

本文档是 ModernSetupPkg 的**规范性数据源契约**:对 App 与 DisplayEngine 显示的
每一个值,钉死它**来自哪个标准 edk2 接口**(SMBIOS 结构类型 / ACPI 表 / edk2
协议)、**只读边界**、**数据源优先级/回退**与**演进规则**。它是表现侧
[App 功能规范](AppFeatureStandard.zh-CN.md) 与参考资料
[IBV 与平台 Setup 调研](IbvAndPlatformSetupSurvey.zh-CN.md) 的数据侧伴随文档。

关键词 **必须/禁止/应当/可以** 按 RFC 2119。

## 1. 原则

1. **只用标准源。** provider 字段**必须**取自标准固件接口(SMBIOS / ACPI / UEFI
   或 edk2 协议),绝不依赖写死的板级假设或厂商私有后门。
2. **只读。** provider **禁止**写变量、编程硬件或改策略。它只观察;所有改动归
   原生 FormBrowser/HII。
3. **优雅缺省。** 源缺失的字段**必须**退化为本地化 `N/A`/`Unknown`(或隐藏行),
   绝不出现 tofu、乱码或假值。
4. **架构中立门控。** 字段由*源是否在线*门控,而非写死的 `ARCH`。同一 App 构建
   运行于 X64/AARCH64/LOONGARCH64/RISCV64,只是回应的源不同(见 §6)。
5. **附加式演进。** `Include/ModernUi/*Data.h` 的 summary 结构体只能在末尾追加
   字段(按 `API_COMPATIBILITY.md`),绝不重排。
6. **单一访问层。** 结构化表访问(SMBIOS 遍历 + 字符串/UUID 提取、ACPI 查表)
   **应当**走单一共享辅助库,而非每个 provider 各写一遍(见 §5)。

## 2. 参考:IBV / 信创 设置页显示什么

仅信息架构参考(不复用素材/字符串)。主流 IBV(AMI Aptio、Insyde H2O、Phoenix
SecureCore)与信创平台(龙芯 Loongson、飞腾 Phytium、鲲鹏 Kunpeng、海光 Hygon、
兆芯 Zhaoxin;固件多为 edk2 衍生或 ByoCore/昆仑)在**系统信息/主页**面收敛到
相似形态,外加更深的硬件页:

| 常见信息页项 | 典型设置标签 | 对应的标准源 |
| --- | --- | --- |
| 固件/BIOS 版本+日期 | "BIOS Version"、"Build Date" | SMBIOS Type 0 |
| 系统身份 | "Product Name"、"Serial"、"UUID" | SMBIOS Type 1 |
| 主板 | "Motherboard"、"Board Serial" | SMBIOS Type 2 |
| 处理器 | "Processor Type"、"Speed"、"Count" | SMBIOS Type 4(+ MP Services 取实时数) |
| 处理器缓存 | "L1/L2/L3 Cache" | SMBIOS Type 7(Arm 上 ACPI PPTT) |
| 总量+逐条内存 | "Total Memory"、"DIMM #, Size, Speed, Type" | SMBIOS Type 16/17(+ UEFI 内存映射取总量) |
| 内存插槽 | "Slot population" | SMBIOS Type 17(每槽一条) |
| 存储设备 | "SATA/NVMe device list" | BlockIo / DiskInfo / 设备路径 |
| PCIe/扩展槽 | "Slot occupancy, link" | SMBIOS Type 9 + PciIo(逐设备) |
| 网络 | "MAC address"、"NIC" | SimpleNetwork / 设备路径 |
| 安全态势 | "Secure Boot"、"TPM/TCM" | UEFI 变量 + TCG2 协议 |
| 信创特有 | "可信计算 TCM(国密)"、"国产化平台标识" | TCG2/厂商协议 + SMBIOS 身份 |

信创要点:国产平台强调**可信计算/国密 TCM**(与 TPM 并列或替代),以及**平台/
厂商标识("国产化标识")**。二者都映射到既有只读源(TCG2 存在性、SMBIOS
Type 1/2)——不引入任何新的策略面。

## 3. 数据源映射表(域 → 字段 → edk2 源 → 现状)

现状:**Done** = 今天已暴露;**Gap** = 字段已知、源已知但尚未接;**Roadmap** =
更大的后续。

### 平台/系统身份 — `ModernUiPlatformDataLib`

| 字段 | 标准源 | 现状 |
| --- | --- | --- |
| 固件厂商 | `gST->FirmwareVendor` | Done |
| 固件版本(人性化) | `gST->FirmwareRevision` | Done |
| BIOS 版本/发布日期 | **SMBIOS Type 0** | Done |
| 系统产品/制造商 | **SMBIOS Type 1** | Done |
| 序列号/UUID | **SMBIOS Type 1** | Done |
| 主板 | **SMBIOS Type 2** | Done |
| 外形 | **SMBIOS Type 3**(机箱类型) | Done |
| 架构 | 编译期 `MDE_CPU_*` | Done |
| 内存总量(MiB) | **UEFI 内存映射** | Done |
| 内存类型/速度/条数 | **SMBIOS Type 17**(聚合) | Done |
| 显示模式 | **GraphicsOutput** 当前模式 | Done |

### 处理器 — `ModernUiPlatformDataLib` + `ModernUiPerformanceDataLib`

| 字段 | 标准源 | 现状 |
| --- | --- | --- |
| 处理器版本/型号 | **SMBIOS Type 4**(`ProcessorVersion`) | Done |
| 核/线程数 | **SMBIOS Type 4**(`CoreCount`/`ThreadCount` + `*2`) | Done |
| 实时已启用核/线程数 | **MP Services**(`EFI_MP_SERVICES_PROTOCOL`) | Done |
| 当前/最大频率(MHz) | **SMBIOS Type 4**(`CurrentSpeed`/`MaxSpeed`) | Done |
| **L1/L2/L3 缓存** | **SMBIOS Type 7**(Cache);Arm 上 **ACPI PPTT** | Done(Type 7) |
| 处理器清单存在性 | SMBIOS Type 4 存在性 | Done(布尔) |

> 注:CPU 数据当前是分裂的 —— 身份在 Platform、存在性布尔在 Performance。契约
> 目标是由 Type 4 + Type 7(+ MP Services 取实时数)喂养的单一连贯处理器摘要,
> Performance 只保留调优/RAS 的*入口可用性*提示。

### 内存(逐条) — `ModernUiPlatformDataLib`

| 字段 | 标准源 | 现状 |
| --- | --- | --- |
| 聚合类型/速度/条数 | SMBIOS Type 17(首条已装) | Done |
| 逐槽:定位符、容量、速度、类型、rank | **SMBIOS Type 17**(每槽一条) | Gap |
| 阵列最大容量/槽数 | **SMBIOS Type 16** | Gap |

### PCIe — `ModernUiPcieDataLib`

| 字段 | 标准源 | 现状 |
| --- | --- | --- |
| 控制器/根桥/端点/桥计数 | **PciIo / PciRootBridgeIo** 枚举 | Done |
| 策略入口存在性提示(ReBAR/4G/SR-IOV/ASPM/…) | 协议存在性探测 | Done(只读提示) |
| 逐设备厂商/设备 ID、类 | **PciIo** 配置空间 `0x00`/`0x09` | Done |
| 逐设备链路速率/宽度 | **PciIo** PCIe 能力(`0x10` cap)配置读 | Done |
| 物理槽占用 | **SMBIOS Type 9**(System Slots) | Gap |

> 边界(不变,smoke 强制):PCIe **策略** —— ReBAR、Above-4G、SR-IOV、ASPM、
> bifurcation、热插拔、ACS/ARI、IOMMU、BAR/资源分配 —— **仍归原生 HII/FormBrowser**。
> 这里的 PciIo 是**只读枚举供显示**;App 禁止调 `SetBarAttributes` 或改配置空间。

### 存储 —(roadmap provider)

| 字段 | 标准源 | 现状 |
| --- | --- | --- |
| 可启动存储存在性 | 设备路径清单(经 Devices) | Done(间接) |
| 设备型号/类型(NVMe/SATA) | **DiskInfo**(`EFI_DISK_INFO_PROTOCOL`)+ BlockIo | Roadmap |

### 网络 — `ModernUiManagementDataLib`(服务器)/ Devices

| 字段 | 标准源 | 现状 |
| --- | --- | --- |
| 管理 host interface | **SMBIOS Type 38/42**(IPMI/Redfish) | Done(存在性) |
| IPMI / Redfish 协议存在性 | 协议探测 | Done |
| NIC MAC / 身份 | **SimpleNetwork** / 设备路径 | Roadmap |

### 诊断 / ACPI — `ModernUiDiagnosticsDataLib`

| 字段 | 标准源 | 现状 |
| --- | --- | --- |
| ACPI 表存在性 | **ACPI**(RSDP/XSDT 经配置表或 `EFI_ACPI_SDT_PROTOCOL`) | Done |
| SMBIOS 表存在性 | SMBIOS 协议 | Done |
| 内存映射/句柄/配置表计数 | UEFI 启动服务 | Done |
| NUMA 拓扑(节点、距离) | **ACPI SRAT / SLIT** | Roadmap(服务器) |

### 电源/散热 — `ModernUiPowerDataLib` + `ModernUiHardwareHealthDataLib`

| 字段 | 标准源 | 现状 |
| --- | --- | --- |
| ACPI 表/协议状态 | ACPI 存在性 | Done |
| 机箱热状态、电源 | **SMBIOS Type 3 / Type 39** | Done(存在性) |
| 真实传感器温度 | 平台传感器源(UEFI 无标准) | 仅演示 |

### 安全 — `ModernUiSecurityDataLib`

| 字段 | 标准源 | 现状 |
| --- | --- | --- |
| Secure Boot / Setup Mode | **UEFI 变量**(`SecureBoot`、`SetupMode`) | Done |
| PK/KEK/db/dbx 存在性 | UEFI 变量 | Done |
| TPM / TCG 存在性 | **TCG2**(`EFI_TCG2_PROTOCOL`) | Done |
| TCM / 国密可信计算 | 厂商/TCG 协议存在性 | Roadmap(信创) |

## 4. 数据源优先级与回退

当多个标准源都能回答某字段时,provider **必须**按此顺序优先,缺失则下穿:

1. **实时/动态协议**(反映*运行*态时,如 MP Services 取实际启用核数;PciIo 取
   在位设备)。
2. **SMBIOS**(静态身份/清单;x86/Arm 服务器常有,RISC-V/LoongArch 可选)。
3. **ACPI**(SMBIOS 薄时取拓扑,如 Arm ACPI PPTT 的缓存/CPU 拓扑、SRAT/SLIT 的
   NUMA)。
4. **UEFI 核心服务**(内存映射、句柄库)作为架构中立的底线。
5. **`N/A` / 隐藏行**(无源回应时)。

占位串("To Be Filled By O.E.M."、"Not Specified"…)**必须**当作缺失(SMBIOS
身份已这么做)。

## 5. 共享访问层(要"连接"的活)

今天四个 provider 各自 `LocateProtocol(gEfiSmbiosProtocolGuid)` 并独立走表,
SMBIOS 字符串/UUID 提取每个 provider 重写一遍 —— 重复,也正是严格对齐 bug 藏身
处(AArch64 packed-UUID 那次)。契约目标:

- 单一 **`ModernUiPlatformTablesLib`**(或等价)统管:缓存的 SMBIOS 入口、
  `FindStructure(type[, index])`、NUL 安全字符串提取、对齐安全的定长字段拷贝
  (GUID/UINT16/…)、以及 ACPI RSDP/XSDT + `FindAcpiTable(signature)`。
- 每个 `ModernUi*DataLib` 改调它,不再各写各的。
- smoke **应当**加一条守卫:provider 源在共享库之外直接 `LocateProtocol` SMBIOS
  或走 ACPI,即为 finding。

这是既有 `*Data.h` 契约背后的纯重构 —— 无公开 API 变更 —— 也是让可选协议步骤
(§7)干净的前置。

## 6. 架构覆盖(XArch)

| 源 | X64 | AARCH64 | LOONGARCH64 | RISCV64 |
| --- | --- | --- | --- | --- |
| SMBIOS(Type 0–17、38/42) | 常见 | 常见(服务器) | 可选 | 可选 |
| ACPI(PPTT/SRAT/SLIT/MADT) | 常见 | 常见 | ACPI 或 DT | ACPI 或 DT |
| PciIo / PciRootBridgeIo | 常见 | 平台相关 | 平台相关 | 萌芽 |
| MP Services | 常见 | 常见 | 平台相关 | 平台相关 |
| TCG2 | 常见 | 平台相关 | 平台相关(含国密 TCM) | 平台相关 |
| UEFI 内存映射 / GOP | 是 | 是 | 是 | 是 |

没有字段以写死的 ARCH 门控;薄 SMBIOS 目标(RISC-V/LoongArch VM)按 §4 下穿到
ACPI/UEFI/`N/A`。纯设备树平台是有记录的后续(UEFI 尚无标准 DT 消费面)。

## 7. 可选:provider 协议(延后,ABI 门控)

11 个 `*Data.h` 摘要当前是**链接期(LibraryClass)契约**。把它们提升为**运行时
协议**(`EFI_MODERN_SETUP_*_PROVIDER_PROTOCOL`,GUID 入 `ModernSetupPkg.dec`)
会让平台/OEM 驱动*安装*更丰富的 provider(真实 BMC/传感器/RAS 数据),App 运行
时拾取,内置 provider 兜底。`ModernSetupAppProvider.c`(唯一调 provider 的地方)
是天然接缝:先 `LocateProtocol`,内置兜底。

这是一次**公开 ABI 承诺**(永久二进制兼容;摘要结构体加 `Size`/`Revision` 字段),
**必须**按 `API_COMPATIBILITY.md` 走 `core-api` review。它**应当**延后到有具体
消费者(想注入数据的平台)出现 —— 过早协议化只会平白背 ABI 负担。§5(共享访问)
与 §3 的缺口填补在此期间以零 ABI 成本交付"统一数据源"的价值。

## 8. 变更控制

新增字段或源映射是用户可见的:记入 `CHANGELOG.md`,在同一 PR 内同步更新本契约与
其[English 镜像](ProviderDataContract.md),并按 `API_COMPATIBILITY.md` 保持
摘要结构体附加式变更。表现侧(字段显示在哪页)由
[AppFeatureStandard.zh-CN.md](AppFeatureStandard.zh-CN.md) 治理。
