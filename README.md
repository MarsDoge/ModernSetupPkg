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

This is not a full HII/FormBrowser replacement. The v1 goal is a usable modern
setup shell that can be launched from the firmware boot manager path.

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
