<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# 产品化验证矩阵

语言：[English](ProductizationValidationMatrix.md) | 简体中文

本矩阵是 Phase30 的 XArch 产品化证据清单。它记录当前可以通过仓库文档、smoke 测试、脚本和既有 provider 边界验证的内容。它不是功能承诺，也不是 edk2 构建架构抽象。

XArch 是 ModernSetupPkg 的跨架构验证/产品化术语。XArch 不会替代 edk2 ARCH 值。产品集成、DSC/FDF overlay、构建脚本和工具链仍然使用具体 edk2 ARCH 值：`ARCH=X64`、`ARCH=AARCH64`、`ARCH=LOONGARCH64` 和 `ARCH=RISCV64`。当前不存在受支持的 `ARCH=XArch` 或 `TARGET=XArch` 构建含义。

相关事实来源文档：

- [XArch.zh-CN.md](XArch.zh-CN.md)：目标映射和验证语言。
- [ProductizationFeatureMatrix.zh-CN.md](ProductizationFeatureMatrix.zh-CN.md)：功能归属与 App/provider 范围。
- [CompatibilityMatrix.md](CompatibilityMatrix.md)：DisplayEngine/FormBrowser 证据。
- [Tests/Smoke/README.md](../Tests/Smoke/README.md)：主机端 smoke gate。

## 证据语言

以下验证术语只描述当前证据：

- `Smoke`：主机端 `Tests/Smoke/smoke_validate.py` 静态检查和 overlay dry-run 检查。
- `Script`：仓库脚本存在，并被语法/元数据检查覆盖。
- `Manual`：本地维护者验证路径已有文档，但不是 CI gate。
- `Captured`：相关路径有截图或 screendump 证据。
- `Visual reviewed`：维护者已检查 native-vs-modern 截图；不要将此术语用于 static smoke、仅构建或仅 QEMU 启动结果。
- `Build/script validation`：脚本或 overlay 路径已验证，但不声明图形运行证据。
- `Planned`：仅为规划或已记录目标，不应描述成已验证。

## XArch 目标验证矩阵

| XArch 目标 | 具体 edk2 ARCH | 平台路径 | 主要脚本 | 当前成熟度证据 | 产品化验证说明 |
| --- | --- | --- | --- | --- | --- |
| X64 / OVMF X64 | `X64` | `OvmfPkg/OvmfPkgX64` | `Scripts/build-ovmf-x64.sh`, `Scripts/run-ovmf-x64.sh`, `Scripts/capture-ovmf-x64.sh`, `Scripts/capture-displayengine-ovmf-x64.sh` | Manual OVMF 构建/运行/捕获路径；smoke overlay generation；本地/手动 App 验证；Phase35 native-vs-modern DisplayEngine 证据路径待 visual review。 | 证据覆盖目标元数据、native/modern DisplayEngine overlay 分离和本地截图捕获路径。DisplayEngine A/B helper 默认输出到 `${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64`，只有 `--mode capture` 产出 artifact 后才属于截图证据。 |
| AARCH64 / ArmVirtQemu | `AARCH64` | `ArmVirtPkg/ArmVirtQemu` | `Scripts/build-armvirt.sh`, `Scripts/run-armvirt.sh`, `Scripts/capture-armvirt.sh`, `Scripts/build-modern-app.sh` | Captured ArmVirt before/after 证据；active 构建/运行路径；smoke overlay generation。 | native UiApp/FormBrowser 加 ModernDisplayEngine 的主要兼容性捕获路径。 |
| LOONGARCH64 / LoongArchVirtQemu | `LOONGARCH64` | `OvmfPkg/LoongArchVirt/LoongArchVirtQemu` | `Scripts/build-loongarchvirt.sh`, `Scripts/run-loongarchvirt.sh` | Active 构建/运行脚本路径；smoke overlay generation。 | 证据覆盖生成 overlay 和已记录的手动运行路径；外部交叉工具链仍由产品团队负责。 |
| RISCV64 / RiscVVirtQemu | `RISCV64` | `OvmfPkg/RiscVVirt/RiscVVirtQemu` | `Scripts/build-riscvvirt.sh` | Build/script validation；smoke overlay generation。 | Phase30 中 RISCV64 仍保持 Build/script validation；不声明图形 QEMU helper 或捕获 UI 证据。 |

