<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# ModernSetup App 功能规范

语言：[English](AppFeatureStandard.md) | 简体中文

本文档是 ModernSetup 标准首页 App 的**规范性**定义：它规定 App 暴露哪些页面、
仪表盘结构、快捷分类卡片，以及这些内容如何按平台类别自适应。
[IBV 与平台 Setup 调研](IbvAndPlatformSetupSurvey.zh-CN.md) 和
[产品化功能矩阵](ProductizationFeatureMatrix.zh-CN.md) 是*参考*资料（更广的固件
生态在做什么），而本文档是*强制性*的（App 必须/应当怎么做才算合规）。
`Application/ModernSetupApp/` 的重排和 `Tests/Smoke/smoke_validate.py` 的守卫
都以本规范为准。

关键词 **必须**(MUST)、**禁止**(MUST NOT)、**应当**(SHOULD)、**不应**(SHOULD NOT)、
**可以**(MAY) 按 RFC 2119 语义使用。

## 原生 HII 呈现

原生表单继续由 FormBrowser 负责编辑和校验。空闲等待与键盘编辑等待每秒只刷新
顶部时钟文字区域，不触发表单刷新，也不重启表单退出超时。清屏后停止绘制时钟，
直到重新绘制页面框架。

高度至少为 720px 的全屏图形表单使用目标行高 32px 的私有网格，不修改固件控制台
模式；调用者明确指定屏幕区域时保持原网格。消息弹窗使用相同视口。配置页显示
真实表单标题、石墨色选中行和值控件、上下文帮助，不再重复显示硬件信息侧栏；
该侧栏保留在原生首页。

## Secure Boot 原生配置入口

Advanced > Runtime 的键盘和鼠标导航均进入 Quick Settings，避免 Dashboard
卡片过滤后入口不可达；Power 保留 Dashboard 入口。

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

## 1. 使用对象模型与所有权边界

本 App 规范服务两个对象，UI **禁止**把两者混在一起：

| 使用对象 | 关心什么 | 应放在哪里 |
| --- | --- | --- |
| 界面使用用户 | 界面分类与显示实现：字段属于哪个 Setup 分类、如何分组、显示什么 label/value/status、哪一行可选、UI 暴露哪个原生入口。 | 可见 UI、页面/分类布局、label、row state、entry-point affordance、截图。 |
| 开发者/参与用户 | 接口流相关实现：数据如何从 SMBIOS/ACPI/UEFI services/PI protocols/native HII/app NV 流入 provider，App 如何消费，谁拥有写入，fallback/status 如何解释，以及验证如何守住这条流。 | `ProviderDataContract.md`、#45 等 issue、代码注释、smoke guard、provider header、实现文档。 |

因此可见页面应聚焦 category、label、value、state 和安全入口（例如 “Native
Security setup owns keys and TPM policy”）。正式
source/access/provider/native-owner/fallback/status 流程放在开发者文档和 issue 中。

App 是一个只读首页加一组安全入口，它**不是**第二套 Setup 策略引擎。

- App **禁止**解析 IFR/VFR、实现 `ConfigAccess`、改写 HII 表单或写 varstore。
  任何真实配置动作要么通过 `UefiBootManagerLib` 启动一个 boot 项，要么通过
  `EFI_FORM_BROWSER2_PROTOCOL.SendForm()` 进入原生 edk2 FormBrowser。
- App **必须**把平台专属策略（CPU 频率/电压、内存时序/profile/RAS、芯片组/SoC
  strap、风扇曲线、PCIe 资源策略、BMC 网络、密钥/TPM 管理）仅作为*摘要*和/或
  原生*入口*呈现，这些仍归原生所有。
- App 显示的信息**必须**来自只读的 `ModernUi*DataLib` provider（见功能矩阵的
  Provider Roadmap），绝不依赖写死的板级假设。

此边界与 smoke 强制的边界一致，本文档不放宽它。

## 2. 平台类别

App 围绕五个平台类别做标准化。类别在运行时由 SMBIOS form factor 与管理能力
provider 推导，仅作*呈现*提示，绝不用于决定安全或策略。

| 类别 | 代码意图 | 典型示例 |
| --- | --- | --- |
| `Client-Desktop` | 台式/工作站/AIO/NUC/迷你机。 | OVMF X64 台式、ARM/LoongArch 台式。 |
| `Client-Mobile` | 笔记本/二合一/带电池平板。 | 笔记本类产品。 |
| `Server` | 带管理的机架/刀片/服务器主板。 | x86/Arm/LoongArch 服务器、RISC-V 服务器原型。 |
| `Embedded` | 工业/专用设备主板。 | ARM/RISC-V/LoongArch 板卡。 |
| `Unknown` | 未上报 form factor（虚拟机常见）。 | 无 SMBIOS chassis 数据的 QEMU/OVMF。 |

