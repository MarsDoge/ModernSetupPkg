/** @file
  Shared standard-firmware-table access for ModernSetupPkg providers.

  See Include/ModernUi/ModernUiPlatformTables.h and Docs/ProviderDataContract.md.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <IndustryStandard/SmBios.h>
#include <IndustryStandard/Acpi.h>
#include <Protocol/Smbios.h>
#include <Guid/Acpi.h>
#include <ModernUi/ModernUiPlatformTables.h>

/**
  Find the Index-th (0-based) SMBIOS structure of a given type. See the contract
  in ModernUi/ModernUiPlatformTables.h.

  @param[in] Type   SMBIOS structure type.
  @param[in] Index  0-based occurrence within that type.

  @retval NULL    SMBIOS unavailable or no such occurrence.
  @retval others  Read-only pointer into the live SMBIOS table.
**/
EFI_SMBIOS_TABLE_HEADER *
EFIAPI
ModernUiSmbiosFindStructure (
  IN EFI_SMBIOS_TYPE  Type,
  IN UINTN            Index
  )
{
  EFI_STATUS               Status;
  EFI_SMBIOS_PROTOCOL      *Smbios;
  EFI_SMBIOS_HANDLE        Handle;
  EFI_SMBIOS_TABLE_HEADER  *Record;
  EFI_SMBIOS_TYPE          SearchType;
  UINTN                    Seen;

  Smbios = NULL;
  Status = gBS->LocateProtocol (&gEfiSmbiosProtocolGuid, NULL, (VOID **)&Smbios);
  if (EFI_ERROR (Status) || (Smbios == NULL)) {
    return NULL;
  }

  Seen   = 0;
  Handle = SMBIOS_HANDLE_PI_RESERVED;
  for ( ; ;) {
    SearchType = Type;
    Record     = NULL;
    Status     = Smbios->GetNext (Smbios, &Handle, &SearchType, &Record, NULL);
    if (EFI_ERROR (Status) || (Record == NULL)) {
      return NULL;
    }

    if (Seen == Index) {
      return Record;
    }

    Seen++;
  }
}

/**
  Return TRUE when at least one SMBIOS structure of the given type exists.

  @param[in] Type  SMBIOS structure type.

  @retval TRUE   At least one structure of Type is present.
  @retval FALSE  None present, or SMBIOS unavailable.
**/
BOOLEAN
EFIAPI
ModernUiSmbiosTypePresent (
  IN EFI_SMBIOS_TYPE  Type
  )
{
  return (BOOLEAN)(ModernUiSmbiosFindStructure (Type, 0) != NULL);
}

/**
  Return a pointer to the Nth (1-based) string in an SMBIOS record's string set.

  @param[in] Record        SMBIOS record header. May be NULL.
  @param[in] StringNumber  1-based SMBIOS string reference; 0 means none.

  @retval NULL    Record is NULL, StringNumber is 0, or the string is absent.
  @retval others  Read-only pointer to the NUL-terminated ASCII string.
**/
CHAR8 *
EFIAPI
ModernUiSmbiosGetString (
  IN EFI_SMBIOS_TABLE_HEADER  *Record,
  IN UINT8                    StringNumber
  )
{
  CHAR8  *String;
  UINT8  Index;

  if ((Record == NULL) || (StringNumber == 0)) {
    return NULL;
  }

  String = (CHAR8 *)Record + Record->Length;
  for (Index = 1; Index < StringNumber; Index++) {
    if (*String == 0) {
      //
      // The string set ended before reaching StringNumber.
      //
      return NULL;
    }

    while (*String != 0) {
      String++;
    }

    String++;
  }

  return (*String != 0) ? String : NULL;
}

