/** @file
  Read-only storage and network device inventory provider for ModernSetupApp.

  Typed read-only summaries sourced from standard UEFI device protocols
  (EFI_BLOCK_IO_PROTOCOL + EFI_DISK_INFO_PROTOCOL for storage,
  EFI_SIMPLE_NETWORK_PROTOCOL for NICs). Inventory/identity only -- no policy,
  no configuration. See Docs/ProviderDataContract.md.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_INVENTORY_DATA_H_
#define MODERN_UI_INVENTORY_DATA_H_

#include <Uefi.h>

#define MODERN_UI_INV_MAX_STORAGE   12
#define MODERN_UI_INV_MAX_NIC        8
#define MODERN_UI_INV_LABEL_MAX      64

//
// Storage bus/interface classification, derived from the DiskInfo interface
// GUID when present.
//
typedef enum {
  ModernUiStorageBusUnknown = 0,
  ModernUiStorageBusNvme,
  ModernUiStorageBusAhci,
  ModernUiStorageBusIde,
  ModernUiStorageBusScsi,
  ModernUiStorageBusUsb,
  ModernUiStorageBusSdMmc,
  ModernUiStorageBusUfs,
  ModernUiStorageBusVirtio
} MODERN_UI_STORAGE_BUS;

typedef struct {
  UINT8     Bus;                                 // MODERN_UI_STORAGE_BUS value.
  BOOLEAN   Removable;                           // BlockIo Media->RemovableMedia.
  UINT64    SizeMb;                              // Capacity in MiB; 0 = unknown.
  CHAR16    Label[MODERN_UI_INV_LABEL_MAX];      // "<bus> <size>" e.g. "NVMe 32 GB".
} MODERN_UI_INV_STORAGE;

typedef struct {
  UINT8     Mac[6];                              // First 6 bytes of the MAC.
  BOOLEAN   MediaPresent;                        // SNP Mode->MediaPresent (link).
  CHAR16    Label[MODERN_UI_INV_LABEL_MAX];      // "aa:bb:cc:dd:ee:ff up".
} MODERN_UI_INV_NIC;

typedef struct {
  UINTN                    StorageCount;         // Valid entries in Storage[] (physical media only).
  MODERN_UI_INV_STORAGE    Storage[MODERN_UI_INV_MAX_STORAGE];
  UINTN                    NicCount;             // Valid entries in Nic[].
  MODERN_UI_INV_NIC        Nic[MODERN_UI_INV_MAX_NIC];
} MODERN_UI_INVENTORY_SUMMARY;

/**
  Collect a read-only storage and network device inventory.

  Storage is enumerated from EFI_BLOCK_IO_PROTOCOL handles (physical media only;
  logical partitions are skipped to avoid double-counting), with the bus type
  taken from EFI_DISK_INFO_PROTOCOL when available and the capacity from the
  BlockIo media descriptor. NICs are enumerated from EFI_SIMPLE_NETWORK_PROTOCOL
  handles (MAC + link state). Each list is capped; counts may saturate at the
  cap on systems with more devices.

  @param[out] Summary  Inventory summary to fill. Must not be NULL. Zeroed on
                       entry; partial results are retained on per-device errors.

  @retval EFI_SUCCESS            Summary was filled (possibly with zero devices).
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiInventoryDataGetSummary (
  OUT MODERN_UI_INVENTORY_SUMMARY  *Summary
  );

#endif
