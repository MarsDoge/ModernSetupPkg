<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# ModernSetupPkg Development Guide

ModernSetupPkg is intended to become a reusable modern setup UI framework plus a
default setup application. New code should keep ArmVirt useful as the first test
target, but must not bake ArmVirt, QEMU, AArch64, LoongArch, x86, or any IBV
policy into the UI core.

## New contributor quickstart

1. Identify the area you are changing in `Docs/AGENT_OWNERSHIP.md`.
2. Check `Docs/MODULE_BOUNDARIES.md` before moving behavior between layers.
3. If you touch `Include/ModernUi/*.h` or `ModernSetupPkg.dec`, follow
   `Docs/API_COMPATIBILITY.md` and request Core API review.
4. Keep changes focused. Docs-only and small script fixes should not need the
   full firmware checklist; mark unrelated PR-template items as N/A.
5. Record validation in the PR. If QEMU or a platform target is unavailable,
   say so and describe the closest validation you did run.

## Function Contracts

Every function must have an edk2-style Doxygen comment before its declaration or
definition. Public functions in headers must document the stable interface.
Private functions in C files must document the local contract.

Each function comment must cover:

- Expected input values for every `@param[in]` and `@param[in,out]`.
- Expected output values for every `@param[out]` and `@param[in,out]`.
- Whether `NULL` is accepted.
- Buffer size requirements and ownership transfer rules.
- Protocol, PCD, global state, or initialization preconditions.
- Return value semantics, including the important failure codes.
- Whether partial drawing, partial state updates, or unchanged state are expected
  when the function fails.

Use this shape:

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

## Architecture Rules

- UI pages consume structured data models. They should not directly own platform
  enumeration, variable parsing, security policy, or architecture-specific
  behavior.
- `ModernUiEngineLib` is the shared visual contract between the native
  DisplayEngine path and standard front-page app path. DisplayEngine adapters and app
  pages should build engine models instead of drawing their own tabs, rows,
  value selectors, popups, or footers.
- `ModernSetupApp` is a standard front-page shell. It may own navigation,
  language/theme state, dashboard composition, boot selection, and entry points,
  but it must not parse IFR, evaluate VFR conditions, call ConfigAccess
  directly, or write HII varstores.
- New App-facing product features must first be categorized in
  `Docs/ProductizationFeatureMatrix.md`. If a feature is platform-private or
  policy-heavy, the App should expose a summary or entry point and hand control
  to the owning HII page.
- App entries for real setup pages must use `EFI_FORM_BROWSER2_PROTOCOL.SendForm()`.
  That keeps GUID formset handling, callbacks, defaults, validation, and
  variable routing inside native edk2 FormBrowser.
- Platform and IBV differences should be introduced through LibraryClass
  instances, PCDs, or small platform overlays.
- Renderer APIs must hide the concrete graphics backend. GOP is the first
  backend, not the app-level abstraction.
- Input APIs must expose UI events, not raw keyboard or pointer protocols.
- Theme values must be tokenized. Page code must not hard-code vendor colors,
  fonts, logos, or image assets.
- Layout code must derive positions from resolution and safe-area data. New UI
  must be usable without overlap at 800x600, 1024x768, and 1280x800.
- HII/FormBrowser should not be replaced wholesale in early versions. Keep the
  classic UiApp/FormBrowser path available while ModernSetup is still proving
  compatibility with existing VFR/HII data and EFI applications that depend on
  the legacy display stack.
- The modern HII bridge may become the only setup engine only after it supports
  the real platform form contract: form navigation, question rendering,
  defaults, validation, expressions, callback policy, varstore routing,
  localization, and safe failure reporting.
- New HII bridge code must fail closed. Unsupported opcodes, unknown varstore
  paths, callback-driven questions, or unevaluated expressions should render as
  read-only/fallback rows instead of forcing writes around the owning driver.
- Commercial firmware screens may be used only as visual and interaction
  references. Do not copy closed-source code, fonts, icons, images, layouts, or
  proprietary assets.

## Preferred Extension Points

Future architecture and IBV adaptation should prefer these layers:

- `ModernUiRendererLib` for drawing primitives and backend adaptation.
- `ModernUiEngineLib` for reusable page chrome, layout, tab, row, value,
  popup, footer, help panel, and right-rail drawing models.
- `ModernUiInputLib` for keyboard, pointer, touch, and serial event mapping.
- `ModernUiThemeLib` for style tokens and vendor/theme selection.
- `ModernUiLayoutLib` for resolution-aware geometry.
- `ModernUiPlatformLib` for platform identity and capability reporting.
- `ModernUiBootDataLib` for Boot#### and BootOrder access.
- `ModernUiDeviceDataLib` for handle/device-path inventory.
- `ModernUiSecurityDataLib` for Secure Boot and related security state.
- `ModernUiDeviceDataLib` should expose FormBrowser entry points, not decoded
  IFR controls.
- App provider libraries follow the same split: `ModernUiFirmwareDataLib` for
  capsule/update state, `ModernUiDiagnosticsDataLib` for logs and bring-up
  health, and `ModernUiManagementDataLib` for BMC/IPMI/Redfish-style management
  summaries.

The current prototype does not have all of these libraries yet. When code starts
to grow around one of these responsibilities, add or extend the matching shared
library instead of expanding `ModernSetupApp` or `ModernDisplayEngineDxe`
directly.

## Multi-agent Maintenance

Phase 1 collaboration scaffolding lives in:

- `Docs/AGENT_OWNERSHIP.md` for module owners, labels, and review gates.
- `Docs/MODULE_BOUNDARIES.md` for stable contracts and dependency rules.
- `Docs/API_COMPATIBILITY.md` for public API, DEC, and deprecation policy.

Use these before expanding shared headers, `ModernSetupPkg.dec`, DisplayEngine
behavior, provider contracts, or experimental HII bridge surfaces.

## Validation matrix

Use the lightest validation that proves the changed area. Mark unrelated items
as N/A in the PR.

| Change type | Expected validation |
| --- | --- |
| Docs-only | Spell/link sanity and affected policy owner review when policy changes. |
| Public API / DEC | Header/DEC review, affected libraries build or compile-plan noted, changelog for public impact. |
| Renderer / theme / layout | Build or focused smoke test, plus notes for 800x600, 1024x768, and 1280x800 impact. |
| DisplayEngine / HII | Native FormBrowser compatibility check, fallback behavior notes, no bypassed ConfigAccess/varstore writes. |
| App / provider | App smoke path or provider contract check; real setup pages still use `SendForm()`. |
| Platform / scripts | Script dry run or syntax/shellcheck-style review, target platform named, generated-file impact noted. |

QEMU validation is preferred for firmware behavior changes, but unavailable QEMU
is not a blocker for every PR. State what was unavailable and what substitute
validation was performed.

## Change Discipline

- Update `CHANGELOG.md` for user-visible behavior, architecture decisions, build
  flow changes, or platform support changes.
- Update `Tests/` when behavior changes. UI interaction changes should update
  manual QEMU checks; provider, layout, or parser changes should add or update
  smoke/unit tests when those layers exist.
- Keep ArmVirt overlay scripts non-invasive. They may generate files under
  `Build/ModernSetupPkgOverlay`, but must not edit upstream `ArmVirtPkg` files.
- Keep the package buildable as an edk2 package in a workspace submodule.
- Prefer focused commits that separate framework changes, app changes, platform
  overlays, and documentation.
