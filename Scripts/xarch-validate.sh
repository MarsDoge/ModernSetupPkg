#!/usr/bin/env bash
# Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
# Author: MarsDoge (Dongyan Qian)
# Open source: https://github.com/MarsDoge/ModernSetupPkg
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
set -euo pipefail

PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_FILTER="all"
MODE="dry-run"
FORMAT="table"

usage() {
  cat <<'USAGE'
Usage: Scripts/xarch-validate.sh [--target x64|aarch64|loongarch64|riscv64|all] [--mode dry-run] [--format table|markdown|json]
       Scripts/xarch-validate.sh --all

Run the lightweight XArch validation runner. The Phase20 runner is dry-run only:
it validates target metadata and checks that expected scripts/docs exist. It does
not invoke edk2 builds, QEMU, capture helpers, or firmware setup writes.

Options:
  --help                 Show this help text.
  --target TARGET        Select x64, aarch64, loongarch64, riscv64, or all.
                         Default: all.
  --all                  Alias for --target all.
  --mode dry-run         Dry-run mode only for this phase. Default: dry-run.
  --format FORMAT        table, markdown, or json. Default: table.
USAGE
}

normalize_target() {
  case "$1" in
    x64|X64) echo "x64" ;;
    aarch64|AARCH64|arm64|ARM64) echo "aarch64" ;;
    loongarch64|LOONGARCH64|la64|LA64) echo "loongarch64" ;;
    riscv64|RISCV64|riscv|RISCV) echo "riscv64" ;;
    all|ALL) echo "all" ;;
    *)
      echo "Unsupported --target '$1'; use x64, aarch64, loongarch64, riscv64, or all" >&2
      exit 2
      ;;
  esac
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --help|-h)
      usage
      exit 0
      ;;
    --target)
      if [[ $# -lt 2 ]]; then
        echo "Missing value for --target" >&2
        exit 2
      fi
      TARGET_FILTER="$(normalize_target "$2")"
      shift 2
      ;;
    --target=*)
      TARGET_FILTER="$(normalize_target "${1#--target=}")"
      shift
      ;;
    --all)
      TARGET_FILTER="all"
      shift
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
    --format)
      if [[ $# -lt 2 ]]; then
        echo "Missing value for --format" >&2
        exit 2
      fi
      FORMAT="$2"
      shift 2
      ;;
    --format=*)
      FORMAT="${1#--format=}"
      shift
      ;;
    *)
      echo "Unknown option '$1'" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ "${MODE}" != "dry-run" ]]; then
  echo "Unsupported --mode '${MODE}'; Phase20 supports dry-run only" >&2
  exit 2
fi

case "${FORMAT}" in
  table|markdown|json) ;;
  *)
    echo "Unsupported --format '${FORMAT}'; use table, markdown, or json" >&2
    exit 2
    ;;
esac

TARGET_KEYS=(x64 aarch64 loongarch64 riscv64)

label_for() {
  case "$1" in
    x64) echo "X64 / OVMF X64" ;;
    aarch64) echo "AARCH64 / ArmVirtQemu" ;;
    loongarch64) echo "LOONGARCH64 / LoongArchVirtQemu" ;;
    riscv64) echo "RISCV64 / RiscVVirtQemu" ;;
  esac
}

arch_for() {
  case "$1" in
    x64) echo "X64" ;;
    aarch64) echo "AARCH64" ;;
    loongarch64) echo "LOONGARCH64" ;;
    riscv64) echo "RISCV64" ;;
  esac
}

platform_for() {
  case "$1" in
    x64) echo "OvmfPkg/OvmfPkgX64" ;;
    aarch64) echo "ArmVirtPkg/ArmVirtQemu" ;;
    loongarch64) echo "OvmfPkg/LoongArchVirt/LoongArchVirtQemu" ;;
    riscv64) echo "OvmfPkg/RiscVVirt/RiscVVirtQemu" ;;
  esac
}

level_for() {
  case "$1" in
    x64) echo "Manual/captured local path" ;;
    aarch64) echo "Captured/active compatibility path" ;;
    loongarch64) echo "Active build/run path" ;;
    riscv64) echo "Build/script validation" ;;
  esac
}

checks_for() {
  case "$1" in
    x64) echo "Scripts/build-ovmf-x64.sh, Scripts/run-ovmf-x64.sh, Scripts/capture-ovmf-x64.sh, Tests/Manual/OvmfX64Qemu.md, Docs/XArch.md" ;;
    aarch64) echo "Scripts/build-armvirt.sh, Scripts/run-armvirt.sh, Scripts/capture-armvirt.sh, Tests/Manual/ArmVirtQemu.md, Docs/XArch.md" ;;
    loongarch64) echo "Scripts/build-loongarchvirt.sh, Scripts/run-loongarchvirt.sh, Tests/Manual/LoongArchVirtQemu.md, Docs/XArch.md" ;;
    riscv64) echo "Scripts/build-riscvvirt.sh, Tests/Manual/RiscVVirtQemu.md, Docs/XArch.md" ;;
  esac
}

