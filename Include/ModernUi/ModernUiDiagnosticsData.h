/** @file
  Diagnostics and platform table summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_DIAGNOSTICS_DATA_H_
#define MODERN_UI_DIAGNOSTICS_DATA_H_

#include <Uefi.h>

typedef struct {
  BOOLEAN    AcpiPresent;
  BOOLEAN    SmbiosPresent;
  UINTN      MemoryDescriptorCount;
  UINTN      HandleCount;
  UINTN      ConfigurationTableCount;
} MODERN_UI_DIAGNOSTICS_SUMMARY;

/**
  Collect read-only diagnostics and bring-up summary data.

  @param[out] Summary  Diagnostics summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
  @retval EFI_OUT_OF_RESOURCES   Temporary allocation failed.
  @retval others                 Status returned by GetMemoryMap() or
                                 LocateHandleBuffer().
**/
EFI_STATUS
EFIAPI
ModernUiDiagnosticsDataGetSummary (
  OUT MODERN_UI_DIAGNOSTICS_SUMMARY  *Summary
  );

#endif
