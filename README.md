# ModernSetupPkg

ModernSetupPkg is an experimental edk2 package for a modern graphical firmware
setup shell. The first target is ArmVirtQemu on macOS/Apple Silicon; LoongArch
integration is planned after the ArmVirt prototype is stable.

The UI intentionally uses only open source edk2 interfaces and original visual
assets. Commercial IBV firmware screens are treated only as visual and
interaction references.

## Current Scope

- GOP-based rendering through `ModernUiRendererLib`
- `ModernDisplayEngineDxe`, an edk2 DisplayEngine-compatible GOP frontend for
  `SetupBrowserDxe/FormBrowser2`
- Simplified Chinese UI strings by default, with English fallback strings
- Minimal built-in 18px anti-aliased glyphs generated from Noto Sans CJK SC
  Regular
- Keyboard navigation through `EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL`
- Optional pointer polling through `EFI_ABSOLUTE_POINTER_PROTOCOL`
- Native edk2 HII/IFR/VFR parsing, GUID formset discovery, ConfigAccess,
  callback, condition, and variable write handling through the existing
  FormBrowser stack
- A standalone `ModernSetupApp` and `ModernUiHiiBridgeLib` retained as prototype
  and debug code, not as the default setup compatibility path
- A static GUID page adapter registry for future OEM/IBV formset-specific pages
- ArmVirtQemu overlay scripts that keep upstream `ArmVirtPkg` files unchanged
- Development rules for function contracts, multi-architecture extension points,
  and IBV-friendly adaptation

The default ArmVirt path is now compatibility-first: edk2 still owns HII parsing
and setup semantics, while ModernSetup replaces the display engine drawing
backend. The custom HII bridge remains useful for experiments, but it is not the
main route for Device Manager, DriverSample, Boot Maintenance, or third-party
HII driver pages.

## Architecture

Current code and planned extension points are separated below. The core engine
now follows the same separation used by IBV-style setup stacks: FormBrowser
semantics, DisplayEngine frontend, renderer, theme/input, and optional
GUID-bound page adaptation.

