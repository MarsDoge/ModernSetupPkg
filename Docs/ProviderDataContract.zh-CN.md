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

使用对象：本文档是**开发者/参与用户**视角的接口流实现说明。界面使用用户应以
页面、label、可见 value/status、row 行为和截图等分类/显示需求提出反馈；本文档再把
这些请求映射到 source/access/provider/native-owner/fallback 契约。

关键词 **必须/禁止/应当/可以** 按 RFC 2119。

## Secure Boot 原生配置入口

Quick Settings 的 Secure Boot 行（索引 4）与 Security 页面可选择的
**Secure Boot configuration** 入口使用固定 edk2 中
`SecurityPkg/Include/Guid/SecureBootConfigHii.h` 的 formset GUID
`5daf50a5-ea81-4de2-8f9b-cabda9cf5c14`，通过共享 DeviceData 缓存匹配全部
条目，不猜测标题，也不受 Devices 可见行数限制。
`Open setup` / `Unavailable` / `Discovery error` 表示入口发现状态，与只读的
SecureBoot 启用状态独立。激活时重新发现，经 `ModernUiDeviceDataOpenEntry()`
进入 FormBrowser2，继续由配置的 ModernDisplayEngine/LVGL 渲染。
缺失时返回 `EFI_NOT_FOUND`，不跳转 UiApp、BootManagerMenuApp 或其他表单。
原生回调负责密钥与策略，不新增 IFR 解析、ConfigAccess 或变量写入。
共享交接在返回（包括错误）后失效 boot/device/provider 缓存，安全摘要按需刷新。
TPM 仍只显示协议存在状态，不新增 TPM 配置入口。键盘与指针共用激活路径；
Quick Settings 保持先选择再激活，绘制和命中测试共用分组行坐标。

主机验证：`python3 Tests/Smoke/boot_handoff_test.py` 和 smoke guards。
固件构建、LVGL 视觉及原生回调验证仍需单独执行。

## 1. 原则

1. **只用标准源。** provider 字段**必须**取自标准固件接口(SMBIOS / ACPI / UEFI
   或 edk2 协议),绝不依赖写死的板级假设或厂商私有后门。
2. **只读。** provider **禁止**写变量、编程硬件或改策略。它只观察;所有改动归
   原生 FormBrowser/HII。
3. **优雅缺省。** 目录字段缺源/缺 owner 时**必须**保留可见并显示 `N/A`，
   不得隐藏、编辑或提交；仍可选中查看帮助/原因。有效的 `Disabled` / `0` 不是缺失。
   平台安全 suppression 与授权仍优先（§2.2）。其他只读摘要可使用本地化
   `Unknown`，绝不伪造值。
4. **架构中立门控。** 字段由*源是否在线*门控,而非写死的 `ARCH`。同一 App 构建
   运行于 X64/AARCH64/LOONGARCH64/RISCV64,只是回应的源不同(见 §6)。
5. **附加式演进。** `Include/ModernUi/*Data.h` 的 summary 结构体只能在末尾追加
   字段(按 `API_COMPATIBILITY.md`),绝不重排。
6. **单一访问层。** 结构化表访问(SMBIOS 遍历 + 字符串/UUID 提取、ACPI 查表)
   **应当**走单一共享辅助库,而非每个 provider 各写一遍(见 §5)。

## 2. 参考:IBV 设置页显示什么

仅信息架构参考(不复用素材/字符串)。主流 IBV 与其他平台固件(多为 edk2 衍生),覆盖 x86、Arm、LoongArch、RISC-V,
在**系统信息/主页**面收敛到
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
| 可信计算(TCM) | "可信计算模块(TCM)"、平台标识 | TCG2/厂商协议 + SMBIOS 身份 |

要点:部分平台强调**可信计算模块(TCM)**(与 TPM 并列或替代),以及**平台/
厂商标识**字符串。二者都映射到既有只读源(TCG2 存在性、SMBIOS
Type 1/2)——不引入任何新的策略面。

## 2.1 字段契约形态

每个新增的 provider 可见字段都**必须**在写代码前或同一改动中用同一契约形态记录。
这样可以保证 App 像一套 IBV Setup 首页，而不是一组临时 probe。

| 契约列 | 含义 |
| --- | --- |
| 字段 | 用户可见值或入口提示，例如 BIOS Version、CPU Model、Above 4G entry present。 |
| 标准源 | SMBIOS type/field、ACPI table、UEFI/PI protocol、UEFI variable、Boot#### variable 或 Boot Services query。 |
| 访问 API | 读取它的库/API（`ModernUiPlatformTablesLib`、BootDataLib、PciIo、DiskInfo、SimpleNetwork 等）。 |
| Provider owner | 归一化值所属的 `ModernUi*DataLib` summary。 |
| 显示面 | Main / Advanced / Chipset / Boot / Security / Server Management / Power & Thermal / Diagnostics / Save & Exit。 |
| 原生所有者 | 真实编辑所属的 HII/FormBrowser 页面或平台服务（如果有）。 |
| 回退 | 按 §4 使用 `N/A`、`Unknown` 或经验证的低优先级源；不得仅因缺源/缺 owner 隐藏目录项。 |
| 现状 | Done、Gap 或 Roadmap。 |

