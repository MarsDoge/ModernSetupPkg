<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# 可配置项与快捷设置（规范）

语言：[English](ConfigurableItemsAndQuickSettings.md) | 简体中文

本文档定义 **ModernSetupPkg 在现代壳里"把平台配置做成可配置"能做到什么程度、怎么做**，
并调研 IBV 与其他平台 BIOS 中的高频可配置项，把每一类绑定到一个受控的处理档位。

配套文档：

- [IbvAndPlatformSetupSurvey.md](IbvAndPlatformSetupSurvey.zh-CN.md) —— 更广的设置面分类。
- [ProviderDataContract.md](ProviderDataContract.zh-CN.md) —— 只读数据源契约。
- [MODULE_BOUNDARIES.md](MODULE_BOUNDARIES.zh-CN.md) —— 分层契约。

> **一句话铁律。** 平台策略的"编辑"**只**发生在原生 FormBrowser、经各驱动的 ConfigAccess；
> 现代壳负责**策划、深链与渲染**，自己**绝不**写 varstore。"现代界面可配置 SR-IOV"指的是
> *一个策划入口，打开平台真正的 SR-IOV 问题、由 ModernDisplayEngine 渲染*——而不是
> `ModernSetupApp` 去写变量。

## 1. edk2 里到底什么可配置

PCD 不是可配置项的完整清单。设置必须有真实的平台 owner 与消费方；
PCD 类型本身不会提供界面。

| PCD 类型 | 运行时可改？ | 界面可配置？ |
| --- | --- | --- |
| `FixedAtBuild` / `FeatureFlag` | 否（构建期） | 不是运行时 Setup 控件 |
| `PatchableInModule` | 模块内可修补值 | 不自动成为持久化 Setup 控件；需要明确的平台契约 |
| `Dynamic` / `DynamicEx` | 是（PCD 数据库/HOB） | 仅当平台把它接到 HII 问题上 |
| `DynamicHii` / `DynamicExHii` | HII 支持的变量绑定 | 只是存储绑定方式之一，不是唯一可配置来源，也不证明存在可编辑问题 |

原生 HII 问题可以使用 buffer、name/value、EFI-variable varstore、回调或平台服务，
不需要 PCD 绑定。易失状态与动作不一定有 NV 存储。SR-IOV / Above-4G / IOMMU /
SATA 模式必须依据实际平台的 VFR/ConfigAccess 契约，不能假设通用布局。
系统日期时间是平台 RTC 状态（`GetTime` / `SetTime`），不是 App 自有偏好；
任何编辑入口必须走其原生 owner（B/C 档）。

## 2. 不可逾越的边界（smoke 强制）

`ModernSetupApp` **禁止**解析 IFR、调用 ConfigAccess、写 varstore。
`Tests/Smoke/smoke_validate.py` 一旦在 app 源码里发现 `ExtractConfig`、`RouteConfig`、
`SetVariable`、`HiiSetBrowserData` 即 fail。PCIe 策略
（ReBAR / Above-4G / SR-IOV / ASPM / bifurcation / hot-plug / ACS / ARI / IOMMU / BAR）归原生。

为什么直写是错的（而非仅"不允许"）：直接写裸变量会绕过平台 ConfigAccess 回调——跨问题的
`suppressif`/`grayoutif`/`disableif` 逻辑、范围/一致性校验、默认值处理、交互式告警/重配。
不走这条链就把"SR-IOV 使能"那个字节写下去，可能悄无声息地配错甚至变砖。依赖图归平台所有，
现代壳不拥有它。

## 3. 处理档位

每个可配置项只归一个档位。

