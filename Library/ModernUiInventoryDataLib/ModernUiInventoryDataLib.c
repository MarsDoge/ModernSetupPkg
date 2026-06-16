/** @file
  Read-only storage and network device inventory provider for ModernSetupApp.

  See Include/ModernUi/ModernUiInventoryData.h and Docs/ProviderDataContract.md.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskInfo.h>
#include <Protocol/SimpleNetwork.h>

#include <ModernUi/ModernUiInventoryData.h>

/**
  Classify a storage handle's bus type from its DiskInfo interface GUID.

  @param[in] Handle  Storage device handle. Must not be NULL.

  @return A MODERN_UI_STORAGE_BUS value; ModernUiStorageBusUnknown when DiskInfo
          is absent or its interface GUID is unrecognized.
**/
STATIC
UINT8
GetStorageBus (
  IN EFI_HANDLE  Handle
  )
{
  EFI_STATUS              Status;
  EFI_DISK_INFO_PROTOCOL  *DiskInfo;

  DiskInfo = NULL;
  Status   = gBS->HandleProtocol (Handle, &gEfiDiskInfoProtocolGuid, (VOID **)&DiskInfo);
  if (EFI_ERROR (Status) || (DiskInfo == NULL)) {
    return ModernUiStorageBusUnknown;
  }

  if (CompareGuid (&DiskInfo->Interface, &gEfiDiskInfoNvmeInterfaceGuid)) {
    return ModernUiStorageBusNvme;
  }

  if (CompareGuid (&DiskInfo->Interface, &gEfiDiskInfoAhciInterfaceGuid)) {
    return ModernUiStorageBusAhci;
  }

  if (CompareGuid (&DiskInfo->Interface, &gEfiDiskInfoIdeInterfaceGuid)) {
    return ModernUiStorageBusIde;
  }

  if (CompareGuid (&DiskInfo->Interface, &gEfiDiskInfoScsiInterfaceGuid)) {
    return ModernUiStorageBusScsi;
  }

  if (CompareGuid (&DiskInfo->Interface, &gEfiDiskInfoUsbInterfaceGuid)) {
    return ModernUiStorageBusUsb;
  }

  if (CompareGuid (&DiskInfo->Interface, &gEfiDiskInfoSdMmcInterfaceGuid)) {
    return ModernUiStorageBusSdMmc;
  }

  if (CompareGuid (&DiskInfo->Interface, &gEfiDiskInfoUfsInterfaceGuid)) {
    return ModernUiStorageBusUfs;
  }

  return ModernUiStorageBusUnknown;
}

/**
  Return a short display label for a storage bus type.

  @param[in] Bus  MODERN_UI_STORAGE_BUS value.

  @return Non-NULL static short label, e.g. L"NVMe".
**/
STATIC
CONST CHAR16 *
StorageBusName (
  IN UINT8  Bus
  )
{
  switch (Bus) {
    case ModernUiStorageBusNvme:
      return L"NVMe";
    case ModernUiStorageBusAhci:
      return L"SATA";
    case ModernUiStorageBusIde:
      return L"IDE";
    case ModernUiStorageBusScsi:
      return L"SCSI";
    case ModernUiStorageBusUsb:
      return L"USB";
    case ModernUiStorageBusSdMmc:
      return L"SD/MMC";
    case ModernUiStorageBusUfs:
      return L"UFS";
    default:
      return L"Disk";
  }
}

/**
  Format a MiB capacity into a short human string (GB when >= 1 GiB).

  @param[out] Buffer  Destination. Must not be NULL.
  @param[in]  Count   CHAR16 entries in Buffer.
  @param[in]  SizeMb  Capacity in MiB; 0 renders nothing usable.
**/
STATIC
VOID
FormatCapacity (
  OUT CHAR16  *Buffer,
  IN  UINTN   Count,
  IN  UINT64  SizeMb
  )
{
  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  if (SizeMb >= 1024) {
    //
    // Round to the nearest GiB for a compact summary.
    //
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%lu GB", (SizeMb + 512) / 1024);
  } else {
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%lu MB", SizeMb);
  }
}

