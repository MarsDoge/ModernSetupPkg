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
GENERATE_ONLY="${GENERATE_ONLY:-0}"
OVERLAY_DIR="${WORKSPACE}/Build/ModernSetupPkgOverlay"

export PATH="/opt/homebrew/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/opt/lld/bin:${PATH}"
export CLANGDWARF_BIN="${CLANGDWARF_BIN:-/opt/homebrew/opt/llvm/bin/}"
export WORKSPACE
ConfigureModernSetupPackagePath

if [[ ! -d "${WORKSPACE}/MdePkg" || ! -d "${WORKSPACE}/ArmVirtPkg" ]]; then
  echo "WORKSPACE does not look like an edk2 checkout: ${WORKSPACE}" >&2
  exit 1
fi

mkdir -p "${OVERLAY_DIR}"

python3 - <<'PY' "${WORKSPACE}" "${OVERLAY_DIR}" "${MODERN_SETUP_DEMO_DRIVER_SAMPLE}" "${MODERN_SETUP_THEME}" "${MODERN_SETUP_DISPLAY_ENGINE}"
from pathlib import Path
import re
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
modern_display_fdf_inf = "  INF ModernSetupPkg/Universal/ModernDisplayEngineDxe/ModernDisplayEngineDxe.inf"
driver_sample_component = "  MdeModulePkg/Universal/DriverSampleDxe/DriverSampleDxe.inf"
driver_sample_fdf_inf = "  INF MdeModulePkg/Universal/DriverSampleDxe/DriverSampleDxe.inf"
ui_app_guid_bytes = "{ 0x21, 0xaa, 0x2c, 0x46, 0x14, 0x76, 0x03, 0x45, 0x83, 0x6e, 0x8a, 0xb6, 0xf4, 0x66, 0x23, 0x31 }"
library_block = """  ModernUiEngineLib|ModernSetupPkg/Library/ModernUiEngineLib/ModernUiEngineLib.inf
  ModernUiRendererLib|ModernSetupPkg/Library/ModernUiRendererLib/ModernUiRendererLib.inf
  ModernUiThemeLib|ModernSetupPkg/Library/ModernUiThemeLib/ModernUiThemeLib.inf
"""

dsc = (workspace / "ArmVirtPkg/ArmVirtQemu.dsc").read_text()
dsc = re.sub(
    r"gEfiMdeModulePkgTokenSpaceGuid\.PcdBootManagerMenuFile\|\{[^}]+\}",
    f"gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerMenuFile|{ui_app_guid_bytes}",
    dsc,
    count=1,
)
dsc = dsc.replace(
    "  FLASH_DEFINITION               = ArmVirtPkg/ArmVirtQemu.fdf",
    "  FLASH_DEFINITION               = Build/ModernSetupPkgOverlay/ArmVirtQemuModernSetup.fdf",
)
if "ModernUiEngineLib|ModernSetupPkg" not in dsc:
    if display_engine == "modern":
        dsc = dsc.replace("[LibraryClasses.common]\n", "[LibraryClasses.common]\n" + library_block, 1)
if display_engine == "modern":
    dsc = dsc.replace(
        "  CustomizedDisplayLib|MdeModulePkg/Library/CustomizedDisplayLib/CustomizedDisplayLib.inf",
        "  CustomizedDisplayLib|ModernSetupPkg/Library/ModernUiCustomizedDisplayLib/ModernUiCustomizedDisplayLib.inf",
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
(overlay / "ArmVirtQemuModernSetup.dsc").write_text(dsc)

fdf = (workspace / "ArmVirtPkg/ArmVirtQemu.fdf").read_text()
fdf = fdf.replace("!include VarStore.fdf.inc", "!include ArmVirtPkg/VarStore.fdf.inc")
fdf = fdf.replace("!include ArmVirtRules.fdf.inc", "!include ArmVirtPkg/ArmVirtRules.fdf.inc")
fdf = fdf.replace(
    "!include ArmVirtQemuFvMain.fdf.inc",
    "!include Build/ModernSetupPkgOverlay/ArmVirtQemuModernSetupFvMain.fdf.inc",
)
(overlay / "ArmVirtQemuModernSetup.fdf").write_text(fdf)

fv = (workspace / "ArmVirtPkg/ArmVirtQemuFvMain.fdf.inc").read_text()
fv = fv.replace("!include ArmVirtRules.fdf.inc", "!include ArmVirtPkg/ArmVirtRules.fdf.inc")
if display_engine == "modern":
    fv = fv.replace(
        "  INF MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf",
        modern_display_fdf_inf,
        1,
    )
if enable_driver_sample and driver_sample_fdf_inf not in fv:
    fv = fv.replace(
        "  INF MdeModulePkg/Application/UiApp/UiApp.inf",
        driver_sample_fdf_inf + "\n  INF MdeModulePkg/Application/UiApp/UiApp.inf",
        1,
    )
(overlay / "ArmVirtQemuModernSetupFvMain.fdf.inc").write_text(fv)
PY

echo "Generated: ${OVERLAY_DIR}/ArmVirtQemuModernSetup.dsc"
echo "Generated: ${OVERLAY_DIR}/ArmVirtQemuModernSetup.fdf"
echo "DisplayEngine: ${MODERN_SETUP_DISPLAY_ENGINE}"

if [[ "${GENERATE_ONLY}" == "1" ]]; then
  exit 0
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
  -a AARCH64 \
  -t CLANGDWARF \
  -p Build/ModernSetupPkgOverlay/ArmVirtQemuModernSetup.dsc \
  -b "${TARGET}" \
  -n "${JOBS}"

echo "Built: ${WORKSPACE}/Build/ArmVirtQemu-AArch64/${TARGET}_CLANGDWARF/FV/QEMU_EFI.fd"
echo "Vars:  ${WORKSPACE}/Build/ArmVirtQemu-AArch64/${TARGET}_CLANGDWARF/FV/QEMU_VARS.fd"
