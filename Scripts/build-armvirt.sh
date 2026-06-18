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
MODERN_SETUP_THEME="${MODERN_SETUP_THEME:-graphite-gold}"
MODERN_SETUP_DISPLAY_ENGINE="${MODERN_SETUP_DISPLAY_ENGINE:-modern}"
MODERN_SETUP_REPLACE_UIAPP="${MODERN_SETUP_REPLACE_UIAPP:-0}"
GENERATE_ONLY="${GENERATE_ONLY:-0}"
OVERLAY_DIR="${WORKSPACE}/Build/ModernSetupPkgOverlay"

export PATH="/opt/homebrew/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/opt/lld/bin:${PATH}"
export CLANGDWARF_BIN="${CLANGDWARF_BIN:-/opt/homebrew/opt/llvm/bin/}"
export WORKSPACE
ConfigureModernSetupPackagePath
# Experimental/ hosts LvglSpikePkg, consumed only by MODERN_SETUP_DISPLAY_ENGINE=lvgl.
AppendPackagePath "${PKG_DIR}/Experimental"
export PACKAGES_PATH

if [[ ! -d "${WORKSPACE}/MdePkg" || ! -d "${WORKSPACE}/ArmVirtPkg" ]]; then
  echo "WORKSPACE does not look like an edk2 checkout: ${WORKSPACE}" >&2
  exit 1
fi

mkdir -p "${OVERLAY_DIR}"

python3 - <<'PY' "${WORKSPACE}" "${OVERLAY_DIR}" "${MODERN_SETUP_DEMO_DRIVER_SAMPLE}" "${MODERN_SETUP_THEME}" "${MODERN_SETUP_DISPLAY_ENGINE}" "${MODERN_SETUP_REPLACE_UIAPP}"
from pathlib import Path
import re
import sys

workspace = Path(sys.argv[1])
overlay = Path(sys.argv[2])
enable_driver_sample = sys.argv[3] != "0"
theme_name = sys.argv[4].strip().lower()
display_engine = sys.argv[5].strip().lower()
replace_uiapp_flag = sys.argv[6].strip().lower()
theme_pcd = {
    "orange": "0x00",
    "amber": "0x00",
    "dark-orange": "0x00",
    "red": "0x01",
    "accent-red": "0x01",
    "dark-red": "0x01",
    "graphite-gold": "0x02",
    "graphite": "0x02",
}.get(theme_name)
if theme_pcd is None:
    raise SystemExit(
        f"Unsupported MODERN_SETUP_THEME={theme_name!r}; "
        "use orange, amber, dark-orange, red, accent-red, dark-red, graphite, or graphite-gold"
    )
if display_engine not in {"modern", "native", "lvgl"}:
    raise SystemExit(
        f"Unsupported MODERN_SETUP_DISPLAY_ENGINE={display_engine!r}; use modern, native, or lvgl"
    )
if replace_uiapp_flag not in {"0", "1", "false", "true", "no", "yes"}:
    raise SystemExit(
        f"Unsupported MODERN_SETUP_REPLACE_UIAPP={replace_uiapp_flag!r}; use 0 or 1"
    )
replace_uiapp = replace_uiapp_flag in {"1", "true", "yes"}

modern_setup_app_component_boot_manager_fallback = """  ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf {
    <BuildOptions>
      GCC:*_*_*_CC_FLAGS = -DMODERN_SETUP_NATIVE_FALLBACK_BOOT_MANAGER_MENU=1
  }"""