`Scripts/xarch-validate.sh --all --mode dry-run --format json` 是快速目标元数据 smoke 辅助检查。Phase30 smoke gate 会断言四个目标均为 `PASS`，并保持 RISCV64 的 `Build/script validation` 成熟度用语。

## Phase35 DisplayEngine 视觉证据路径

`Tests/Manual/DisplayEngineOvmfX64Visual.md` 记录 OVMF X64 native-vs-modern DisplayEngine 视觉工作流。`Scripts/capture-displayengine-ovmf-x64.sh` 使用 `MODERN_SETUP_DISPLAY_ENGINE=native` 和 `MODERN_SETUP_DISPLAY_ENGINE=modern` 两次驱动既有 OVMF overlay 生成器，将 artifact 分离到 `overlays/native`、`overlays/modern`、`firmware/native`、`firmware/modern` 以及可选的 `native`/`modern` capture 目录，并只在 `Build/ModernSetupPkgOverlay` 下写 overlay，保持 upstream edk2 平台文件不被修改。

本矩阵中的 Phase35 当前状态仅为 `Script`/`Manual` foundation。Static smoke 可检查 helper 和手动工作流存在；`--mode generate-only` 可检查 overlay snapshot；`--mode build` 可检查 firmware FD snapshot；只有 `--mode capture` 成功产出 QEMU `screendump` 后才形成视觉截图证据，并且该 helper 不检查像素，也不会将视觉等价标记为 verified。

## Phase32 响应式页面布局矩阵

Phase32（`ModernSetupGetPageListLayout`，`Application/ModernSetupApp/ModernSetupAppActions.c`，已在 `038a156` 落地）让 Boot/Devices/provider 摘要页的列表行高、padding、可见行上限以及 Devices 预览分栏跟随 app 自有的 `DashboardDensity` 偏好和当前内容矩形；绘制与键盘行数共用同一 helper，smoke 固化其 compact/comfortable 分支。

分辨率下限（适用于下表每一行）：`SelectPreferredGopMode`（`Library/ModernUiRendererLib/ModernUiRendererLib.c`，`MODERN_UI_TARGET_WIDTH` 1024、`MODERN_UI_TARGET_HEIGHT` 768）在当前 GOP 模式已 `>=1024x768` 时保持不变，否则升到满足下限的最小合格模式，因此只要存在合格模式，800x600 这类 < 1024 的模式 App 不会用到。这覆盖了最初 800x600 / 1024x768 / 1280x800 的设想：App 不会在 1024x768 下限之下渲染设置页。

| 页面 | 受测的 helper 驱动布局 | 捕获分辨率（OVMF X64） | 证据 | 结果 |
| --- | --- | --- | --- | --- |
| Boot | 密度行、可见行上限、右侧值列、原生 boot-tools 行 | 1280x800（固件 GOP 默认，达下限） | `Captured` | 行无截断；串口日志无 `Exception`/`#PF`/`ASSERT`。 |
| Devices | 密度行加 `>=720` 宽度的 native-setup 预览分栏 | 1280x800 | `Captured` | 左列表与预览栏均渲染；无缺字方块、无值列重叠。 |
| Firmware（provider 摘要） | 只读 provider 摘要的密度行 | 1280x800 | `Captured` | 本地化 zh 标签与 `N/A`/只读状态渲染干净。 |

经 `Scripts/capture-ovmf-x64.sh`（`BOOT_APP=1` 加 tab `SENDKEY_SEQUENCE`）在重建当前 `main` HEAD 的 App ESP 后捕获；作为「仅 modern App」产物审阅，**不是** native-vs-modern 的 maintainer `Visual reviewed` 签署。截图默认输出到 `${TMPDIR:-/tmp}/modernsetup-qemu`，不作为资产提交。

