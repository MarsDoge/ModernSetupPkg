<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# ModernSetupPkg Documentation

Language: English | [简体中文](README.zh-CN.md)

This index points to the canonical English documentation and the matching
Simplified Chinese counterparts for the core project docs.

## Core docs

| Topic | English | 简体中文 |
| --- | --- | --- |
| XArch architecture model | [XArch.md](XArch.md) | [XArch.zh-CN.md](XArch.zh-CN.md) |
| App feature standard (normative) | [AppFeatureStandard.md](AppFeatureStandard.md) | [AppFeatureStandard.zh-CN.md](AppFeatureStandard.zh-CN.md) |
| Productization feature matrix | [ProductizationFeatureMatrix.md](ProductizationFeatureMatrix.md) | [ProductizationFeatureMatrix.zh-CN.md](ProductizationFeatureMatrix.zh-CN.md) |
| Productization validation matrix | [ProductizationValidationMatrix.md](ProductizationValidationMatrix.md) | [ProductizationValidationMatrix.zh-CN.md](ProductizationValidationMatrix.zh-CN.md) |
| Module boundaries | [MODULE_BOUNDARIES.md](MODULE_BOUNDARIES.md) | [MODULE_BOUNDARIES.zh-CN.md](MODULE_BOUNDARIES.zh-CN.md) |
| Development guide | [DEVELOPMENT.md](DEVELOPMENT.md) | [DEVELOPMENT.zh-CN.md](DEVELOPMENT.zh-CN.md) |
| IBV and platform setup survey | [IbvAndPlatformSetupSurvey.md](IbvAndPlatformSetupSurvey.md) | [IbvAndPlatformSetupSurvey.zh-CN.md](IbvAndPlatformSetupSurvey.zh-CN.md) |

## Compatibility and process docs

- [CompatibilityMatrix.md](CompatibilityMatrix.md) records native/modern DisplayEngine evidence.
- [BeforeAfter.md](BeforeAfter.md) records before/after capture notes.
- [BASELINE.md](BASELINE.md) records the pinned edk2 baseline.
- [API_COMPATIBILITY.md](API_COMPATIBILITY.md) defines public API compatibility rules.
- [AGENT_OWNERSHIP.md](AGENT_OWNERSHIP.md) maps maintenance ownership.
- [ISSUE_BACKLOG.md](ISSUE_BACKLOG.md) tracks seeded work.
- [CONTRIBUTING.md](CONTRIBUTING.md) describes contribution workflow.

## Boundary reminder

`ModernSetupApp` is a standard front-page shell for navigation, read-only
summaries, App-private preferences, and native FormBrowser entry points. Real
setup pages, HII/VFR/IFR parsing, callbacks, validation, defaults, and variable
writes remain owned by edk2 FormBrowser and platform HII drivers.
