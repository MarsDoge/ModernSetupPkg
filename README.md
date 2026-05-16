# ModernSetupPkg

ModernSetupPkg is an experimental edk2 package for a modern graphical firmware
setup shell. The first target is ArmVirtQemu on macOS/Apple Silicon; LoongArch
integration is planned after the ArmVirt prototype is stable.

The UI intentionally uses only open source edk2 interfaces and original visual
assets. Commercial IBV firmware screens are treated only as visual and
interaction references.

## Current Scope

- GOP-based rendering through `ModernUiRendererLib`
- Keyboard navigation through `EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL`
- Optional pointer polling through `EFI_ABSOLUTE_POINTER_PROTOCOL`
- A standalone `ModernSetupApp` with Dashboard, Boot, Devices, Security, and Exit pages
- ArmVirtQemu overlay scripts that keep upstream `ArmVirtPkg` files unchanged
- Development rules for function contracts, multi-architecture extension points,
  and IBV-friendly adaptation

This is not a full HII/FormBrowser replacement. The v1 goal is a usable modern
setup shell that can be launched from the firmware boot manager path.

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
    |       +-- Exit page       (continue, reset, UiApp fallback)
    |
    +-- Public interfaces
    |   |
    |   +-- Include/ModernUi/ModernUiRenderer.h
    |   +-- Include/ModernUi/ModernUiInput.h
    |   +-- Include/ModernUi/ModernUiTheme.h
    |
    +-- Implemented framework libraries
    |   |
    |   +-- ModernUiRendererLib
    |   |   |
    |   |   +-- GOP framebuffer primitives
    |   |   +-- HII Font text rendering
    |   |
    |   +-- ModernUiInputLib
    |   |   |
    |   |   +-- SimpleTextInputEx keyboard mapping
    |   |   +-- AbsolutePointer optional pointer events
    |   |
    |   +-- ModernUiThemeLib
    |       |
    |       +-- Dark theme token table
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
    |   |   +-- Secure Boot and security state provider
    |   |
    |   +-- ModernUiHiiBridgeLib     (planned)
    |       |
    |       +-- existing HII strings and setup data bridge
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

Run with graphics:

```sh
GRAPHICS=1 RESET_VARS=1 ModernSetupPkg/Scripts/run-armvirt.sh
```

Click the QEMU window and press `Esc` or `F2` during BDS wait to enter the
ModernSetupApp boot manager menu.
