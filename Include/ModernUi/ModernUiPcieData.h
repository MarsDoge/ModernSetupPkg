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

#define MODERN_UI_PCIE_MAX_DEVICES       16
#define MODERN_UI_PCIE_DEVICE_LABEL_MAX  48

//
// Read-only per-device PCIe identity captured from EFI_PCI_IO_PROTOCOL. This is
// inventory/telemetry only -- no policy fields (ReBAR/Above-4G/SR-IOV/ASPM/etc.
// stay owned by platform HII).
//
typedef struct {
  UINT16    Segment;
  UINT8     Bus;
  UINT8     Device;
  UINT8     Function;
  UINT16    VendorId;
  UINT16    DeviceId;
  UINT8     BaseClass;
  UINT8     SubClass;
  UINT8     LinkSpeed;                                  // PCIe current link-speed encoding (1..7 => Gen1..Gen7); 0 = unknown/non-PCIe.
  UINT8     LinkWidth;                                  // Negotiated lane count; 0 = unknown.
  CHAR16    Label[MODERN_UI_PCIE_DEVICE_LABEL_MAX];     // "BB:DD.F vvvv:dddd <class> GenN xN".
} MODERN_UI_PCIE_DEVICE;

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
  //
  // Appended (additive): read-only per-device identity captured from PciIo.
  // DeviceCount is the number of valid entries in Devices (0..MODERN_UI_PCIE_MAX_DEVICES);
  // it may be fewer than ControllerCount when more than the cap are present.
  //
  UINTN                    DeviceCount;
  MODERN_UI_PCIE_DEVICE    Devices[MODERN_UI_PCIE_MAX_DEVICES];
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
