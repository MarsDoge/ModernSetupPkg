<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Agent Ownership

This file is a routing guide for humans and agents. It is not a list of mandatory bot accounts, and it should not block a human maintainer from reviewing a small or urgent change. GitHub CODEOWNERS intentionally uses the real fallback maintainer; logical owners live here so agents and contributors can find the right reviewers.

Use ownership to answer three questions:

1. Who understands the contract being changed?
2. Which downstream users may be affected?
3. What validation notes should the PR include?

| Area | Logical owner | Owned paths | Owns | Cross-review when |
| --- | --- | --- | --- | --- |
| core-api | Core API agent | `Include/ModernUi/*.h`, `ModernSetupPkg.dec` | Public structs, enums, function contracts, LibraryClasses, GUIDs, PCDs | Any header/DEC change also needs the affected implementation owner |
| renderer/theme/ui-engine | Renderer/theme/UI engine agents | `Library/ModernUiRendererLib/`, `Library/ModernUiThemeLib/`, `Library/ModernUiEngineLib/`, `Library/ModernUiInputLib/`, `Library/ModernUiPageAdapterLib/`, matching headers | Drawing primitives, theme tokens, layout models, input events, page adapters | Public model changes need core-api; DisplayEngine/App shared behavior needs those owners |
| display-engine | DisplayEngine compatibility agent | `Universal/ModernDisplayEngineDxe/`, `Library/ModernUiCustomizedDisplayLib/` | Native edk2 DisplayEngine/FormBrowser rendering and compatibility behavior | UI model changes need renderer/theme/ui-engine; public hooks need core-api |
| app/provider | App shell and provider agents | `Application/ModernSetupApp/`, `Library/ModernUi*DataLib/`, provider/data headers | Front-page shell, dashboards, navigation, typed read-only summaries, FormBrowser entry points | Real setup-page behavior needs display-engine/FormBrowser review; public provider models need core-api |
| hii-bridge | Experimental HII bridge agent | `Library/ModernUiHiiBridgeLib/`, `Include/ModernUi/ModernUiHiiBridge.h` | Experimental HII interpretation research | Promotion beyond experimental needs core-api and display-engine review |
| platform-ci | Platform/CI/release agent | `Scripts/`, `Tests/`, `Experimental/`, `*.dsc`, docs, `.github/`, release notes | Build scripts, QEMU/manual validation, package integration, maintainer docs | Script or DSC changes that hide behavior changes need affected code owner review |

## Practical routing

- Docs-only changes may be reviewed by any maintainer; tag the logical owner only if the doc changes a contract.
- Public API or DEC changes require core-api plus the implementation owner.
- Cross-layer behavior changes need every owner whose stable contract is consumed.
- Platform-specific behavior should be routed through LibraryClass instances, PCDs, or overlays instead of hard-coding policy into shared UI layers.
- HII bridge work remains experimental unless promotion is explicitly reviewed by core-api and display-engine owners.
