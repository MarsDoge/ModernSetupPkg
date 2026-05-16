#!/usr/bin/env bash
set -euo pipefail

PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${WORKSPACE:-$(cd "${PKG_DIR}/.." && pwd)}"
TARGET="${TARGET:-DEBUG}"
ACCEL="${ACCEL:-tcg}"
GRAPHICS="${GRAPHICS:-1}"
MEMORY="${MEMORY:-1024}"
FV_DIR="${WORKSPACE}/Build/ArmVirtQemu-AArch64/${TARGET}_CLANGDWARF/FV"
CODE_FD="${FV_DIR}/QEMU_EFI.fd"
VARS_FD="${FV_DIR}/QEMU_VARS.fd"
PFLASH_CODE="${FV_DIR}/QEMU_EFI.modern.pflash.fd"
PFLASH_VARS="${FV_DIR}/QEMU_VARS.modern.work.fd"

export PATH="/opt/homebrew/bin:${PATH}"

if [[ ! -f "${CODE_FD}" || ! -f "${VARS_FD}" ]]; then
  echo "Missing firmware files. Build first with: ${PKG_DIR}/Scripts/build-armvirt.sh" >&2
  exit 1
fi

make_pflash_image() {
  local source_file="$1"
  local target_file="$2"

  dd if=/dev/zero of="${target_file}" bs=1m count=64 status=none
  dd if="${source_file}" of="${target_file}" conv=notrunc status=none
}

make_pflash_image "${CODE_FD}" "${PFLASH_CODE}"

if [[ "${RESET_VARS:-0}" == "1" || ! -f "${PFLASH_VARS}" ]]; then
  make_pflash_image "${VARS_FD}" "${PFLASH_VARS}"
fi

case "${ACCEL}" in
  hvf)
    MACHINE_ACCEL="hvf"
    CPU_MODEL="host"
    ;;
  tcg)
    MACHINE_ACCEL="tcg"
    CPU_MODEL="${CPU:-cortex-a57}"
    ;;
  *)
    echo "Unsupported ACCEL='${ACCEL}'. Use ACCEL=tcg or ACCEL=hvf." >&2
    exit 1
    ;;
esac

QEMU_ARGS=(
  -machine "virt,accel=${MACHINE_ACCEL},gic-version=3"
  -cpu "${CPU_MODEL}"
  -m "${MEMORY}"
  -monitor none
  -drive "if=pflash,format=raw,unit=0,readonly=on,file=${PFLASH_CODE}"
  -drive "if=pflash,format=raw,unit=1,file=${PFLASH_VARS}"
  -device virtio-net-pci,netdev=net0
  -netdev user,id=net0
)

if [[ "${GRAPHICS}" == "1" ]]; then
  QEMU_ARGS+=(
    -display cocoa
    -device ramfb
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

echo "Press Esc or F2 during BDS wait to enter ModernSetupApp."
exec qemu-system-aarch64 "${QEMU_ARGS[@]}"
