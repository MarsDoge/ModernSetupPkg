<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# ModernSetupPkg 开发指南

语言：[English](DEVELOPMENT.md) | 简体中文

ModernSetupPkg 目标是成为可复用的现代 Setup UI 框架和默认 Setup 应用。新代码应继续让 ArmVirt 作为首个测试目标保持有用，但不能把 ArmVirt、QEMU、AArch64、LoongArch、x86 或任何 IBV 策略写进 UI core。

## 新贡献者快速开始

1. 在 `Docs/AGENT_OWNERSHIP.md` 中确认改动区域。
2. 查看 `Docs/ISSUE_BACKLOG.md`，了解种子任务、标签、验证说明和治理 issue。
3. 修改跨层行为前先看 `Docs/MODULE_BOUNDARIES.md`。
4. 修改 `Include/ModernUi/*.h` 或 `ModernSetupPkg.dec` 时遵守 `Docs/API_COMPATIBILITY.md` 并请求 Core API review。
5. 保持改动聚焦。docs-only 和小脚本修复通常不需要完整固件 checklist；无关 PR 模板项标为 N/A。
6. 在 PR 中记录验证。如果 QEMU 或某个平台不可用，说明原因和替代验证。

## 函数契约

每个函数在声明或定义前都必须有 edk2 风格 Doxygen 注释。公共函数说明稳定接口，私有函数说明本地契约。

函数注释应覆盖：

- 每个 `@param[in]`、`@param[in,out]` 的预期输入。
- 每个 `@param[out]`、`@param[in,out]` 的预期输出。
- 是否接受 `NULL`。
- 缓冲区大小和所有权转移规则。
- Protocol、PCD、全局状态或初始化前置条件。
- 返回值语义，包括重要失败码。
- 失败时是否允许部分绘制、部分状态更新或保持不变。

示例：

```c
/**
  Draw a text label into the active render target.

  @param[in]  Context     Initialized render context. Must not be NULL.
  @param[in]  X           Left coordinate in pixels.
  @param[in]  Y           Top coordinate in pixels.
  @param[in]  Text        Null-terminated UCS-2 text. Must not be NULL.
  @param[in]  Color       Foreground color.
  @param[in]  Background  Background color passed to the font renderer.

  @retval EFI_SUCCESS            Text was submitted to the renderer.
  @retval EFI_INVALID_PARAMETER  Context or Text is NULL.
  @retval EFI_UNSUPPORTED        Required text rendering backend is unavailable.
  @retval EFI_OUT_OF_RESOURCES   Temporary rendering allocation failed.
**/
```

## 架构规则

- UI 页面消费结构化数据模型，不直接拥有平台枚举、变量解析、安全策略或架构特定行为。
- `ModernUiEngineLib` 是 native DisplayEngine 路径和标准首页 App 的共享视觉契约。DisplayEngine adapters 和 App pages 应构建 engine models，而不是自己绘制 tabs、rows、value selectors、popups 或 footers。
- `ModernSetupApp` 是标准首页壳层。它可拥有导航、language/theme state、dashboard composition、boot selection 和入口，但不得解析 IFR、求值 VFR 条件、直接调用 ConfigAccess 或写 HII varstores。
- 新 App-facing 产品功能必须先归类到 `Docs/ProductizationFeatureMatrix.md`。平台私有或策略较重的功能应只提供摘要或入口，并交给归属 HII 页面。
- 真实 Setup 页入口必须使用 `EFI_FORM_BROWSER2_PROTOCOL.SendForm()`，以保持 GUID formset 处理、回调、默认值、校验和变量路由在原生 edk2 FormBrowser 内。
- 平台和 IBV 差异优先通过 LibraryClass instances、PCDs 或小型平台 overlays 引入。
- Renderer API 必须隐藏具体图形后端；GOP 是第一个后端，不是 app-level 抽象。
- Input API 暴露 UI events，而不是裸键盘或指针协议。
- Theme 值必须 token 化；页面代码不得硬编码厂商颜色、字体、logo 或图像资产。
- Layout 必须根据分辨率和 safe-area 计算位置；新 UI 在 800x600、1024x768、1280x800 下应无重叠。
- 早期版本不要整体替换 HII/FormBrowser。ModernSetup 证明兼容性期间，应保留 classic UiApp/FormBrowser path。
- 现代 HII bridge 只有在支持真实平台 form contract 后才可能成为唯一 setup engine：form navigation、question rendering、defaults、validation、expressions、callback policy、varstore routing、localization 和安全失败报告。
- 新 HII bridge 代码必须 fail closed。不支持 opcode、未知 varstore、callback-driven questions 或未求值 expressions 应渲染为只读/fallback rows，而不是绕过归属驱动强制写入。
- 商业固件界面只能作为视觉和交互参考；不得复制闭源代码、字体、图标、图片、布局或专有资产。

