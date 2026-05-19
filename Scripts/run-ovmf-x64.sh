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
QEMU_BIN="${QEMU_BIN:-qemu-system-x86_64}"
MEMORY="${MEMORY:-1024}"
GRAPHICS="${GRAPHICS:-1}"
DISPLAY_BACKEND="${DISPLAY_BACKEND:-gtk}"
RESET_VARS="${RESET_VARS:-0}"
OVERLAY_DIR="${WORKSPACE}/Build/ModernSetupPkgOverlay"
MUTABLE_VARS_FD="${OVERLAY_DIR}/OvmfX64_VARS.fd"

if [[ "${DISPLAY_BACKEND}" != "gtk" && "${DISPLAY_BACKEND}" != "sdl" && "${DISPLAY_BACKEND}" != "cocoa" && "${DISPLAY_BACKEND}" != "none" ]]; then
  echo "Unsupported DISPLAY_BACKEND=${DISPLAY_BACKEND}; use gtk, sdl, cocoa, or none" >&2
  exit 1
fi

if ! command -v "${QEMU_BIN}" >/dev/null 2>&1; then
  echo "Missing QEMU binary: ${QEMU_BIN}" >&2
  echo "Install QEMU or set QEMU_BIN=/path/to/qemu-system-x86_64." >&2
  exit 1
fi

locate_fd() {
  local name="$1"
  local found=""
  shopt -s nullglob
  for candidate in "${WORKSPACE}"/Build/OvmfX64*/"${TARGET}_${TOOL_CHAIN_TAG}"/FV/"${name}"; do
    if [[ -z "${found}" || "${candidate}" -nt "${found}" ]]; then
      found="${candidate}"
    fi
  done
  shopt -u nullglob
  if [[ -z "${found}" ]]; then
    return 1
  fi
  printf '%s\n' "${found}"
}

CODE_FD="${CODE_FD:-}"
VARS_FD="${VARS_FD:-}"
if [[ -z "${CODE_FD}" ]]; then
  CODE_FD="$(locate_fd OVMF_CODE.fd || true)"
fi
if [[ -z "${VARS_FD}" ]]; then
  VARS_FD="$(locate_fd OVMF_VARS.fd || true)"
fi

if [[ -z "${CODE_FD}" || ! -f "${CODE_FD}" ]]; then
  echo "Unable to locate OVMF_CODE.fd under ${WORKSPACE}/Build/OvmfX64*/${TARGET}_${TOOL_CHAIN_TAG}/FV" >&2
  echo "Build first with Scripts/build-ovmf-x64.sh or set CODE_FD=/path/to/OVMF_CODE.fd." >&2
  exit 1
fi
if [[ -z "${VARS_FD}" || ! -f "${VARS_FD}" ]]; then
  echo "Unable to locate OVMF_VARS.fd under ${WORKSPACE}/Build/OvmfX64*/${TARGET}_${TOOL_CHAIN_TAG}/FV" >&2
  echo "Build first with Scripts/build-ovmf-x64.sh or set VARS_FD=/path/to/OVMF_VARS.fd." >&2
  exit 1
fi

mkdir -p "${OVERLAY_DIR}"
if [[ "${RESET_VARS}" == "1" || ! -f "${MUTABLE_VARS_FD}" ]]; then
  cp "${VARS_FD}" "${MUTABLE_VARS_FD}"
fi

qemu_args=(
  -machine q35,accel=kvm:tcg
  -m "${MEMORY}"
  -drive if=pflash,format=raw,unit=0,readonly=on,file="${CODE_FD}"
  -drive if=pflash,format=raw,unit=1,file="${MUTABLE_VARS_FD}"
  -vga std
  -device qemu-xhci
  -device usb-kbd
  -device usb-tablet
  -serial stdio
)

if [[ "${GRAPHICS}" == "0" || "${DISPLAY_BACKEND}" == "none" ]]; then
  qemu_args+=( -display none )
else
  qemu_args+=( -display "${DISPLAY_BACKEND}" )
fi

echo "Code: ${CODE_FD}"
echo "Vars: ${MUTABLE_VARS_FD}"
exec "${QEMU_BIN}" "${qemu_args[@]}"