示例：

| 字段 | 标准源 | 访问 API | Provider owner | 显示面 | 原生所有者 | 回退 | 现状 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BIOS Version | SMBIOS Type 0 `BiosVersion` | `ModernUiPlatformTablesLib` | `ModernUiPlatformDataLib` | Main | 原生 firmware information 页（如果存在） | `gST->FirmwareRevision` / `Unknown` | Done |
| Above 4G entry present | 需要精确平台 HII owner 注册；仅 PCIe probe 不证明存在可编辑入口 | 只读注册解析器（目标） | `ModernUiPcieDataLib` 域 | Advanced / Chipset / Server Management | 已注册的平台 PCIe policy HII | 可见 `N/A`；可选中看帮助，禁止编辑/提交 | Roadmap（精确绑定；已有提示不构成证明） |

## 2.2 通用设置目录与变量绑定（目标契约）

本节规定集成要求，不代表已实现通用 UI、注册解析器或编辑后端。
上文已有 Secure Boot 原生路由是特定实现，不证明其他设置已经可用。

- **目录身份：** 稳定条目 ID、分类、本地化标签/帮助、值类型/选项/单位、只读源/provider、
  有效性、出处及覆盖状态。禁止把目录收录宣称为平台已实现。
- **精确 owner：** 注册 formset GUID 与经验证的 form/question 标识，必要时用设备路径/
  实例消歧。匹配实时 HII，激活时重新验证。禁止按标题、翻译字符串、关键词或协议存在性
  猜 owner。缺失/歧义/过期目标保持不可用，不回退到无关表单或 Boot picker。
  `SendForm()` 接受表单目标，没有 `QuestionId` 参数；问题聚焦需要另行验证原生机制。
- **独立状态：** 分离值有效性、owner 可用性、编辑/提交能力及选择/帮助能力。
  缺源/owner/绑定时目录行保持可见，显示 `N/A` 与原因，不可编辑、不可提交，但可选中
  看说明。经验证的只读值可与入口不可用的 `N/A` 同时显示。真实 `Disabled` / `0`
  禁止转成 `N/A`，未知值也禁止默认成 `Disabled` / `0`。
- **安全优先：** 不得借此绕过原生授权、安全 suppression、`suppressif`、`grayoutif`
  或 `disableif`。遵守原生隐藏/限制，目录帮助也不得泄露受保护元数据。
- **绑定出处：** 记录平台/版本、来源证据、owner、存储种类，以及适用且已验证的
  变量名/GUID、属性、布局版本、offset/width 或 name/value 键、编码、范围/选项、
  默认值、依赖、重置要求与实际消费方。未知 GUID/offset 保持未绑定，禁止按标签推测
  或照搬板级布局。Buffer/name-value/EFI-variable 存储、回调与服务不必使用
  `DynamicHii`；它只是绑定机制之一，不是唯一可配置来源。日期时间属于平台 RTC 服务
  （`GetTime` / `SetTime`），不是 App NV 偏好，编辑必须仍归原生 owner。
- **不新增写权限：** 绑定描述数据，不授权 App 或 provider 写入。平台变更仍走原生
  FormBrowser/ConfigAccess 与 owner 服务调用。不可用项不得进入待提交变更或报告已保存。
  提交成功、读回确认、重置/重启生效分别表示不同状态；持久化/效果声明需要真实
  owner/消费方测试。

对应表现契约见[可配置项与快捷设置](ConfigurableItemsAndQuickSettings.zh-CN.md)。
平台专属绑定必须有明确且经过评审的契约，不能成为 provider 私有后门；未指定字段保持
缺口。本次文档变更不新增运行时访问实现。

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
| 策略入口存在性提示(ReBAR/4G/SR-IOV/ASPM/…) | 协议存在性探测 | Done（仅只读提示；非精确 owner 绑定，也不证明可编辑） |
| 逐设备厂商/设备 ID、类 | **PciIo** 配置空间 `0x00`/`0x09` | Done |
| 逐设备链路速率/宽度 | **PciIo** PCIe 能力(`0x10` cap)配置读 | Done |
| 物理槽占用 | **SMBIOS Type 9**(System Slots) | Gap |

> 边界(不变,smoke 强制):PCIe **策略** —— ReBAR、Above-4G、SR-IOV、ASPM、
> bifurcation、热插拔、ACS/ARI、IOMMU、BAR/资源分配 —— **仍归原生 HII/FormBrowser**。
> 这里的 PciIo 是**只读枚举供显示**;App 禁止调 `SetBarAttributes` 或改配置空间。

