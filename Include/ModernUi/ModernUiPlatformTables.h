/** @file
  Shared standard-firmware-table access for ModernSetupPkg providers.

  Single place that reads SMBIOS structures and ACPI tables so the read-only
  data providers do not each re-implement protocol location, table walking, and
  string/field extraction. See Docs/ProviderDataContract.md.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_PLATFORM_TABLES_H_
#define MODERN_UI_PLATFORM_TABLES_H_

#include <Uefi.h>
#include <IndustryStandard/SmBios.h>

/**
  Find the Index-th (0-based) SMBIOS structure of a given type.

  @param[in] Type   SMBIOS structure type (e.g. SMBIOS_TYPE_PROCESSOR_INFORMATION).
  @param[in] Index  0-based occurrence within that type.

  @retval NULL    SMBIOS is unavailable or there is no such occurrence.
  @retval others  Read-only pointer into the live SMBIOS table (do not free).
**/
EFI_SMBIOS_TABLE_HEADER *
EFIAPI
ModernUiSmbiosFindStructure (
  IN EFI_SMBIOS_TYPE  Type,
  IN UINTN            Index
  );

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
  );

/**
  Return a pointer to the Nth (1-based) string in an SMBIOS record's string set.

  SMBIOS strings follow the formatted area (Record->Length bytes from the start)
  as a sequence of NUL-terminated ASCII strings ending in a double NUL. String
  number 0 means "no string".

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
  );

/**
  Return TRUE when an SMBIOS identity string carries no meaningful value.

  Treats NULL, empty, and well-known OEM placeholder strings ("To Be Filled By
  O.E.M.", "Not Specified", ...) as absent so callers can fall back cleanly.

  @param[in] String  ASCII string to test. May be NULL.

  @retval TRUE   String is NULL, empty, or a known placeholder.
  @retval FALSE  String carries a usable value.
**/
BOOLEAN
EFIAPI
ModernUiSmbiosIsPlaceholder (
  IN CONST CHAR8  *String
  );

/**
  Find an installed ACPI table by signature.

  Locates the ACPI 2.0+ RSDP from the UEFI configuration table, then scans the
  XSDT (or RSDT) for the first table whose header signature matches. The DSDT is
  reachable via the FADT and is not returned by signature here.

  @param[in] Signature  4-byte ACPI table signature (e.g. SIGNATURE_32('P','P','T','T')).

  @retval NULL    ACPI is unavailable or no such table is installed.
  @retval others  Read-only pointer to the ACPI table header (do not free).
**/
VOID *
EFIAPI
ModernUiAcpiFindTable (
  IN UINT32  Signature
  );

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
  );

#endif