modern_setup_app_uiapp_fdf_inf = "  INF RuleOverride = MODERN_SETUP_UIAPP ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf"
modern_display_component = "  ModernSetupPkg/Universal/ModernDisplayEngineDxe/ModernDisplayEngineDxe.inf"
# In lvgl mode the ModernDisplayEngine force-links compiler intrinsics
# (memcpy/memset) pulled by the LVGL software draw pipeline.
modern_display_component_lvgl = (
    "  ModernSetupPkg/Universal/ModernDisplayEngineDxe/ModernDisplayEngineDxe.inf {\n"
    "    <LibraryClasses>\n"
    "      NULL|CryptoPkg/Library/IntrinsicLib/IntrinsicLib.inf\n"
    "  }"
)
modern_display_fdf_inf = "  INF ModernSetupPkg/Universal/ModernDisplayEngineDxe/ModernDisplayEngineDxe.inf"
driver_sample_component = "  MdeModulePkg/Universal/DriverSampleDxe/DriverSampleDxe.inf"
driver_sample_fdf_inf = "  INF MdeModulePkg/Universal/DriverSampleDxe/DriverSampleDxe.inf"
# LVGL core (upstream lvgl sources as a BASE library); resolved only in lvgl mode
# and consumed transitively through the LVGL renderer library.
lvgl_library_block = "  LvglCoreLib|LvglSpikePkg/Library/LvglLib/LvglCoreLib.inf\n"
# The renderer library class resolves to the LVGL-backed implementation in lvgl
# mode and to the hand-rolled GOP rasterizer otherwise (identical API).
renderer_inf = (
    "ModernSetupPkg/Library/ModernUiLvglRendererLib/ModernUiRendererLib.inf"
    if display_engine == "lvgl"
    else "ModernSetupPkg/Library/ModernUiRendererLib/ModernUiRendererLib.inf"
)
library_block = (
    "  ModernUiEngineLib|ModernSetupPkg/Library/ModernUiEngineLib/ModernUiEngineLib.inf\n"
    f"  ModernUiRendererLib|{renderer_inf}\n"
    "  ModernUiThemeLib|ModernSetupPkg/Library/ModernUiThemeLib/ModernUiThemeLib.inf\n"
    "  ModernUiStringLib|ModernSetupPkg/Library/ModernUiStringLib/ModernUiStringLib.inf\n"
)
app_library_block = """  ModernUiPlatformTablesLib|ModernSetupPkg/Library/ModernUiPlatformTablesLib/ModernUiPlatformTablesLib.inf
  ModernUiPlatformDataLib|ModernSetupPkg/Library/ModernUiPlatformDataLib/ModernUiPlatformDataLib.inf
  ModernUiBootDataLib|ModernSetupPkg/Library/ModernUiBootDataLib/ModernUiBootDataLib.inf
  ModernUiDeviceDataLib|ModernSetupPkg/Library/ModernUiDeviceDataLib/ModernUiDeviceDataLib.inf
  ModernUiSecurityDataLib|ModernSetupPkg/Library/ModernUiSecurityDataLib/ModernUiSecurityDataLib.inf
  ModernUiFirmwareDataLib|ModernSetupPkg/Library/ModernUiFirmwareDataLib/ModernUiFirmwareDataLib.inf
  ModernUiHardwareHealthDataLib|ModernSetupPkg/Library/ModernUiHardwareHealthDataLib/ModernUiHardwareHealthDataLib.inf
  ModernUiHiiBridgeLib|ModernSetupPkg/Library/ModernUiHiiBridgeLib/ModernUiHiiBridgeLib.inf
  ModernUiDiagnosticsDataLib|ModernSetupPkg/Library/ModernUiDiagnosticsDataLib/ModernUiDiagnosticsDataLib.inf
  ModernUiManagementDataLib|ModernSetupPkg/Library/ModernUiManagementDataLib/ModernUiManagementDataLib.inf
  ModernUiPowerDataLib|ModernSetupPkg/Library/ModernUiPowerDataLib/ModernUiPowerDataLib.inf
  ModernUiPerformanceDataLib|ModernSetupPkg/Library/ModernUiPerformanceDataLib/ModernUiPerformanceDataLib.inf
  ModernUiInventoryDataLib|ModernSetupPkg/Library/ModernUiInventoryDataLib/ModernUiInventoryDataLib.inf
  ModernUiPcieDataLib|ModernSetupPkg/Library/ModernUiPcieDataLib/ModernUiPcieDataLib.inf
  ModernUiInputLib|ModernSetupPkg/Library/ModernUiInputLib/ModernUiInputLib.inf
  ModernUiPreferencesLib|ModernSetupPkg/Library/ModernUiPreferencesLib/ModernUiPreferencesLib.inf
"""

