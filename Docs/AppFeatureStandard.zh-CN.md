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

## 1. 范围与所有权边界

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

## 3. 规范页面集

App 的分类页面就是 `SETUP_PAGE` 枚举，且**必须**保持此顺序。新增页面属于附加式
API 变更（追加在 `PageMax` 之前）。

| 页面 | 用途 | App 显示 | 原生所有者 |
| --- | --- | --- | --- |
| `PageDashboard` | 一眼概览平台状态。 | 系统信息 + 平台健康面板、快捷分类网格。 | — |
| `PageBoot` | 启动清单 + 启动。 | `Boot####` 激活/隐藏/类别/路径；回车启动所选项。 | Boot Maintenance HII。 |
| `PageDevices` | 设备/HII 入口清单。 | HII formset、设备路径行、Driver Health。 | 各驱动 formset。 |
| `PageSecurity` | 安全态势。 | Secure Boot、Setup Mode、PK/KEK/db/dbx、TPM/TCG/TCM 存在性。 | SecurityPkg/平台 HII。 |
| `PageFirmware` | 固件生命周期。 | Capsule 支持、人性化固件版本、恢复/更新入口。 | Capsule/更新 HII 或 app。 |
| `PageDiagnostics` | 引导/服务可见性。 | ACPI/SMBIOS 存在性、内存映射/句柄/表计数、provider 健康。 | 平台诊断 HII。 |
| `PageManagement` | 服务器/远程管理。 | BMC/IPMI/Redfish 存在性、host interface。 | BMC/Redfish HII。 |
| `PagePower` | 电源/散热可见性。 | ACPI 状态、机箱热状态、电源记录存在性、演示健康趋势。 | 平台电源/散热 HII。 |
| `PagePerformance` | CPU/内存 + 调优入口。 | 处理器/内存清单、CPU I/O 协议、虚拟化/RAS 入口提示。 | 平台调优/RAS/PCIe HII。 |
| `PageServerInventory` | 服务器资产/管理汇总 + PCIe 策略提示。 | 管理 + PCIe 能力摘要；ReBAR/4G/SR-IOV 入口提示。 | 平台管理/PCIe HII。 |
| `PagePreferences` | App 本地 UX 偏好。 | 主题、密度、语言、OEM 水印开关。 | —（App 自有，无平台状态）。 |
| `PageExit` | 会话/shell 控制。 | 继续、重置、原生 UiApp 回退、语言。 | 原生 FormBrowser 保存/放弃。 |

PCIe 策略通过 `PageServerInventory`/`PagePerformance` 的入口提示呈现；它在 App
里**不**单独成为顶级页面，且 App **禁止**暴露可写的 PCIe 控制。

## 4. 仪表盘结构

仪表盘**必须**自上而下分三个区：

1. **系统信息面板** —— 只读身份/清单：固件厂商、人性化固件版本
   （`主.次 (0x十六进制)`）、平台、form factor、boot mode、内存、显示模式。
   解析为 `N/A`/`Unknown`/`Limited data` 的行**应当**折叠（行上浮），而不是
   显示成一条死占位。
2. **平台健康面板** —— 架构、provider 健康摘要、覆盖度、首个问题。横向空间足够
   时显示；否则系统面板占满整宽。
3. **快捷分类网格** —— §5 的标准化导航卡片。

已经出现在面板 1–2 里的状态**禁止**成为某个快捷卡片的*唯一*目的：每个快捷卡片
首先是导航入口（回车跳到对应页面），其次才是一行状态。

## 5. 标准化快捷分类卡片

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
