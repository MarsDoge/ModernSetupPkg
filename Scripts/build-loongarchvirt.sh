#!/usr/bin/env bash
# Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
# Author: MarsDoge (Dongyan Qian)
# Open source: https://github.com/MarsDoge/ModernSetupPkg
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
set -euo pipefail

PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "${PKG_DIR}/Scripts/edk2-workspace.sh"
WORKSPACE="$(DetectWorkspace)"
TARGET="${TARGET:-DEBUG}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
MODERN_SETUP_DEMO_DRIVER_SAMPLE="${MODERN_SETUP_DEMO_DRIVER_SAMPLE:-1}"
MODERN_SETUP_THEME="${MODERN_SETUP_THEME:-orange}"
MODERN_SETUP_DISPLAY_ENGINE="${MODERN_SETUP_DISPLAY_ENGINE:-modern}"
GCC_LOONGARCH64_PREFIX="${GCC_LOONGARCH64_PREFIX:-loongarch64-unknown-linux-gnu-}"
GENERATE_ONLY="${GENERATE_ONLY:-0}"
OVERLAY_DIR="${WORKSPACE}/Build/ModernSetupPkgOverlay"

export PATH="/opt/homebrew/bin:${PATH}"
export WORKSPACE
export GCC_LOONGARCH64_PREFIX
ConfigureModernSetupPackagePath

if [[ ! -d "${WORKSPACE}/MdePkg" || ! -d "${WORKSPACE}/OvmfPkg/LoongArchVirt" ]]; then
  echo "WORKSPACE does not look like an edk2 checkout with LoongArchVirt: ${WORKSPACE}" >&2
  exit 1
fi

mkdir -p "${OVERLAY_DIR}"

python3 - <<'PY' "${WORKSPACE}" "${OVERLAY_DIR}" "${MODERN_SETUP_DEMO_DRIVER_SAMPLE}" "${MODERN_SETUP_THEME}" "${MODERN_SETUP_DISPLAY_ENGINE}"
from pathlib import Path
import sys

workspace = Path(sys.argv[1])
overlay = Path(sys.argv[2])
enable_driver_sample = sys.argv[3] != "0"
theme_name = sys.argv[4].strip().lower()
display_engine = sys.argv[5].strip().lower()
theme_pcd = {
    "orange": "0x00",
    "amber": "0x00",
    "dark-orange": "0x00",
    "red": "0x01",
    "accent-red": "0x01",
    "dark-red": "0x01",
}.get(theme_name)
if theme_pcd is None:
    raise SystemExit(
        f"Unsupported MODERN_SETUP_THEME={theme_name!r}; "
        "use orange, amber, dark-orange, red, accent-red, or dark-red"
    )
if display_engine not in {"modern", "native"}:
    raise SystemExit(
        f"Unsupported MODERN_SETUP_DISPLAY_ENGINE={display_engine!r}; use modern or native"
    )

modern_display_component = "  ModernSetupPkg/Universal/ModernDisplayEngineDxe/ModernDisplayEngineDxe.inf"
modern_display_fdf_inf = "INF  ModernSetupPkg/Universal/ModernDisplayEngineDxe/ModernDisplayEngineDxe.inf"
driver_sample_component = "  MdeModulePkg/Universal/DriverSampleDxe/DriverSampleDxe.inf"
driver_sample_fdf_inf = "INF  MdeModulePkg/Universal/DriverSampleDxe/DriverSampleDxe.inf"
library_block = """  ModernUiEngineLib|ModernSetupPkg/Library/ModernUiEngineLib/ModernUiEngineLib.inf
  ModernUiRendererLib|ModernSetupPkg/Library/ModernUiRendererLib/ModernUiRendererLib.inf
  ModernUiThemeLib|ModernSetupPkg/Library/ModernUiThemeLib/ModernUiThemeLib.inf
"""

dsc_path = workspace / "OvmfPkg/LoongArchVirt/LoongArchVirtQemu.dsc"
dsc = dsc_path.read_text()
dsc = dsc.replace(
    "  FLASH_DEFINITION               = OvmfPkg/LoongArchVirt/LoongArchVirtQemu.fdf",
    "  FLASH_DEFINITION               = Build/ModernSetupPkgOverlay/LoongArchVirtQemuModernSetup.fdf",
    1,
)
dsc = dsc.replace(
    "!include LoongArchVirt.fdf.inc",
    "!include OvmfPkg/LoongArchVirt/LoongArchVirt.fdf.inc",
    1,
)
if "ModernUiEngineLib|ModernSetupPkg" not in dsc:
    if display_engine == "modern":
        dsc = dsc.replace("[LibraryClasses.common]\n", "[LibraryClasses.common]\n" + library_block, 1)