### 存储 — `ModernUiInventoryDataLib`

| 字段 | 标准源 | 现状 |
| --- | --- | --- |
| 可启动存储存在性 | 设备路径清单(经 Devices) | Done(间接) |
| 设备总线类型(NVMe/SATA…)+ 容量 | **DiskInfo**(`EFI_DISK_INFO_PROTOCOL`)+ BlockIo | Done |
| 设备型号字符串 | DiskInfo Identify/Inquiry 解析 | Roadmap |

### 网络 — `ModernUiInventoryDataLib` + `ModernUiManagementDataLib`(服务器)

| 字段 | 标准源 | 现状 |
| --- | --- | --- |
| 管理 host interface | **SMBIOS Type 38/42**(IPMI/Redfish) | Done(存在性) |
| IPMI / Redfish 协议存在性 | 协议探测 | Done |
| NIC MAC / 链路状态 | **SimpleNetwork**(`Mode->CurrentAddress`/`MediaPresent`) | Done |

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
| 可信计算(TCM) | 厂商/TCG 协议存在性 | Roadmap |

## 4. 数据源优先级与回退

当多个标准源都能回答某字段时,provider **必须**按此顺序优先,缺失则下穿:

1. **实时/动态协议**(反映*运行*态时,如 MP Services 取实际启用核数;PciIo 取
   在位设备)。
2. **SMBIOS**(静态身份/清单;x86/Arm 服务器常有,RISC-V/LoongArch 可选)。
3. **ACPI**(SMBIOS 薄时取拓扑,如 Arm ACPI PPTT 的缓存/CPU 拓扑、SRAT/SLIT 的
   NUMA)。
4. **UEFI 核心服务**(内存映射、句柄库)作为架构中立的底线。
5. **可见 `N/A`**（无源回应时）；目录行仍可选中看帮助，不可编辑/提交。
   缺 owner 不是隐藏理由；平台安全 suppression 仍有最终决定权（§2.2）。

占位串("To Be Filled By O.E.M."、"Not Specified"…)**必须**当作缺失(SMBIOS
身份已这么做)。

## 5. 共享访问层(“connect”工作)

此前四个 provider 各自 `LocateProtocol(gEfiSmbiosProtocolGuid)` 并独立走表，
SMBIOS 字符串/UUID 提取每个 provider 重写一遍 —— 重复，也正是严格对齐 bug 藏身
处(AArch64 packed-UUID 那次)。

**现状：已实现。** **`ModernUiPlatformTablesLib`**
(`Include/ModernUi/ModernUiPlatformTables.h`) 是单一表访问层：

- `ModernUiSmbiosFindStructure(type, index)`、`ModernUiSmbiosTypePresent(type)`、
  NUL 安全的 `ModernUiSmbiosGetString()` 和 `ModernUiSmbiosIsPlaceholder()`。
- `ModernUiSmbiosPresent()` / `ModernUiAcpiPresent()` source-liveness probe，供
  provider 报告 SMBIOS/ACPI 是否整体存在。
- 通过 `ModernUiAcpiFindTable(signature)` / `ModernUiAcpiTablePresent(signature)`
  走 ACPI RSDP/XSDT（RSDT fallback）。
- `ModernUiPlatformDataLib`、`ModernUiDiagnosticsDataLib`、`ModernUiPowerDataLib`
  已消费它来处理标准 table/source liveness，不再使用私有 SMBIOS/ACPI/config-table
  probe。

剩余工作：provider-specific live protocol（IPMI、Redfish、CpuIo2、AcpiSdt 等）
仍保留在各自 domain provider；但 SMBIOS/ACPI 表存在性和结构查找都必须走
`ModernUiPlatformTablesLib`。smoke 现在会拒绝共享表访问层之外的 provider 直接访问
SMBIOS protocol 或 ACPI configuration table。

这是既有 `*Data.h` summary 背后的附加 helper 扩展 —— 不重排 summary 结构体、
不承诺 provider ABI —— 也是让可选协议步骤 (§7) 干净的前置。

## 6. 架构覆盖(XArch)

| 源 | X64 | AARCH64 | LOONGARCH64 | RISCV64 |
| --- | --- | --- | --- | --- |
| SMBIOS(Type 0–17、38/42) | 常见 | 常见(服务器) | 可选 | 可选 |
| ACPI(PPTT/SRAT/SLIT/MADT) | 常见 | 常见 | ACPI 或 DT | ACPI 或 DT |
| PciIo / PciRootBridgeIo | 常见 | 平台相关 | 平台相关 | 萌芽 |
| MP Services | 常见 | 常见 | 平台相关 | 平台相关 |
| TCG2 | 常见 | 平台相关 | 平台相关(含 TCM) | 平台相关 |
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
