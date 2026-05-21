<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# XArch 架构模型

语言：[English](XArch.md) | 简体中文

XArch 是 ModernSetupPkg 的跨架构项目模型：在 X64、AARCH64、LOONGARCH64 和 RISCV64 目标之间保持同一套 Setup UX、同一条 HII/FormBrowser 所有权边界，以及同一套验证词汇。

XArch 是项目、产品和验证词汇，不替代 edk2 的 `ARCH` 值。构建脚本、DSC/FDF overlay、工具链选择和 edk2 构建变量仍使用具体值：`ARCH=X64`、`ARCH=AARCH64`、`ARCH=LOONGARCH64`、`ARCH=RISCV64`。

## 目标

- 在受支持的架构族之间复用 ModernSetup 视觉语言和首页信息架构。
- 保持固件 Setup 语义由 edk2 HII、UiApp、SetupBrowserDxe、FormBrowser2、ConfigAccess、回调、默认值、校验和 varstore 路由拥有。
- 保持平台集成具体可追踪：每个目标都映射到 edk2 平台包、固件镜像、构建脚本、工具链和 QEMU/手工验证路径。
- 明确区分验证证据：已截图、可手工验证、活跃路径、仅构建/脚本验证或计划中，避免夸大成熟度。

## 目标映射

| XArch 目标 | edk2 ARCH 值 | edk2/QEMU 平台 | 主要脚本 | 当前角色 |
| --- | --- | --- | --- | --- |
| X64 / OVMF X64 | `X64` | `OvmfPkg/OvmfPkgX64` | `build-ovmf-x64.sh`, `run-ovmf-x64.sh`, `capture-ovmf-x64.sh` | 本地/手工 OVMF 验证和截图捕获路径。 |
| AARCH64 / ArmVirtQemu | `AARCH64` | `ArmVirtPkg/ArmVirtQemu` | `build-armvirt.sh`, `run-armvirt.sh`, `capture-armvirt.sh`, `build-modern-app.sh` | 主要兼容性截图路径和 App bring-up 路径。 |
| LOONGARCH64 / LoongArchVirtQemu | `LOONGARCH64` | `OvmfPkg/LoongArchVirt/LoongArchVirtQemu` | `build-loongarchvirt.sh`, `run-loongarchvirt.sh` | 活跃构建/运行路径，包含 native UiApp + ModernDisplayEngine 和可选 App ESP。 |
| RISCV64 / RiscVVirtQemu | `RISCV64` | `OvmfPkg/RiscVVirt/RiscVVirtQemu` | `build-riscvvirt.sh` | 构建/脚本 overlay 验证；图形 QEMU helper 仍是未来工作。 |

## 验证等级

- `Captured`：已有相关路径的截图或 screendump 证据。
- `Manual`：有本地运行说明，可用于手工 QEMU 验证，但不是 CI gate。
- `Active`：构建/运行路径存在，并属于当前维护者验证范围。
- `Build/script validation`：脚本可为目标生成 overlay 或构建，但图形 UI 行为尚未验证。
- `Planned`：目标或能力是未来扩展，不应描述为已验证。

这些等级描述证据，不等同于产品质量。

## 轻量验证 Runner

`Scripts/xarch-validate.sh` 让 XArch 目标模型可执行，但不启动完整固件构建或 QEMU。当前阶段仅支持 dry-run：检查目标元数据、预期脚本和手工文档是否存在，并报告每个目标的证据等级。

```sh
Scripts/xarch-validate.sh --all --mode dry-run
Scripts/xarch-validate.sh --target aarch64 --mode dry-run
Scripts/xarch-validate.sh --target all --mode dry-run --format json
Scripts/xarch-validate.sh --all --mode dry-run --format markdown --output Build/Reports/xarch-validation.md
Scripts/xarch-validate.sh --all --mode dry-run --format json --output Build/XArchValidation.json
```

当前范围：目标为 `x64`、`aarch64`、`loongarch64`、`riscv64` 或 `all`；模式仅 `dry-run`；输出格式为 `table`、`markdown`、`json`。重型构建、QEMU 和截图捕获仍是维护者显式动作。

## 所有权边界

- edk2 拥有 HII 包注册、VFR/IFR 解析、表达式求值、ConfigAccess 回调、BrowserActions、校验、默认值和 varstore 路由。
- `ModernDisplayEngineDxe` 只负责 SetupBrowserDxe/FormBrowser2 已准备表单的 GOP 显示引擎呈现。
- `ModernSetupApp` 只拥有标准首页壳层、导航状态、只读摘要、App 私有 UX 偏好和进入原生 FormBrowser2 的入口。
- Provider 库报告 inventory、posture、capability、health 和原生入口可用性，不应成为平台策略引擎。
- 真实设置修改留在归属平台 HII formset、原生 FormBrowser 流程或专门的平台/厂商工具中。

## 当前成熟度

| 领域 | X64 / OVMF X64 | AARCH64 / ArmVirtQemu | LOONGARCH64 / LoongArchVirtQemu | RISCV64 / RiscVVirtQemu |
| --- | --- | --- | --- | --- |
| Native UiApp + ModernDisplayEngine overlay | Manual | Captured | Active | Build/script validation |
| DisplayEngine 前后对比证据 | Manual OVMF capture path | Captured ArmVirt evidence | Manual | Planned |
| ModernSetupApp 首页壳层 | Local/manual OVMF-compatible app build | Active bring-up path | Optional ESP path | Planned |
| QEMU 图形运行 helper | Manual | Active | Active | Planned |
| Smoke overlay 生成 | Active script contract | Active script contract | Active script contract | Active script contract |

## 下一步扩展规则

- 新增目标时先记录 XArch 映射：具体 edk2 ARCH 值、平台 DSC/FDF、构建脚本、可用的 run/capture helper 和验证等级。
- 不要把构建变量或脚本族改名为 XArch；命令中继续使用具体 edk2 ARCH 和平台名。
- 新增目标特定 App 行为前，先复用共享 UX 模型、provider 词汇和所有权边界。
- 优先使用只读 provider 摘要和原生 FormBrowser 入口，不在 App 中实现平台策略控制。
- XArch 目标或验证术语变化时，同步更新 `Docs/CompatibilityMatrix.md`、`Docs/ProductizationFeatureMatrix.md` 和 smoke 静态文档契约。
