#!/usr/bin/env bash
# Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
# Author: MarsDoge (Dongyan Qian)
# Open source: https://github.com/MarsDoge/ModernSetupPkg
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
# Initialize the pinned edk2 baseline submodule used by ModernSetupPkg builds.

set -euo pipefail

PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EDK2_DIR="${EDK2_DIR:-${PKG_DIR}/External/edk2}"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
BUILD_BASETOOLS="${BUILD_BASETOOLS:-0}"

cd "${PKG_DIR}"

if [[ ! -f .gitmodules ]] || ! git config -f .gitmodules --get submodule.External/edk2.url >/dev/null; then
  echo "Missing External/edk2 entry in .gitmodules." >&2
  exit 1
fi

echo "Updating edk2 submodule: External/edk2"
git submodule update --init -- External/edk2

# Initialize edk2 first-level dependencies only.  A full recursive update pulls
# optional nested OpenSSL test submodules, which are not required for the
# ModernSetupPkg BaseTools/app/OVMF baseline and can make validation slow,
# fragile, and dirty.  Keep the normal bootstrap narrow and reproducible.
echo "Updating edk2 first-level submodules (avoiding optional nested OpenSSL tests)"
git -C "${EDK2_DIR}" submodule update --init --checkout

# MbedTLS uses a required nested framework submodule for edk2 crypto builds;
# initialize just that nested dependency when the parent checkout is present.
MBEDTLS_DIR="${EDK2_DIR}/CryptoPkg/Library/MbedTlsLib/mbedtls"
if [[ -d "${MBEDTLS_DIR}" ]]; then
  echo "Updating required MbedTLS framework submodule"
  git -C "${MBEDTLS_DIR}" submodule update --init --checkout framework
fi

if [[ ! -d "${EDK2_DIR}/MdePkg" || ! -d "${EDK2_DIR}/BaseTools" ]]; then
  echo "Submodule does not look like an edk2 checkout: ${EDK2_DIR}" >&2
  exit 1
fi

EDK2_SHA="$(git -C "${EDK2_DIR}" rev-parse HEAD)"
echo "edk2 baseline: ${EDK2_SHA}"

if [[ "${BUILD_BASETOOLS}" == "1" ]]; then
  echo "Building BaseTools with JOBS=${JOBS}"
  make -C "${EDK2_DIR}/BaseTools" -j"${JOBS}"
else
  echo "BaseTools build skipped. To build now, run:"
  echo "  BUILD_BASETOOLS=1 ${PKG_DIR}/Scripts/bootstrap-edk2.sh"
  echo "Or build an app directly, for example:"
  echo "  WORKSPACE=${EDK2_DIR} ARCH=X64 TARGET=DEBUG TOOL_CHAIN_TAG=GCC ${PKG_DIR}/Scripts/build-modern-app.sh"
fi
