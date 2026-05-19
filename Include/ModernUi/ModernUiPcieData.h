/** @file
  Read-only PCIe policy and capability summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_PCIE_DATA_H_
#define MODERN_UI_PCIE_DATA_H_

#include <Uefi.h>

typedef struct {
  UINTN      ControllerCount;
  UINTN      RootBridgeCount;
  BOOLEAN    RootBridgeIoPresent;
  UINTN      EndpointCount;
  UINTN      BridgeCount;
  BOOLEAN    EnumerationCompleteProtocolPresent;
  BOOLEAN    HotPlugRequestProtocolPresent;
  BOOLEAN    HotPlugInitProtocolPresent;
  BOOLEAN    IoMmuProtocolPresent;
  BOOLEAN    PciePolicyEntryPresent;
  BOOLEAN    ResizeBarPolicyEntryPresent;
  BOOLEAN    Above4GPolicyEntryPresent;
  BOOLEAN    SriovPolicyEntryPresent;
  BOOLEAN    AspmPolicyEntryPresent;
  BOOLEAN    BifurcationPolicyEntryPresent;
  BOOLEAN    HotPlugPolicyEntryPresent;
  BOOLEAN    AcsPolicyEntryPresent;
  BOOLEAN    AriPolicyEntryPresent;
  BOOLEAN    IommuPolicyEntryPresent;
  UINTN      ResizableBarDeviceCount;
  UINTN      SriovDeviceCount;
  UINTN      AcsDeviceCount;
  UINTN      AriDeviceCount;
  UINTN      HotPlugPortCount;
  UINTN      AspmCapableLinkCount;
} MODERN_UI_PCIE_SUMMARY;

/**
  Collect read-only PCIe inventory, protocol presence, and policy-entry hints.

  @param[out] Summary  PCIe summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiPcieDataGetSummary (
  OUT MODERN_UI_PCIE_SUMMARY  *Summary
  );

#endif
