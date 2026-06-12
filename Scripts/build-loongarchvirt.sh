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
MODERN_SETUP_DISPLAY_ENGINE="${MODERN_SETUP_DISPLAY_ENGINE:-lvgl}"
MODERN_SETUP_INCLUDE_APP="${MODERN_SETUP_INCLUDE_APP:-0}"
MODERN_SETUP_REPLACE_UIAPP="${MODERN_SETUP_REPLACE_UIAPP:-0}"
GCC_LOONGARCH64_PREFIX="${GCC_LOONGARCH64_PREFIX:-}"
GENERATE_ONLY="${GENERATE_ONLY:-0}"
OVERLAY_DIR="${WORKSPACE}/Build/ModernSetupPkgOverlay"

if [[ -z "${GCC_LOONGARCH64_PREFIX}" ]]; then
  if command -v loongarch64-unknown-linux-gnu-gcc >/dev/null 2>&1; then
    GCC_LOONGARCH64_PREFIX="loongarch64-unknown-linux-gnu-"
  elif command -v loongarch64-linux-gnu-gcc >/dev/null 2>&1; then
    GCC_LOONGARCH64_PREFIX="loongarch64-linux-gnu-"
  else
    GCC_LOONGARCH64_PREFIX="loongarch64-unknown-linux-gnu-"
  fi
fi

export PATH="/opt/homebrew/bin:${PATH}"
export WORKSPACE
export GCC_LOONGARCH64_PREFIX
ConfigureModernSetupPackagePath
# Experimental LvglSpikePkg is only consumed by MODERN_SETUP_DISPLAY_ENGINE=lvgl;
# making it resolvable here is harmless for native/modern.
AppendPackagePath "${PKG_DIR}/Experimental"
export PACKAGES_PATH

# LoongArch64/RISC-V64 UEFI support is upstream in the pinned External/lvgl
# baseline; the submodule is consumed pristine with no local patching.

if [[ ! -d "${WORKSPACE}/MdePkg" || ! -d "${WORKSPACE}/OvmfPkg/LoongArchVirt" ]]; then
  echo "WORKSPACE does not look like an edk2 checkout with LoongArchVirt: ${WORKSPACE}" >&2
  exit 1
fi

mkdir -p "${OVERLAY_DIR}"

python3 - <<'PY' "${WORKSPACE}" "${OVERLAY_DIR}" "${MODERN_SETUP_DEMO_DRIVER_SAMPLE}" "${MODERN_SETUP_THEME}" "${MODERN_SETUP_DISPLAY_ENGINE}" "${MODERN_SETUP_INCLUDE_APP}" "${MODERN_SETUP_REPLACE_UIAPP}"
from pathlib import Path
import re
import sys

workspace = Path(sys.argv[1])
overlay = Path(sys.argv[2])
enable_driver_sample = sys.argv[3] != "0"
theme_name = sys.argv[4].strip().lower()
display_engine = sys.argv[5].strip().lower()
include_app_flag = sys.argv[6].strip().lower()
replace_uiapp_flag = sys.argv[7].strip().lower()
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
if include_app_flag not in {"0", "1", "false", "true", "no", "yes"}:
    raise SystemExit(
        f"Unsupported MODERN_SETUP_INCLUDE_APP={include_app_flag!r}; use 0 or 1"
    )
if replace_uiapp_flag not in {"0", "1", "false", "true", "no", "yes"}:
    raise SystemExit(
        f"Unsupported MODERN_SETUP_REPLACE_UIAPP={replace_uiapp_flag!r}; use 0 or 1"
    )
replace_uiapp = replace_uiapp_flag in {"1", "true", "yes"}
include_modern_setup_app = (include_app_flag in {"1", "true", "yes"}) or replace_uiapp

