/** @file
  Power and thermal summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_POWER_DATA_H_
#define MODERN_UI_POWER_DATA_H_

#include <Uefi.h>

#define MODERN_UI_POWER_TEXT_MAX  96

typedef struct {
  BOOLEAN    AcpiTablePresent;
  BOOLEAN    AcpiSdtProtocolPresent;
  BOOLEAN    SmbiosChassisPresent;
  BOOLEAN    SmbiosPowerSupplyPresent;
  CHAR16     ChassisThermalState[MODERN_UI_POWER_TEXT_MAX];
} MODERN_UI_POWER_SUMMARY;

/**
  Collect read-only power and thermal capability state.

  @param[out] Summary  Power summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiPowerDataGetSummary (
  OUT MODERN_UI_POWER_SUMMARY  *Summary
  );

#endif