def replace_regex_once(text: str, pattern: str, replacement: str, description: str) -> str:
    text, count = re.subn(pattern, replacement, text, count=1, flags=re.MULTILINE)
    if count != 1:
        raise SystemExit(f"Expected {description} anchor matching: {pattern}")
    return text

dsc = (workspace / "ArmVirtPkg/ArmVirtQemu.dsc").read_text()
#
# The UiApp component, the upstream DisplayEngine component, and the
# CustomizedDisplayLib resolution live in the included ArmVirt.dsc.inc, not the
# top-level ArmVirtQemu.dsc. Edits that swap or remove those must therefore
# operate on a shadow copy of the include (mirroring how the FDF side shadows
# ArmVirtQemuFvMain.fdf.inc); the overlay DSC is repointed to that copy. Library
# additions stay in the top-level [LibraryClasses.common] because global
# resolution there reaches the components declared in the include. The include's
# own !include lines are all package-root relative, so the shadow copy resolves
# them unchanged.
#
inc = (workspace / "ArmVirtPkg/ArmVirt.dsc.inc").read_text()
boot_manager_menu_guid_bytes = "{ 0xdc, 0x5b, 0xc2, 0xee, 0xf2, 0x67, 0x95, 0x4d, 0xb1, 0xd5, 0xf8, 0x1b, 0x20, 0x39, 0xd1, 0x1d }"
ui_app_guid_bytes = "{ 0x21, 0xaa, 0x2c, 0x46, 0x14, 0x76, 0x03, 0x45, 0x83, 0x6e, 0x8a, 0xb6, 0xf4, 0x66, 0x23, 0x31 }"
dsc = re.sub(
    r"gEfiMdeModulePkgTokenSpaceGuid\.PcdBootManagerMenuFile\|\{[^}]+\}",
    f"gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerMenuFile|{boot_manager_menu_guid_bytes if replace_uiapp else ui_app_guid_bytes}",
    dsc,
    count=1,
)
dsc = dsc.replace(
    "  FLASH_DEFINITION               = ArmVirtPkg/ArmVirtQemu.fdf",
    "  FLASH_DEFINITION               = Build/ModernSetupPkgOverlay/ArmVirtQemuModernSetup.fdf",
)
dsc = replace_regex_once(
    dsc,
    r"^!include ArmVirtPkg/ArmVirt\.dsc\.inc\s*$",
    "!include Build/ModernSetupPkgOverlay/ArmVirtModernSetup.dsc.inc",
    "ArmVirt.dsc.inc include",
)
if "ModernUiEngineLib|ModernSetupPkg" not in dsc:
    if (display_engine == "modern" or display_engine == "lvgl") or replace_uiapp:
        dsc = dsc.replace("[LibraryClasses.common]\n", "[LibraryClasses.common]\n" + library_block, 1)
if display_engine == "lvgl" and "LvglCoreLib|LvglSpikePkg" not in dsc:
    dsc = dsc.replace("[LibraryClasses.common]\n", "[LibraryClasses.common]\n" + lvgl_library_block, 1)
if replace_uiapp and "ModernUiPlatformDataLib|ModernSetupPkg" not in dsc:
    dsc = dsc.replace("[LibraryClasses.common]\n", "[LibraryClasses.common]\n" + app_library_block, 1)
if replace_uiapp:
    inc = replace_regex_once(
        inc,
        r"^  MdeModulePkg/Application/UiApp/UiApp\.inf \{\r?\n(?:    [^\r\n]*\r?\n)*?  \}\r?\n",
        modern_setup_app_component_boot_manager_fallback + "\n",
        "UiApp DSC component",
    )
