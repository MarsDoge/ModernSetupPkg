#!/usr/bin/env bash
# Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
# Author: MarsDoge (Dongyan Qian)
# Open source: https://github.com/MarsDoge/ModernSetupPkg
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
set -euo pipefail

PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Capture the replace-UiApp product flow:
#   ModernSetupApp shell -> Devices tab -> selected HII/FormBrowser entry.
# The final screen should be a FormBrowser page rendered by ModernDisplayEngineDxe,
# not the native UiApp entry point and not only the ModernSetupApp shell.
export TARGET="${TARGET:-RELEASE}"
export MODERN_SETUP_DISPLAY_ENGINE="${MODERN_SETUP_DISPLAY_ENGINE:-modern}"
export MODERN_SETUP_REPLACE_UIAPP="${MODERN_SETUP_REPLACE_UIAPP:-1}"
export BOOT_APP="${BOOT_APP:-0}"
export BOOT_WAIT_SECONDS="${BOOT_WAIT_SECONDS:-8}"
export POST_SENDKEY_WAIT_SECONDS="${POST_SENDKEY_WAIT_SECONDS:-3}"
if [[ "${SENDKEY_SEQUENCE+x}" != "x" ]]; then
  export SENDKEY_SEQUENCE="enter,wait:3,ret@1000,wait:8,down,right,ret,wait:2,down,down,down,down,ret,wait:6"
elif [[ "${SENDKEY_SEQUENCE}" == "none" ]]; then
  export SENDKEY_SEQUENCE=""
fi

"${PKG_DIR}/Scripts/capture-ovmf-x64.sh" "$@"