/**
  Enumerate physical block devices into the inventory.

  @param[in,out] Summary  Inventory to append storage entries to. Must not be NULL.
**/
STATIC
VOID
CollectStorage (
  IN OUT MODERN_UI_INVENTORY_SUMMARY  *Summary
  )
{
  EFI_STATUS             Status;
  EFI_HANDLE             *Handles;
  UINTN                  HandleCount;
  UINTN                  Index;
  EFI_BLOCK_IO_PROTOCOL  *BlockIo;
  MODERN_UI_INV_STORAGE  *Entry;
  UINT64                 SizeMb;
  CHAR16                 SizeText[24];

  Handles     = NULL;
  HandleCount = 0;
  Status      = gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &HandleCount, &Handles);
  if (EFI_ERROR (Status) || (Handles == NULL)) {
    return;
  }

  for (Index = 0; (Index < HandleCount) && (Summary->StorageCount < MODERN_UI_INV_MAX_STORAGE); Index++) {
    BlockIo = NULL;
    Status  = gBS->HandleProtocol (Handles[Index], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
    if (EFI_ERROR (Status) || (BlockIo == NULL) || (BlockIo->Media == NULL)) {
      continue;
    }

    //
    // Count whole physical media only; logical partitions ride the same disk.
    //
    if (BlockIo->Media->LogicalPartition) {
      continue;
    }

    SizeMb = MultU64x32 (BlockIo->Media->LastBlock + 1, BlockIo->Media->BlockSize) / (1024 * 1024);

    Entry = &Summary->Storage[Summary->StorageCount];
    ZeroMem (Entry, sizeof (*Entry));
    Entry->Bus       = GetStorageBus (Handles[Index]);
    Entry->Removable = BlockIo->Media->RemovableMedia;
    Entry->SizeMb    = SizeMb;

    SizeText[0] = L'\0';
    FormatCapacity (SizeText, ARRAY_SIZE (SizeText), SizeMb);
    UnicodeSPrint (Entry->Label, sizeof (Entry->Label), L"%s %s", StorageBusName (Entry->Bus), SizeText);

    Summary->StorageCount++;
  }

  FreePool (Handles);
}

/**
  Enumerate simple-network controllers into the inventory.

  @param[in,out] Summary  Inventory to append NIC entries to. Must not be NULL.
**/
STATIC
VOID
CollectNics (
  IN OUT MODERN_UI_INVENTORY_SUMMARY  *Summary
  )
{
  EFI_STATUS                   Status;
  EFI_HANDLE                   *Handles;
  UINTN                        HandleCount;
  UINTN                        Index;
  EFI_SIMPLE_NETWORK_PROTOCOL  *Snp;
  MODERN_UI_INV_NIC            *Entry;

  Handles     = NULL;
  HandleCount = 0;
  Status      = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleNetworkProtocolGuid, NULL, &HandleCount, &Handles);
  if (EFI_ERROR (Status) || (Handles == NULL)) {
    return;
  }

  for (Index = 0; (Index < HandleCount) && (Summary->NicCount < MODERN_UI_INV_MAX_NIC); Index++) {
    Snp    = NULL;
    Status = gBS->HandleProtocol (Handles[Index], &gEfiSimpleNetworkProtocolGuid, (VOID **)&Snp);
    if (EFI_ERROR (Status) || (Snp == NULL) || (Snp->Mode == NULL)) {
      continue;
    }

    Entry = &Summary->Nic[Summary->NicCount];
    ZeroMem (Entry, sizeof (*Entry));
    CopyMem (Entry->Mac, &Snp->Mode->CurrentAddress, sizeof (Entry->Mac));
    Entry->MediaPresent = (BOOLEAN)(Snp->Mode->MediaPresentSupported && Snp->Mode->MediaPresent);

    UnicodeSPrint (
      Entry->Label,
      sizeof (Entry->Label),
      L"%02x:%02x:%02x:%02x:%02x:%02x %s",
      Entry->Mac[0],
      Entry->Mac[1],
      Entry->Mac[2],
      Entry->Mac[3],
      Entry->Mac[4],
      Entry->Mac[5],
      Snp->Mode->MediaPresentSupported ? (Entry->MediaPresent ? L"up" : L"down") : L""
      );

    Summary->NicCount++;
  }

  FreePool (Handles);
}

/**
  Collect a read-only storage and network device inventory.

  @param[out] Summary  Inventory summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled (possibly with zero devices).
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiInventoryDataGetSummary (
  OUT MODERN_UI_INVENTORY_SUMMARY  *Summary
  )
{
  if (Summary == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Summary, sizeof (*Summary));
  CollectStorage (Summary);
  CollectNics (Summary);
  return EFI_SUCCESS;
}
