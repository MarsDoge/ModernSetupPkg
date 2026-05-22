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
TOOL_CHAIN_TAG="${TOOL_CHAIN_TAG:-GCC}"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
MODERN_SETUP_THEME="${MODERN_SETUP_THEME:-orange}"
MODERN_SETUP_DISPLAY_ENGINE="${MODERN_SETUP_DISPLAY_ENGINE:-modern}"
MODERN_SETUP_REPLACE_UIAPP="${MODERN_SETUP_REPLACE_UIAPP:-0}"
GENERATE_ONLY="${GENERATE_ONLY:-0}"
OVERLAY_DIR="${WORKSPACE}/Build/ModernSetupPkgOverlay"

export WORKSPACE
ConfigureModernSetupPackagePath

if [[ ! -d "${WORKSPACE}/MdePkg" || ! -d "${WORKSPACE}/OvmfPkg" ]]; then
  echo "WORKSPACE does not look like an edk2 checkout with MdePkg and OvmfPkg: ${WORKSPACE}" >&2
  exit 1
fi

if [[ ! -f "${WORKSPACE}/OvmfPkg/OvmfPkgX64.dsc" || ! -f "${WORKSPACE}/OvmfPkg/OvmfPkgX64.fdf" ]]; then
  echo "Missing OVMF X64 platform files under ${WORKSPACE}/OvmfPkg" >&2
  echo "Expected: OvmfPkg/OvmfPkgX64.dsc and OvmfPkg/OvmfPkgX64.fdf" >&2
  exit 1
fi

mkdir -p "${OVERLAY_DIR}"

python3 - <<'PY' "${WORKSPACE}" "${OVERLAY_DIR}" "${MODERN_SETUP_THEME}" "${MODERN_SETUP_DISPLAY_ENGINE}" "${MODERN_SETUP_REPLACE_UIAPP}"
from pathlib import Path
import re
import sys

workspace = Path(sys.argv[1])
overlay = Path(sys.argv[2])
theme_name = sys.argv[3].strip().lower()
display_engine = sys.argv[4].strip().lower()
replace_uiapp_flag = sys.argv[5].strip().lower()
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
if replace_uiapp_flag not in {"0", "1", "false", "true", "no", "yes"}:
    raise SystemExit(
        f"Unsupported MODERN_SETUP_REPLACE_UIAPP={replace_uiapp_flag!r}; use 0 or 1"
    )
replace_uiapp = replace_uiapp_flag in {"1", "true", "yes"}

modern_setup_app_component_boot_manager_fallback = """  ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf {
    <BuildOptions>
      GCC:*_*_*_CC_FLAGS = -DMODERN_SETUP_NATIVE_FALLBACK_BOOT_MANAGER_MENU=1
  }"""
modern_setup_app_uiapp_fdf_inf = "INF  RuleOverride = MODERN_SETUP_UIAPP ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf"
modern_display_component = "  ModernSetupPkg/Universal/ModernDisplayEngineDxe/ModernDisplayEngineDxe.inf"
modern_display_fdf_inf = "INF  ModernSetupPkg/Universal/ModernDisplayEngineDxe/ModernDisplayEngineDxe.inf"
boot_manager_menu_component = "  MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf"
boot_manager_menu_fdf_inf = "INF  MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf"
library_block = """  ModernUiEngineLib|ModernSetupPkg/Library/ModernUiEngineLib/ModernUiEngineLib.inf
  ModernUiRendererLib|ModernSetupPkg/Library/ModernUiRendererLib/ModernUiRendererLib.inf
  ModernUiThemeLib|ModernSetupPkg/Library/ModernUiThemeLib/ModernUiThemeLib.inf
"""
app_library_block = """  ModernUiPlatformDataLib|ModernSetupPkg/Library/ModernUiPlatformDataLib/ModernUiPlatformDataLib.inf
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
  ModernUiPcieDataLib|ModernSetupPkg/Library/ModernUiPcieDataLib/ModernUiPcieDataLib.inf
  ModernUiInputLib|ModernSetupPkg/Library/ModernUiInputLib/ModernUiInputLib.inf
  ModernUiPreferencesLib|ModernSetupPkg/Library/ModernUiPreferencesLib/ModernUiPreferencesLib.inf
  ModernUiStringLib|ModernSetupPkg/Library/ModernUiStringLib/ModernUiStringLib.inf
"""

def replace_once(text: str, old: str, new: str, description: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"Expected exactly one {description} anchor, found {count}: {old}")
    return text.replace(old, new, 1)

def replace_regex_once(text: str, pattern: str, replacement: str, description: str) -> str:
    text, count = re.subn(pattern, replacement, text, count=1, flags=re.MULTILINE)
    if count != 1:
        raise SystemExit(f"Expected {description} anchor matching: {pattern}")
    return text

