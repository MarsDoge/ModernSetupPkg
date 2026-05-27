/** @file
  Platform summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <IndustryStandard/SmBios.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/Smbios.h>
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

/**
  Convert an SMBIOS chassis type to a product form factor string.

  @param[in] ChassisType  SMBIOS Type 3 chassis type field.

  @return Non-NULL UCS-2 form factor name.
**/
STATIC
CONST CHAR16 *
GetFormFactorName (
  IN UINT8  ChassisType
  )
{
  switch (ChassisType & 0x7F) {
    case MiscChassisTypeDeskTop:
    case MiscChassisTypeLowProfileDesktop:
    case MiscChassisTypeMiniTower:
    case MiscChassisTypeTower:
      return L"Desktop";
    case MiscChassisTypePortable:
    case MiscChassisTypeLapTop:
    case MiscChassisTypeNotebook:
    case MiscChassisTypeSubNotebook:
    case MiscChassisConvertible:
    case MiscChassisDetachable:
      return L"Laptop / 2-in-1";
    case MiscChassisTypeAllInOne:
      return L"All-in-one";
    case MiscChassisTypeSpaceSaving:
    case MiscChassisTypeSealedCasePc:
    case MiscChassisMiniPc:
    case MiscChassisStickPc:
      return L"Mini PC";
    case MiscChassisTypeMainServerChassis:
    case MiscChassisTypeRackMountChassis:
    case MiscChassisBlade:
    case MiscChassisBladeEnclosure:
      return L"Server";
    case MiscChassisIoTGateway:
    case MiscChassisEmbeddedPc:
    case MiscChassisTypeHandHeld:
      return L"Embedded / appliance";
    default:
      //
      // Unrecognized chassis type: report empty so the consumer can show its
      // own localized "unknown" text instead of duplicating the platform name.
      //
      return L"";
  }
}

/**
  Read the platform form factor from SMBIOS Type 3 when available.

  @param[out] Buffer  Destination buffer. Must not be NULL.
  @param[in]  Count   Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
GetSmbiosFormFactor (
  OUT CHAR16  *Buffer,
  IN  UINTN   Count
  )
{
  EFI_STATUS              Status;
  EFI_SMBIOS_PROTOCOL     *Smbios;
  EFI_SMBIOS_HANDLE       Handle;
  EFI_SMBIOS_TABLE_HEADER *Record;
  EFI_SMBIOS_TYPE         Type;

  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  //
  // Default to empty (form factor not reported). When SMBIOS Type 3 is absent
  // the consumer applies its own localized "unknown" text rather than echoing
  // the generic platform name.
  //
  Buffer[0] = L'\0';
  Smbios    = NULL;
  Status = gBS->LocateProtocol (&gEfiSmbiosProtocolGuid, NULL, (VOID **)&Smbios);
  if (EFI_ERROR (Status) || (Smbios == NULL)) {
    return;
  }

  Handle = SMBIOS_HANDLE_PI_RESERVED;
  Type   = SMBIOS_TYPE_SYSTEM_ENCLOSURE;
  Record = NULL;
  Status = Smbios->GetNext (Smbios, &Handle, &Type, &Record, NULL);
  if (!EFI_ERROR (Status) && (Record != NULL)) {
    UnicodeSPrint (
      Buffer,
      Count * sizeof (CHAR16),
      L"%s",
      GetFormFactorName (((SMBIOS_TABLE_TYPE3 *)Record)->Type)
      );
  }
}

/**
  Return the boot mode label for the current application context.

  @param[out] Buffer  Destination buffer. Must not be NULL.
  @param[in]  Count   Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
GetBootModeName (
  OUT CHAR16  *Buffer,
  IN  UINTN   Count
  )
{
  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"UEFI");
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
  GetSmbiosFormFactor (Summary->FormFactor, ARRAY_SIZE (Summary->FormFactor));
  GetBootModeName (Summary->BootMode, ARRAY_SIZE (Summary->BootMode));

  Status = GetMemorySizeMb (&Summary->MemorySizeMb);
  if (EFI_ERROR (Status)) {
    Summary->MemorySizeMb = 0;
    return Status;
  }

  return EFI_SUCCESS;
}
