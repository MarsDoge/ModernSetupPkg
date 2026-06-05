#!/usr/bin/env bash
# Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
# experimental/lvgl-spike build validation:
# Compile + link the LVGL core + software renderer + upstream UEFI port (with the
# LoongArch64/RISC-V64 arch-gate patch in External/lvgl) for LOONGARCH64 under the
# real edk2 LoongArch GCC toolchain. Builds a single UEFI_APPLICATION module
# (LvglSpikeProbe) against the stock LoongArchVirtQemu platform DSC -- no FD, no
# default overlay touched. Answers: does the patch let LVGL build on LoongArch?
set -euo pipefail

PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "${PKG_DIR}/Scripts/edk2-workspace.sh"
WORKSPACE="$(DetectWorkspace)"
TARGET="${TARGET:-DEBUG}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

GCC_LOONGARCH64_PREFIX="${GCC_LOONGARCH64_PREFIX:-}"
if [[ -z "${GCC_LOONGARCH64_PREFIX}" ]]; then
  if command -v loongarch64-unknown-linux-gnu-gcc >/dev/null 2>&1; then
    GCC_LOONGARCH64_PREFIX="loongarch64-unknown-linux-gnu-"
  elif command -v loongarch64-linux-gnu-gcc >/dev/null 2>&1; then
    GCC_LOONGARCH64_PREFIX="loongarch64-linux-gnu-"
  else
    GCC_LOONGARCH64_PREFIX="loongarch64-unknown-linux-gnu-"
  fi
fi

export WORKSPACE
export GCC_LOONGARCH64_PREFIX
ConfigureModernSetupPackagePath
# Make the experimental LvglSpikePkg resolvable as a package.
AppendPackagePath "${PKG_DIR}/Experimental"
export PACKAGES_PATH

MODULE="${PKG_DIR}/Experimental/LvglSpikePkg/Library/LvglLib/LvglSpikeProbe.inf"
PLATFORM_DSC="${PKG_DIR}/Experimental/LvglSpikePkg/LvglSpikeLoongArch.dsc"

# LoongArch64/RISC-V64 UEFI support is upstream in the pinned External/lvgl
# baseline; the submodule is consumed pristine with no local patching.

if ! command -v "${GCC_LOONGARCH64_PREFIX}gcc" >/dev/null 2>&1; then
  echo "Missing LoongArch GCC: ${GCC_LOONGARCH64_PREFIX}gcc" >&2
  exit 1
fi
if [[ ! -f "${WORKSPACE}/OvmfPkg/LoongArchVirt/LoongArchVirtQemu.dsc" ]]; then
  echo "WORKSPACE missing LoongArchVirtQemu platform: ${WORKSPACE}" >&2
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

echo "== Building LvglSpikeProbe (LVGL+UEFI port) for LOONGARCH64, TARGET=${TARGET} =="
build \
  -a LOONGARCH64 \
  -t GCC \
  -p "${PLATFORM_DSC}" \
  -b "${TARGET}" \
  -n "${JOBS}"

echo ""
echo "OK: LVGL built for LOONGARCH64."
echo "EFI: ${WORKSPACE}/Build/*/${TARGET}_GCC/LOONGARCH64/LvglSpikeProbe.efi"
