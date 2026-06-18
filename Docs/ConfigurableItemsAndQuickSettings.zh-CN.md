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

不是每个 PCD 都能从界面改。决定权在机制，不在意愿。

| PCD 类型 | 运行时可改？ | 界面可配置？ |
| --- | --- | --- |
| `FixedAtBuild` / `PatchableInModule` / `FeatureFlag` | 否（编译期） | **不能**——需重编/构建开关，任何界面都管不了 |
| `Dynamic` / `DynamicEx` | 是（PCD 数据库/HOB） | 仅当平台把它接到 HII 问题上 |
| **`DynamicHii`** | 是——**绑定到 NV 变量** | **是——这才是"界面可配置"的真正载体** |

结论：edk2 里用户可配置的面 = **绑定 NV varstore 的 HII 问题**（常以 `DynamicHii` PCD 暴露）。
真实平台上 SR-IOV / Above-4G / IOMMU / SATA 模式通常正是这种由芯片/平台代码用 VFR 写的问题。
若某项是 `FixedAtBuild`，再好的界面也改不了。

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
| **B —— 策划式快捷设置深链** | 定位已知高频 HII 问题，分组在现代页呈现；激活时 `SendForm()` 跳进归属 formset/form/问题。**编辑仍原生；ModernDisplayEngine 渲染。** | 原生 FormBrowser + 平台 ConfigAccess | Secure Boot、TPM 使能、VT-d/IOMMU、SR-IOV、Above-4G、ReBAR、SATA 模式、主显选择、WoL、掉电恢复、Fast Boot、TCM |
| **C —— 整页原生** | `SendForm()` 打开整张 formset | 原生 FormBrowser + 平台 ConfigAccess | RAS、NUMA、内存时序、CPU 电压、BMC 网络、多问题联动流程 |

B 档是新增的产品化工作。它**不放松**边界：新增代码只有*发现 + 分组 + 深链 `SendForm` 目标*。
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
| 自有/品牌 | 语言、EZ/Advanced、收藏夹、主题、日期时间 | 全有 | 全有 | 高 | **A** |

## 5. 快捷设置（B 档）设计

一个新的现代页（`PageQuickSettings`，可选开启）：

1. **发现**：用现有关键字探测（provider 里 `HasHiiFormsetKeyword` 那套）在已装 HII formset 中
   定位已知高频问题，产出只读的 *(分组、标签、归属 formset/form/问题坐标、是否存在)* 列表。
   不改 IFR、不碰 ConfigAccess。
2. **分组**（安全 / 虚拟化 / PCIe / 电源 / 启动）：每项渲染为现代行；能只读读到当前值时
   显示当前值，否则显示"配置 ›"。
3. **激活**：调 `EFI_FORM_BROWSER2_PROTOCOL.SendForm()` 跳到归属 formset（可选跳到具体
   `FormId`/`QuestionId`）。原生浏览器完成编辑，`ModernDisplayEngineDxe` 渲染成现代外观。

约束（带进评审与 smoke）：

- 发现 provider 是**只读**的：可读 HII 字符串/formset 元数据；**禁止**
  `ExtractConfig`/`RouteConfig`/`SetVariable`/`HiiSetBrowserData`，
  也**禁止**用 `EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL` 的 set 路径。
- 快捷设置行是**入口**，不是编辑器。App 唯一写的状态是它自己的收藏/排序（A 档）。
- 归属问题不存在的项隐藏（优雅降级），绝不臆造。

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