`Unknown` **必须**表现为仍然安全可显示的最宽容超集 —— 即遵循 `Client-Desktop`
卡片集，外加任何其 provider 上报了实时数据的卡片。App 绝不*因为*类别未知而隐藏
卡片；它只隐藏那些既不适用于当前类别**又**没有可用 provider 的卡片。

## 3. 规范页面集与 IBV 产品信息架构

C 实现仍然暴露 `SETUP_PAGE` 枚举，新增枚举值**必须**继续以附加方式追加在
`PageMax` 之前。但面向产品的信息架构应当读起来像一套 IBV Setup：**Main、
Advanced、Chipset / Platform、Boot、Security、Server Management、Power & Thermal、
Diagnostics、Preferences、Save & Exit**。下表是该 IBV 产品 IA 到当前 enum 名称的
规范映射。

| 产品 IA | 当前页面 | 用途 | App 显示 | 原生所有者 |
| --- | --- | --- | --- | --- |
| Main | `PageDashboard` + `PageSystemInfo` | 第一眼平台状态和只读系统规格详情。 | BIOS/固件版本、构建/发布日期、产品/主板身份、CPU、内存、架构、启动模式、显示模式、provider 健康、快捷分类网格。 | —（SMBIOS/UEFI/provider 只读）。 |
| Boot | `PageBoot` | 启动清单 + 启动。 | `Boot####` 激活/隐藏/类别/路径、BootNext/BootOrder affordance、原生 boot tools fallback、面向用户的所有权/不可用提示。 | Boot Maintenance HII。 |
| Advanced | `PageDevices` + `PageQuickSettings` + 部分 `PagePerformance` 行 | 面向高频平台设置的 curated 入口，但不成为策略所有者。 | HII formset、设备路径行、Driver Health、Quick Settings 原生归属提示、CPU/内存/调优入口可用性。 | 各驱动 formset，经 FormBrowser2。 |
| Chipset / Platform | `PageDevices` + `PageServerInventory` + provider 子区 | 平台 fabric 和板载设备可见性。 | HII/device entries、PCIe/root-bridge 清单、资源入口提示、provider 可暴露的存储/网络/设备清单。 | 平台 chipset/SoC/PCIe/storage HII。 |
| Security | `PageSecurity` | 安全态势。 | Secure Boot、Setup Mode、PK/KEK/db/dbx、TPM/TCG/TCM 存在性、原生安全所有权提示。 | SecurityPkg/平台 HII。 |
| Firmware Update / Recovery | `PageFirmware` | 固件生命周期。 | Capsule 支持、人性化固件版本、恢复/更新入口。 | Capsule/update HII 或 app。 |
| Server Management | `PageManagement` + `PageServerInventory` | 服务器/远程管理和资产汇总。 | BMC/IPMI/Redfish 存在性、host interface、管理清单、PCIe 策略入口提示。 | BMC/Redfish/平台管理 HII。 |
| Power & Thermal | `PagePower` | 电源/散热可见性。 | ACPI 状态、机箱热状态、电源记录存在性、sensor provider 摘要、面向用户的所有权/不可用提示。 | 平台电源/散热 HII、EC、BMC 或服务 app。 |
| Performance / Tuning | `PagePerformance` | CPU/内存和调优可见性。 | 处理器/内存清单、CPU I/O 协议、虚拟化/RAS/原生调优入口提示。 | 平台 performance/tuning/RAS HII。 |
| Diagnostics | `PageDiagnostics` | Bring-up/服务可见性。 | ACPI/SMBIOS 存在性、内存映射/句柄/表计数、provider 健康、首个 degraded provider。 | 平台诊断 HII 或服务 app。 |
| Preferences / UX | `PagePreferences` | App 本地 UX 偏好。 | 主题、密度、语言、OEM 水印开关。 | —（App 自有，无平台状态）。 |
| Save & Exit | `PageExit` | 会话/shell 控制。 | 继续、重置、原生 UiApp 回退、语言。 | 原生 FormBrowser 保存/放弃/default 工作流。 |

`PageQuickSettings` 是 Tier-B curated 入口面，不是可写策略页：行可被选中，
这样 Enter 可以报告原生所有者或 handoff 状态，但编辑仍只在进入归属 FormBrowser
页面后发生。因此 PCIe policy 通过 Advanced / Chipset / Server Management 的入口
提示呈现；它在 App 中**不**成为独立可写顶级页，且 App **禁止**暴露可写 PCIe 控制。

### 启动配置交接