/**
  Return TRUE when an SMBIOS identity string carries no meaningful value.

  @param[in] String  ASCII string to test. May be NULL.

  @retval TRUE   String is NULL, empty, or a known placeholder.
  @retval FALSE  String carries a usable value.
**/
BOOLEAN
EFIAPI
ModernUiSmbiosIsPlaceholder (
  IN CONST CHAR8  *String
  )
{
  STATIC CONST CHAR8  *Placeholders[] = {
    "To Be Filled By O.E.M.",
    "Not Specified",
    "Not Applicable",
    "System manufacturer",
    "System Product Name",
    "Default string",
    "Default String",
    "None",
    "OEM",
    "O.E.M."
  };
  UINTN  Index;

  if ((String == NULL) || (String[0] == '\0')) {
    return TRUE;
  }

  for (Index = 0; Index < ARRAY_SIZE (Placeholders); Index++) {
    if (AsciiStrCmp (String, Placeholders[Index]) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Return the ACPI 2.0+ RSDP from the UEFI configuration table, or NULL.

  @retval NULL    No ACPI 2.0+ RSDP is installed.
  @retval others  Read-only pointer to the RSDP.
**/
STATIC
EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *
ModernUiAcpiGetRsdp (
  VOID
  )
{
  VOID  *Rsdp;

  Rsdp = NULL;
  if (EFI_ERROR (EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &Rsdp)) || (Rsdp == NULL)) {
    if (EFI_ERROR (EfiGetSystemConfigurationTable (&gEfiAcpi10TableGuid, &Rsdp))) {
      return NULL;
    }
  }

  return (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)Rsdp;
}

/**
  Find an installed ACPI table by signature via the XSDT (preferred) or RSDT.

  @param[in] Signature  4-byte ACPI table signature.

  @retval NULL    ACPI unavailable or no such table.
  @retval others  Read-only pointer to the ACPI table header.
**/
VOID *
EFIAPI
ModernUiAcpiFindTable (
  IN UINT32  Signature
  )
{
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER     *Rsdp;
  EFI_ACPI_DESCRIPTION_HEADER                      *Xsdt;
  EFI_ACPI_DESCRIPTION_HEADER                      *Entry;
  UINTN                                            Count;
  UINTN                                            Index;

  Rsdp = ModernUiAcpiGetRsdp ();
  if (Rsdp == NULL) {
    return NULL;
  }

  //
  // Prefer the 64-bit XSDT; entries are 8 bytes. Fall back to the 32-bit RSDT
  // (4-byte entries) when no XSDT is present.
  //
  if (Rsdp->XsdtAddress != 0) {
    Xsdt  = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->XsdtAddress;
    Count = (Xsdt->Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) / sizeof (UINT64);
    for (Index = 0; Index < Count; Index++) {
      UINT64  EntryAddr;
      CopyMem (&EntryAddr, (UINT8 *)(Xsdt + 1) + Index * sizeof (UINT64), sizeof (UINT64));
      Entry = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)EntryAddr;
      if ((Entry != NULL) && (Entry->Signature == Signature)) {
        return Entry;
      }
    }

    return NULL;
  }

  if (Rsdp->RsdtAddress != 0) {
    Xsdt  = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->RsdtAddress;
    Count = (Xsdt->Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) / sizeof (UINT32);
    for (Index = 0; Index < Count; Index++) {
      UINT32  EntryAddr;
      CopyMem (&EntryAddr, (UINT8 *)(Xsdt + 1) + Index * sizeof (UINT32), sizeof (UINT32));
      Entry = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)EntryAddr;
      if ((Entry != NULL) && (Entry->Signature == Signature)) {
        return Entry;
      }
    }
  }

  return NULL;
}

/**
  Return TRUE when an ACPI table with the given signature is installed.

  @param[in] Signature  4-byte ACPI table signature.

  @retval TRUE   The table is present.
  @retval FALSE  Not present, or ACPI unavailable.
**/
BOOLEAN
EFIAPI
ModernUiAcpiTablePresent (
  IN UINT32  Signature
  )
{
  return (BOOLEAN)(ModernUiAcpiFindTable (Signature) != NULL);
}
