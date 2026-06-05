#!/usr/bin/env bash
# Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
# experimental/lvgl-spike build validation (RISC-V):
# Compile + link the LVGL core + software renderer + upstream UEFI port (with the
# LoongArch64/RISC-V64 arch-gate patch) for RISCV64 under the edk2 RISC-V GCC
# toolchain. Per repo convention RISC-V is build-only (no run/capture helper), so
# this proves compile+link, not render. Builds one UEFI_APPLICATION; no FD.
set -euo pipefail

PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "${PKG_DIR}/Scripts/edk2-workspace.sh"
WORKSPACE="$(DetectWorkspace)"
TARGET="${TARGET:-DEBUG}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

GCC_RISCV64_PREFIX="${GCC_RISCV64_PREFIX:-}"
if [[ -z "${GCC_RISCV64_PREFIX}" ]]; then
  if command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
    GCC_RISCV64_PREFIX="riscv64-unknown-elf-"
  elif command -v riscv64-linux-gnu-gcc >/dev/null 2>&1; then
    GCC_RISCV64_PREFIX="riscv64-linux-gnu-"
  else
    GCC_RISCV64_PREFIX="riscv64-unknown-elf-"
  fi
fi

export WORKSPACE
export GCC_RISCV64_PREFIX
ConfigureModernSetupPackagePath
AppendPackagePath "${PKG_DIR}/Experimental"
export PACKAGES_PATH

PLATFORM_DSC="${PKG_DIR}/Experimental/LvglSpikePkg/LvglSpikeRiscV.dsc"

# LoongArch64/RISC-V64 UEFI support is upstream in the pinned External/lvgl
# baseline; the submodule is consumed pristine with no local patching.

if ! command -v "${GCC_RISCV64_PREFIX}gcc" >/dev/null 2>&1; then
  echo "Missing RISC-V GCC: ${GCC_RISCV64_PREFIX}gcc" >&2
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

echo "== Building LvglSpikeProbe (LVGL+UEFI port) for RISCV64, TARGET=${TARGET} =="
build \
  -a RISCV64 \
  -t GCC \
  -p "${PLATFORM_DSC}" \
  -b "${TARGET}" \
  -n "${JOBS}"

echo ""
echo "OK: LVGL built for RISCV64."
echo "EFI: ${WORKSPACE}/Build/LvglSpikeRiscV/${TARGET}_GCC/RISCV64/LvglSpikeProbe.efi"