Boot 页的 **Native Boot Tools** 行先刷新 DeviceData 清单，再遍历全部条目，
按 edk2 Boot Maintenance formset GUID
`642237c7-35d4-472d-8365-12e0ccf27a22` 匹配，不受 Devices 可见行数限制。
`ModernUiDeviceDataOpenEntry()` 进入 FormBrowser2，由所选 DisplayEngine
（含 LVGL）渲染原生表单。App 不解析 IFR、不调用 ConfigAccess、不新增策略写入；
现有 BootNext/BootOrder provider wrapper 保持不变，这些操作不是只读操作。

替换 UiApp 的 overlay 在替换组件上保留上游 `BootMaintenanceManagerUiLib`
NULL library，由其构造/析构函数注册和清理 HII，原生回调拥有配置写入。
否则替换 UiApp 会同时移除 Boot Maintenance 所有者。独立 App 构建仍依赖
已安装的 formset 或原生回退，不强制链接该库。

没有可用匹配时，保留既有回退：普通镜像启动 UiApp，替换镜像启动
BootManagerMenuApp 以避免递归。**BootManagerMenuApp 只是启动选择器，不是
Boot Maintenance。** 枚举错误和已匹配交接的错误原样返回，避免在可能已有修改后
再次打开另一个 UI。任意 Devices FormBrowser 交接或原生回退 `StartImage()`
返回（含错误）后，启动选项、设备条目和 provider 快照缓存失效，下次读取按需重建。
Exit 页原生回退与 Boot 配置入口保持分离。Boot/Devices 重绘前按新行数收敛选择索引，
Boot 路径及说明文本限制在值区域左侧，避免重叠。

主机验证：`python3 Tests/Smoke/boot_handoff_test.py` 编译真实路由函数，
使用模拟 provider/cache API；smoke 检查路由及 overlay 契约。
固件验证还需进入 Boot → Native Boot Tools、保存原生启动设置、返回并检查刷新结果。
主机测试本身不证明原生回调或 LVGL 渲染正确。

## 4. 仪表盘结构

仪表盘**必须**自上而下分三个区：

1. **系统信息面板** —— 只读身份/清单：固件厂商、人性化固件版本
   （`主.次 (0x十六进制)`）、平台、form factor、boot mode、CPU identity、内存、
   显示模式。解析为 `N/A`/`Unknown`/`Limited data` 的行**应当**折叠（行上浮），
   而不是显示成一条死占位。详细 `PageSystemInfo` 视图**应当**按
   System Identity、Firmware Identity、Processor、Memory、Runtime 分组；provider
   有上报时可展示 serial number、UUID、BIOS version/date、processor speed、cache、
   logical processor count 等 SMBIOS/UEFI 明细。
2. **平台健康面板** —— 架构、provider 健康摘要、覆盖度、首个问题。横向空间足够
   时显示；否则系统面板占满整宽。
3. **快捷分类网格** —— §5 的标准化导航卡片。

已经出现在面板 1–2 里的状态**禁止**成为某个快捷卡片的*唯一*目的：每个快捷卡片
首先是导航入口（回车跳到对应页面），其次才是一行状态。

## 5. 标准化快捷分类卡片与顶部导航

顶部导航**必须**只暴露收敛后的一级产品 IA，而不是每个具体页面都做横向标签。
横向 strip 限定为主要分类：`Main`、`Advanced`、`Boot`、`Security`、`Exit`。
Firmware、Diagnostics、Management、Power、Performance、Quick Settings、Assets、
Preferences 等详细页面属于二级目的地，通过 Dashboard 快捷卡片目录或所属分类进入。

二级归属应作为最左侧的固定可见、可点击竖排 rail 展示，也不是再把所有页面横向铺满。
Dashboard 首页保持全宽入口目录，不显示该 rail；详细/分类页才显示二级 rail。rail 只显示当前一级分类下的分组，例如 Advanced 竖排显示 `Platform`、`Runtime`、
`Service`、`UX`，Security 显示 `Posture`、`Secure Boot`、`TPM`，Boot 显示
`Order`、`Native Tools`。点击二级分组会进入该分组的代表三级页面；三级归属保留在页面标题层级和内容卡片/列表中，
例如 `Advanced > Runtime > Power` 或 `Main > System > Inventory`。

快捷网格是一个有序目录。每个卡片是导航入口：`Title` 是分类，`Value`/`Detail`
是一行实时状态，回车路由到映射的页面/焦点。规范目录：

| # | 卡片 | 路由到 | 分组 | 一行状态 |
| --- | --- | --- | --- | --- |
| 0 | 继续启动 | `PageExit` / content | Exit | “等同原生 Continue”。 |
| 1 | 启动选项 | `PageBoot` / content | Boot & Devices | 启动项数 + 模式/安全提示。 |
| 2 | 设备 | `PageDevices` / content | Boot & Devices | HII 句柄/表计数。 |
| 3 | Provider 状态 | `PageDiagnostics` / nav | Platform Health | provider 健康 + 覆盖度。 |
| 4 | 固件 | `PageFirmware` / nav | Platform Health | 厂商 + 人性化版本；capsule 存在性。 |
| 5 | 电源/散热 | `PagePower` / nav | Power & Performance | 机箱热/传感器或 ACPI+SMBIOS 存在性。 |
| 6 | 性能 | `PagePerformance` / nav | Power & Performance | CPU/内存/PCIe 就绪度。 |
| 7 | 服务器清单 | `PageServerInventory` / nav | Management | 管理存在性 + PCIe root 数。 |

