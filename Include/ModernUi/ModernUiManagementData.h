/** @file
  Server and remote management summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_MANAGEMENT_DATA_H_
#define MODERN_UI_MANAGEMENT_DATA_H_

#include <Uefi.h>

typedef struct {
  BOOLEAN    IpmiProtocolPresent;
  BOOLEAN    RedfishDiscoverPresent;
  BOOLEAN    SmbiosManagementInterfacePresent;
} MODERN_UI_MANAGEMENT_SUMMARY;

/**
  Collect read-only management capability state.

  @param[out] Summary  Management summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiManagementDataGetSummary (
  OUT MODERN_UI_MANAGEMENT_SUMMARY  *Summary
  );

#endif
