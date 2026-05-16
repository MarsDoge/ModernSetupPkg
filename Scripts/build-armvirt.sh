#!/usr/bin/env bash
set -euo pipefail

PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${WORKSPACE:-$(cd "${PKG_DIR}/.." && pwd)}"
TARGET="${TARGET:-DEBUG}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
MODERN_SETUP_LANGUAGE="${MODERN_SETUP_LANGUAGE:-zh-Hans}"
MODERN_SETUP_DEMO_DRIVER_SAMPLE="${MODERN_SETUP_DEMO_DRIVER_SAMPLE:-1}"
OVERLAY_DIR="${WORKSPACE}/Build/ModernSetupPkgOverlay"

export PATH="/opt/homebrew/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/opt/lld/bin:${PATH}"
export CLANGDWARF_BIN="${CLANGDWARF_BIN:-/opt/homebrew/opt/llvm/bin/}"
export WORKSPACE

if [[ ! -d "${WORKSPACE}/MdePkg" || ! -d "${WORKSPACE}/ArmVirtPkg" ]]; then
  echo "WORKSPACE does not look like an edk2 checkout: ${WORKSPACE}" >&2
  exit 1
fi

if [[ "$(cd "${PKG_DIR}" && pwd)" != "${WORKSPACE}/ModernSetupPkg" ]]; then
  echo "ModernSetupPkg should be checked out at ${WORKSPACE}/ModernSetupPkg" >&2
  echo "Current package path: ${PKG_DIR}" >&2
  exit 1
fi

mkdir -p "${OVERLAY_DIR}"

python3 - <<'PY' "${WORKSPACE}" "${OVERLAY_DIR}" "${MODERN_SETUP_LANGUAGE}" "${MODERN_SETUP_DEMO_DRIVER_SAMPLE}"
from pathlib import Path
import re
import sys

workspace = Path(sys.argv[1])
overlay = Path(sys.argv[2])
language = sys.argv[3]
enable_driver_sample = sys.argv[4] != "0"

app_component = "  ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf"
app_fdf_inf = "  INF ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf"
driver_sample_component = "  MdeModulePkg/Universal/DriverSampleDxe/DriverSampleDxe.inf"
driver_sample_fdf_inf = "  INF MdeModulePkg/Universal/DriverSampleDxe/DriverSampleDxe.inf"
guid_bytes = "{ 0x3a, 0x8b, 0xc9, 0x26, 0xdd, 0x29, 0x73, 0x4f, 0xa0, 0x7a, 0x4a, 0x4e, 0x0e, 0x1d, 0x9c, 0x4c }"
library_block = """  ModernUiRendererLib|ModernSetupPkg/Library/ModernUiRendererLib/ModernUiRendererLib.inf
  ModernUiInputLib|ModernSetupPkg/Library/ModernUiInputLib/ModernUiInputLib.inf
  ModernUiThemeLib|ModernSetupPkg/Library/ModernUiThemeLib/ModernUiThemeLib.inf
  ModernUiStringLib|ModernSetupPkg/Library/ModernUiStringLib/ModernUiStringLib.inf
  ModernUiHiiBridgeLib|ModernSetupPkg/Library/ModernUiHiiBridgeLib/ModernUiHiiBridgeLib.inf
"""

dsc = (workspace / "ArmVirtPkg/ArmVirtQemu.dsc").read_text()
dsc = re.sub(
    r"gEfiMdeModulePkgTokenSpaceGuid\.PcdBootManagerMenuFile\|\{[^}]+\}",
    f"gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerMenuFile|{guid_bytes}",
    dsc,
    count=1,
)
dsc = dsc.replace(
    "  FLASH_DEFINITION               = ArmVirtPkg/ArmVirtQemu.fdf",
    "  FLASH_DEFINITION               = Build/ModernSetupPkgOverlay/ArmVirtQemuModernSetup.fdf",
)
if "ModernUiRendererLib|ModernSetupPkg" not in dsc:
    dsc = dsc.replace("[LibraryClasses.common]\n", "[LibraryClasses.common]\n" + library_block, 1)
if app_component not in dsc:
    dsc = dsc.replace(
        "  MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf",
        app_component + "\n  MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf",
        1,
    )
if enable_driver_sample and driver_sample_component not in dsc:
    dsc = dsc.replace(
        app_component,
        app_component + "\n" + driver_sample_component,
        1,
    )
if "gModernSetupPkgTokenSpaceGuid.PcdModernSetupDefaultLanguage" not in dsc:
    dsc += f'\n[PcdsFixedAtBuild]\n  gModernSetupPkgTokenSpaceGuid.PcdModernSetupDefaultLanguage|"{language}"\n'
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
if app_fdf_inf not in fv:
    fv = fv.replace(
        "  INF MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf",
        app_fdf_inf + "\n  INF MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf",
        1,
    )
if enable_driver_sample and driver_sample_fdf_inf not in fv:
    fv = fv.replace(
        app_fdf_inf,
        app_fdf_inf + "\n" + driver_sample_fdf_inf,
        1,
    )
(overlay / "ArmVirtQemuModernSetupFvMain.fdf.inc").write_text(fv)
PY

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