| 档位 | 壳做什么 | 谁来写 | 例子 |
| --- | --- | --- | --- |
| **A —— App 自有直编** | 壳读写自己的状态 | `ModernSetupApp`（自有存储） | 语言、主题、EZ/Advanced、收藏夹、App 偏好（`ModernUiPreferencesLib`） |
| **B —— 策划式快捷设置深链** | 定位已注册高频 HII owner，分组在现代页呈现；激活时 `SendForm()` 跳进归属 formset/form（问题聚焦仅限已验证机制；§5）。**编辑仍原生；ModernDisplayEngine 渲染。** | 原生 FormBrowser + 平台 ConfigAccess | Secure Boot、TPM 使能、VT-d/IOMMU、SR-IOV、Above-4G、ReBAR、SATA 模式、主显选择、WoL、掉电恢复、Fast Boot、TCM |
| **C —— 整页原生** | `SendForm()` 打开整张 formset | 原生 FormBrowser + 平台 ConfigAccess | RAS、NUMA、内存时序、CPU 电压、BMC 网络、多问题联动流程 |

B 档定义产品化方向，不代表清单中每项均已实现。它**不放松**边界：壳的职责是
*发现 + 分组 + 已注册的原生目标*。
现代"开关"的外观来自 DisplayEngine 渲染平台已有的 checkbox/oneof 问题，而非 App 拥有那个值。

## 4. 可配置项清单

变更频率 = 终端用户改它的频繁度。档位见 §3。

| 域 | 高频项 | IBV 固件 | 其他平台 | 频率 | 档位 |
| --- | --- | --- | --- | --- | --- |
| 启动 | 启动顺序、Fast Boot、CSM/Legacy(x86)、PXE/HTTP boot、超时 | 全有 | 多有(非 x86 平台通常无 CSM) | 高 | B/C |
| 安全 | Secure Boot、TPM/PTT/fTPM 使能、清 TPM、密码 | 全有 | **+ TCM(可信计算)**、安全启动证书 | 高 | B/C |
| 虚拟化/隔离 | VT-x/SVM、**VT-d/IOMMU/SMMU**、**SR-IOV**、ACS/ARI/PASID | 全有 | 服务器平台;部分 Arm 暴露 SMMU | 中-高 | **B/C** |
| PCIe 资源 | **Above-4G、ReBAR、ASPM、链路速率、bifurcation、hot-plug** | 全有 | 服务器侧 | 中-高 | B/C |
| CPU | SMT、C-states、Turbo/Boost、P-state/CPPC、核数开关 | 全有 | 服务器平台;其余部分 | 中 | C |
| 内存 | XMP/EXPO、频率、**ECC、patrol scrub、NUMA/SNC/NPS、interleave** | 桌面 XMP；服务器 RAS | 服务器 RAS/NUMA | 中 | C |
| 存储 | **SATA 模式(AHCI/RAID)**、VMD、NVMe RAID、Opal | x86 全有 | 平台相关 | 中 | B/C |
| 显示 | 主显选择(iGPU/dGPU/Auto)、UMA 显存、hybrid/mux | 桌面/笔电 | 集显平台 | 中 | B/C |
| 电源 | **ErP/Deep S5、Wake-on-LAN、掉电恢复**、RTC 唤醒;(笔电)充电阈值/合盖 | 全有 | 多有 | 高 | B/C |
| 散热 | 风扇模式(静音/标准/性能)、风扇曲线 | 桌面/服务器 | 服务器/工控 | 中 | C |
| 网络 | 板载网卡使能、WoL、网络栈、MAC 透传 | 全有 | 多有 | 中 | B/C |
| 管理(服务器) | BMC 网络(DHCP/静态)、IPMI over LAN、Redfish 使能 | 服务器 | 服务器平台 | 中 | C |
| 可信计算 | **可信计算模块(TCM)、兼容模式、内核完整性度量** | — | 部分平台 | 中 | B/C |
| 系统时钟 | 日期时间（平台 RTC，非 App 偏好） | 全有 | 全有 | 高 | B/C |
| 自有/品牌 | 语言、EZ/Advanced、收藏夹、主题 | 全有 | 全有 | 高 | **A** |

## 5. 快捷设置（B 档）设计

以下为 `PageQuickSettings` 与通用设置目录的目标契约。本次文档变更不实现目录驱动
UI、owner 注册、变量访问或新编辑后端。已有个别原生入口不代表整个清单已覆盖。

