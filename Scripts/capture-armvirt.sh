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
ACCEL="${ACCEL:-tcg}"
MEMORY="${MEMORY:-1024}"
CAPTURE_PREFIX="${CAPTURE_PREFIX:-after-armvirt}"
CAPTURE_OUT_DIR="${CAPTURE_OUT_DIR:-${PKG_DIR}/Assets/Screenshots}"
CAPTURE_WORK_DIR="${CAPTURE_WORK_DIR:-${WORKSPACE}/Build/ModernSetupPkgCapture}"
CAPTURE_DRIVER_SAMPLE="${CAPTURE_DRIVER_SAMPLE:-1}"
FV_DIR="${WORKSPACE}/Build/ArmVirtQemu-AArch64/${TARGET}_CLANGDWARF/FV"
CODE_FD="${FV_DIR}/QEMU_EFI.fd"
VARS_FD="${FV_DIR}/QEMU_VARS.fd"

export PATH="/opt/homebrew/bin:${PATH}"

if [[ ! -f "${CODE_FD}" || ! -f "${VARS_FD}" ]]; then
  echo "Missing firmware files. Build first with: ${PKG_DIR}/Scripts/build-armvirt.sh" >&2
  exit 1
fi

mkdir -p "${CAPTURE_OUT_DIR}" "${CAPTURE_WORK_DIR}"
rm -f "${CAPTURE_WORK_DIR}/monitor.sock"

# Create a 64 MiB pflash image from the smaller edk2 firmware file.
#
# Arguments:
#   $1 - Source firmware file.
#   $2 - Destination pflash file.
make_pflash_image() {
  local source_file="$1"
  local target_file="$2"

  dd if=/dev/zero of="${target_file}" bs=1m count=64 status=none
  dd if="${source_file}" of="${target_file}" conv=notrunc status=none
}

case "${ACCEL}" in
  hvf)
    MACHINE_ACCEL="hvf"
    CPU_MODEL="host"
    GIC_VERSION="${GIC_VERSION:-2}"
    ;;
  tcg)
    MACHINE_ACCEL="tcg"
    CPU_MODEL="${CPU:-cortex-a57}"
    GIC_VERSION="${GIC_VERSION:-3}"
    ;;
  *)
    echo "Unsupported ACCEL='${ACCEL}'. Use ACCEL=tcg or ACCEL=hvf." >&2
    exit 1
    ;;
esac

make_pflash_image "${CODE_FD}" "${CAPTURE_WORK_DIR}/code.fd"
make_pflash_image "${VARS_FD}" "${CAPTURE_WORK_DIR}/vars.fd"

qemu-system-aarch64 \
  -machine "virt,accel=${MACHINE_ACCEL},gic-version=${GIC_VERSION}" \
  -cpu "${CPU_MODEL}" \
  -m "${MEMORY}" \
  -drive "if=pflash,format=raw,unit=0,readonly=on,file=${CAPTURE_WORK_DIR}/code.fd" \
  -drive "if=pflash,format=raw,unit=1,file=${CAPTURE_WORK_DIR}/vars.fd" \
  -device ramfb \
  -device qemu-xhci \
  -device usb-kbd \
  -device virtio-net-pci,netdev=net0 \
  -netdev user,id=net0 \
  -serial "file:${CAPTURE_WORK_DIR}/serial.log" \
  -monitor "unix:${CAPTURE_WORK_DIR}/monitor.sock,server=on,wait=off" \
  -display none \
  -daemonize

python3 - <<'PY' "${CAPTURE_WORK_DIR}/monitor.sock" "${CAPTURE_WORK_DIR}" "${CAPTURE_PREFIX}" "${CAPTURE_DRIVER_SAMPLE}"
from pathlib import Path
import os
import socket
import sys
import time

sock_path = Path(sys.argv[1])
work_dir = Path(sys.argv[2])
prefix = sys.argv[3]
capture_driver_sample = sys.argv[4] != "0"

for _ in range(30):
    if sock_path.exists():
        break
    time.sleep(1)

monitor = socket.socket(socket.AF_UNIX)
monitor.connect(str(sock_path))
monitor.settimeout(1)


def drain():
    try:
        monitor.recv(65536)
    except Exception:
        pass


def command(text, delay=1.0):
    monitor.sendall((text + "\n").encode())
    time.sleep(delay)
    drain()


def shot(name):
    command(f"screendump {work_dir / (prefix + '-' + name + '.ppm')}", 0.5)


time.sleep(8)
command("sendkey ret", 3)
shot("frontpage")
command("sendkey down", 1)
shot("frontpage-device-selected")
command("sendkey ret", 3)
shot("device-manager")
if capture_driver_sample:
    for _ in range(3):
        command("sendkey down", 0.4)
    shot("browser-testcase-selected")
    command("sendkey ret", 3)
    shot("driver-sample-first-page")
    for _ in range(5):
        command("sendkey down", 0.4)
    shot("driver-sample-oneof-selected")
    command("sendkey ret", 2)
    shot("driver-sample-oneof-popup")
command("quit", 0.2)
PY

for ppm in "${CAPTURE_WORK_DIR}"/"${CAPTURE_PREFIX}"-*.ppm; do
  [[ -f "${ppm}" ]] || continue
  png="${CAPTURE_OUT_DIR}/$(basename "${ppm%.ppm}.png")"
  if command -v sips >/dev/null 2>&1; then
    sips -s format png "${ppm}" --out "${png}" >/dev/null
  else
    cp "${ppm}" "${CAPTURE_OUT_DIR}/$(basename "${ppm}")"
    echo "sips not found; left PPM capture at ${CAPTURE_OUT_DIR}/$(basename "${ppm}")" >&2
    continue
  fi
  echo "Captured: ${png}"
done
