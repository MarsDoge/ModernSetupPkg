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
WORKSPACE="$(DetectWorkspace)"
TARGET="${TARGET:-DEBUG}"
TOOL_CHAIN_TAG="${TOOL_CHAIN_TAG:-GCC}"
QEMU_BIN="${QEMU_BIN:-qemu-system-x86_64}"
MEMORY="${MEMORY:-1024}"
BOOT_WAIT_SECONDS="${BOOT_WAIT_SECONDS:-10}"
SENDKEY_SEQUENCE="${SENDKEY_SEQUENCE:-}"
RESET_VARS="${RESET_VARS:-1}"
BOOT_APP="${BOOT_APP:-1}"
ESP_DIR="${ESP_DIR:-${WORKSPACE}/Build/ModernSetupAppEsp}"
CAPTURE_OUT_DIR="${CAPTURE_OUT_DIR:-${TMPDIR:-/tmp}/modernsetup-qemu}"
CAPTURE_WORK_DIR="${CAPTURE_WORK_DIR:-${WORKSPACE}/Build/ModernSetupPkgCapture/OvmfX64}"
CAPTURE_PREFIX="${CAPTURE_PREFIX:-modernsetup-ovmf-x64-$(date +%Y%m%d-%H%M%S)}"

if [[ -z "${CAPTURE_PREFIX}" || "${CAPTURE_PREFIX}" == *..* || ! "${CAPTURE_PREFIX}" =~ ^[A-Za-z0-9._-]+$ ]]; then
  echo "Invalid CAPTURE_PREFIX: '${CAPTURE_PREFIX}'" >&2
  echo "Use only A-Z, a-z, 0-9, dot, underscore, and hyphen; do not use slashes, backslashes, '..', or an empty value." >&2
  exit 1
fi

if ! command -v "${QEMU_BIN}" >/dev/null 2>&1; then
  echo "Missing QEMU binary: ${QEMU_BIN}" >&2
  echo "Install QEMU or set QEMU_BIN=/path/to/qemu-system-x86_64." >&2
  exit 1
fi

locate_build_fd() {
  local name="$1"
  local found=""
  shopt -s nullglob
  for candidate in "${WORKSPACE}"/Build/OvmfX64*/"${TARGET}_${TOOL_CHAIN_TAG}"/FV/"${name}"; do
    if [[ -z "${found}" || "${candidate}" -nt "${found}" ]]; then
      found="${candidate}"
    fi
  done
  shopt -u nullglob
  if [[ -n "${found}" ]]; then
    printf '%s\n' "${found}"
  fi
}