# In lvgl mode the ModernSetupApp shell and the ModernDisplayEngine both pull the
# LVGL-backed renderer, so they force-link compiler intrinsics (memcpy/memset).
_app_lvgl_intrinsics = (
    "    <LibraryClasses>\n"
    "      NULL|CryptoPkg/Library/IntrinsicLib/IntrinsicLib.inf\n"
    if display_engine == "lvgl"
    else ""
)
modern_setup_app_component = (
    "  ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf"
    if display_engine != "lvgl"
    else (
        "  ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf {\n"
        f"{_app_lvgl_intrinsics}"
        "  }"
    )
)
modern_setup_app_component_boot_manager_fallback = (
    "  ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf {\n"
    "    <BuildOptions>\n"
    "      GCC:*_*_*_CC_FLAGS = -DMODERN_SETUP_NATIVE_FALLBACK_BOOT_MANAGER_MENU=1\n"
    f"{_app_lvgl_intrinsics}"
    "  }"
)
modern_setup_app_fdf_inf = "INF  ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf"
modern_setup_app_uiapp_fdf_inf = "INF  RuleOverride = MODERN_SETUP_UIAPP ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf"
modern_display_component = "  ModernSetupPkg/Universal/ModernDisplayEngineDxe/ModernDisplayEngineDxe.inf"
modern_display_component_lvgl = (
    "  ModernSetupPkg/Universal/ModernDisplayEngineDxe/ModernDisplayEngineDxe.inf {\n"
    "    <LibraryClasses>\n"
    "      NULL|CryptoPkg/Library/IntrinsicLib/IntrinsicLib.inf\n"
    "  }"
)
modern_display_fdf_inf = "INF  ModernSetupPkg/Universal/ModernDisplayEngineDxe/ModernDisplayEngineDxe.inf"
boot_manager_menu_component = "  MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf"
boot_manager_menu_fdf_inf = "INF  MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf"
driver_sample_component = "  MdeModulePkg/Universal/DriverSampleDxe/DriverSampleDxe.inf"
driver_sample_fdf_inf = "INF  MdeModulePkg/Universal/DriverSampleDxe/DriverSampleDxe.inf"
# Upstream USB absolute-pointer driver (relative HID mouse -> EFI_ABSOLUTE_POINTER):
# the app's mouse support consumes it; the upstream platform ships only UsbKbDxe.
usb_pointer_component = "  MdeModulePkg/Bus/Usb/UsbMouseAbsolutePointerDxe/UsbMouseAbsolutePointerDxe.inf"
usb_pointer_fdf_inf = "INF  MdeModulePkg/Bus/Usb/UsbMouseAbsolutePointerDxe/UsbMouseAbsolutePointerDxe.inf"
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
    if (display_engine == "modern" or display_engine == "lvgl") or include_modern_setup_app:
        dsc = dsc.replace("[LibraryClasses.common]\n", "[LibraryClasses.common]\n" + library_block, 1)
if display_engine == "lvgl" and "LvglCoreLib|LvglSpikePkg" not in dsc:
    dsc = dsc.replace("[LibraryClasses.common]\n", "[LibraryClasses.common]\n" + lvgl_library_block, 1)
if include_modern_setup_app and "ModernUiPlatformDataLib|ModernSetupPkg" not in dsc:
    dsc = dsc.replace("[LibraryClasses.common]\n", "[LibraryClasses.common]\n" + app_library_block, 1)
if include_modern_setup_app and modern_setup_app_component not in dsc:
    dsc = dsc.replace(
        "  MdeModulePkg/Application/UiApp/UiApp.inf {",
        (modern_setup_app_component_boot_manager_fallback if replace_uiapp else modern_setup_app_component) + "\n  MdeModulePkg/Application/UiApp/UiApp.inf {",
        1,
    )
if replace_uiapp and boot_manager_menu_component not in dsc:
    dsc = dsc.replace(
        "  MdeModulePkg/Application/UiApp/UiApp.inf {",
        boot_manager_menu_component + "\n  MdeModulePkg/Application/UiApp/UiApp.inf {",
        1,
    )
if replace_uiapp:
    dsc, uiapp_component_count = re.subn(
        r"(?m)^  MdeModulePkg/Application/UiApp/UiApp\.inf \{\r?\n"
        r"(?:    [^\r\n]*\r?\n)*?"
        r"  \}\r?\n",
        "",
        dsc,
        count=1,
    )
    if uiapp_component_count != 1:
        raise SystemExit("MODERN_SETUP_REPLACE_UIAPP could not remove native UiApp DSC component")
    if enable_driver_sample and driver_sample_component not in dsc:
        dsc = dsc.replace(
            "  OvmfPkg/QemuKernelLoaderFsDxe/QemuKernelLoaderFsDxe.inf {",
            driver_sample_component + "\n  OvmfPkg/QemuKernelLoaderFsDxe/QemuKernelLoaderFsDxe.inf {",
            1,
        )
