# ModernSetupPkg Development Guide

ModernSetupPkg is intended to become a reusable modern setup UI framework plus a
default setup application. New code should keep ArmVirt useful as the first test
target, but must not bake ArmVirt, QEMU, AArch64, LoongArch, x86, or any IBV
policy into the UI core.

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
- Platform and IBV differences should be introduced through LibraryClass
  instances, PCDs, or small platform overlays.
- Renderer APIs must hide the concrete graphics backend. GOP is the first
  backend, not the app-level abstraction.
- Input APIs must expose UI events, not raw keyboard or pointer protocols.
- Theme values must be tokenized. Page code must not hard-code vendor colors,
  fonts, logos, or image assets.
- Layout code must derive positions from resolution and safe-area data. New UI
  must be usable without overlap at 800x600, 1024x768, and 1280x800.
- HII/FormBrowser should not be replaced wholesale in early versions. Keep room
  for a HII bridge so existing platform setup data, strings, permissions, and
  localization can be reused.
- Commercial firmware screens may be used only as visual and interaction
  references. Do not copy closed-source code, fonts, icons, images, layouts, or
  proprietary assets.

## Preferred Extension Points

Future architecture and IBV adaptation should prefer these layers:

- `ModernUiRendererLib` for drawing primitives and backend adaptation.
- `ModernUiInputLib` for keyboard, pointer, touch, and serial event mapping.
- `ModernUiThemeLib` for style tokens and vendor/theme selection.
- `ModernUiLayoutLib` for resolution-aware geometry.
- `ModernUiPlatformLib` for platform identity and capability reporting.
- `ModernUiBootDataLib` for Boot#### and BootOrder access.
- `ModernUiDeviceDataLib` for handle/device-path inventory.
- `ModernUiSecurityDataLib` for Secure Boot and related security state.

The current prototype does not have all of these libraries yet. When code starts
to grow around one of these responsibilities, add the library instead of
expanding `ModernSetupApp` directly.

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
