/** @file
  Performance and tuning summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_PERFORMANCE_DATA_H_
#define MODERN_UI_PERFORMANCE_DATA_H_

#include <Uefi.h>

typedef struct {
  BOOLEAN    ProcessorInventoryPresent;
  BOOLEAN    MemoryInventoryPresent;
  BOOLEAN    CpuIo2ProtocolPresent;
  BOOLEAN    VirtualizationPolicyEntryPresent;
  BOOLEAN    RasPolicyEntryPresent;
} MODERN_UI_PERFORMANCE_SUMMARY;

/**
  Collect read-only performance and tuning capability state.

  @param[out] Summary  Performance summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiPerformanceDataGetSummary (
  OUT MODERN_UI_PERFORMANCE_SUMMARY  *Summary
  );

#endif