### 5.1 按平台类别的适用性

每个卡片要么 `Always`（所有类别都显示），要么按类别限定。被类别限定的卡片，当其
类别匹配**或**其后端 provider 上报实时数据时显示；否则隐藏，网格回流。

| 卡片 | Client-Desktop | Client-Mobile | Server | Embedded | Driver |
| --- | --- | --- | --- | --- | --- |
| 继续启动 | Always | Always | Always | Always | — |
| 启动选项 | Always | Always | Always | Always | `ModernUiBootDataLib` |
| 设备 | Always | Always | Always | Always | `ModernUiDeviceDataLib` |
| Provider 状态 | Always | Always | Always | Always | 诊断汇总 |
| 固件 | Always | Always | Always | Always | `ModernUiFirmwareDataLib` |
| 电源/散热 | Always | Always（强调电池） | Always | provider 有数据则显示 | `ModernUiPowerDataLib` |
| 性能 | Always | Always | Always | provider 有数据则显示 | `ModernUiPerformanceDataLib` |
| **服务器清单** | **隐藏** | **隐藏** | **Always** | 管理/PCIe 有数据则显示 | `ModernUiManagementDataLib` / `ModernUiPcieDataLib` |

当前唯一硬性按类别限定的卡片是**服务器清单**：它是服务器类内容（BMC/IPMI/Redfish
+ PCIe root 策略），在 `Client-Desktop`/`Client-Mobile` 上**必须**隐藏，**除非**
有管理或 PCIe provider 上报实时数据（这样带管理的工作站、或能发现 PCIe 策略的
台式机仍会显示它）。在 `Unknown` 上遵循 live-provider 规则。

未来按类别新增（如 `Client-Mobile` 的电池卡、`Embedded` 的恢复卡）**应当**扩展
本表，而不是在绘制代码里临时分支。

### 5.2 已知缺口（非阻塞，在此追踪）

- **Security 没有快捷卡片。** 安全态势可经导航栏（`PageSecurity`）到达，并在仪表
  盘里有摘要，但一个一等的 Security 快捷卡片是推荐的未来新增（在调研里它是 P0
  面）。新增它属于对本目录的附加式变更。
- **电池/恢复卡片**尚未为 `Client-Mobile`/`Embedded` 实现；适用性表为它们预留了
  位置。

## 6. 合规与强制

- 可见快捷卡片数按平台类别**可变**。smoke **必须**断言*目录*数（数组长度）与
  *路由表*长度一致、且每个目录卡片都映射到合法的 `SETUP_PAGE`。smoke **禁止**
  断言固定的*可见*数量，因为它现在依类别而定。
- 卡片隐藏**必须**由数据/类别驱动（单一适用性谓词），而不是散落在
  `ModernSetupDrawDashboard` 里的逐卡 `if`。
- 每个卡片路由**必须**解析到真实页面；隐藏的卡片**禁止**可聚焦或可回车激活
  （键盘导航跳过隐藏卡片）。
- 本地化卡片文本**必须**只用内嵌 Noto Sans CJK SC 子集
  （`Library/ModernUiRendererLib/ModernUiGlyphs.c`）里有的字形；当某简体中文词
  未被覆盖，英文词作为优雅回退（见
  [LvglProductizationPlan.md](LvglProductizationPlan.md) 的 CJK 策略）。

## 7. XArch（按架构）说明

卡片*目录*与*适用性*是架构中立的 —— 同一 App 构建运行于 X64、AARCH64、
LOONGARCH64、RISCV64。架构只影响哪些 provider 上报实时数据：

- `服务器清单`通常在 x86/Arm 服务器显示；在 LoongArch/RISC-V 服务器原型上由
  provider 决定，在所有客户端/VM 目标上除非有 provider 实时数据否则隐藏。
- `电源/散热`与`性能`在 ACPI/SMBIOS/清单 provider 较薄的目标上退化为存在性/`N/A`
  （RISC-V/LoongArch VM 常见）。
- 没有卡片以写死的 `ARCH` 值门控；门控仅依 provider 实时性与平台类别。

## 8. 变更控制

对规范页面集（§3）或卡片目录（§5）的变更是用户可见的，**必须**记入
`CHANGELOG.md`，并在同一 PR 内同时反映到本规范与 smoke 守卫。中文镜像
（本文件）**必须**随英文源同步更新。
