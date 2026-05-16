# ModernSetupPkg

ModernSetupPkg is an experimental edk2 package for a modern graphical firmware
setup shell. The first target is ArmVirtQemu on macOS/Apple Silicon; LoongArch
integration is planned after the ArmVirt prototype is stable.

The UI intentionally uses only open source edk2 interfaces and original visual
assets. Commercial IBV firmware screens are treated only as visual and
interaction references.

## Current Scope

- GOP-based rendering through `ModernUiRendererLib`
- Simplified Chinese UI strings by default, with English fallback strings
- Minimal built-in 18px anti-aliased glyphs generated from Noto Sans CJK SC
  Regular
- Keyboard navigation through `EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL`
- Optional pointer polling through `EFI_ABSOLUTE_POINTER_PROTOCOL`
- A standalone `ModernSetupApp` with Dashboard, Boot, Devices, Security, HII,
  and Exit pages
- A first-stage HII/IFR bridge demo that renders edk2 DriverSample VFR pages as
  ModernSetup subpages
- ArmVirtQemu overlay scripts that keep upstream `ArmVirtPkg` files unchanged
- Development rules for function contracts, multi-architecture extension points,
  and IBV-friendly adaptation

This is not a full HII/FormBrowser replacement. The v1 goal is a usable modern
setup shell that can launch from the firmware boot manager path and prove that
existing HII/VFR content can be bridged incrementally. During early development,
the classic edk2 UiApp/FormBrowser path is intentionally kept as a fallback for
EFI applications and platform flows that still depend on the legacy display
stack. ModernSetup should become the only setup engine only after it can render,
navigate, validate, and safely route existing VFR/HII data with production-level
compatibility.

## Architecture

Current code and planned extension points are separated below. Boxes marked
`planned` are the intended direction for multi-architecture and IBV adaptation,
but are not implemented yet.

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
    +-- Application
    |   |
    |   +-- ModernSetupApp
    |       |
    |       +-- Dashboard page
    |       +-- Boot page       (read-only v1)
    |       +-- Devices page    (read-only v1)
    |       +-- Security page   (read-only v1)
    |       +-- HII page        (DriverSample bridge demo)
    |       +-- Exit page       (continue, reset, UiApp fallback)
    |
    +-- Public interfaces
    |   |
    |   +-- Include/ModernUi/ModernUiRenderer.h
    |   +-- Include/ModernUi/ModernUiInput.h
    |   +-- Include/ModernUi/ModernUiTheme.h
    |   +-- Include/ModernUi/ModernUiString.h
    |   +-- Include/ModernUi/ModernUiHiiBridge.h
    |
    +-- Implemented framework libraries
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
    |       |
    |       +-- DriverSample HII handle enumeration
    |       +-- IFR subset parsing and ConfigAccess routing
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
ModernSetupApp
  |
  +--> UI framework libraries
  |      |
  |      +--> renderer / input / theme / layout
  |
  +--> data provider libraries
         |
         +--> boot / device / security / platform / HII bridge
                |
                +--> edk2 protocols, variables, PCDs, and platform overrides
```

Platform-specific code should enter through provider libraries, PCDs, or overlay
DSC/FDF files. Page rendering code should remain architecture-neutral.

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

The ArmVirt overlay includes edk2 `DriverSampleDxe` by default so the HII bridge
demo has VFR content to render. To build without that demo driver:

```sh
MODERN_SETUP_DEMO_DRIVER_SAMPLE=0 ModernSetupPkg/Scripts/build-armvirt.sh
```

The default UI language is Simplified Chinese. To build the ArmVirt overlay with
English as the first-boot fallback:

```sh
MODERN_SETUP_LANGUAGE=en-US ModernSetupPkg/Scripts/build-armvirt.sh
```

The running UI can also switch language from the Exit page. Select the
`Language` row and press `Enter` to open the language drop-down, choose
`Chinese` or `English`, then press `Enter` again. ModernSetup updates the screen
immediately and persists the choice in the `ModernSetupLanguage` UEFI variable.
The runtime variable takes precedence over the build-time PCD on later boots.

ModernSetup asks GOP for a larger display mode during renderer initialization.
If the firmware exposes a suitable mode, it switches away from small 800x600
defaults to at least 1024x768.

Run with graphics:

```sh
GRAPHICS=1 RESET_VARS=1 ModernSetupPkg/Scripts/run-armvirt.sh
```

Click the QEMU window and press `Esc` or `F2` during BDS wait to enter the
ModernSetupApp boot manager menu.

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

## HII Bridge Demo

`ModernUiHiiBridgeLib` is intentionally scoped to `DriverSampleDxe` in this
first demo. The ArmVirt overlay builds the driver without modifying its `.vfr`,
`.uni`, or C source, then ModernSetup enumerates the runtime HII database and
renders the DriverSample formsets under the HII tab.

The v1 bridge parses a practical IFR subset: formset, form, subtitle, text,
goto/ref, checkbox, one-of, one-of option, numeric, and string. Checkbox,
one-of, and numeric questions backed by buffer varstores can be advanced through
the driver's `EFI_HII_CONFIG_ACCESS_PROTOCOL.RouteConfig()` when they are not
read-only and not callback-driven. String and complex controls are displayed as
read-only until ModernSetup grows a text editor and fuller FormBrowser behavior.

The compatibility policy is two-track: keep the classic FormBrowser available
while ModernSetup learns enough VFR/HII semantics, then reduce the legacy path
only when the modern engine can cover real platform forms without data loss,
incorrect writes, missing validation, or broken callbacks.

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

- `armvirt-dashboard.png` - first screen after entering ModernSetup.
- `armvirt-hii-driver-sample.png` - DriverSample VFR bridge rendered as a
  ModernSetup subpage.
- `armvirt-exit-language-dropdown.png` - runtime Chinese/English language
  selector.

Run the ArmVirt graphics command in the Build and Run section, switch QEMU to
the target page, then capture the window at 1024x768 or larger for README and
GitHub repository presentation.
