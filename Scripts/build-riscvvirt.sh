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
GCC_RISCV64_PREFIX="${GCC_RISCV64_PREFIX:-riscv64-linux-gnu-}"
GENERATE_ONLY="${GENERATE_ONLY:-0}"
OVERLAY_DIR="${WORKSPACE}/Build/ModernSetupPkgOverlay"

export WORKSPACE
export GCC_RISCV64_PREFIX

if [[ ! -d "${WORKSPACE}/MdePkg" || ! -d "${WORKSPACE}/OvmfPkg/RiscVVirt" || ! -d "${WORKSPACE}/ModernSetupPkg" ]]; then
  echo "WORKSPACE does not look like an edk2 checkout with MdePkg, OvmfPkg/RiscVVirt, and ModernSetupPkg: ${WORKSPACE}" >&2
  exit 1
fi

if [[ "$(cd "${PKG_DIR}" && pwd)" != "${WORKSPACE}/ModernSetupPkg" ]]; then
  echo "ModernSetupPkg should be checked out at ${WORKSPACE}/ModernSetupPkg" >&2
  echo "Current package path: ${PKG_DIR}" >&2
  exit 1
fi

if [[ ! -f "${WORKSPACE}/OvmfPkg/RiscVVirt/RiscVVirtQemu.dsc" || ! -f "${WORKSPACE}/OvmfPkg/RiscVVirt/RiscVVirtQemu.fdf" ]]; then
  echo "Missing RiscVVirtQemu platform files under ${WORKSPACE}/OvmfPkg/RiscVVirt" >&2
  echo "Expected: OvmfPkg/RiscVVirt/RiscVVirtQemu.dsc and OvmfPkg/RiscVVirt/RiscVVirtQemu.fdf" >&2
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

def replace_regex_once(text: str, pattern: str, replacement: str, description: str) -> str:
    text, count = re.subn(pattern, replacement, text, count=1, flags=re.MULTILINE)
    if count != 1:
        raise SystemExit(f"Expected {description} anchor matching: {pattern}")
    return text

def replace_once(text: str, old: str, new: str, description: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"Expected exactly one {description} anchor, found {count}: {old}")
    return text.replace(old, new, 1)

dsc = (workspace / "OvmfPkg/RiscVVirt/RiscVVirtQemu.dsc").read_text()
dsc = replace_regex_once(
    dsc,
    r"^(\s*FLASH_DEFINITION\s*=\s*)OvmfPkg/RiscVVirt/RiscVVirtQemu\.fdf\s*$",
    r"\1Build/ModernSetupPkgOverlay/RiscVVirtQemuModernSetup.fdf",
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
(overlay / "RiscVVirtQemuModernSetup.dsc").write_text(dsc)

fdf = (workspace / "OvmfPkg/RiscVVirt/RiscVVirtQemu.fdf").read_text()
fdf = replace_once(fdf, "!include RiscVVirt.fdf.inc", "!include OvmfPkg/RiscVVirt/RiscVVirt.fdf.inc", "RiscVVirt.fdf.inc include")
fdf = replace_once(fdf, "!include VarStore.fdf.inc", "!include OvmfPkg/RiscVVirt/VarStore.fdf.inc", "VarStore.fdf.inc include")
if display_engine == "modern":
    fdf = replace_regex_once(
        fdf,
        r"^\s*INF\s+MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe\.inf\s*$",
        modern_display_fdf_inf,
        "DisplayEngineDxe FDF INF",
    )
(overlay / "RiscVVirtQemuModernSetup.fdf").write_text(fdf)
PY

echo "Generated: ${OVERLAY_DIR}/RiscVVirtQemuModernSetup.dsc"
echo "Generated: ${OVERLAY_DIR}/RiscVVirtQemuModernSetup.fdf"
echo "DisplayEngine: ${MODERN_SETUP_DISPLAY_ENGINE}"

if [[ "${GENERATE_ONLY}" == "1" ]]; then
  exit 0
fi

if ! command -v "${GCC_RISCV64_PREFIX}gcc" >/dev/null 2>&1; then
  echo "Missing RISC-V GCC cross compiler: ${GCC_RISCV64_PREFIX}gcc" >&2
  echo "Install/provide a RISC-V GCC/binutils toolchain, then export GCC_RISCV64_PREFIX=<prefix>." >&2
  echo "Example default prefix: riscv64-linux-gnu-" >&2
  exit 1
fi

if ! command -v "${GCC_RISCV64_PREFIX}objcopy" >/dev/null 2>&1; then
  echo "Missing RISC-V binutils objcopy: ${GCC_RISCV64_PREFIX}objcopy" >&2
  echo "Install/provide a RISC-V GCC/binutils toolchain, then export GCC_RISCV64_PREFIX=<prefix>." >&2
  echo "Example default prefix: riscv64-linux-gnu-" >&2
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
  -a RISCV64 \
  -t "${TOOL_CHAIN_TAG}" \
  -p Build/ModernSetupPkgOverlay/RiscVVirtQemuModernSetup.dsc \
  -b "${TARGET}" \
  -n "${JOBS}"

echo "Built: ${WORKSPACE}/Build/RiscVVirtQemu/${TARGET}_${TOOL_CHAIN_TAG}/FV/RISCV_VIRT_CODE.fd"
echo "Vars:  ${WORKSPACE}/Build/RiscVVirtQemu/${TARGET}_${TOOL_CHAIN_TAG}/FV/RISCV_VIRT_VARS.fd"