if (display_engine == "modern" or display_engine == "lvgl"):
    dsc = dsc.replace(
        "  CustomizedDisplayLib             | MdeModulePkg/Library/CustomizedDisplayLib/CustomizedDisplayLib.inf",
        "  CustomizedDisplayLib             | ModernSetupPkg/Library/ModernUiCustomizedDisplayLib/ModernUiCustomizedDisplayLib.inf",
        1,
    )
    dsc = dsc.replace(
        "  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf",
        modern_display_component_lvgl if display_engine == "lvgl" else modern_display_component,
        1,
    )
if enable_driver_sample and driver_sample_component not in dsc:
    dsc = dsc.replace(
        "  MdeModulePkg/Application/UiApp/UiApp.inf {",
        driver_sample_component + "\n  MdeModulePkg/Application/UiApp/UiApp.inf {",
        1,
    )
if usb_pointer_component not in dsc:
    # Pointer input for the app: anchor on the existing USB keyboard driver so
    # the pointer driver sits with the rest of the USB stack.
    dsc = dsc.replace(
        "  MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf",
        "  MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf\n" + usb_pointer_component,
        1,
    )
if (display_engine == "modern" or display_engine == "lvgl"):
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
if (display_engine == "modern" or display_engine == "lvgl"):
    fdf = fdf.replace(
        "INF  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf",
        modern_display_fdf_inf,
        1,
    )
if replace_uiapp:
    if modern_setup_app_uiapp_fdf_inf not in fdf:
        fdf = fdf.replace(
            "INF  MdeModulePkg/Application/UiApp/UiApp.inf",
            modern_setup_app_uiapp_fdf_inf,
            1,
        )
    if boot_manager_menu_fdf_inf not in fdf:
        fdf = fdf.replace(
            modern_setup_app_uiapp_fdf_inf,
            boot_manager_menu_fdf_inf + "\n" + modern_setup_app_uiapp_fdf_inf,
            1,
        )
elif include_modern_setup_app and modern_setup_app_fdf_inf not in fdf:
    fdf = fdf.replace(
        "INF  MdeModulePkg/Application/UiApp/UiApp.inf",
        modern_setup_app_fdf_inf + "\nINF  MdeModulePkg/Application/UiApp/UiApp.inf",
        1,
    )
if enable_driver_sample and driver_sample_fdf_inf not in fdf:
    if replace_uiapp:
        fdf = fdf.replace(
            modern_setup_app_uiapp_fdf_inf,
            driver_sample_fdf_inf + "\n" + modern_setup_app_uiapp_fdf_inf,
            1,
        )
    else:
        fdf = fdf.replace(
            "INF  MdeModulePkg/Application/UiApp/UiApp.inf",
            driver_sample_fdf_inf + "\nINF  MdeModulePkg/Application/UiApp/UiApp.inf",
            1,
        )
if replace_uiapp and "[Rule.Common.UEFI_APPLICATION.MODERN_SETUP_UIAPP]" not in fdf:
    fdf += (
        "\n[Rule.Common.UEFI_APPLICATION.MODERN_SETUP_UIAPP]\n"
        "  FILE APPLICATION = 462CAA21-7614-4503-836E-8AB6F4662331 {\n"
        "    PE32     PE32                    $(INF_OUTPUT)/$(MODULE_NAME).efi\n"
        "    UI       STRING=\"ModernSetupApp\" Optional\n"
        "  }\n"
    )
if usb_pointer_fdf_inf not in fdf:
    fdf = fdf.replace(
        "INF  MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf",
        "INF  MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf\n" + usb_pointer_fdf_inf,
        1,
    )
(overlay / "LoongArchVirtQemuModernSetup.fdf").write_text(fdf)
PY

echo "Generated: ${OVERLAY_DIR}/LoongArchVirtQemuModernSetup.dsc"
echo "Generated: ${OVERLAY_DIR}/LoongArchVirtQemuModernSetup.fdf"
echo "DisplayEngine: ${MODERN_SETUP_DISPLAY_ENGINE}"
if [[ "${MODERN_SETUP_REPLACE_UIAPP}" =~ ^(1|true|yes)$ ]]; then
  echo "ModernSetupApp in FV: 1 (via MODERN_SETUP_REPLACE_UIAPP)"
else
  echo "ModernSetupApp in FV: ${MODERN_SETUP_INCLUDE_APP}"
fi
echo "Replace UiApp with ModernSetupApp: ${MODERN_SETUP_REPLACE_UIAPP}"

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
