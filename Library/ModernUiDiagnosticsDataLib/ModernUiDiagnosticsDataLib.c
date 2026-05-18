/** @file
  Diagnostics and platform table summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/Acpi.h>
#include <Guid/SmBios.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <ModernUi/ModernUiDiagnosticsData.h>

/**
  Return whether a system configuration table is installed.

  @param[in] TableGuid  Configuration table GUID. Must not be NULL.

  @retval TRUE   The configuration table is present.
  @retval FALSE  The configuration table is absent or TableGuid is NULL.
**/
STATIC
BOOLEAN
IsConfigurationTablePresent (
  IN CONST EFI_GUID  *TableGuid
  )
{
  UINTN  Index;

  if (TableGuid == NULL) {
    return FALSE;
  }

  for (Index = 0; Index < gST->NumberOfTableEntries; Index++) {
    if (CompareGuid (&gST->ConfigurationTable[Index].VendorGuid, TableGuid)) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Count descriptors in the current UEFI memory map.

  @param[out] DescriptorCount  Receives the descriptor count. Must not be NULL.

  @retval EFI_SUCCESS            Descriptor count was computed.
  @retval EFI_INVALID_PARAMETER  DescriptorCount is NULL.
  @retval EFI_OUT_OF_RESOURCES   Memory map allocation failed.
  @retval others                 Status returned by GetMemoryMap().
**/
STATIC
EFI_STATUS
GetMemoryDescriptorCount (
  OUT UINTN  *DescriptorCount
  )
{
  EFI_STATUS             Status;
  EFI_MEMORY_DESCRIPTOR  *MemoryMap;
  UINTN                  MemoryMapSize;
  UINTN                  MapKey;
  UINTN                  DescriptorSize;
  UINT32                 DescriptorVersion;

  if (DescriptorCount == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *DescriptorCount  = 0;
  MemoryMap         = NULL;
  MemoryMapSize     = 0;
  MapKey            = 0;
  DescriptorSize    = 0;
  DescriptorVersion = 0;

  Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  MemoryMapSize += DescriptorSize * 8;
  MemoryMap = AllocateZeroPool (MemoryMapSize);
  if (MemoryMap == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
  if (!EFI_ERROR (Status) && (DescriptorSize != 0)) {
    *DescriptorCount = MemoryMapSize / DescriptorSize;
  }

  FreePool (MemoryMap);
  return Status;
}

/**
  Count all handles currently visible to boot services.

  @param[out] HandleCount  Receives handle count. Must not be NULL.

  @retval EFI_SUCCESS            Handle count was computed.
  @retval EFI_INVALID_PARAMETER  HandleCount is NULL.
  @retval EFI_OUT_OF_RESOURCES   Handle buffer allocation failed.
  @retval others                 Status returned by LocateHandleBuffer().
**/
STATIC
EFI_STATUS
GetHandleCount (
  OUT UINTN  *HandleCount
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *Handles;

  if (HandleCount == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *HandleCount = 0;
  Handles      = NULL;
  Status = gBS->LocateHandleBuffer (
                  AllHandles,
                  NULL,
                  NULL,
                  HandleCount,
                  &Handles
                  );
  if (!EFI_ERROR (Status) && (Handles != NULL)) {
    FreePool (Handles);
  }

  return Status;
}

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
  )
{
  EFI_STATUS  Status;
  EFI_STATUS  MemoryStatus;
  EFI_STATUS  HandleStatus;

  if (Summary == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Summary, sizeof (*Summary));
  Summary->AcpiPresent             = IsConfigurationTablePresent (&gEfiAcpi20TableGuid);
  Summary->SmbiosPresent           = (BOOLEAN)(IsConfigurationTablePresent (&gEfiSmbios3TableGuid) || IsConfigurationTablePresent (&gEfiSmbiosTableGuid));
  Summary->ConfigurationTableCount = gST->NumberOfTableEntries;

  MemoryStatus = GetMemoryDescriptorCount (&Summary->MemoryDescriptorCount);
  HandleStatus = GetHandleCount (&Summary->HandleCount);

  Status = EFI_SUCCESS;
  if (EFI_ERROR (MemoryStatus)) {
    Status = MemoryStatus;
  } else if (EFI_ERROR (HandleStatus)) {
    Status = HandleStatus;
  }

  return Status;
}
