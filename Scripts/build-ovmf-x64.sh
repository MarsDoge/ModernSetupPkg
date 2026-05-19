#!/usr/bin/env bash
# Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
# Author: MarsDoge (Dongyan Qian)
# Open source: https://github.com/MarsDoge/ModernSetupPkg
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
set -euo pipefail

PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${WORKSPACE:-$(cd "${PKG_DIR}/.." && pwd)}"
TARGET="${TARGET:-DEBUG}"
TOOL_CHAIN_TAG="${TOOL_CHAIN_TAG:-GCC}"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
MODERN_SETUP_THEME="${MODERN_SETUP_THEME:-orange}"
MODERN_SETUP_DISPLAY_ENGINE="${MODERN_SETUP_DISPLAY_ENGINE:-modern}"
GENERATE_ONLY="${GENERATE_ONLY:-0}"
OVERLAY_DIR="${WORKSPACE}/Build/ModernSetupPkgOverlay"

export WORKSPACE

if [[ ! -d "${WORKSPACE}/MdePkg" || ! -d "${WORKSPACE}/OvmfPkg" || ! -d "${WORKSPACE}/ModernSetupPkg" ]]; then
  echo "WORKSPACE does not look like an edk2 checkout with MdePkg, OvmfPkg, and ModernSetupPkg: ${WORKSPACE}" >&2
  exit 1
fi

if [[ "$(cd "${PKG_DIR}" && pwd)" != "${WORKSPACE}/ModernSetupPkg" ]]; then
  echo "ModernSetupPkg should be checked out at ${WORKSPACE}/ModernSetupPkg" >&2
  echo "Current package path: ${PKG_DIR}" >&2
  exit 1
fi

if [[ ! -f "${WORKSPACE}/OvmfPkg/OvmfPkgX64.dsc" || ! -f "${WORKSPACE}/OvmfPkg/OvmfPkgX64.fdf" ]]; then
  echo "Missing OVMF X64 platform files under ${WORKSPACE}/OvmfPkg" >&2
  echo "Expected: OvmfPkg/OvmfPkgX64.dsc and OvmfPkg/OvmfPkgX64.fdf" >&2
  exit 1
fi

mkdir -p "${OVERLAY_DIR}"

python3 - <<'PY' "${WORKSPACE}" "${OVERLAY_DIR}" "${MODERN_SETUP_THEME}" "${MODERN_SETUP_DISPLAY_ENGINE}"
from pathlib import Path
import re
import sys

workspace = Path(sys.argv[1])
overlay = Path(sys.argv[2])
theme_name = sys.argv[3].strip().lower()
display_engine = sys.argv[4].strip().lower()
theme_pcd = {
    "orange": "0x00",
    "aorus": "0x00",
    "red": "0x01",
    "asus": "0x01",
}.get(theme_name)
if theme_pcd is None:
    raise SystemExit(f"Unsupported MODERN_SETUP_THEME={theme_name!r}; use orange or red")
if display_engine not in {"modern", "native"}:
    raise SystemExit(
        f"Unsupported MODERN_SETUP_DISPLAY_ENGINE={display_engine!r}; use modern or native"
    )

modern_display_component = "  ModernSetupPkg/Universal/ModernDisplayEngineDxe/ModernDisplayEngineDxe.inf"
modern_display_fdf_inf = "INF  ModernSetupPkg/Universal/ModernDisplayEngineDxe/ModernDisplayEngineDxe.inf"
library_block = """  ModernUiEngineLib|ModernSetupPkg/Library/ModernUiEngineLib/ModernUiEngineLib.inf
  ModernUiRendererLib|ModernSetupPkg/Library/ModernUiRendererLib/ModernUiRendererLib.inf
  ModernUiThemeLib|ModernSetupPkg/Library/ModernUiThemeLib/ModernUiThemeLib.inf
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
if display_engine == "modern":
    if "ModernUiEngineLib|ModernSetupPkg" not in dsc:
        if "[LibraryClasses.common]\n" in dsc:
            dsc = replace_once(dsc, "[LibraryClasses.common]\n", "[LibraryClasses.common]\n" + library_block, "LibraryClasses.common")
        elif "[LibraryClasses]\n" in dsc:
            dsc = replace_once(dsc, "[LibraryClasses]\n", "[LibraryClasses]\n" + library_block, "LibraryClasses")
        else:
            raise SystemExit("Expected LibraryClasses anchor: [LibraryClasses.common] or [LibraryClasses]")
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
(overlay / "OvmfX64ModernSetup.fdf").write_text(fdf)
PY

echo "Generated: ${OVERLAY_DIR}/OvmfX64ModernSetup.dsc"
echo "Generated: ${OVERLAY_DIR}/OvmfX64ModernSetup.fdf"
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
  -a X64 \
  -t "${TOOL_CHAIN_TAG}" \
  -p Build/ModernSetupPkgOverlay/OvmfX64ModernSetup.dsc \
  -b "${TARGET}" \
  -n "${JOBS}"

echo "Built: ${WORKSPACE}/Build/OvmfX64/${TARGET}_${TOOL_CHAIN_TAG}/FV/OVMF_CODE.fd"
echo "Vars:  ${WORKSPACE}/Build/OvmfX64/${TARGET}_${TOOL_CHAIN_TAG}/FV/OVMF_VARS.fd"
