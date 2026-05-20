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
| app/provider | App shell and provider agents | `Application/ModernSetupApp/`, `Library/ModernUi*DataLib/`, provider/data headers | Front-page shell, dashboards, navigation, typed read-only summaries, FormBrowser entry points, PCIe policy summary hints | Real setup-page behavior needs display-engine/FormBrowser review; public provider models need core-api |
| hii-bridge | Experimental HII bridge agent | `Library/ModernUiHiiBridgeLib/`, `Include/ModernUi/ModernUiHiiBridge.h` | Experimental HII interpretation research | Promotion beyond experimental needs core-api and display-engine review |
| platform-ci | Platform/CI/release agent | `Scripts/`, `Tests/`, `Experimental/`, `*.dsc`, docs, `.github/`, release notes | Build scripts, QEMU/manual validation, package integration, maintainer docs | Script or DSC changes that hide behavior changes need affected code owner review |
| docs | Docs/governance route through platform-ci | `Docs/`, `.github/ISSUE_TEMPLATE/`, `.github/labels.yml`, `.github/PULL_REQUEST_TEMPLATE.md` | Contributor docs, issue backlog, ownership routing, issue templates, label metadata | Policy changes that affect a stable code contract need that logical owner review |

## ModernSetupApp internal module map

`Application/ModernSetupApp/` is owned as one app shell. Keep page ownership coarse enough for phase-sized PRs; do not split every page into a separate PR unless the behavior or validation scope requires it.

| Module | Primary role | Route guidance |
| --- | --- | --- |
| `ModernSetupApp.c` | UEFI application entry, top-level state loop, shared app lifetime wiring | Route shell lifecycle, protocol lookup, and main-loop changes through app/provider; cross-review display-engine when FormBrowser launch behavior changes |
| `ModernSetupAppChrome.c` | App frame, tabs, footer/status chrome, common drawing shell | Route visual shell/chrome changes through app/provider; cross-review renderer/theme only for shared drawing or token contract changes |
| `ModernSetupAppDashboard.c` | Dashboard-only drawing and dashboard card layout | Keep `ModernSetupDrawDashboard()` defined here; route dashboard summary/card presentation through app/provider and keep expanded cards backed by the app-private provider snapshot |
| `ModernSetupAppProvider.c` | App-private read-only provider snapshot, fallback normalization, and derived provider health/readiness summary | Keep provider LibraryClass calls centralized here so Dashboard/pages consume normalized summaries without parsing setup data or duplicating fallback policy |
| `ModernSetupAppPages.c` | Existing page drawing and page dispatch for Boot, Devices, Security, provider summaries, and Exit | Keep current pages together for now; real setup entries must hand off to FormBrowser/`SendForm()` instead of implementing IFR behavior in the app |
| `ModernSetupAppActions.c` | App actions such as boot option launch, language selection, and setup handoff helpers | Route behavior-affecting actions through app/provider; cross-review display-engine/FormBrowser for setup-page handoff changes |

ModernSetupApp boundary rules:

- `ModernSetupApp` may compose provider summaries and launch native setup pages, but it must not parse IFR, implement ConfigAccess callbacks, or write HII varstores directly.
- Real setup pages remain owned by native FormBrowser/DisplayEngine paths and should be opened through FormBrowser2/`SendForm()` handoff.
- Experimental HII bridge and page adapter headers/libraries stay out of `Application/ModernSetupApp/` unless an explicit promotion is reviewed by core-api and display-engine owners.
- When adding an app `.c` file matching `Application/ModernSetupApp/ModernSetupApp*.c`, update `Application/ModernSetupApp/ModernSetupApp.inf` `[Sources]` in the same PR and run `python3 Tests/Smoke/smoke_validate.py`.
- App-internal refactors should preserve public API/DEC contracts unless the PR explicitly routes through core-api.
- Lightweight App information-architecture work may add visual grouping or
  subsection labels inside existing Dashboard/provider pages, but should retain
  the current rows, selectable card count, navigation behavior, provider snapshot
  boundary, and diagnostic Present/Absent/Available/N/A text unless a later
  cleanup phase explicitly owns that removal.
- PCIe policy summaries from `ModernUiPcieDataLib` are read-only capability and
  native HII entry hints. ReBAR, Above 4G decoding, SR-IOV, ASPM, bifurcation,
  hot-plug, ACS/ARI, IOMMU, and BAR resource policy changes remain owned by
  platform HII/FormBrowser flows, not App/provider code.

## Practical routing

- Docs-only changes may be reviewed by any maintainer; tag the logical owner only if the doc changes a contract.
- Backlog-driven work should start from `Docs/ISSUE_BACKLOG.md`; keep its issue seeds, routing labels, and validation expectations synchronized with this file.
- Keep `area/*` labels in `.github/labels.yml` aligned with the Area dropdown in `.github/ISSUE_TEMPLATE/module-request.yml`.
- Public API or DEC changes require core-api plus the implementation owner.
- Cross-layer behavior changes need every owner whose stable contract is consumed.
- Platform-specific behavior should be routed through LibraryClass instances, PCDs, or overlays instead of hard-coding policy into shared UI layers.
- HII bridge work remains experimental unless promotion is explicitly reviewed by core-api and display-engine owners.