intended_for() {
  case "$1" in
    x64) echo "build-ovmf-x64/run-ovmf-x64/capture-ovmf-x64 when full validation is requested" ;;
    aarch64) echo "build-armvirt/run-armvirt/capture-armvirt when full validation is requested" ;;
    loongarch64) echo "build-loongarchvirt/run-loongarchvirt when full validation is requested" ;;
    riscv64) echo "build-riscvvirt overlay/build validation when full validation is requested" ;;
  esac
}

json_escape() {
  local s="$1"
  s="${s//\\/\\\\}"
  s="${s//\"/\\\"}"
  s="${s//$'\n'/\\n}"
  echo "${s}"
}

RESULT_KEYS=()
RESULT_LABELS=()
RESULT_ARCHES=()
RESULT_PLATFORMS=()
RESULT_LEVELS=()
RESULT_CHECKS=()
RESULT_RESULTS=()
RESULT_NOTES=()
EXIT_STATUS=0

for key in "${TARGET_KEYS[@]}"; do
  if [[ "${TARGET_FILTER}" != "all" && "${TARGET_FILTER}" != "${key}" ]]; then
    continue
  fi

  checked="$(checks_for "${key}")"
  IFS=',' read -ra check_items <<< "${checked}"
  missing=()
  for item in "${check_items[@]}"; do
    item="${item# }"
    if [[ ! -e "${PKG_DIR}/${item}" ]]; then
      missing+=("${item}")
    fi
  done

  result="PASS"
  note="dry-run metadata/files present; intended scripts: $(intended_for "${key}")"
  if [[ ${#missing[@]} -gt 0 ]]; then
    result="FAIL"
    note="missing: ${missing[*]}"
    EXIT_STATUS=1
  fi

  RESULT_KEYS+=("${key}")
  RESULT_LABELS+=("$(label_for "${key}")")
  RESULT_ARCHES+=("$(arch_for "${key}")")
  RESULT_PLATFORMS+=("$(platform_for "${key}")")
  RESULT_LEVELS+=("$(level_for "${key}")")
  RESULT_CHECKS+=("${checked}")
  RESULT_RESULTS+=("${result}")
  RESULT_NOTES+=("${note}")
done

if [[ ${#RESULT_KEYS[@]} -eq 0 ]]; then
  echo "No targets selected" >&2
  exit 2
fi

case "${FORMAT}" in
  table)
    echo "XArch validation report (mode: ${MODE})"
    printf '%-30s | %-11s | %-46s | %-34s | %-6s | %s\n' "Target" "edk2 ARCH" "Platform" "Validation level" "Result" "Checked scripts/docs"
    printf '%-30s-+-%-11s-+-%-46s-+-%-34s-+-%-6s-+-%s\n' "------------------------------" "-----------" "----------------------------------------------" "----------------------------------" "------" "--------------------"
    for i in "${!RESULT_KEYS[@]}"; do
      printf '%-30s | %-11s | %-46s | %-34s | %-6s | %s\n' \
        "${RESULT_LABELS[$i]}" "${RESULT_ARCHES[$i]}" "${RESULT_PLATFORMS[$i]}" \
        "${RESULT_LEVELS[$i]}" "${RESULT_RESULTS[$i]}" "${RESULT_CHECKS[$i]}"
      printf '  note: %s\n' "${RESULT_NOTES[$i]}"
    done
    ;;
  markdown)
    echo "# XArch validation report"
    echo
    echo "Mode: ${MODE}"
    echo
    echo "| Target | edk2 ARCH | Platform | Validation level | Checked scripts/docs | Result |"
    echo "| --- | --- | --- | --- | --- | --- |"
    for i in "${!RESULT_KEYS[@]}"; do
      echo "| ${RESULT_LABELS[$i]} | ${RESULT_ARCHES[$i]} | ${RESULT_PLATFORMS[$i]} | ${RESULT_LEVELS[$i]} | ${RESULT_CHECKS[$i]} | ${RESULT_RESULTS[$i]} |"
    done
    ;;
  json)
    echo "{"
    echo "  \"mode\": \"${MODE}\","
    echo "  \"targets\": ["
    for i in "${!RESULT_KEYS[@]}"; do
      comma=","
      if [[ $i -eq $((${#RESULT_KEYS[@]} - 1)) ]]; then
        comma=""
      fi
      echo "    {\"target\": \"$(json_escape "${RESULT_LABELS[$i]}")\", \"edk2_arch\": \"${RESULT_ARCHES[$i]}\", \"platform\": \"$(json_escape "${RESULT_PLATFORMS[$i]}")\", \"validation_level\": \"$(json_escape "${RESULT_LEVELS[$i]}")\", \"checked\": \"$(json_escape "${RESULT_CHECKS[$i]}")\", \"result\": \"${RESULT_RESULTS[$i]}\", \"note\": \"$(json_escape "${RESULT_NOTES[$i]}")\"}${comma}"
    done
    echo "  ]"
    echo "}"
    ;;
esac

exit "${EXIT_STATUS}"
