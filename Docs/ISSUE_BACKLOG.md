<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Issue Backlog and Agent Routing

This backlog is the lightweight landing page for multi-agent work. It records issue seeds that are useful to open or claim later, but it is not a substitute for maintainer judgment. Keep issues focused, route them through `Docs/AGENT_OWNERSHIP.md`, and prefer phase-sized PRs that can be validated as one coherent ownership or feature slice instead of scattering tiny mechanical PRs.

Do not create GitHub issues from this file automatically unless a maintainer asks for that explicitly. When an issue is opened, copy the relevant phase/theme, apply the matching labels from `.github/labels.yml`, and link back to this document if the issue is part of a tracked phase.

## Label and template sync

Area choices in `.github/ISSUE_TEMPLATE/module-request.yml` should stay aligned with the `area/*` labels in `.github/labels.yml`:

- `area/core-api`
- `area/renderer`
- `area/display-engine`
- `area/app-provider`
- `area/hii-bridge`
- `area/platform-ci`
- `area/docs`

Use the general `documentation` label for docs-only work and pair it with `area/docs` when the docs are the primary area. Use `module-request` for proposed module, boundary, or ownership changes.

Manual label sync is intentionally maintainer-driven and does not require third-party Actions or repository secrets. From a trusted checkout with GitHub CLI authenticated, maintainers may run:

```sh
python3 - <<'PY'
import subprocess
import sys
try:
    import yaml
except ImportError:
    sys.exit('Install PyYAML locally or use another trusted YAML parser before syncing labels.')

with open('.github/labels.yml', 'r', encoding='utf-8') as stream:
    labels = yaml.safe_load(stream) or []
for label in labels:
    name = label['name']
    color = label.get('color', '').lstrip('#')
    description = label.get('description', '')
    subprocess.run([
        'gh', 'label', 'create', name,
        '--color', color,
        '--description', description,
        '--force',
    ], check=True)
PY
```

Before syncing labels, review the diff and confirm the target repository with `gh repo view`.

## Owner and agent routing

Use this routing before claiming or opening a seeded issue:

| Issue area | Primary route | Typical labels | Cross-review trigger |
| --- | --- | --- | --- |
| Public headers, DEC, GUIDs, PCDs | Core API agent | `area/core-api`, `api-change` | Any implementation or compatibility behavior changes |
| Renderer, theme, layout, input, shared engine | Renderer/theme/UI engine agents | `area/renderer` | Public model or DisplayEngine/App shared behavior changes |
| Native setup browser compatibility | DisplayEngine compatibility agent | `area/display-engine`, `compat` | Renderer model, HII, or public hook changes |
| ModernSetupApp and provider summaries | App shell and provider agents | `area/app-provider` | Real setup pages, public provider models, or platform policy changes |
| Experimental HII bridge | Experimental HII bridge agent | `area/hii-bridge`, `compat` | Promotion beyond experimental behavior |
| Scripts, tests, package integration, release flow | Platform/CI/release agent | `area/platform-ci` | Script changes that hide behavior changes |
| Governance, contributor docs, issue templates, labels | Docs/governance route through platform-ci | `area/docs`, `documentation` | Policy changes that affect a code owner contract |

## Validation expectations

Every opened issue should state the lightest useful validation before implementation starts. PRs should record what actually ran, and should mark unavailable firmware or QEMU coverage explicitly instead of implying it ran.

| Change type | Minimum expected validation |
| --- | --- |
| Docs, labels, issue templates | YAML parse where applicable, markdown sanity, link/path review, `git diff --check` |
| Public API or DEC | Header/DEC review, affected library build or compile plan, compatibility note |
| Renderer/theme/layout/input | Build or focused smoke plan, resolution impact notes for 800x600, 1024x768, and 1280x800 |
| DisplayEngine/HII | Native FormBrowser compatibility check, fallback behavior notes, no bypassed ConfigAccess or varstore writes |
| App/provider | App smoke path or provider contract check; real setup page entries still use `SendForm()` |
| Platform/scripts | Script dry run or syntax review, named target platform, generated-file impact stated |