```text
edk2 workspace
|
+-- ModernSetupPkg
    |
    +-- Package metadata
    |   |
    |   +-- ModernSetupPkg.dec
    |   +-- ModernSetupPkg.dsc
    |
    +-- Universal
    |   |
    |   +-- ModernDisplayEngineDxe
    |       |
    |       +-- EDKII_FORM_DISPLAY_ENGINE_PROTOCOL producer
    |       +-- EFI_HII_POPUP_PROTOCOL producer
    |       +-- edk2 DisplayEngine behavior
    |       +-- GOP-backed customized drawing backend
    |
    +-- Application
    |   |
    |   +-- ModernSetupApp      (legacy prototype, not default ArmVirt entry)
    |
    +-- Public interfaces
    |   |
    |   +-- Include/ModernUi/ModernUiRenderer.h
    |   +-- Include/ModernUi/ModernUiInput.h
    |   +-- Include/ModernUi/ModernUiTheme.h
    |   +-- Include/ModernUi/ModernUiString.h
    |   +-- Include/ModernUi/ModernUiHiiBridge.h
    |   +-- Include/ModernUi/ModernUiPageAdapter.h
    |
    +-- Implemented framework libraries
    |   |
    |   +-- ModernUiCustomizedDisplayLib
    |   |   |
    |   |   +-- edk2 CustomizedDisplayLib-compatible API
    |   |   +-- redirects DisplayEngine text cells to GOP drawing
    |   |
    |   +-- ModernUiRendererLib
    |   |   |
    |   |   +-- GOP framebuffer primitives
    |   |   +-- HII Font text rendering
    |   |   +-- built-in minimal CJK glyph fallback
    |   |
    |   +-- ModernUiInputLib
    |   |   |
    |   |   +-- SimpleTextInputEx keyboard mapping
    |   |   +-- AbsolutePointer optional pointer events
    |   |
    |   +-- ModernUiThemeLib
    |   |   |
    |   |   +-- Dark theme token table
    |   |
    |   +-- ModernUiStringLib
    |   |   |
    |   |   +-- zh-Hans / en-US setup strings
    |   |
    |   +-- ModernUiHiiBridgeLib
    |   |   |
    |   |   +-- HII handle enumeration
    |   |   +-- IFR subset parsing and ConfigAccess routing
    |   |   +-- optional debug/prototype FormSet GUID filtering
    |   |
    |   +-- ModernUiPageAdapterLib
    |       |
    |       +-- static FormSet GUID adapter registry
    |       +-- highest-priority adapter selection
    |       +-- generic HII fallback when no adapter is registered
    |
    +-- Planned framework libraries
    |   |
    |   +-- ModernUiLayoutLib        (planned)
    |   |   +-- resolution-aware layout
    |   |   +-- safe-area and scaling policy
    |   |
    |   +-- ModernUiPlatformLib      (planned)
    |   |   +-- platform identity
    |   |   +-- architecture and capability reporting
    |   |
    |   +-- ModernUiBootDataLib      (planned)
    |   |   +-- Boot#### / BootOrder provider
    |   |
    |   +-- ModernUiDeviceDataLib    (planned)
    |   |   +-- handle and device-path provider
    |   |
    |   +-- ModernUiSecurityDataLib  (planned)
    |       +-- Secure Boot and security state provider
    |
    +-- Platform integration
    |   |
    |   +-- Scripts/build-armvirt.sh
    |   |   |
    |   |   +-- generates Build/ModernSetupPkgOverlay/*.dsc/*.fdf
    |   |   +-- keeps upstream ArmVirtPkg files unchanged
    |   |
    |   +-- Scripts/run-armvirt.sh
    |       |
    |       +-- QEMU ArmVirt graphics validation
    |
    +-- Project records
    |   |
    |   +-- Assets/Fonts
    |   +-- Scripts/generate-font-glyphs.py
    |   +-- Docs/DEVELOPMENT.md
    |   +-- CHANGELOG.md
    |   +-- LICENSE
    |
    +-- Tests
        |
        +-- Manual/ArmVirtQemu.md
        +-- Smoke       (planned)
        +-- Unit        (planned)
```

The intended dependency direction is:

```text
Driver VFR / UNI / ConfigAccess
  |
  +--> HII database
        |
        +--> SetupBrowserDxe / FormBrowser2
               |
               +--> EDKII_FORM_DISPLAY_ENGINE_PROTOCOL
                      |
                      +--> ModernDisplayEngineDxe
                             |
                             +--> ModernUiCustomizedDisplayLib
                                    |
                                    +--> ModernUiRendererLib / Theme / Fonts
                                           |
                                           +--> EFI_GRAPHICS_OUTPUT_PROTOCOL
```

Platform-specific code should enter through provider libraries, PCDs, or overlay
DSC/FDF files. Page rendering code should remain architecture-neutral.

## GUID Page Adapter Engine

`ModernUiPageAdapterLib` provides the first static registry for IBV/OEM-style
custom setup pages. A platform can bind a modern renderer to a HII formset by
matching `FormSetGuid`; the adapter receives a neutral draw/input context and
can render a richer page while still using the same HII identity and firmware
protocol data.

The adapter registry is not in the default ArmVirt setup path yet. Native edk2
FormBrowser and `ModernDisplayEngineDxe` remain the compatibility baseline. The
registry is kept as an experiment for a future layer where selected
`FormSetGuid` pages can opt into richer modern rendering while unbound pages
fall back to the standard DisplayEngine model.

## Development Documents

- `Docs/DEVELOPMENT.md` defines coding rules, function comment requirements,
  architecture boundaries, and extension points.
- `CHANGELOG.md` records development progress, user-visible changes, and planned
  version work.
- `Tests/README.md` defines the test layout and current validation scope.

## Build and Run

Add this repository as a submodule at the root of an edk2 workspace:

```sh
git submodule add git@github.com:MarsDoge/ModernSetupPkg.git ModernSetupPkg
git submodule update --init --recursive
```