## 产品类别验证矩阵

| 产品类别 | 有证据支持的 App 角色 | 原生 owner / 边界 | 当前验证证据 |
| --- | --- | --- | --- |
| Desktop / workstation | Dashboard、Boot、Devices/HII、Security、Firmware、Diagnostics、Power/Thermal、Performance、PCIe capability summary、Preferences、Exit。 | Boot order 编辑、Secure Boot 密钥管理、CPU/内存调优、风扇策略、PCIe 策略和芯片组控制保持 native HII/FormBrowser 所有。 | OVMF X64 和 ArmVirt 路径记录桌面/工作站风格检查；smoke 验证边界 token。 |
| Server | Dashboard provider health、Management、Diagnostics、Performance、Hardware Health demo 可视化、PCIe inventory/policy-entry hints、Exit/native entries。 | BMC/IPMI/Redfish 配置、RAS/NUMA 策略、PCIe resource policy、ACS/ARI/IOMMU 策略、SEL/log clearing 保持 native 或 service app 所有。 | Provider snapshot、server inventory、management、performance、PCIe 和 diagnostics 边界由 smoke 检查。 |
| Embedded / industrial | Device/HII 入口列表、boot/recovery 姿态、firmware update 状态、diagnostics 证据、安全姿态。 | GPIO、serial、watchdog、provisioning、boot-pin behavior、board muxes 和 power-restore policy 保持平台 HII/native。 | ArmVirt、LoongArchVirt 和 RiscVVirt 脚本路径提供跨架构元数据证据；产品细节需要平台验证。 |
| Tablet / appliance | 最小 Dashboard、Boot/recovery 入口、Security、Firmware、Power/Thermal、Preferences、Exit。 | Battery/adapter policy、display panel/backlight、thermal trip points、recovery writes 和 appliance provisioning 保持 native/platform 所有。 | App/provider 文档定义只读状态和入口行为；不声明产品特定运行证据。 |

## App / Provider 验证矩阵

