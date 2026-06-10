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
  CHAR16    FormFactor[MODERN_UI_PLATFORM_TEXT_MAX];
  CHAR16    BootMode[MODERN_UI_PLATFORM_TEXT_MAX];
  UINT64    MemorySizeMb;
  //
  // Appended (additive): processor identity from SMBIOS Type 4, e.g.
  // "QEMU Virtual CPU version 2.5+ (4C/8T)". Empty/Unknown when Type 4 is absent.
  //
  CHAR16    Processor[MODERN_UI_PLATFORM_TEXT_MAX];
  //
  // Appended (additive): memory type/speed/DIMM-count detail from SMBIOS Type 17,
  // e.g. "DDR4-3200, 2 DIMMs". Empty when Type 17 is absent; the total size is in
  // MemorySizeMb above.
  //
  CHAR16    MemoryDetail[MODERN_UI_PLATFORM_TEXT_MAX];
  //
  // Appended (additive): deeper system-detail identity for the System
  // Information page. Each is empty when its SMBIOS source is absent.
  //   Serial       - system serial number (SMBIOS Type 1).
  //   Uuid         - system UUID, canonical string form (SMBIOS Type 1).
  //   Baseboard    - "<Manufacturer> <Product>" (SMBIOS Type 2).
  //   BiosVersion  - firmware version string (SMBIOS Type 0).
  //   BiosDate     - firmware release date (SMBIOS Type 0).
  //
  CHAR16    Serial[MODERN_UI_PLATFORM_TEXT_MAX];
  CHAR16    Uuid[MODERN_UI_PLATFORM_TEXT_MAX];
  CHAR16    Baseboard[MODERN_UI_PLATFORM_TEXT_MAX];
  CHAR16    BiosVersion[MODERN_UI_PLATFORM_TEXT_MAX];
  CHAR16    BiosDate[MODERN_UI_PLATFORM_TEXT_MAX];
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