Build ArmVirtQemu:

```sh
ModernSetupPkg/Scripts/build-armvirt.sh
```

The ArmVirt overlay includes edk2 `DriverSampleDxe` by default so the native
Device Manager/FormBrowser path has a known VFR test target. To build without
that demo driver:

```sh
MODERN_SETUP_DEMO_DRIVER_SAMPLE=0 ModernSetupPkg/Scripts/build-armvirt.sh
```

The prototype `ModernSetupApp` default language is Simplified Chinese. To build
that prototype app with English as its first-boot fallback:

```sh
MODERN_SETUP_LANGUAGE=en-US ModernSetupPkg/Scripts/build-armvirt.sh
```

`ModernSetupApp` can still switch language from its Exit page when launched
manually. The default ArmVirt setup path uses the native UiApp/FormBrowser entry
and receives strings from the HII packages and language selection handled by
edk2.

The renderer asks GOP for a larger display mode during initialization. If the
firmware exposes a suitable mode, it switches away from small 800x600 defaults
to at least 1024x768.

Run with graphics:

```sh
GRAPHICS=1 RESET_VARS=1 ModernSetupPkg/Scripts/run-armvirt.sh
```

Click the QEMU window and press `Esc` or `F2` during BDS wait to enter the
native UiApp firmware setup. Rendering is handled by `ModernDisplayEngineDxe`.

## Fonts and Localization

ModernSetupPkg keeps UI strings in `ModernUiStringLib`. The default language is
controlled by `gModernSetupPkgTokenSpaceGuid.PcdModernSetupDefaultLanguage`,
which defaults to `zh-Hans`. At runtime, `ModernUiStringLib` first checks the
non-volatile `ModernSetupLanguage` variable and falls back to the PCD when the
variable is missing or unsupported.

Chinese glyphs do not depend on platform firmware fonts. A minimal bitmap table
is generated from Noto Sans CJK SC Regular and compiled into
`ModernUiRendererLib`; the current table uses 18px anti-aliased glyphs for the
ModernSetup UI strings and selected DriverSample `.uni` strings. ASCII-only text
can still use edk2 HII Font rendering. The full font file is not committed. See
`Assets/Fonts/README.md` for source, license, and regeneration details.

## DisplayEngine Path

`DriverSampleDxe` remains the first compatibility target. The ArmVirt overlay
builds the driver without modifying its `.vfr`, `.uni`, or C source, then edk2
registers the formsets in the HII database. Native `SetupBrowserDxe` and
`FormBrowser2` enumerate the formsets, parse IFR, evaluate conditions, call
ConfigAccess callbacks, and perform writes. `ModernDisplayEngineDxe` receives
the prepared `FORM_DISPLAY_ENGINE_FORM` and statement model and only changes how
the UI is drawn.

This is the intended long-term architecture: keep the edk2 HII contract intact,
replace the old text display backend with a modern GOP surface, and then improve
visual styling inside the DisplayEngine/customized display layer.

## Visual Showcase

The current ArmVirt prototype uses a dense firmware-setup layout: a dark canvas,
a top status bar, horizontal page tabs, raised content panels, setting rows, and
keyboard-first interaction. The style is original, but it intentionally follows
the interaction direction common in high-end UEFI utilities: clear mode/status
signals, strong active-page affordance, and compact settings lists rather than a
legacy text-only form browser.

Screenshots for GitHub presentation belong under `Assets/Screenshots/`. Keep
captures focused on ModernSetup itself, not vendor firmware screens or copied
assets. Recommended first captures:

- `armvirt-uiapp-frontpage.png` - native UiApp rendered by ModernDisplayEngine.
- `armvirt-device-manager.png` - Device Manager showing automatically loaded
  HII driver pages.
- `armvirt-driver-sample.png` - DriverSample rendered through native
  FormBrowser plus ModernDisplayEngine.

Run the ArmVirt graphics command in the Build and Run section, switch QEMU to
the target page, then capture the window at 1024x768 or larger for README and
GitHub repository presentation.
