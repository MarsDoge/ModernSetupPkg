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
MODE="${MODE:-dry-run}"
BOOT_APP="${BOOT_APP:-0}"
BOOT_WAIT_SECONDS="${BOOT_WAIT_SECONDS:-12}"
SENDKEY_SEQUENCE="${SENDKEY_SEQUENCE:-esc,ret}"
CAPTURE_OUT_DIR="${CAPTURE_OUT_DIR:-${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64}"
CAPTURE_WORK_DIR="${CAPTURE_WORK_DIR:-${WORKSPACE}/Build/ModernSetupPkgCapture/DisplayEngineOvmfX64}"
BUILD_SCRIPT="${PKG_DIR}/Scripts/build-ovmf-x64.sh"
CAPTURE_SCRIPT="${PKG_DIR}/Scripts/capture-ovmf-x64.sh"

usage() {
  cat <<'USAGE'
Usage: Scripts/capture-displayengine-ovmf-x64.sh [--mode dry-run|generate-only|build|capture]

Create an OVMF X64 native-vs-modern DisplayEngine evidence directory without
modifying upstream edk2 platform files. The helper drives the existing OVMF
overlay generator twice, once with MODERN_SETUP_DISPLAY_ENGINE=native and once
with MODERN_SETUP_DISPLAY_ENGINE=modern.

Modes:
  dry-run        Print the planned artifact paths and commands only. Default.
  generate-only Generate and copy native/modern overlay DSC/FDF files only.
  build         Build each variant and copy OVMF_CODE.fd/OVMF_VARS.fd into the
                evidence directory for stable later capture.
  capture       Build each variant, then invoke Scripts/capture-ovmf-x64.sh for
                QEMU screendumps in native/ and modern/ subdirectories.

Important environment overrides:
  WORKSPACE             edk2 workspace. Auto-detected like other scripts.
  TARGET, TOOL_CHAIN_TAG, JOBS, MODERN_SETUP_THEME
  CAPTURE_OUT_DIR       default: ${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64
  CAPTURE_WORK_DIR      default: ${WORKSPACE}/Build/ModernSetupPkgCapture/DisplayEngineOvmfX64
  BOOT_APP              default: 0, firmware/FormBrowser capture path
  BOOT_WAIT_SECONDS     default: 12
  SENDKEY_SEQUENCE      default: esc,ret; comma-separated QEMU monitor sendkey list

Limitations: capture mode collects QEMU screendumps and serial logs, but it does not inspect pixels and cannot guarantee host-independent navigation into a given
FormBrowser page. Treat generated files/builds/captures as separate evidence
levels in docs and reports.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --help|-h)
      usage
      exit 0
      ;;
    --mode)
      if [[ $# -lt 2 ]]; then
        echo "Missing value for --mode" >&2
        exit 2
      fi
      MODE="$2"
      shift 2
      ;;
    --mode=*)
      MODE="${1#--mode=}"
      shift
      ;;
    *)
      echo "Unknown option '$1'" >&2
      usage >&2
      exit 2
      ;;
  esac
done

case "${MODE}" in
  dry-run|generate-only|build|capture) ;;
  *)
    echo "Unsupported MODE/--mode '${MODE}'; use dry-run, generate-only, build, or capture" >&2
    exit 2
    ;;
esac

if [[ -z "${CAPTURE_OUT_DIR}" ]]; then
  echo "CAPTURE_OUT_DIR must not be empty" >&2
  exit 2
fi

variants=(native modern)

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

copy_overlay_artifacts() {
  local variant="$1"
  local variant_overlay_dir="${CAPTURE_OUT_DIR}/overlays/${variant}"
  mkdir -p "${variant_overlay_dir}"
  cp "${WORKSPACE}/Build/ModernSetupPkgOverlay/OvmfX64ModernSetup.dsc" "${variant_overlay_dir}/OvmfX64ModernSetup.dsc"
  cp "${WORKSPACE}/Build/ModernSetupPkgOverlay/OvmfX64ModernSetup.fdf" "${variant_overlay_dir}/OvmfX64ModernSetup.fdf"
}

