#!/usr/bin/env bash
# Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
# Author: MarsDoge (Dongyan Qian)
# Open source: https://github.com/MarsDoge/ModernSetupPkg
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
# Shared edk2 workspace discovery helpers. Source this file from scripts after
# setting PKG_DIR to the ModernSetupPkg repository root.

DetectWorkspace() {
  local PkgParent

  if [[ -n "${WORKSPACE:-}" ]]; then
    printf '%s\n' "${WORKSPACE}"
    return
  fi

  PkgParent="$(cd "${PKG_DIR}/.." && pwd)"

  if [[ -d "${PKG_DIR}/External/edk2/MdePkg" && -d "${PKG_DIR}/External/edk2/BaseTools" ]]; then
    printf '%s\n' "${PKG_DIR}/External/edk2"
    return
  fi

  if [[ -d "${PkgParent}/edk2/MdePkg" && -d "${PkgParent}/edk2/BaseTools" ]]; then
    printf '%s\n' "${PkgParent}/edk2"
    return
  fi

  if [[ -d "${PkgParent}/MdePkg" && -d "${PkgParent}/BaseTools" ]]; then
    printf '%s\n' "${PkgParent}"
    return
  fi

  printf '%s\n' "${PKG_DIR}/External/edk2"
}

AppendPackagePath() {
  local PathEntry="$1"
  local ExistingEntry
  local ExistingEntries=()

  if [[ -n "${PACKAGES_PATH:-}" ]]; then
    IFS=':' read -r -a ExistingEntries <<< "${PACKAGES_PATH}"
  fi

  for ExistingEntry in "${ExistingEntries[@]}"; do
    if [[ "${ExistingEntry}" == "${PathEntry}" ]]; then
      return
    fi
  done

  if [[ -n "${PACKAGES_PATH:-}" ]]; then
    PACKAGES_PATH="${PACKAGES_PATH}:${PathEntry}"
  else
    PACKAGES_PATH="${PathEntry}"
  fi
}

ConfigureModernSetupPackagePath() {
  local PkgParent

  PkgParent="$(cd "${PKG_DIR}/.." && pwd)"
  if [[ "$(cd "${PKG_DIR}" && pwd)" != "${WORKSPACE}/ModernSetupPkg" ]]; then
    AppendPackagePath "${PkgParent}"
    export PACKAGES_PATH
  fi
}