1. **精确注册解析**：将已知设置的注册信息与已安装 HII owner 匹配：formset GUID、
   受支持且有文档依据的 form/question 标识，必要时加入设备路径/实例判别条件。
   禁止用翻译标题、关键词、协议存在性或相似标签选择 owner。
   缺失、歧义或过期注册保持不可用，不猜测目标。
2. **分组**（安全 / 虚拟化 / PCIe / 电源 / 启动）：每项渲染为现代行；能只读读到当前值时
   显示当前值，否则显示 `N/A`，附原因并单独显示入口状态。
3. **激活**：调 `EFI_FORM_BROWSER2_PROTOCOL.SendForm()` 跳到已注册归属 formset 与
   受支持的 `FormId`。`SendForm()` 没有 `QuestionId` 参数；问题级聚焦需要另行验证
   的原生导航机制，否则打开已注册表单。原生浏览器完成编辑，
   `ModernDisplayEngineDxe` 渲染成现代外观。

约束（带进评审与 smoke）：

- 发现 provider 是**只读**的：可读 HII 字符串/formset 元数据；**禁止**
  `ExtractConfig`/`RouteConfig`/`SetVariable`/`HiiSetBrowserData`，
  也**禁止**用 `EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL` 的 set 路径。
- 快捷设置行是**入口**，不是编辑器。App 唯一写的状态是它自己的收藏/排序（A 档）。
- 目录项缺少 owner/问题/绑定时，必须保留可见并以 `N/A` 表示不可用，禁止编辑和提交，
  但仍可选中查看帮助及缺源/缺 owner 原因。可选择不等于可编辑。
  独立验证的只读值可以与不可用的入口状态同时保留显示。
- 真实 `Disabled` 或 `0` 是有效数据，不是 `N/A`；值有效性、owner 可用性、
  可编辑性、选择/帮助状态必须分开。
- 缺失保留规则禁止绕过平台安全 suppression、授权、`suppressif`、`grayoutif` 或
  `disableif`。原生策略可以隐藏/限制问题；目录不得泄露受保护细节或强行深链。

### 通用目录与变量绑定契约（目标）

每个目录项需要稳定 ID、域/分类、本地化标签/帮助、值类型/选项/单位、只读来源与有效性、
精确原生 owner 注册、不可用原因，以及独立声明的选择/编辑/提交能力。
目录收录不代表平台已经实现该项。

绑定按平台与版本区分，依据平台源码或明确的 owner 契约，并记录出处。
记录存储种类，以及适用且已验证的变量名/GUID、属性、布局版本、offset/width 或
name/value 键、编码、范围/选项、默认值、依赖、重置要求和实际消费方。
服务/回调动作需要自己的契约，不能臆造变量坐标。未知 GUID/offset 必须保持未绑定；
禁止按标签猜测或照搬另一块板的布局。

目录元数据不是写入授权。provider 保持只读；平台编辑/提交仍走原生
FormBrowser/ConfigAccess 及 owner 的服务路径。不可用项不得进入待提交变更集或显示已保存。
原生提交成功、读回确认、重置/重启后生效是不同状态；持久化与效果声明必须验证真实
owner 和消费方。

### 范围外（需单独架构评审）

不重进 FormBrowser 的"内联单问题直编"——无论经 `EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL`
（x-UEFI 关键字 Get/SetData）还是直接驱动 ConfigAccess——**明确排除在外**。它绕过 FormBrowser 的
`suppressif`/`grayoutif`、校验、默认值与交互回调，当前被 smoke 拦截，需要它自己的设计评审
（一个"FormBrowser 背书的内联问题宿主"，仍要驱动完整校验链）。

## 6. 参考

- UEFI PI/UEFI 规范 —— HII、ConfigAccess、ConfigKeywordHandler、FormBrowser2。
- edk2 `MdeModulePkg` —— `SetupBrowserDxe`、`DisplayEngine`、PCD 数据库、HII。
- 主流 IBV 设置参考（仅视觉/信息架构；
  见 IbvAndPlatformSetupSurvey）。
- 其他平台 UEFI 设置参考;可信计算（TPM/TCM）文档。