copy_firmware_artifacts() {
  local variant="$1"
  local code_fd vars_fd variant_firmware_dir
  code_fd="$(locate_build_fd OVMF_CODE.fd)"
  vars_fd="$(locate_build_fd OVMF_VARS.fd)"
  if [[ -z "${code_fd}" || -z "${vars_fd}" || ! -f "${code_fd}" || ! -f "${vars_fd}" ]]; then
    echo "Unable to locate built OVMF_CODE.fd/OVMF_VARS.fd for ${variant} after build" >&2
    exit 1
  fi
  variant_firmware_dir="${CAPTURE_OUT_DIR}/firmware/${variant}"
  mkdir -p "${variant_firmware_dir}"
  cp "${code_fd}" "${variant_firmware_dir}/OVMF_CODE.fd"
  cp "${vars_fd}" "${variant_firmware_dir}/OVMF_VARS.fd"
}

run_variant() {
  local variant="$1"
  local generate_only=1
  local variant_capture_dir

  case "${MODE}" in
    dry-run)
      echo "Would run: MODERN_SETUP_DISPLAY_ENGINE=${variant} GENERATE_ONLY=1 ${BUILD_SCRIPT}"
      echo "Would write overlay artifacts: ${CAPTURE_OUT_DIR}/overlays/${variant}/"
      if [[ "${variant}" == "modern" ]]; then
        echo "Modern variant keeps product path: EDKII_FORM_DISPLAY_ENGINE_PROTOCOL -> ModernDisplayEngineDxe -> ModernUiCustomizedDisplayLib -> private FormModel -> renderer"
      else
        echo "Native variant keeps upstream MdeModulePkg DisplayEngineDxe/CustomizedDisplayLib"
      fi
      return 0
      ;;
    generate-only)
      generate_only=1
      ;;
    build|capture)
      generate_only=0
      ;;
  esac

  MODERN_SETUP_DISPLAY_ENGINE="${variant}" GENERATE_ONLY="${generate_only}" "${BUILD_SCRIPT}"
  copy_overlay_artifacts "${variant}"

  if [[ "${MODE}" == "build" || "${MODE}" == "capture" ]]; then
    copy_firmware_artifacts "${variant}"
  fi

  if [[ "${MODE}" == "capture" ]]; then
    variant_capture_dir="${CAPTURE_OUT_DIR}/${variant}"
    mkdir -p "${variant_capture_dir}"
    OVMF_CODE="${CAPTURE_OUT_DIR}/firmware/${variant}/OVMF_CODE.fd" \
    OVMF_VARS="${CAPTURE_OUT_DIR}/firmware/${variant}/OVMF_VARS.fd" \
    BOOT_APP="${BOOT_APP}" \
    BOOT_WAIT_SECONDS="${BOOT_WAIT_SECONDS}" \
    SENDKEY_SEQUENCE="${SENDKEY_SEQUENCE}" \
    CAPTURE_OUT_DIR="${variant_capture_dir}" \
    CAPTURE_WORK_DIR="${CAPTURE_WORK_DIR}/${variant}" \
    CAPTURE_PREFIX="displayengine-ovmf-x64-${variant}" \
      "${CAPTURE_SCRIPT}"
  fi
}

mkdir -p "${CAPTURE_OUT_DIR}" "${CAPTURE_WORK_DIR}"

cat > "${CAPTURE_OUT_DIR}/README.txt" <<EOF_README
DisplayEngine OVMF X64 native-vs-modern evidence directory
Mode: ${MODE}
Workspace: ${WORKSPACE}
Target/toolchain: ${TARGET}_${TOOL_CHAIN_TAG}
Native selector: MODERN_SETUP_DISPLAY_ENGINE=native
Modern selector: MODERN_SETUP_DISPLAY_ENGINE=modern
Default output root: \${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64

Evidence levels:
- dry-run: no overlay, build, QEMU, or visual evidence.
- generate-only: overlay DSC/FDF snapshots only.
- build: overlay plus OVMF_CODE.fd/OVMF_VARS.fd snapshots.
- capture: build artifacts plus QEMU screendump files. This script does not
  inspect pixels and does not mark visual equivalence as verified.
EOF_README

for variant in "${variants[@]}"; do
  run_variant "${variant}"
done

echo "DisplayEngine OVMF X64 evidence root: ${CAPTURE_OUT_DIR}"
echo "Native artifacts: ${CAPTURE_OUT_DIR}/overlays/native"
echo "Modern artifacts: ${CAPTURE_OUT_DIR}/overlays/modern"
if [[ "${MODE}" == "capture" ]]; then
  echo "Native capture dir: ${CAPTURE_OUT_DIR}/native"
  echo "Modern capture dir: ${CAPTURE_OUT_DIR}/modern"
  echo "Note: captures are screendump artifacts only; no pixel inspection was performed."
fi