## Phase 1 app ownership readiness

Current app/provider acceptance target:

- Title: `phase1(app): 建立 ModernSetupApp 多代理模块边界与 smoke 校验`.
- Route: App shell and provider agents, with platform-ci for smoke validation.
- Labels: `area/app-provider`, `area/platform-ci`, `documentation`, `module-request` when mirrored to GitHub.
- Scope: keep the Dashboard extraction as the app-internal module boundary example, document the `ModernSetupApp` internal module map, and add smoke checks that make future app source additions update `ModernSetupApp.inf` and preserve app/FormBrowser boundaries.
- Non-goals: no UI behavior change, no public API/DEC change, no DisplayEngine/HII workflow change, no provider model expansion, and no additional page-by-page splits for Boot, Devices, or provider summary pages.
- Expected validation: `python3 Tests/Smoke/smoke_validate.py`, `git diff --check origin/main...HEAD`, and PR notes that QEMU/manual firmware UI validation is N/A for this ownership/readiness phase.

Phase 1 is complete when:

1. `Docs/AGENT_OWNERSHIP.md` identifies the `ModernSetupApp` modules: app entry, Chrome, Dashboard, Pages, and Actions.
2. The ownership rules state that the app does not parse IFR, does not implement ConfigAccess, and does not write HII varstores directly.
3. Real setup pages continue to route through FormBrowser2/`SendForm()`.
4. The smoke harness validates `Application/ModernSetupApp/ModernSetupApp*.c` coverage in `ModernSetupApp.inf` `[Sources]` and verifies the Dashboard/Page boundary.
5. `Tests/README.md` and `Tests/Smoke/README.md` describe the new static app ownership checks.

## Phase 2 app/provider contract hardening

Current app/provider acceptance target:

- Title: `phase2(app): harden dashboard/provider read-only summary contract`.
- Route: App shell and provider agents, with platform-ci for smoke validation.
- Labels: `area/app-provider`, `area/platform-ci` when mirrored to GitHub.
- Scope: centralize dashboard and provider summary collection behind an
  app-private snapshot helper, preserve provider LibraryClass read-only surfaces,
  and add static smoke checks so presentation modules do not bypass that helper.
- Non-goals: no public API/DEC change, no provider model expansion, no native
  FormBrowser behavior change, and no direct HII varstore or setup-page writes in
  the app.
- Expected validation: `python3 Tests/Smoke/smoke_validate.py`,
  `git diff --check origin/main...HEAD`, and PR notes that QEMU/manual firmware
  UI validation is optional unless visual behavior is changed.

Phase 2 is complete when:

1. Dashboard/provider summary presentation code uses an app-owned snapshot helper
   instead of each page duplicating provider fallback handling.
2. Provider collection remains read-only and routed through existing provider
   LibraryClasses.
3. `ModernSetupApp` continues to avoid experimental HII bridge/page adapter and
   native setup mutation paths.
4. Smoke validation checks the app provider boundary and INF source coverage.

## Phase 3 app/provider dashboard usefulness

Current app/provider acceptance target:

- Title: `phase3(app): add provider health summary to dashboard and diagnostics`.
- Route: App shell and provider agents, with platform-ci for smoke validation.
- Labels: `area/app-provider`, `area/platform-ci` when mirrored to GitHub.
- Scope: derive an app-private read-only provider health/readiness summary from
  the existing provider snapshot, render compact health/coverage information in
  Dashboard, add diagnostics detail for degraded provider collection, and extend
  smoke checks so presentation code continues to consume the snapshot boundary.
- Non-goals: no public API/DEC/GUID/PCD change, no IFR parsing, no ConfigAccess
  implementation, no HII varstore writes, and no coupling to the experimental
  HII bridge or page adapter.
