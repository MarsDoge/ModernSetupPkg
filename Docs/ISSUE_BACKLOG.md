<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Issue Backlog and Agent Routing

This backlog is the lightweight landing page for multi-agent work. It records issue seeds that are useful to open or claim later, but it is not a substitute for maintainer judgment. Keep issues focused, route them through `Docs/AGENT_OWNERSHIP.md`, and prefer small PRs that can be validated independently.

Do not create GitHub issues from this file automatically unless a maintainer asks for that explicitly. When an issue is opened, copy the relevant seed, apply the matching labels from `.github/labels.yml`, and link back to this document if the issue is part of the PR2/PR3/PR4 sequence.

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

## PR sequence

- PR2: Governance landing and backlog. Align labels and issue templates, add this backlog, and document low-friction manual label sync.
- PR3: Contributor intake cleanup. Open or refine the first real GitHub issues from these seeds, then update module request guidance based on maintainer feedback.
- PR4: Validation scaffolding. Add safe checks that can run without private infrastructure, such as documentation linting, YAML validation, and focused script dry runs.

Keep PR2/PR3/PR4 docs/tooling-oriented unless a maintainer explicitly expands the scope.

## Initial issue seeds

### Seed: Core API surface inventory

- Route: Core API agent.
- Labels: `area/core-api`, `documentation`, `api-change` if public contract changes are proposed.
- Goal: Inventory public headers and DEC entries, then identify which contracts need stability notes, versioning, or deprecation guidance.
- Expected validation: Header/DEC review and docs sanity. No firmware build required for inventory-only work.

### Seed: Renderer and engine responsibility map

- Route: Renderer/theme/UI engine agents.
- Labels: `area/renderer`, `documentation`.
- Goal: Map renderer, theme, layout, input, and engine responsibilities so future page work lands in the right library.
- Expected validation: Docs sanity and cross-check against `Docs/MODULE_BOUNDARIES.md`.

### Seed: DisplayEngine compatibility checklist

- Route: DisplayEngine compatibility agent.
- Labels: `area/display-engine`, `compat`, `documentation`.
- Goal: Draft a checklist for native FormBrowser compatibility, fallback behavior, and prohibited ConfigAccess/varstore bypasses.
- Expected validation: Docs sanity and owner review. QEMU/manual validation is N/A until behavior changes are proposed.

### Seed: App/provider dashboard contract plan

- Route: App shell and provider agents.
- Labels: `area/app-provider`, `module-request`.
- Goal: Propose focused provider contracts for dashboard summaries without moving platform policy into `ModernSetupApp`.
- Expected validation: Boundary review, affected public model notes, and app smoke plan if code follows.

### Seed: Experimental HII bridge safety notes

- Route: Experimental HII bridge agent.
- Labels: `area/hii-bridge`, `compat`, `documentation`.
- Goal: Document unsupported opcode, varstore, callback, expression, localization, and fail-closed/read-only behavior expectations.
- Expected validation: Docs sanity and DisplayEngine/Core API cross-review if promotion is discussed.

### Seed: Platform CI validation catalog

- Route: Platform/CI/release agent.
- Labels: `area/platform-ci`, `documentation`.
- Goal: Catalog safe local checks for docs, YAML, scripts, and generated-file impact without requiring private infrastructure.
- Expected validation: Run the documented local commands where available and record unavailable tools.

### Seed: Label and issue template drift check

- Route: Docs/governance through platform-ci.
- Labels: `area/docs`, `documentation`, `module-request` if template fields change.
- Goal: Keep `.github/labels.yml`, issue templates, ownership docs, and this backlog synchronized.
- Expected validation: YAML parse, markdown sanity, privacy scan, and `git diff --check`.
