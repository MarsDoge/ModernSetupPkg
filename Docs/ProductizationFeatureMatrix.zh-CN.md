<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# 产品化功能矩阵

语言：[English](ProductizationFeatureMatrix.md) | 简体中文

另请参阅：[ProductizationValidationMatrix.zh-CN.md](ProductizationValidationMatrix.zh-CN.md)，其中记录 Phase30 基于证据的验证矩阵和 smoke gate 契约。

ModernSetup 有两层 XArch 产品化边界：

```text
标准首页 App
  -> 通用 dashboard、inventory、boot、security、diagnostics 和入口

原生 edk2 FormBrowser 路径
  -> 平台/OEM HII 页面、回调、校验、varstore 和策略
```

`ModernSetupApp` 应为 x86/X64、ARM/AARCH64、RISC-V/RISCV64 和 LoongArch/LOONGARCH64 产品提供一致的第一屏。它不能克隆平台 Setup 策略或解析 IFR；平台专有设置应显示摘要或入口，然后通过 `EFI_FORM_BROWSER2_PROTOCOL.SendForm()` 打开归属 HII 表单。

## 平台类型

| 平台类型 | App 目标 | 示例 |
| --- | --- | --- |
| Desktop / workstation | 快速展示 boot、安全、设备、固件更新和诊断概览。 | OVMF/x86、ARM workstation、LoongArch desktop。 |
| Server | 展示平台 inventory、management、RAS、boot policy、安全态势和远程生命周期入口。 | x86 server、Arm server、LoongArch server、RISC-V server prototypes。 |
| Embedded / industrial | 展示设备 inventory、boot source、安全状态、更新状态和恢复入口。 | ARM/LoongArch/RISC-V boards。 |
| Tablet / appliance | 精简 dashboard、boot/recovery、安全、固件更新和输入/显示状态。 | ARM appliance-class products。 |

## 标准 App 页面

| 页面 | 通用目的 | App 应显示 | 复杂设置归属 | 当前状态 |
| --- | --- | --- | --- | --- |
| Dashboard | 第一眼平台状态。 | firmware vendor/revision、architecture、form factor、boot mode、platform name、memory、display mode、boot count、Secure Boot、HII/device count、provider availability。 | App data providers。 | Basic implemented。 |
| Boot | Boot 清单和启动入口。 | `BootOrder`、`Boot####`、active/hidden state、category、device-path summary、launch selected option。 | Boot Maintenance HII pages。 | Basic implemented。 |
| Devices / HII | 平台 Setup 页面和设备 inventory 入口。 | HII formsets、driver/device path rows、Driver Health entry、inventory rows。 | Each driver formset via FormBrowser2。 | Basic implemented。 |
| Security | 只读安全态势。 | Secure Boot、Setup Mode、PK/KEK/db/dbx、TPM/TCG/TCM presence。 | Security HII pages and platform policy drivers。 | Basic implemented。 |
| Firmware Update | 固件生命周期入口。 | Capsule support、firmware version、recovery/update entry、last update state。 | Capsule/update HII or platform update app。 | Basic read-only implemented。 |
| Diagnostics / Logs | Bring-up 和服务可见性。 | POST/log summary、ACPI/SMBIOS、memory map、test hooks。 | Platform diagnostics HII or service app。 | Basic read-only implemented。 |
| Management | 服务器/远程管理摘要。 | BMC/IPMI/Redfish、management NIC、host interface、remote update support。 | BMC/IPMI/Redfish platform drivers。 | Basic read-only implemented。 |
| Power / Thermal | 电源和散热可见性。 | ACPI state、chassis thermal state、power supply record、demo Hardware Health trend。 | Platform fan/battery/thermal/power-policy HII。 | Basic read-only; Hardware Health 为 demo data。 |
| Performance / Tuning | CPU/memory 和 tuning 入口可见性。 | Processor inventory、memory inventory、CPU I/O protocol、virtualization/RAS entry availability。 | Platform performance/overclocking/NUMA/RAS/PCIe policy HII。 | Basic read-only implemented。 |
| PCIe Policy | PCIe inventory 和策略入口提示。 | controller/root-bridge/endpoint counts、protocol presence、ReBAR、Above 4G、SR-IOV、ASPM、bifurcation、hot-plug、ACS/ARI、IOMMU hints。 | Platform PCIe policy HII and native FormBrowser pages。 | Basic read-only foundation implemented。 |
| Exit | 会话和 shell 控制。 | Continue、reset、native UiApp、language、theme、app/version info。 | Native FormBrowser save/discard。 | Basic implemented。 |

## 广义 Setup 分类到 App IA / Provider 映射

`ModernSetupApp` 将广义 IBV/平台 Setup 表面映射到标准分类落地页和只读 provider 摘要。它不是第二个 Setup 策略引擎：CPU 频率/电压、内存时序/Profile、Chipset/SoC、风扇曲线、PCIe 资源策略等主板策略仍由原生 HII/FormBrowser 拥有，除非未来经过 allowlist 设计评审。