## 推荐扩展点

- `ModernUiRendererLib`：绘制 primitives 和后端适配。
- `ModernUiEngineLib`：可复用 page chrome、layout、tab、row、value、popup、footer、help panel、right-rail models。
- `ModernUiInputLib`：keyboard、pointer、touch、serial event mapping。
- `ModernUiThemeLib`：style tokens 和 vendor/theme selection。
- `ModernUiLayoutLib`：分辨率感知 geometry。
- `ModernUiPlatformLib`：platform identity 和 capability reporting。
- `ModernUiBootDataLib`：Boot#### 和 BootOrder access。
- `ModernUiDeviceDataLib`：handle/device-path inventory，并提供 FormBrowser entry points，而不是 decoded IFR controls。
- App providers：`ModernUiFirmwareDataLib`、`ModernUiDiagnosticsDataLib`、`ModernUiManagementDataLib`、`ModernUiPowerDataLib`、`ModernUiPerformanceDataLib` 等只报告状态和入口。

当某个职责开始增长，应新增或扩展匹配的共享库，而不是继续膨胀 `ModernSetupApp` 或 `ModernDisplayEngineDxe`。

## 多 Agent 维护

协作脚手架包括：

- `Docs/AGENT_OWNERSHIP.md`：模块 owner、标签和 review gates。
- `Docs/ISSUE_BACKLOG.md`：初始 issue seeds、owner/agent routing、验证期望和排序。
- `Docs/MODULE_BOUNDARIES.md`：稳定契约和依赖规则。
- `Docs/API_COMPATIBILITY.md`：公共 API、DEC 和 deprecation policy。

扩展 shared headers、`ModernSetupPkg.dec`、DisplayEngine behavior、provider contracts 或 experimental HII bridge surfaces 前先查看这些文档。

## 验证矩阵

涉及脚本、overlay 逻辑、治理文档或 native/modern 路径分离时，先运行轻量 smoke harness：

```sh
python3 Tests/Smoke/smoke_validate.py
```

该 harness 不要求 edk2 checkout、固件工具链或 QEMU。它检查 shell 语法，并对 synthetic edk2 fixtures 运行 overlay-generation dry runs。

| 改动类型 | 预期验证 |
| --- | --- |
| Docs-only | 拼写/链接 sanity；策略变化需对应 owner review。 |
| Public API / DEC | Header/DEC review、相关库构建或 compile-plan、必要 changelog。 |
| Renderer / theme / layout | 构建或 focused smoke test，并说明 800x600、1024x768、1280x800 影响。 |
| DisplayEngine / HII | Native FormBrowser compatibility check、fallback behavior notes、无绕过 ConfigAccess/varstore writes。 |
| App / provider | App smoke path 或 provider contract check；真实 Setup 页面仍使用 `SendForm()`。 |
| Platform / scripts | Script dry run 或 syntax check、目标平台名称、generated-file 影响。 |

QEMU 验证对固件行为变化更好，但不是每个 PR 的硬阻断；记录不可用项和替代验证。

## 变更纪律

- 用户可见行为、架构决策、构建流或平台支持变化时更新 `CHANGELOG.md`。
- 行为变化时更新 `Tests/`；UI 交互变化应更新手工 QEMU 检查，provider/layout/parser 变化应添加或更新 smoke/unit tests。
- ArmVirt overlay 脚本保持非侵入；只能在 `Build/ModernSetupPkgOverlay` 下生成文件，不编辑上游 `ArmVirtPkg`。
- 保持包可作为 edk2 workspace submodule 构建。
- 提交应聚焦，尽量分离 framework、app、platform overlays 和 documentation。