- Expected validation: `python3 Tests/Smoke/smoke_validate.py`,
  `git diff --check origin/main...HEAD`, and PR notes that QEMU/manual firmware
  UI validation is recommended for visual review but was not required for the
  host-side boundary change.

Phase 3 is complete when:

1. Dashboard shows whether provider-backed app data is ready, degraded, or not
   ready, including provider coverage from the existing snapshot statuses.
2. Diagnostics exposes enough provider health detail to identify the first
   degraded/unavailable provider without adding a new public API.
3. The health summary remains app-private and read-only.
4. Smoke validation enforces the health summary boundary alongside existing app
   provider snapshot checks.

## Phase 4 Dashboard card content expansion

Current app/provider acceptance target:

- Title: `phase4(app): expand Dashboard card content from provider snapshots`.
- Route: App shell and provider agents, with platform-ci for smoke validation.
- Labels: `area/app-provider`, `area/platform-ci` when mirrored to GitHub.
- Scope: expand Dashboard Quick Access/status cards from the initial three tiles
  into a richer provider-backed card set covering boot readiness, device
  visibility, provider health, firmware lifecycle, power/thermal, and
  performance inventory. Keep the content derived from the existing app-private
  provider snapshot and read-only provider LibraryClasses.
- Non-goals: no public API/DEC/GUID/PCD change, no IFR parsing, no ConfigAccess
  implementation, no HII varstore writes, and no coupling to experimental HII
  bridge/page adapter code.
- Expected validation: `python3 Tests/Smoke/smoke_validate.py`,
  `git diff --check origin/main...HEAD`, and PR notes that QEMU/manual firmware
  UI validation is recommended for visual review but not required by the
  host-side smoke harness.

Phase 4 is complete when:

1. Dashboard exposes at least six useful cards and keeps navigation count in one
   app-private constant.
2. Expanded cards use the normalized provider snapshot fields rather than
   calling provider LibraryClasses from presentation modules.
3. Narrow or short content areas degrade without obvious card overflow by
   increasing cards-per-row and reducing optional detail rows.
4. Smoke validation checks the expanded card tokens and provider snapshot
   backing in addition to existing app/provider boundaries.

## Phase 7 PCIe policy provider foundation

Current app/provider acceptance target:

- Title: `phase7(app): add PCIe policy provider foundation`.
- Route: App shell and provider agents for the read-only provider, core-api for
  the new `ModernUiPcieDataLib` header/DEC surface, and platform-ci for smoke
  validation.
- Labels: `area/app-provider`, `area/core-api`, `area/platform-ci`,
  `documentation` when mirrored to GitHub.
- Scope: add a read-only PCIe capability summary and native HII entry hints for
  controller/root-bridge inventory, ReBAR, Above 4G decoding, SR-IOV, ASPM,
  bifurcation, hot-plug, ACS/ARI, and IOMMU visibility. Wire the provider into
  the experimental App DSC and keep smoke checks focused on provider wiring,
  app-provider call boundaries, and mutation-token exclusion.
- Non-goals: actual ReBAR, Above 4G, SR-IOV, ASPM, bifurcation, hot-plug, ACS,
  ARI, or IOMMU policy edits remain platform HII/FormBrowser-owned. PCIe
  varstores, ConfigAccess callbacks, BAR programming, and HII form lifecycles
  remain native HII/FormBrowser-owned, outside the App/provider layer.
- Expected validation: `python3 Tests/Smoke/smoke_validate.py`,
  `git diff --check origin/main...HEAD`, and PR notes that QEMU/manual firmware
  UI validation is not required unless a visual or platform run path changes.

Phase 7 is complete when:

1. `ModernUiPcieDataLib` has a public header, library implementation/INF, DEC
   LibraryClass entry, Experimental DSC mapping/component, and App INF
   LibraryClass wiring where the App consumes the provider set.
2. The provider reports read-only capability and native HII entry hints only.
3. `ModernSetupApp` calls `ModernUiPcieDataGetSummary()` only from
   `ModernSetupAppProvider.c` when the App snapshot consumes PCIe data.