| 领域 / 子系统 | 标准 App IA | Provider / 数据方向 | App 行为 | 原生归属 / 非目标 |
| --- | --- | --- | --- | --- |
| System overview / Main | Dashboard；Exit 语言/会话入口。 | `ModernUiPlatformDataLib`、UEFI system table、SMBIOS。 | 显示固件、架构、形态、平台名、内存/显示摘要。 | date/time、password、defaults、save/discard 保持原生。 |
| Boot manager / boot policy | Boot；Dashboard 快捷分类。 | `ModernUiBootDataLib`、Boot#### variables、Boot Manager services。 | 列出 boot entries、状态、分类、路径摘要并启动选中项。 | boot order 编辑、one-time boot、fast boot、PXE/HTTP policy 保持原生。 |
| Device / HII formset inventory | Devices / HII。 | `ModernUiDeviceDataLib`、HII database、device paths、Driver Health。 | 列出原生 setup/device entries 并用 `EFI_FORM_BROWSER2_PROTOCOL.SendForm()` 打开。 | App 不解析 IFR、不实现 ConfigAccess、不写 varstore。 |
| Security / identity | Security summary。 | `ModernUiSecurityDataLib`、Secure Boot variables、TCG/TCM/TPM probes。 | 显示 Secure Boot、Setup Mode、key presence、TPM/TCM availability。 | key enrollment、password、physical presence、measured boot policy 保持原生。 |
| Firmware update / recovery | Firmware Update。 | `ModernUiFirmwareDataLib`、capsule probes、platform update hints。 | 显示 update/recovery 可用性并路由到原生 updater。 | 不构造 capsule、不刷写 flash、不写 recovery policy。 |
| Diagnostics / logs | Diagnostics / Logs。 | `ModernUiDiagnosticsDataLib`、ACPI/SMBIOS/config table/handle/memory map。 | 展示服务/debug 证据和 provider health。 | POST log 管理、错误清除、厂商诊断保持原生/服务 App。 |
| Management / BMC | Management。 | `ModernUiManagementDataLib`、IPMI/Redfish/SMBIOS Type 38/42。 | 显示 BMC/IPMI/Redfish 存在性并打开原生管理页。 | BMC 网络、用户、KVM/media、SEL policy 保持 BMC/原生所有。 |
| Power / Thermal | Power / Thermal。 | `ModernUiPowerDataLib` 和 demo `ModernUiHardwareHealthDataLib`。 | 显示 power/thermal 能力和 demo trend，不声称真实平台读数。 | fan curves、trip points、battery/power policy 保持原生。 |
| Performance / Tuning | Performance / Tuning。 | `ModernUiPerformanceDataLib`。 | 显示 CPU/memory inventory 和 tuning/RAS 入口可用性。 | CPU multiplier、voltage、memory timing、NUMA/RAS 策略保持原生。 |
| PCIe resource / fabric policy | PCIe Policy。 | `ModernUiPcieDataLib`。 | 显示 PCIe inventory 和 ReBAR/Above 4G/SR-IOV/ASPM 等只读提示。 | 所有资源分配和策略修改保持原生 PCIe HII/FormBrowser。 |

## 产品形态能力矩阵

`Display` 表示 App 可直接显示只读状态；`Entry` 表示 App 应提供原生 FormBrowser/HII 入口；`Native` 表示功能由平台 HII 拥有，不应在 App 中实现。

| 能力 | Desktop/workstation | Laptop/2-in-1 | AIO/NUC/mini PC | Server | Embedded/tablet |
| --- | --- | --- | --- | --- | --- |
| Firmware and platform summary | Display | Display | Display | Display | Display |
| Boot inventory and launch | Display | Display | Display | Display | Display |
| Boot order editing | Entry | Entry | Entry | Entry | Entry |
| Device and HII entries | Display + Entry | Display + Entry | Display + Entry | Display + Entry | Display + Entry |
| Secure Boot and TPM posture | Display + Entry | Display + Entry | Display + Entry | Display + Entry | Display + Entry |
| Key management and TPM physical presence | Native | Native | Native | Native | Native |
| Capsule/update/recovery | Display + Entry | Display + Entry | Display + Entry | Display + Entry | Display + Entry |
| Diagnostics/log summary | Display + Entry | Display + Entry | Display + Entry | Display + Entry | Display + Entry |
| BMC/IPMI/Redfish management | N/A | N/A | N/A | Display + Entry | Platform-dependent |
| Power/thermal status | Display + Entry | Display + Entry | Display + Entry | Display + Entry | Display + Entry |
| Battery and adapter policy | N/A | Native | N/A | N/A | Platform-dependent |
| Performance/tuning policy | Entry | Entry | Entry | Entry | Platform-dependent |
| RAS/NUMA/PCIe policy | Platform-dependent | N/A | Platform-dependent | Native | Platform-dependent |
| PCIe capability summary and native policy entry hints | Display + Entry | Platform-dependent | Display + Entry | Display + Entry | Platform-dependent |

