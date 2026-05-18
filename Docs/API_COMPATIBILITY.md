<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# API Compatibility

This policy applies to public contracts in `Include/ModernUi/*.h`, `ModernSetupPkg.dec`, and documented LibraryClass/GUID/PCD behavior. It is meant to keep contributors safe without adding heavy process.

## Default rule: additive first

- Prefer new functions, optional fields, new enum values, new LibraryClasses, or new PCDs over changing existing behavior in place.
- Do not remove or rename public APIs without a documented deprecation path and core-api review.
- Public comments must document null handling, buffer ownership, side effects, important preconditions, and failure behavior.
- PCD defaults should be safe for ArmVirt and LoongArchVirt unless platform overlays override them.

## Headers and DEC files

Header or DEC changes need:

1. Core API review.
2. Review from the owner of the implementation or consumer being changed.
3. A short changelog note when the public contract, build integration, or compatibility behavior changes.

New DEC entries should state whether they are a LibraryClass, GUID, PCD, include path, or package-level integration point.

## Structs, enums, and versioning

- Append struct fields at the end when layout or ABI stability may matter; do not reorder existing fields.
- Append enum values; do not renumber values used by persisted state, logs, tests, or external consumers.
- For structs likely to cross module or binary boundaries, use a `Size`, `Version`, or `V2` pattern instead of silently changing layout assumptions.
- Reserve and preserve unknown flag bits where possible.
- If an incompatible break is unavoidable, document the reason, migration path, and affected consumers.

## Deprecation

- Deprecated APIs should remain buildable for at least one release track unless they are unsafe or isolated experimental HII bridge surfaces.
- Deprecation comments should name the replacement and expected removal track.
- Compatibility-sensitive changes should keep native FormBrowser fallback behavior available.

## Changelog expectations

Use `CHANGELOG.md` for user-visible or integration-visible impact. Group entries by whichever buckets apply:

- Public API / DEC
- DisplayEngine compatibility
- App shell / providers
- Renderer / theme / layout
- Experimental HII bridge
- Platform / CI / validation

Docs-only edits do not need a changelog entry unless they change documented policy or contributor workflow.