if display_engine == "modern":
    dsc = dsc.replace(
        "  CustomizedDisplayLib             | MdeModulePkg/Library/CustomizedDisplayLib/CustomizedDisplayLib.inf",
        "  CustomizedDisplayLib             | ModernSetupPkg/Library/ModernUiCustomizedDisplayLib/ModernUiCustomizedDisplayLib.inf",
        1,
    )
    dsc = dsc.replace(
        "  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf",
        modern_display_component,
        1,
    )
if enable_driver_sample and driver_sample_component not in dsc:
    dsc = dsc.replace(
        "  MdeModulePkg/Application/UiApp/UiApp.inf {",
        driver_sample_component + "\n  MdeModulePkg/Application/UiApp/UiApp.inf {",
        1,
    )
if display_engine == "modern":
    dsc += (
        "\n[PcdsFixedAtBuild]\n"
        f"  gModernSetupPkgTokenSpaceGuid.PcdModernSetupTheme|{theme_pcd}\n"
    )
(overlay / "LoongArchVirtQemuModernSetup.dsc").write_text(dsc)

fdf_path = workspace / "OvmfPkg/LoongArchVirt/LoongArchVirtQemu.fdf"
fdf = fdf_path.read_text()
fdf = fdf.replace(
    "!include LoongArchVirt.fdf.inc",
    "!include OvmfPkg/LoongArchVirt/LoongArchVirt.fdf.inc",
    1,
)
fdf = fdf.replace(
    "!include VarStore.fdf.inc",
    "!include OvmfPkg/LoongArchVirt/VarStore.fdf.inc",
    1,
)
if display_engine == "modern":
    fdf = fdf.replace(
        "INF  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf",
        modern_display_fdf_inf,
        1,
    )
if enable_driver_sample and driver_sample_fdf_inf not in fdf:
    fdf = fdf.replace(
        "INF  MdeModulePkg/Application/UiApp/UiApp.inf",
        driver_sample_fdf_inf + "\nINF  MdeModulePkg/Application/UiApp/UiApp.inf",
        1,
    )
(overlay / "LoongArchVirtQemuModernSetup.fdf").write_text(fdf)
PY

echo "Generated: ${OVERLAY_DIR}/LoongArchVirtQemuModernSetup.dsc"
echo "Generated: ${OVERLAY_DIR}/LoongArchVirtQemuModernSetup.fdf"
echo "DisplayEngine: ${MODERN_SETUP_DISPLAY_ENGINE}"

if [[ "${GENERATE_ONLY}" == "1" ]]; then
  exit 0
fi

if ! command -v "${GCC_LOONGARCH64_PREFIX}gcc" >/dev/null 2>&1; then
  echo "Missing LoongArch GCC cross compiler: ${GCC_LOONGARCH64_PREFIX}gcc" >&2
  echo "Install/provide GCC 13+ and Binutils 2.40+, then export GCC_LOONGARCH64_PREFIX=<prefix>." >&2
  echo "edk2 reference: OvmfPkg/LoongArchVirt/Readme.md" >&2
  exit 1
fi

if ! command -v "${GCC_LOONGARCH64_PREFIX}objcopy" >/dev/null 2>&1; then
  echo "Missing LoongArch binutils objcopy: ${GCC_LOONGARCH64_PREFIX}objcopy" >&2
  echo "Install/provide Binutils 2.40+, then export GCC_LOONGARCH64_PREFIX=<prefix>." >&2
  exit 1
fi

cd "${WORKSPACE}"

if [[ ! -x BaseTools/Source/C/bin/VfrCompile ]]; then
  make -C BaseTools -j"${JOBS}"
fi

set +u
# shellcheck disable=SC1091
source edksetup.sh
set -u

build \
  -a LOONGARCH64 \
  -t GCC \
  -p Build/ModernSetupPkgOverlay/LoongArchVirtQemuModernSetup.dsc \
  -b "${TARGET}" \
  -n "${JOBS}"

echo "Built: ${WORKSPACE}/Build/LoongArchVirtQemu/${TARGET}_GCC/FV/QEMU_EFI.fd"
echo "Vars:  ${WORKSPACE}/Build/LoongArchVirtQemu/${TARGET}_GCC/FV/QEMU_VARS.fd"
