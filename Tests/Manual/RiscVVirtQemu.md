<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# RiscVVirtQemu Build Validation

This page covers the local RISC-V/RiscVVirt build-validation path for
ModernSetupPkg. The current scope is overlay generation and optional firmware
compile against an edk2 workspace; graphical QEMU validation is future/manual
work unless a local maintainer adds a run script.

## Workspace layout

Use an edk2 workspace that contains `MdePkg`, `OvmfPkg/RiscVVirt`, and a checkout
or symlink of `ModernSetupPkg` at the workspace root:

```text
edk2/
|-- MdePkg/
|-- OvmfPkg/RiscVVirt/
`-- ModernSetupPkg/
```

The build script refuses to run if `ModernSetupPkg` is not located at
`${WORKSPACE}/ModernSetupPkg`.

## Overlay generation only

Use `GENERATE_ONLY=1` when validating anchors or reviewing generated DSC/FDF
without requiring a RISC-V toolchain:

```sh
cd /path/to/edk2
WORKSPACE=$PWD GENERATE_ONLY=1 ModernSetupPkg/Scripts/build-riscvvirt.sh
```

Generated files:

```text
Build/ModernSetupPkgOverlay/RiscVVirtQemuModernSetup.dsc
Build/ModernSetupPkgOverlay/RiscVVirtQemuModernSetup.fdf
```

The default mode is `MODERN_SETUP_DISPLAY_ENGINE=modern`. It replaces the native
edk2 `DisplayEngineDxe` entry with `ModernDisplayEngineDxe`, wires the shared
Modern UI engine/renderer/theme libraries, and sets `PcdModernSetupTheme`.

For native before/after comparison, generate an overlay that keeps the edk2
DisplayEngine path unchanged:

```sh
WORKSPACE=$PWD GENERATE_ONLY=1 MODERN_SETUP_DISPLAY_ENGINE=native \
  ModernSetupPkg/Scripts/build-riscvvirt.sh
```

Theme selection:

```sh
MODERN_SETUP_THEME=orange ModernSetupPkg/Scripts/build-riscvvirt.sh
MODERN_SETUP_THEME=red    ModernSetupPkg/Scripts/build-riscvvirt.sh
```

Supported values are `orange` and `red`.

## Optional full build

A real compile requires a RISC-V GCC/binutils cross toolchain. The script defaults
to `GCC_RISCV64_PREFIX=riscv64-linux-gnu-` and fails clearly if
`${GCC_RISCV64_PREFIX}gcc` or `${GCC_RISCV64_PREFIX}objcopy` is not on `PATH`.

```sh
cd /path/to/edk2
export WORKSPACE=$PWD
export GCC_RISCV64_PREFIX=riscv64-linux-gnu-
ModernSetupPkg/Scripts/build-riscvvirt.sh
```

Optional knobs:

```sh
TARGET=RELEASE
TOOL_CHAIN_TAG=GCC
JOBS=8
MODERN_SETUP_DISPLAY_ENGINE=modern   # or native
MODERN_SETUP_THEME=orange            # or red
```

The build command issued by the script is:

```sh
build -a RISCV64 -t ${TOOL_CHAIN_TAG} \
  -p Build/ModernSetupPkgOverlay/RiscVVirtQemuModernSetup.dsc \
  -b ${TARGET} -n ${JOBS}
```

Expected firmware outputs follow the RiscVVirtQemu platform output directory:

```text
Build/RiscVVirtQemu/${TARGET}_${TOOL_CHAIN_TAG}/FV/RISCV_VIRT_CODE.fd
Build/RiscVVirtQemu/${TARGET}_${TOOL_CHAIN_TAG}/FV/RISCV_VIRT_VARS.fd
```

## Current limitations

- This is build/script validation only; no RISC-V QEMU graphics run script is
  provided yet.
- The default overlay does not include the experimental `ModernSetupApp` path or
  custom HII bridge/page-adapter libraries.
- If upstream edk2 changes the RiscVVirt DisplayEngine, CustomizedDisplayLib, or
  include anchors, the script fails during overlay generation rather than
  silently producing a partial overlay.