if (display_engine == "modern" or display_engine == "lvgl"):
    inc = inc.replace(
        "  CustomizedDisplayLib|MdeModulePkg/Library/CustomizedDisplayLib/CustomizedDisplayLib.inf",
        "  CustomizedDisplayLib|ModernSetupPkg/Library/ModernUiCustomizedDisplayLib/ModernUiCustomizedDisplayLib.inf",
        1,
    )
    inc = inc.replace(
        "  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf",
        modern_display_component_lvgl if display_engine == "lvgl" else modern_display_component,
        1,
    )
if enable_driver_sample and driver_sample_component not in inc:
    if replace_uiapp:
        inc = inc.replace(
            modern_setup_app_component_boot_manager_fallback,
            driver_sample_component + "\n" + modern_setup_app_component_boot_manager_fallback,
            1,
        )
    else:
        inc = inc.replace(
            "  MdeModulePkg/Application/UiApp/UiApp.inf {",
            driver_sample_component + "\n  MdeModulePkg/Application/UiApp/UiApp.inf {",
            1,
        )
if (display_engine == "modern" or display_engine == "lvgl"):
    dsc += (
        "\n[PcdsFixedAtBuild]\n"
        f"  gModernSetupPkgTokenSpaceGuid.PcdModernSetupTheme|{theme_pcd}\n"
    )
(overlay / "ArmVirtQemuModernSetup.dsc").write_text(dsc)
(overlay / "ArmVirtModernSetup.dsc.inc").write_text(inc)

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
if (display_engine == "modern" or display_engine == "lvgl"):
    fv = fv.replace(
        "  INF MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf",
        modern_display_fdf_inf,
        1,
    )
if replace_uiapp:
    fv = replace_regex_once(
        fv,
        r"^\s*INF\s+MdeModulePkg/Application/UiApp/UiApp\.inf\s*$",
        modern_setup_app_uiapp_fdf_inf,
        "UiApp FDF INF",
    )
if enable_driver_sample and driver_sample_fdf_inf not in fv:
    if replace_uiapp:
        fv = fv.replace(
            modern_setup_app_uiapp_fdf_inf,
            driver_sample_fdf_inf + "\n" + modern_setup_app_uiapp_fdf_inf,
            1,
        )
    else:
        fv = fv.replace(
            "  INF MdeModulePkg/Application/UiApp/UiApp.inf",
            driver_sample_fdf_inf + "\n  INF MdeModulePkg/Application/UiApp/UiApp.inf",
            1,
        )
if replace_uiapp and "[Rule.Common.UEFI_APPLICATION.MODERN_SETUP_UIAPP]" not in fv:
    fv += (
        "\n[Rule.Common.UEFI_APPLICATION.MODERN_SETUP_UIAPP]\n"
        "  FILE APPLICATION = 462CAA21-7614-4503-836E-8AB6F4662331 {\n"
        "    PE32     PE32                    $(INF_OUTPUT)/$(MODULE_NAME).efi\n"
        "    UI       STRING=\"ModernSetupApp\" Optional\n"
        "  }\n"
    )
(overlay / "ArmVirtQemuModernSetupFvMain.fdf.inc").write_text(fv)
PY

echo "Generated: ${OVERLAY_DIR}/ArmVirtQemuModernSetup.dsc"
echo "Generated: ${OVERLAY_DIR}/ArmVirtQemuModernSetup.fdf"
echo "Generated: ${OVERLAY_DIR}/ArmVirtQemuModernSetupFvMain.fdf.inc"
echo "DisplayEngine: ${MODERN_SETUP_DISPLAY_ENGINE}"
echo "Replace UiApp with ModernSetupApp: ${MODERN_SETUP_REPLACE_UIAPP}"

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
