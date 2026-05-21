<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# 模块边界

语言：[English](MODULE_BOUNDARIES.md) | 简体中文

ModernSetupPkg 最容易维护的方式，是让每一层保持小而明确的契约。内部实现可以演进，但公共头文件、DEC 条目和跨层行为应保持稳定；涉及的 owner 评审后才能修改。

## 稳定契约

- 公共头文件：`Include/ModernUi/*.h`。
- 包元数据：`ModernSetupPkg.dec`。
- 公共 `LibraryClass` 名称、GUID 和 PCD。
- Provider 数据模型语义、renderer/theme tokens、engine draw models 和 DisplayEngine 兼容行为。

## 分层规则

| 层 | 可以做 | 不得做 |
| --- | --- | --- |
| Renderer/theme | 绘制 primitives、文本/字形、颜色、theme tokens 和安全后端抽象。 | 解析 HII/IFR、枚举 providers、拥有 app navigation、拥有 FormBrowser policy。 |
| UI engine/layout/input | 把类型化模型转换为可复用 layout、rows、popups、footers 和 UI events。 | 直接读取 Boot####、写 varstores、硬编码平台策略。 |
| DisplayEngine path | 在使用现代渲染组件时保留 edk2 DisplayEngine/FormBrowser 行为。 | 替代 FormBrowser 语义、绕过 ConfigAccess/callbacks、发明 App-only policy。 |
| App shell | 拥有首页导航、dashboard composition、language/theme state 和 `SendForm()` 入口。 | 解析 IFR、计算 VFR 条件、直接调用 ConfigAccess、写 HII varstores。 |
| Providers | 从平台/固件数据暴露类型化摘要和 FormBrowser 入口。 | 绘制 UI、选择 layout/theme、拥有 app navigation policy。 |
| HII bridge | 作为实验 parser/adapter research 保持隔离。 | 成为默认兼容路径、强制写入、把不支持 opcode 当作安全可写。 |
| Platform/CI | 构建脚本、overlays、QEMU/手工验证、release packaging。 | 把行为变化隐藏在脚本或 docs-only PR 中。 |

## 不要做什么

- App 代码不得解析 IFR 或写 HII varstores。真实 Setup 页面应通过 `EFI_FORM_BROWSER2_PROTOCOL.SendForm()` 进入原生 FormBrowser。
- Providers 不得绘制 UI；它们只返回数据和入口，presentation 由 renderer/engine/app 决定。
- Renderer 不得拥有 FormBrowser policy；它只绘制上层要求绘制的内容。
- DisplayEngine 修改必须保留原生 HII 语义，除非明确评审兼容性影响。
- HII bridge 仅用于实验。不支持的构造应 fail closed 或渲染为只读/fallback rows。
- 平台特定策略属于 LibraryClass instances、PCDs 或 overlays，不属于共享 UI core。

## Contract-first 工作流

1. 如果修改 `Include/ModernUi/*.h` 或 `ModernSetupPkg.dec`，先描述公共契约，再连接 consumers。
2. 共享 structs/enums 尽量使用 append-only 变更。
3. 公共契约请求 core-api review，相关实现请求对应 owner review。
4. 添加与改动层匹配的验证说明；QEMU 不可用可以接受，但要说明原因。
5. 行为、构建流、公共 API、平台支持或兼容性变化时更新 `CHANGELOG.md`。
