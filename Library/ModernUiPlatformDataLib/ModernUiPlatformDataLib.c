/** @file
  Platform summary provider for ModernSetupApp.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <ModernUi/ModernUiPlatformData.h>

/**
  Return the compile-time CPU architecture string.

  @return Non-NULL UCS-2 architecture name.
**/
STATIC
CONST CHAR16 *
GetArchitectureName (
  VOID
  )
{
#if defined (MDE_CPU_AARCH64)
  return L"AARCH64";
#elif defined (MDE_CPU_ARM)
  return L"ARM";
#elif defined (MDE_CPU_X64)
  return L"X64";
#elif defined (MDE_CPU_IA32)
  return L"IA32";
#elif defined (MDE_CPU_LOONGARCH64)
  return L"LOONGARCH64";
#elif defined (MDE_CPU_RISCV64)
  return L"RISCV64";
#else
  return L"UNKNOWN";
#endif
}

/**
  Sum a coarse memory size from the current UEFI memory map.

  @param[out] MemorySizeMb  Receives memory size in MiB. Must not be NULL.

  @retval EFI_SUCCESS            Memory size was computed.
  @retval EFI_INVALID_PARAMETER  MemorySizeMb is NULL.
  @retval EFI_OUT_OF_RESOURCES   Allocation failed.
  @retval others                 Status returned by GetMemoryMap().
**/
STATIC
EFI_STATUS
GetMemorySizeMb (
  OUT UINT64  *MemorySizeMb
  )
{
  EFI_STATUS                 Status;
  EFI_MEMORY_DESCRIPTOR      *MemoryMap;
  EFI_MEMORY_DESCRIPTOR      *Descriptor;
  UINTN                      MemoryMapSize;
  UINTN                      MapKey;
  UINTN                      DescriptorSize;
  UINT32                     DescriptorVersion;
  UINTN                      Index;
  UINTN                      DescriptorCount;
  UINT64                     Pages;

  if (MemorySizeMb == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *MemorySizeMb     = 0;
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
  if (EFI_ERROR (Status)) {
    FreePool (MemoryMap);
    return Status;
  }

  Pages           = 0;
  DescriptorCount = MemoryMapSize / DescriptorSize;
  for (Index = 0; Index < DescriptorCount; Index++) {
    Descriptor = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemoryMap + (Index * DescriptorSize));
    Pages += Descriptor->NumberOfPages;
  }

  *MemorySizeMb = EFI_PAGES_TO_SIZE (Pages) / (1024 * 1024);
  FreePool (MemoryMap);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ModernUiPlatformDataGetSummary (
  OUT MODERN_UI_PLATFORM_SUMMARY  *Summary
  )
{
  EFI_STATUS  Status;

  if (Summary == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Summary, sizeof (*Summary));
  UnicodeSPrint (
    Summary->FirmwareVendor,
    sizeof (Summary->FirmwareVendor),
    L"%s",
    (gST->FirmwareVendor == NULL) ? L"Unknown" : gST->FirmwareVendor
    );
  UnicodeSPrint (Summary->FirmwareRevision, sizeof (Summary->FirmwareRevision), L"0x%08x", gST->FirmwareRevision);
  UnicodeSPrint (Summary->Architecture, sizeof (Summary->Architecture), L"%s", GetArchitectureName ());
  UnicodeSPrint (Summary->Platform, sizeof (Summary->Platform), L"UEFI platform");

  Status = GetMemorySizeMb (&Summary->MemorySizeMb);
  if (EFI_ERROR (Status)) {
    Summary->MemorySizeMb = 0;
    return Status;
  }

  return EFI_SUCCESS;
}