| 区域 | 有证据支持的 App/provider 行为 | 必须保持的边界 token | Phase30 验证证据 |
| --- | --- | --- | --- |
| Dashboard | 显示 provider-backed platform、boot、device、firmware、power/thermal、performance 和 provider-health summary。 | Dashboard 消费 `ModernSetupAppProvider.c` snapshot；不直接从展示模块调用 provider LibraryClasses。 | Smoke 检查 Dashboard 绘制归属、quick-card count、provider snapshot 使用和 density layout。 |
| Boot | 列出 boot inventory，并在可用时启动选中的 boot option。 | Boot policy editing 和 Boot Maintenance 保持 native；无 App-owned platform policy writes。 | Smoke 覆盖 App source boundary；feature matrix 记录 display/entry 行为。 |
| Devices / HII | 列出 HII/device entries，并通过 `EFI_FORM_BROWSER2_PROTOCOL.SendForm()` 打开真实 setup 页面。 | Native `FormBrowser2`、native HII、VFR/IFR 解析、`ConfigAccess`、callbacks、validation、defaults 和 varstore writes 拥有语义。ModernSetupApp 不得解析 IFR、不得实现 ConfigAccess、不得写 HII varstores、不得写 platform policy。 | Smoke 检查 HII bridge read-only preview 边界和 App source 禁止 token。 |
| Security | 显示只读 Secure Boot、Setup Mode、key presence 和可发现的 TPM/TCG/TCM 姿态。 | Key enrollment、password、physical-presence、measured-boot policy 和 chassis security policy 保持 native。 | Smoke 验证 provider snapshot boundary 和 App mutation-token 排除。 |
| Firmware | 显示 capsule/update/recovery availability 和 native entry hints。 | Capsule construction、flash programming、rollback policy 和 recovery writes 保持 native/platform utility 所有。 | Feature 和 validation 文档仅使用证据语言。 |
| Diagnostics | 显示 table/log/provider-health summaries 和 service/debug evidence。 | POST log management、error clearing、vendor diagnostics 和 repair flows 保持 native 或 service app 所有。 | Smoke 检查 provider health derivation 和 diagnostics inclusion。 |
| Management | 显示 BMC/IPMI/Redfish presence 和 management host hints。 | BMC networking、users、KVM/media、SEL policy 和 remote update configuration 保持 BMC/native 所有。 | Smoke 检查 provider snapshot boundary 和 server inventory summary。 |
| Power / Thermal | 显示 ACPI/chassis/power-supply status，并可展示 demo Hardware Health curves。 | Fan curves、pump headers、thermal trip points、acoustic profiles、battery 和 adapter policy 保持 native HII/FormBrowser-owned。 | Smoke 检查 Power provider 经 App boundary 的 wiring。 |
| Hardware Health | demo-only/read-only provider，用于确定性的 temperature trend UX。 | Hardware Health demo provider 不声明真实传感器，不编程 SMBus、I2C、IPMI、SuperIO、MMIO、PCI、fan 或 trip-point policy。 | Smoke 检查 demo provider 文件、demo 文案、只读文档和禁止硬件/mutation token。 |
| Performance | 显示 CPU/memory inventory 和 tuning/RAS entry availability。 | CPU frequency/voltage、memory timing/profile、NUMA/RAS 和 workload profile policy 保持 native。 | Smoke 检查 provider snapshot 使用和 server inventory summary。 |
| PCIe | 显示 PCIe inventory，以及 ReBAR、Above 4G、SR-IOV、ASPM、bifurcation、hot-plug、ACS、ARI、IOMMU 的只读 capability/native policy-entry hints。 | ReBAR、Above 4G、SR-IOV、ASPM、bifurcation、hot-plug、ACS、ARI、IOMMU、BAR/resource allocation 和 fabric policy changes 保持 native HII/FormBrowser-owned。 | Smoke 检查 PCIe provider wiring、mutation-token 排除和文档语言。 |
| Preferences | App-owned UX preferences 通过 `ModernUiPreferencesLib`。 | App-owned preferences 不是 platform policy；BootOrder、SecureBoot、CPU、fan、chipset 和 PCIe policy 等平台变量不得进入 preferences library。 | Smoke 检查 `ModernUiPreferencesLib`、App 使用、schema/version fields 和无 runtime variable access。 |
| Exit | 提供 session actions、app/version info、language/theme preference access，以及可用时的 native UiApp/native setup entries。 | 真实 setup variables 的 save/discard/defaults 工作流保持 native FormBrowser/platform HII。 | Smoke 检查 App source boundary 和 preference routing。 |

## Phase30 Smoke Gate 要求

Smoke gate 必须保持快速且确定性。Productization Validation 会检查：

- 英文和 zh-CN 验证矩阵文件存在且互相链接。
- X64、AARCH64、LOONGARCH64 和 RISCV64 均带有平台路径和脚本。
- XArch 不会替代 edk2 ARCH 值，并且不存在 `ARCH=XArch` 或 `TARGET=XArch` 构建含义。
- ModernSetupApp 边界：不解析 IFR、不实现 ConfigAccess、不写 HII varstore、不写 platform policy。
- 已记录 `EFI_FORM_BROWSER2_PROTOCOL.SendForm()`、FormBrowser2/native HII/ConfigAccess ownership 和 native semantics。
- Hardware Health 保持 demo-only/read-only。
- App-owned preferences 通过 `ModernUiPreferencesLib`。
- PCIe policy tokens 保持 native-owned：ReBAR、Above 4G、SR-IOV、ASPM、bifurcation、hot-plug、ACS、ARI 和 IOMMU。
- `Scripts/xarch-validate.sh --all --mode dry-run --format json` 报告四个 target `PASS`，并保持 RISCV64 为 `Build/script validation`。
