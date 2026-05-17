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
GRAPHICS="${GRAPHICS:-1}"
MEMORY="${MEMORY:-4096}"
SMP="${SMP:-2}"
CPU="${CPU:-la464}"
FV_DIR="${WORKSPACE}/Build/LoongArchVirtQemu/${TARGET}_GCC/FV"
CODE_FD="${FV_DIR}/QEMU_EFI.fd"
VARS_FD="${FV_DIR}/QEMU_VARS.fd"
PFLASH_VARS="${FV_DIR}/QEMU_VARS.modern.loongarch.work.fd"

export PATH="/opt/homebrew/bin:${PATH}"

find_qemu_binary() {
  local candidate

  if [[ -n "${QEMU_BIN:-}" ]]; then
    echo "${QEMU_BIN}"
    return 0
  fi

  for candidate in /usr/bin/qemu-system-loongarch64 /usr/local/bin/qemu-system-loongarch64 "$(command -v qemu-system-loongarch64 2>/dev/null || true)"; do
    if [[ -n "${candidate}" && -x "${candidate}" ]]; then
      echo "${candidate}"
      return 0
    fi
  done

  return 1
}

qemu_supports_display() {
  local backend="$1"

  "${QEMU}" -display help 2>&1 | grep -qx "${backend}"
}

qemu_supports_pflash() {
  local status

  set +e
  timeout 2s "${QEMU}" \
    -M virt \
    -display none \
    -S \
    -drive "if=pflash,format=raw,unit=0,readonly=on,file=${CODE_FD}" \
    >/dev/null 2>&1
  status=$?
  set -e

  [[ "${status}" == "124" ]]
}

select_display_backend() {
  local backend

  if [[ -n "${DISPLAY_BACKEND:-}" ]]; then
    echo "${DISPLAY_BACKEND}"
    return 0
  fi

  case "$(uname -s)" in
    Darwin)
      backend="cocoa"
      ;;
    Linux)
      for backend in gtk sdl spice-app dbus; do
        if qemu_supports_display "${backend}"; then
          echo "${backend}"
          return 0
        fi
      done
      backend="none"
      ;;
    *)
      backend="none"
      ;;
  esac

  echo "${backend}"
}

if ! QEMU="$(find_qemu_binary)"; then
  echo "Missing qemu-system-loongarch64. Install QEMU first, for example with Homebrew." >&2
  exit 1
fi

if [[ ! -f "${CODE_FD}" || ! -f "${VARS_FD}" ]]; then
  echo "Missing LoongArchVirt firmware files." >&2
  echo "Build first with: ${PKG_DIR}/Scripts/build-loongarchvirt.sh" >&2
  exit 1
fi

if [[ "${RESET_VARS:-0}" == "1" || ! -f "${PFLASH_VARS}" ]]; then
  cp "${VARS_FD}" "${PFLASH_VARS}"
fi

QEMU_ARGS=(
  -M virt
  -cpu "${CPU}"
  -smp "${SMP}"
  -m "${MEMORY}"
  -monitor none
  -device virtio-net-pci,netdev=net0
  -netdev user,id=net0
)

if qemu_supports_pflash; then
  QEMU_ARGS+=(
    -drive "if=pflash,format=raw,unit=0,readonly=on,file=${CODE_FD}"
    -drive "if=pflash,format=raw,unit=1,file=${PFLASH_VARS}"
  )
  FIRMWARE_MODE="pflash"
else
  QEMU_ARGS+=(
    -bios "${CODE_FD}"
  )
  FIRMWARE_MODE="bios"
fi

if [[ "${GRAPHICS}" == "1" ]]; then
  DISPLAY_BACKEND="$(select_display_backend)"
  if ! qemu_supports_display "${DISPLAY_BACKEND}"; then
    echo "${QEMU} does not support DISPLAY_BACKEND='${DISPLAY_BACKEND}'." >&2
    echo "Available display backends:" >&2
    "${QEMU}" -display help >&2
    echo "Set QEMU_BIN=/path/to/qemu-system-loongarch64 or use GRAPHICS=0 for serial validation." >&2
    exit 1
  fi

  if [[ "${DISPLAY_BACKEND}" == "none" ]]; then
    echo "${QEMU} has no usable graphical display backend; use GRAPHICS=0 for serial validation." >&2
    echo "Available display backends:" >&2
    "${QEMU}" -display help >&2
    exit 1
  fi

  QEMU_ARGS+=(
    -display "${DISPLAY_BACKEND}"
    -device virtio-gpu-pci
    -device qemu-xhci
    -device usb-kbd
    -device usb-tablet
    -serial stdio
  )
else
  QEMU_ARGS+=(
    -nographic
    -serial stdio
  )
fi

echo "Using QEMU: ${QEMU}"
echo "Using firmware mode: ${FIRMWARE_MODE}"
if [[ "${FIRMWARE_MODE}" == "bios" ]]; then
  echo "Warning: ${QEMU} does not support LoongArch pflash; QEMU_VARS.fd is not attached."
fi
if [[ "${GRAPHICS}" == "1" ]]; then
  echo "Using display backend: ${DISPLAY_BACKEND}"
fi
echo "Press Esc or F2 during BDS wait to enter native UiApp rendered by ModernDisplayEngineDxe."
exec "${QEMU}" "${QEMU_ARGS[@]}"
