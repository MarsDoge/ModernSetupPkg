<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Contributing

Thanks for helping ModernSetupPkg. Keep PRs focused and developer-friendly.

## Quick path

1. Pick the closest area in `Docs/AGENT_OWNERSHIP.md`.
2. Check `Docs/MODULE_BOUNDARIES.md` before moving responsibilities between modules.
3. For public `Include/ModernUi/*.h` or `ModernSetupPkg.dec` changes, follow `Docs/API_COMPATIBILITY.md`.
4. Run the lightest useful validation from `Docs/DEVELOPMENT.md`.
5. In the PR template, mark unrelated firmware-safety items as N/A.

Docs-only PRs normally need only review/format sanity. Firmware behavior PRs should include build, script, or QEMU/manual notes; QEMU unavailable is acceptable if explained.
