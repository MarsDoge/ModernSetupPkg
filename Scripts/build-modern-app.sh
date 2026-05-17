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
ARCH="${ARCH:-AARCH64}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)}"
APP_ESP="${WORKSPACE}/Build/ModernSetupAppEsp"

export PATH="/opt/homebrew/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/opt/lld/bin:${PATH}"
export CLANGDWARF_BIN="${CLANGDWARF_BIN:-/opt/homebrew/opt/llvm/bin/}"
export WORKSPACE

case "${ARCH}" in
  AARCH64)
    TOOL_CHAIN_TAG="${TOOL_CHAIN_TAG:-CLANGDWARF}"
    BOOT_FILE="BOOTAA64.EFI"
    ;;
  LOONGARCH64)
    TOOL_CHAIN_TAG="${TOOL_CHAIN_TAG:-GCC}"
    if [[ -z "${GCC_LOONGARCH64_PREFIX:-}" ]]; then
      if command -v loongarch64-unknown-linux-gnu-gcc >/dev/null 2>&1; then
        GCC_LOONGARCH64_PREFIX="loongarch64-unknown-linux-gnu-"
      elif command -v loongarch64-linux-gnu-gcc >/dev/null 2>&1; then
        GCC_LOONGARCH64_PREFIX="loongarch64-linux-gnu-"
      else
        GCC_LOONGARCH64_PREFIX="loongarch64-unknown-linux-gnu-"
      fi
    fi
    export GCC_LOONGARCH64_PREFIX
    BOOT_FILE="BOOTLOONGARCH64.EFI"
    ;;
  *)
    echo "Unsupported ARCH='${ARCH}'. Use ARCH=AARCH64 or ARCH=LOONGARCH64." >&2
    exit 1
    ;;
esac

APP_EFI="${WORKSPACE}/Build/ModernSetupPkgExperimental/${TARGET}_${TOOL_CHAIN_TAG}/${ARCH}/ModernSetupApp.efi"

if [[ ! -d "${WORKSPACE}/MdePkg" || ! -d "${WORKSPACE}/ArmVirtPkg" ]]; then
  echo "WORKSPACE does not look like an edk2 checkout: ${WORKSPACE}" >&2
  exit 1
fi

if [[ "$(cd "${PKG_DIR}" && pwd)" != "${WORKSPACE}/ModernSetupPkg" ]]; then
  echo "ModernSetupPkg should be checked out at ${WORKSPACE}/ModernSetupPkg" >&2
  echo "Current package path: ${PKG_DIR}" >&2
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
  -p ModernSetupPkg/Experimental/ModernSetupApp.dsc \
  -a "${ARCH}" \
  -t "${TOOL_CHAIN_TAG}" \
  -b "${TARGET}" \
  -n "${JOBS}"

mkdir -p "${APP_ESP}/EFI/BOOT"
cp "${APP_EFI}" "${APP_ESP}/EFI/BOOT/${BOOT_FILE}"

echo "Built app: ${APP_EFI}"
echo "Prepared ESP: ${APP_ESP}"
echo "Boot file:    ${APP_ESP}/EFI/BOOT/${BOOT_FILE}"
