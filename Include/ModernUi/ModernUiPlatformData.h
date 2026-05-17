/** @file
  Platform summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_PLATFORM_DATA_H_
#define MODERN_UI_PLATFORM_DATA_H_

#include <Uefi.h>

#define MODERN_UI_PLATFORM_TEXT_MAX  96

typedef struct {
  CHAR16    FirmwareVendor[MODERN_UI_PLATFORM_TEXT_MAX];
  CHAR16    FirmwareRevision[MODERN_UI_PLATFORM_TEXT_MAX];
  CHAR16    Architecture[MODERN_UI_PLATFORM_TEXT_MAX];
  CHAR16    Platform[MODERN_UI_PLATFORM_TEXT_MAX];
  UINT64    MemorySizeMb;
} MODERN_UI_PLATFORM_SUMMARY;

/**
  Collect a read-only platform summary for the setup front page.

  @param[out] Summary  Platform summary to fill. Must not be NULL. All string
                       fields are always NUL-terminated on success.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
  @retval others                 Memory map collection failed.
**/
EFI_STATUS
EFIAPI
ModernUiPlatformDataGetSummary (
  OUT MODERN_UI_PLATFORM_SUMMARY  *Summary
  );

#endif