## XArch 产品目标能力矩阵

下表保留具体架构族名称，因为产品团队、edk2 build scripts 和平台包仍然使用这些名称。这里的矩阵是 XArch 对通用 App/provider 行为的视角，不是要求把 `ARCH=X64`、`ARCH=AARCH64`、`ARCH=RISCV64` 或 `ARCH=LOONGARCH64` 隐藏成新的 build 名称。

| 能力 | x86 / X64 | ARM / AARCH64 | RISC-V / RISCV64 | LoongArch / LOONGARCH64 | App 策略 |
| --- | --- | --- | --- | --- | --- |
| Architecture string | Yes | Yes | Yes | Yes | 从构建/运行时架构显示。 |
| Firmware vendor/revision | Yes | Yes | Yes | Yes | 从 `gST->FirmwareVendor` 和 revision 显示。 |
| Memory summary | Yes | Yes | Yes | Yes | 从 UEFI memory map 显示 total usable memory。 |
| GOP display mode | Yes | Yes | Yes | Yes | 显示当前 resolution 和 renderer state。 |
| Boot#### inventory | Yes | Yes | Yes | Yes | 通过 UEFI variables / Boot Manager library 枚举。 |
| HII formset entries | Yes | Yes | Yes | Yes | 枚举 HII handles，并通过 FormBrowser2 打开。 |
| Secure Boot state | Yes | Yes | Yes when implemented | Yes when implemented | 只读取标准 UEFI variables。 |
| TPM / TCG / TCM | PC/server 常见 | Platform-dependent | Platform-dependent | Platform-dependent | 检测 protocol/presence；缺失时显示 `N/A`。 |
| SMBIOS summary | Common | Server 常见 | Optional | Optional | 基础 provider 支持；缺失时显示 `N/A`。 |
| ACPI / device tree | ACPI common | ACPI or DT | ACPI or DT | ACPI or DT | 基础 ACPI presence summary；device-tree detail 留给后续。 |
| PCI / USB / NVMe inventory | Common | Platform-dependent | Platform-dependent | Platform-dependent | 使用 handle/device-path inventory，不 hard-code buses。 |
| BMC / IPMI / Redfish | Server common | Server common | Optional | Server/product dependent | 基础 provider 支持；client 平台隐藏或显示 `N/A`。 |
| Capsule update | Common | Platform-dependent | Platform-dependent | Platform-dependent | 检测 capsule/update support，并交给 native page/app。 |
| RAS / NUMA / PCIe policy | Server/workstation | Server | Emerging | Server/product dependent | App 永不实现 policy；只打开归属 HII formset。 |
| PCIe inventory and policy-entry hints | Common | Platform-dependent | Emerging | Server/product dependent | 从 `ModernUiPcieDataLib` 显示只读能力摘要；实际 PCIe policy changes 仍由 native HII/FormBrowser 拥有。 |

## Provider 路线图

Provider 的职责是报告状态和入口，不执行平台策略：

- `ModernUiPlatformDataLib`：固件、架构、内存、显示、平台名。
- `ModernUiBootDataLib`：Boot option 枚举和启动。
- `ModernUiDeviceDataLib`：HII formset/device entry 发现，并通过 FormBrowser2 打开。
- `ModernUiSecurityDataLib`：Secure Boot、key database、TCG/TPM 状态，只读。
- `ModernUiFirmwareDataLib`：capsule/update/recovery 状态。
- `ModernUiDiagnosticsDataLib`：POST/log/platform health 摘要。
- `ModernUiManagementDataLib`：BMC/IPMI/Redfish/server management 摘要。
- `ModernUiPowerDataLib`：power/thermal 能力摘要。
- `ModernUiHardwareHealthDataLib`：只读 demo Hardware Health 数据，真实 thermal/fan 策略仍归原生。
- `ModernUiPerformanceDataLib`：CPU/memory/tuning 入口可用性。
- `ModernUiPcieDataLib`：PCIe 能力摘要和原生策略入口提示，实际 PCIe policy changes 仍由平台 HII/FormBrowser 拥有。

## 完成标准

- App 页面架构中立，由 provider 驱动，不硬编码板级假设。
- 每个真实配置动作都只能启动 boot option 或进入原生 FormBrowser/HII 所有权。
- 缺失能力显示为 `N/A`、`Unknown`、隐藏行或只读入口，不崩溃也不伪造设置。
- 同一 App 构建可运行于 ArmVirt 和 LoongArchVirt；x86/OVMF 与 RISC-V 作为未来 overlay 规划。
- DisplayEngine 兼容性检查继续通过 `Docs/CompatibilityMatrix.md` 单独跟踪。