first_existing() {
  local candidate
  for candidate in "$@"; do
    if [[ -n "${candidate}" && -f "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done
  return 1
}

BUILD_CODE_FD="$(locate_build_fd OVMF_CODE.fd)"
BUILD_VARS_FD="$(locate_build_fd OVMF_VARS.fd)"

CODE_FD="${BUILD_CODE_FD}"
VARS_FD="${BUILD_VARS_FD}"
FIRMWARE_SOURCE="workspace build"

if [[ -z "${CODE_FD}" || -z "${VARS_FD}" ]]; then
  ENV_CODE_FD="${OVMF_CODE:-${CODE_FD:-}}"
  ENV_VARS_FD="${OVMF_VARS:-${VARS_FD:-}}"
  if [[ -z "${ENV_CODE_FD}" ]]; then
    ENV_CODE_FD="${CODE_FD:-}"
  fi
  if [[ -z "${ENV_VARS_FD}" ]]; then
    ENV_VARS_FD="${VARS_FD:-}"
  fi
  if [[ -n "${ENV_CODE_FD}" && -n "${ENV_VARS_FD}" ]]; then
    CODE_FD="${ENV_CODE_FD}"
    VARS_FD="${ENV_VARS_FD}"
    FIRMWARE_SOURCE="environment"
  else
    CODE_FD="$(first_existing \
      /usr/share/OVMF/OVMF_CODE.fd \
      /usr/share/OVMF/OVMF_CODE_4M.fd \
      /usr/share/edk2/ovmf/OVMF_CODE.fd \
      /usr/share/edk2/ovmf/OVMF_CODE_4M.fd || true)"
    VARS_FD="$(first_existing \
      /usr/share/OVMF/OVMF_VARS.fd \
      /usr/share/OVMF/OVMF_VARS_4M.fd \
      /usr/share/edk2/ovmf/OVMF_VARS.fd \
      /usr/share/edk2/ovmf/OVMF_VARS_4M.fd || true)"
    FIRMWARE_SOURCE="system fallback"
  fi
fi

if [[ -z "${CODE_FD}" || ! -f "${CODE_FD}" ]]; then
  echo "Unable to locate OVMF_CODE.fd." >&2
  echo "Build first with Scripts/build-ovmf-x64.sh or set OVMF_CODE=/path/to/OVMF_CODE.fd." >&2
  exit 1
fi
if [[ -z "${VARS_FD}" || ! -f "${VARS_FD}" ]]; then
  echo "Unable to locate OVMF_VARS.fd." >&2
  echo "Build first with Scripts/build-ovmf-x64.sh or set OVMF_VARS=/path/to/OVMF_VARS.fd." >&2
  exit 1
fi
if [[ "${FIRMWARE_SOURCE}" == "system fallback" ]]; then
  echo "Warning: using system OVMF fallback. Secure Boot builds may reject unsigned BOOTX64.EFI." >&2
fi

if [[ "${BOOT_APP}" != "0" ]]; then
  if [[ ! -f "${ESP_DIR}/EFI/BOOT/BOOTX64.EFI" ]]; then
    echo "Missing ${ESP_DIR}/EFI/BOOT/BOOTX64.EFI." >&2
    echo "Build/copy the app ESP first, set ESP_DIR, or set BOOT_APP=0 for firmware-only capture." >&2
    exit 1
  fi
fi

mkdir -p "${CAPTURE_OUT_DIR}" "${CAPTURE_WORK_DIR}"
MONITOR_SOCK="${CAPTURE_WORK_DIR}/monitor.sock"
PID_FILE="${CAPTURE_WORK_DIR}/qemu.pid"
SERIAL_LOG="${CAPTURE_WORK_DIR}/serial.log"
MUTABLE_VARS_FD="${CAPTURE_WORK_DIR}/OVMF_VARS.fd"
WORK_PPM="${CAPTURE_WORK_DIR}/${CAPTURE_PREFIX}.ppm"
OUT_PPM="${CAPTURE_OUT_DIR}/${CAPTURE_PREFIX}.ppm"
OUT_PNG="${CAPTURE_OUT_DIR}/${CAPTURE_PREFIX}.png"

rm -f "${MONITOR_SOCK}" "${PID_FILE}" "${SERIAL_LOG}" "${WORK_PPM}"
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
  -serial "file:${SERIAL_LOG}"
  -monitor "unix:${MONITOR_SOCK},server=on,wait=off"
  -display none
  -pidfile "${PID_FILE}"
  -daemonize
)
if [[ "${BOOT_APP}" != "0" ]]; then
  qemu_args+=( -drive "if=ide,format=raw,file=fat:rw:${ESP_DIR}" )
fi

"${QEMU_BIN}" "${qemu_args[@]}"

CLEANUP_QEMU=1

qemu_pid_matches_capture() {
  local pid="$1"
  [[ -n "${pid}" && "${pid}" =~ ^[0-9]+$ ]] || return 1
  [[ -d "/proc/${pid}" ]] || return 1
  python3 - <<'PY' "${pid}" "${PID_FILE}" "${MONITOR_SOCK}" "${CODE_FD}" "${MUTABLE_VARS_FD}" 2>/dev/null
from pathlib import Path
import sys

pid, pid_file, monitor_sock, code_fd, vars_fd = sys.argv[1:]
try:
    cmdline = Path("/proc") / pid / "cmdline"
    parts = [part.decode("utf-8", "replace") for part in cmdline.read_bytes().split(b"\0") if part]
except OSError:
    raise SystemExit(1)

text = "\n".join(parts)
required = (pid_file, monitor_sock, code_fd, vars_fd)
if "qemu-system" not in Path(parts[0]).name or not all(token in text for token in required):
    raise SystemExit(1)
PY
}

cleanup_qemu() {
  local status=$?
  if [[ "${CLEANUP_QEMU:-0}" == "1" ]]; then
    local pid=""
    if [[ -f "${PID_FILE}" ]]; then
      pid="$(<"${PID_FILE}")"
    fi

    if [[ -S "${MONITOR_SOCK}" ]]; then
      python3 - <<'PY' "${MONITOR_SOCK}" >/dev/null 2>&1 || true
from pathlib import Path
import socket
import sys

sock_path = Path(sys.argv[1])
monitor = socket.socket(socket.AF_UNIX)
monitor.settimeout(1.0)
monitor.connect(str(sock_path))
try:
    monitor.recv(65536)
except Exception:
    pass
monitor.sendall(b"quit\n")
monitor.close()
PY
      sleep 0.5
    fi

    if [[ -n "${pid}" ]] && qemu_pid_matches_capture "${pid}"; then
      kill "${pid}" 2>/dev/null || true
      for _ in {1..20}; do
        if ! qemu_pid_matches_capture "${pid}"; then
          break
        fi
        sleep 0.1
      done
      if qemu_pid_matches_capture "${pid}"; then
        kill -KILL "${pid}" 2>/dev/null || true
      fi
    fi
  fi
  exit "${status}"
}

trap cleanup_qemu EXIT

python3 - <<'PY' "${MONITOR_SOCK}" "${WORK_PPM}" "${BOOT_WAIT_SECONDS}" "${SENDKEY_SEQUENCE}"
from pathlib import Path
import socket
import sys
import time

sock_path = Path(sys.argv[1])
ppm_path = Path(sys.argv[2])
boot_wait = float(sys.argv[3])
sequence = [key.strip() for key in sys.argv[4].split(",") if key.strip()]

for _ in range(300):
    if sock_path.exists():
        break
    time.sleep(0.1)
else:
    raise SystemExit(f"monitor socket did not appear: {sock_path}")

monitor = socket.socket(socket.AF_UNIX)
monitor.connect(str(sock_path))
monitor.settimeout(1.0)


def drain() -> None:
    while True:
        try:
            data = monitor.recv(65536)
        except socket.timeout:
            return
        if not data:
            return
        if len(data) < 65536:
            return


def command(text: str, delay: float = 0.2) -> None:
    monitor.sendall((text + "\n").encode("utf-8"))
    time.sleep(delay)
    drain()


drain()
time.sleep(boot_wait)
for key in sequence:
    command(f"sendkey {key}", 0.5)
command(f"screendump {ppm_path}", 0.5)
command("quit", 0.1)
monitor.close()
PY

CLEANUP_QEMU=0

cp "${WORK_PPM}" "${OUT_PPM}"

PNG_CREATED=0
if python3 - <<'PY' "${OUT_PPM}" "${OUT_PNG}" >/dev/null 2>&1; then
from PIL import Image
import sys
Image.open(sys.argv[1]).save(sys.argv[2])
PY
  PNG_CREATED=1
elif command -v pnmtopng >/dev/null 2>&1; then
  pnmtopng "${OUT_PPM}" > "${OUT_PNG}"
  PNG_CREATED=1
elif command -v magick >/dev/null 2>&1; then
  magick "${OUT_PPM}" "${OUT_PNG}"
  PNG_CREATED=1
elif command -v convert >/dev/null 2>&1; then
  convert "${OUT_PPM}" "${OUT_PNG}"
  PNG_CREATED=1
elif command -v sips >/dev/null 2>&1; then
  sips -s format png "${OUT_PPM}" --out "${OUT_PNG}" >/dev/null
  PNG_CREATED=1
fi

echo "OVMF source: ${FIRMWARE_SOURCE}"
echo "Code FD: ${CODE_FD}"
echo "Vars FD: ${MUTABLE_VARS_FD}"
if [[ "${BOOT_APP}" != "0" ]]; then
  echo "ESP: ${ESP_DIR}"
fi
echo "Serial log: ${SERIAL_LOG}"
echo "PPM capture: ${OUT_PPM}"
if [[ "${PNG_CREATED}" == "1" ]]; then
  echo "PNG capture: ${OUT_PNG}"
else
  echo "PNG capture: not produced; PPM is the canonical screendump artifact."
fi
if [[ "${FIRMWARE_SOURCE}" == "system fallback" ]]; then
  echo "Note: system OVMF may enforce Secure Boot and reject unsigned BOOTX64.EFI. Prefer a local edk2 OVMF build for app captures."
fi
