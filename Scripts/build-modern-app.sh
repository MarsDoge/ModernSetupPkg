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

ClangSupportsArch() {
  local Dir="$1"

  case "${ARCH}" in
    AARCH64)
      "${Dir}/clang" -print-targets 2>/dev/null | grep -Eq '^[[:space:]]*aarch64[[:space:]]'
      ;;
    X64)
      "${Dir}/clang" -print-targets 2>/dev/null | grep -Eq '^[[:space:]]*x86-64[[:space:]]'
      ;;
    *)
      return 0
      ;;
  esac
}

HasLldForClangDir() {
  local Dir="$1"

  [[ -x "${Dir}/ld.lld" ]] || [[ -x /opt/homebrew/opt/lld/bin/ld.lld ]]
}

FindClangBin() {
  local Dir
  local Clang

  if [[ -n "${CLANGDWARF_BIN:-}" ]]; then
    printf '%s\n' "${CLANGDWARF_BIN%/}/"
    return
  fi

  for Dir in \
    /opt/homebrew/opt/llvm/bin \
    /usr/lib/llvm-*/bin \
    /usr/bin \
    /opt/rocm/llvm/bin \
    /usr/local/opt/llvm/bin; do
    if [[ -x "${Dir}/clang" && -x "${Dir}/llvm-objcopy" && -x "${Dir}/llvm-ar" ]] && ClangSupportsArch "${Dir}" && HasLldForClangDir "${Dir}"; then
      printf '%s/\n' "${Dir}"
      return
    fi
  done

  if Clang="$(command -v clang 2>/dev/null)"; then
    Dir="$(dirname "${Clang}")"
    if [[ -x "${Dir}/llvm-objcopy" && -x "${Dir}/llvm-ar" ]] && ClangSupportsArch "${Dir}" && HasLldForClangDir "${Dir}"; then
      printf '%s/\n' "${Dir}"
      return
    fi
  fi

  printf '%s\n' ""
}

WORKSPACE="$(DetectWorkspace)"
TARGET="${TARGET:-DEBUG}"
ARCH="${ARCH:-AARCH64}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)}"
APP_ESP="${WORKSPACE}/Build/ModernSetupAppEsp"

# Keep Homebrew LLVM/LLD discoverable on macOS, but do not force those paths as
# the CLANGDWARF tool root on Linux hosts with clang installed elsewhere.
export PATH="/opt/homebrew/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/opt/lld/bin:${PATH}"

case "${ARCH}" in
  X64)
    TOOL_CHAIN_TAG="${TOOL_CHAIN_TAG:-CLANGDWARF}"
    BOOT_FILE="BOOTX64.EFI"
    ;;
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
    echo "Unsupported ARCH='${ARCH}'. Use ARCH=X64, ARCH=AARCH64, or ARCH=LOONGARCH64." >&2
    exit 1
    ;;
esac

if [[ "${TOOL_CHAIN_TAG}" == "CLANGDWARF" ]]; then
  CLANGDWARF_BIN="$(FindClangBin)"
  if [[ -z "${CLANGDWARF_BIN}" ]]; then
    echo "Unable to find clang, llvm-objcopy, llvm-ar, and ld.lld for CLANGDWARF ${ARCH}." >&2
    echo "Set CLANGDWARF_BIN=/path/to/llvm/bin/ with a clang/lld build that supports ${ARCH}, then retry." >&2
    exit 1
  fi
  if ! HasLldForClangDir "${CLANGDWARF_BIN%/}"; then
    echo "CLANGDWARF_BIN does not provide ld.lld and no Homebrew lld was found: ${CLANGDWARF_BIN}" >&2
    exit 1
  fi
  export CLANGDWARF_BIN
  export CLANG_BIN="${CLANG_BIN:-${CLANGDWARF_BIN}}"
  export PATH="${CLANGDWARF_BIN%/}:${PATH}"
fi

export WORKSPACE

ConfigureModernSetupPackagePath

APP_EFI="${WORKSPACE}/Build/ModernSetupPkgExperimental/${TARGET}_${TOOL_CHAIN_TAG}/${ARCH}/ModernSetupApp.efi"

if [[ ! -d "${WORKSPACE}/MdePkg" || ! -d "${WORKSPACE}/BaseTools" ]]; then
  echo "WORKSPACE does not look like an edk2 checkout: ${WORKSPACE}" >&2
  echo "Set WORKSPACE to the edk2 checkout root, for example: WORKSPACE=/path/to/edk2 $0" >&2
  exit 1
fi

if [[ ! -d "${WORKSPACE}/ArmVirtPkg" ]]; then
  echo "WORKSPACE is missing ArmVirtPkg: ${WORKSPACE}" >&2
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