dsc_path = workspace / "OvmfPkg/OvmfPkgX64.dsc"
dsc = dsc_path.read_text()
dsc = replace_regex_once(
    dsc,
    r"^(\s*FLASH_DEFINITION\s*=\s*)OvmfPkg/OvmfPkgX64\.fdf\s*$",
    r"\1Build/ModernSetupPkgOverlay/OvmfX64ModernSetup.fdf",
    "FLASH_DEFINITION",
)
if display_engine == "modern" or replace_uiapp:
    if "ModernUiEngineLib|ModernSetupPkg" not in dsc:
        if "[LibraryClasses.common]\n" in dsc:
            dsc = replace_once(dsc, "[LibraryClasses.common]\n", "[LibraryClasses.common]\n" + library_block, "LibraryClasses.common")
        elif "[LibraryClasses]\n" in dsc:
            dsc = replace_once(dsc, "[LibraryClasses]\n", "[LibraryClasses]\n" + library_block, "LibraryClasses")
        else:
            raise SystemExit("Expected LibraryClasses anchor: [LibraryClasses.common] or [LibraryClasses]")
if replace_uiapp and "ModernUiPlatformDataLib|ModernSetupPkg" not in dsc:
    if "[LibraryClasses.common]\n" in dsc:
        dsc = replace_once(dsc, "[LibraryClasses.common]\n", "[LibraryClasses.common]\n" + app_library_block, "LibraryClasses.common for app")
    elif "[LibraryClasses]\n" in dsc:
        dsc = replace_once(dsc, "[LibraryClasses]\n", "[LibraryClasses]\n" + app_library_block, "LibraryClasses for app")
    else:
        raise SystemExit("Expected LibraryClasses anchor for ModernSetupApp")
if replace_uiapp:
    dsc = replace_regex_once(
        dsc,
        r"^  MdeModulePkg/Application/UiApp/UiApp\.inf \{\r?\n(?:    [^\r\n]*\r?\n)*?  \}\r?\n",
        modern_setup_app_component_boot_manager_fallback + "\n",
        "UiApp DSC component",
    )
    if boot_manager_menu_component not in dsc:
        dsc = replace_regex_once(
            dsc,
            r"^(\s*OvmfPkg/QemuKernelLoaderFsDxe/QemuKernelLoaderFsDxe\.inf \{)",
            boot_manager_menu_component + r"\n\1",
            "QemuKernelLoaderFsDxe component",
        )
if display_engine == "modern":
    dsc = replace_regex_once(
        dsc,
        r"^\s*CustomizedDisplayLib\s*\|\s*MdeModulePkg/Library/CustomizedDisplayLib/CustomizedDisplayLib\.inf\s*$",
        "  CustomizedDisplayLib|ModernSetupPkg/Library/ModernUiCustomizedDisplayLib/ModernUiCustomizedDisplayLib.inf",
        "CustomizedDisplayLib",
    )
    dsc = replace_regex_once(
        dsc,
        r"^\s*MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe\.inf\s*$",
        modern_display_component,
        "DisplayEngineDxe component",
    )
    dsc += (
        "\n[PcdsFixedAtBuild]\n"
        f"  gModernSetupPkgTokenSpaceGuid.PcdModernSetupTheme|{theme_pcd}\n"
    )
(overlay / "OvmfX64ModernSetup.dsc").write_text(dsc)

fdf_path = workspace / "OvmfPkg/OvmfPkgX64.fdf"
fdf = fdf_path.read_text()
if display_engine == "modern":
    fdf = replace_regex_once(
        fdf,
        r"^\s*INF\s+MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe\.inf\s*$",
        modern_display_fdf_inf,
        "DisplayEngineDxe FDF INF",
    )
if replace_uiapp:
    fdf = replace_regex_once(
        fdf,
        r"^\s*INF\s+MdeModulePkg/Application/UiApp/UiApp\.inf\s*$",
        modern_setup_app_uiapp_fdf_inf,
        "UiApp FDF INF",
    )
    if boot_manager_menu_fdf_inf not in fdf:
        fdf = replace_regex_once(
            fdf,
            r"^(\s*INF\s+OvmfPkg/QemuKernelLoaderFsDxe/QemuKernelLoaderFsDxe\.inf\s*)$",
            boot_manager_menu_fdf_inf + r"\n\1",
            "QemuKernelLoaderFsDxe FDF INF",
        )
    if "[Rule.Common.UEFI_APPLICATION.MODERN_SETUP_UIAPP]" not in fdf:
        fdf += (
            "\n[Rule.Common.UEFI_APPLICATION.MODERN_SETUP_UIAPP]\n"
            "  FILE APPLICATION = 462CAA21-7614-4503-836E-8AB6F4662331 {\n"
            "    PE32     PE32                    $(INF_OUTPUT)/$(MODULE_NAME).efi\n"
            "    UI       STRING=\"ModernSetupApp\" Optional\n"
            "  }\n"
        )
(overlay / "OvmfX64ModernSetup.fdf").write_text(fdf)
PY

echo "Generated: ${OVERLAY_DIR}/OvmfX64ModernSetup.dsc"
echo "Generated: ${OVERLAY_DIR}/OvmfX64ModernSetup.fdf"
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
  -a X64 \
  -t "${TOOL_CHAIN_TAG}" \
  -p Build/ModernSetupPkgOverlay/OvmfX64ModernSetup.dsc \
  -b "${TARGET}" \
  -n "${JOBS}"

echo "Built: ${WORKSPACE}/Build/OvmfX64/${TARGET}_${TOOL_CHAIN_TAG}/FV/OVMF_CODE.fd"
echo "Vars:  ${WORKSPACE}/Build/OvmfX64/${TARGET}_${TOOL_CHAIN_TAG}/FV/OVMF_VARS.fd"
