<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# ModernSetupPkg 文档

语言：[English](README.md) | 简体中文

本索引汇总核心文档的英文规范版本和简体中文版本。英文文件仍是对外规范入口；中文文件用于快速理解项目边界、架构和开发约束。

## 核心文档

| 主题 | 简体中文 | English |
| --- | --- | --- |
| XArch 架构模型 | [XArch.zh-CN.md](XArch.zh-CN.md) | [XArch.md](XArch.md) |
| 产品化功能矩阵 | [ProductizationFeatureMatrix.zh-CN.md](ProductizationFeatureMatrix.zh-CN.md) | [ProductizationFeatureMatrix.md](ProductizationFeatureMatrix.md) |
| 模块边界 | [MODULE_BOUNDARIES.zh-CN.md](MODULE_BOUNDARIES.zh-CN.md) | [MODULE_BOUNDARIES.md](MODULE_BOUNDARIES.md) |
| 开发指南 | [DEVELOPMENT.zh-CN.md](DEVELOPMENT.zh-CN.md) | [DEVELOPMENT.md](DEVELOPMENT.md) |
| IBV 与平台 Setup 调研 | [IbvAndPlatformSetupSurvey.zh-CN.md](IbvAndPlatformSetupSurvey.zh-CN.md) | [IbvAndPlatformSetupSurvey.md](IbvAndPlatformSetupSurvey.md) |

## 兼容性和流程文档

- [CompatibilityMatrix.md](CompatibilityMatrix.md)：native/modern DisplayEngine 兼容性证据。
- [BeforeAfter.md](BeforeAfter.md)：前后对比截图和捕获说明。
- [BASELINE.md](BASELINE.md)：固定的 edk2 baseline。
- [API_COMPATIBILITY.md](API_COMPATIBILITY.md)：公共 API 兼容性规则。
- [AGENT_OWNERSHIP.md](AGENT_OWNERSHIP.md)：维护所有权和评审分工。
- [ISSUE_BACKLOG.md](ISSUE_BACKLOG.md)：种子任务和后续工作。
- [CONTRIBUTING.md](CONTRIBUTING.md)：贡献流程。

## 边界提醒

`ModernSetupApp` 是标准首页壳层，只负责导航、只读摘要、App 私有偏好和进入原生 FormBrowser 的入口。真实 Setup 页面、HII/VFR/IFR 解析、回调、校验、默认值和变量写入仍归 edk2 FormBrowser 与平台 HII 驱动所有。