4. Smoke validation rejects PCIe-provider or App PCIe code that uses setup
   mutation tokens such as ConfigAccess routes, HII browser writes, variable
   writes, form updates, or BAR attribute programming.
5. Productization docs state that PCIe policy pages and any real policy changes
   remain native platform HII/FormBrowser responsibility.

## Phase 9 App information hierarchy

Current app/provider acceptance target:

- Title: `phase9(app): 增加 App 信息层级与分组提示`.
- Route: App shell and provider agents, with platform-ci for smoke validation.
- Labels: `area/app-provider`, `area/platform-ci` when mirrored to GitHub.
- Scope: add lightweight visual grouping to Dashboard Quick Access and provider
  summary pages so Firmware, Diagnostics, Management, Power, and Performance
  information reads as sections. Preserve existing pages, row labels/values,
  navigation behavior, provider snapshot boundary, and existing Present/Absent
  diagnostics text.
- Non-goals: no navigation-state rewrite, no provider model expansion, no public
  API/DEC change, no ConfigAccess/HII/variable writes, and no removal of current
  diagnostics rows or raw Present/Absent/Available/N/A text.
- Expected validation: `python3 Tests/Smoke/smoke_validate.py`,
  `git diff --check`, and an X64 CLANG app build when the local edk2 workspace is
  available.

Phase 9 is complete when:

1. Dashboard still exposes the same six selectable Quick Access cards while
   adding visible group labels such as Boot & Devices, Platform Health, and
   Power & Performance.
2. Provider pages show lightweight subsection labels for Firmware, Diagnostics,
   Management, Power, and Performance without deleting existing rows.
3. Presentation modules continue to consume `MODERN_SETUP_PROVIDER_SNAPSHOT`,
   with direct provider LibraryClass calls centralized in `ModernSetupAppProvider.c`.
4. Smoke validation continues to reject direct setup mutation paths and direct
   provider calls from pages, without requiring removal of diagnostic text.

## Phase 10 Dashboard category landing

Current app/provider acceptance target:

- Title: `phase10(app): Dashboard 作为设置分类入口`.
- Route: App shell and provider agents, with platform-ci for smoke/build validation.
- Labels: `area/app-provider`, `area/platform-ci` when mirrored to GitHub.
- Scope: keep the existing six Dashboard cards and destinations, but reframe the
  surface as Setup Categories/设置分类. Centralize the card-to-page route table in
  app-private actions code, preserve Boot/Devices content focus and other
  destinations' navigation focus, and add only a small localized open/enter
  affordance that fits the existing layout.
- Non-goals: no big tree-navigation rewrite, no diagnostics cleanup/removal, no
  writable setup config, no ConfigAccess/HII varstore writes, and no public
  provider model expansion.
- Expected validation: `python3 Tests/Smoke/smoke_validate.py`,
  `git diff --check`, and an X64 CLANG app build when the local edk2 workspace is
  available.

Phase 10 is complete when:

1. Dashboard visible copy says Setup Categories/设置分类 for the landing area.
2. Enter routing for the six cards uses a shared app-private helper/table instead
   of hard-coded `if/else` in the main loop.
3. Boot and Devices routes still land in content focus; Diagnostics, Firmware,
   Power, and Performance keep navigation focus.
4. Smoke validation covers the helper/route contract without brittle label
   removal checks.

## Later backlog themes

Keep later work grouped by feature phase rather than creating one issue per file move:

- DisplayEngine/FormBrowser compatibility checklist and native setup-page coverage.
- Core API surface inventory when public headers, DEC entries, GUIDs, PCDs, or LibraryClasses change.
- Renderer/theme/layout validation once shared visual contracts or layout libraries change.
- Experimental HII bridge promotion only after explicit core-api and display-engine review.
- Platform CI validation catalog for checks that can run without private firmware infrastructure.
