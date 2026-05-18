/** @file
  Firmware lifecycle summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_FIRMWARE_DATA_H_
#define MODERN_UI_FIRMWARE_DATA_H_

#include <Uefi.h>

#define MODERN_UI_FIRMWARE_TEXT_MAX  96

typedef struct {
  CHAR16     Vendor[MODERN_UI_FIRMWARE_TEXT_MAX];
  CHAR16     Revision[MODERN_UI_FIRMWARE_TEXT_MAX];
  BOOLEAN    CapsuleRuntimeServices;
  BOOLEAN    CapsuleArchProtocol;
  BOOLEAN    CapsuleReportPresent;
} MODERN_UI_FIRMWARE_SUMMARY;

/**
  Collect read-only firmware lifecycle capability state.

  @param[out] Summary  Firmware summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiFirmwareDataGetSummary (
  OUT MODERN_UI_FIRMWARE_SUMMARY  *Summary
  );

#endif
