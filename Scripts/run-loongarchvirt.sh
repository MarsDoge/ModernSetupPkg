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

if ! command -v qemu-system-loongarch64 >/dev/null 2>&1; then
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
  -drive "if=pflash,format=raw,unit=0,readonly=on,file=${CODE_FD}"
  -drive "if=pflash,format=raw,unit=1,file=${PFLASH_VARS}"
  -device virtio-net-pci,netdev=net0
  -netdev user,id=net0
)

if [[ "${GRAPHICS}" == "1" ]]; then
  QEMU_ARGS+=(
    -display cocoa
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

echo "Press Esc or F2 during BDS wait to enter native UiApp rendered by ModernDisplayEngineDxe."
exec qemu-system-loongarch64 "${QEMU_ARGS[@]}"
