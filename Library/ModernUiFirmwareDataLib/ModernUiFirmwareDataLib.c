/** @file
  Firmware lifecycle summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/CapsuleReport.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/Capsule.h>

#include <ModernUi/ModernUiFirmwareData.h>

/**
  Return whether a protocol is currently installed.

  @param[in] ProtocolGuid  Protocol GUID to locate. Must not be NULL.

  @retval TRUE   The protocol is installed.
  @retval FALSE  The protocol is absent or ProtocolGuid is NULL.
**/
STATIC
BOOLEAN
IsProtocolPresent (
  IN CONST EFI_GUID  *ProtocolGuid
  )
{
  VOID  *Protocol;

  if (ProtocolGuid == NULL) {
    return FALSE;
  }

  Protocol = NULL;
  return (BOOLEAN)!EFI_ERROR (gBS->LocateProtocol ((EFI_GUID *)ProtocolGuid, NULL, &Protocol));
}

/**
  Return whether a variable exists.

  @param[in] Name        Variable name. Must not be NULL.
  @param[in] VendorGuid  Vendor GUID. Must not be NULL.

  @retval TRUE   The variable exists.
  @retval FALSE  The variable is absent or the input is invalid.
**/
STATIC
BOOLEAN
IsVariablePresent (
  IN CONST CHAR16    *Name,
  IN CONST EFI_GUID  *VendorGuid
  )
{
  EFI_STATUS  Status;
  UINTN       Size;

  if ((Name == NULL) || (VendorGuid == NULL)) {
    return FALSE;
  }

  Size   = 0;
  Status = gRT->GetVariable ((CHAR16 *)Name, (EFI_GUID *)VendorGuid, NULL, &Size, NULL);
  return (BOOLEAN)(Status == EFI_BUFFER_TOO_SMALL);
}

/**
  Collect read-only firmware lifecycle capability state.

  @param[out] Summary  Firmware summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiFirmwareDataGetSummary (
  OUT MODERN_UI_FIRMWARE_SUMMARY  *Summary
  )
{
  if (Summary == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Summary, sizeof (*Summary));
  UnicodeSPrint (
    Summary->Vendor,
    sizeof (Summary->Vendor),
    L"%s",
    (gST->FirmwareVendor == NULL) ? L"Unknown" : gST->FirmwareVendor
    );
  //
  // gST->FirmwareRevision is conventionally encoded as (major << 16) | minor.
  // Surface the human-readable major.minor form while keeping the raw hex so a
  // firmware engineer can still read the exact encoded value.
  //
  UnicodeSPrint (
    Summary->Revision,
    sizeof (Summary->Revision),
    L"%u.%02u (0x%08x)",
    (UINT32)(gST->FirmwareRevision >> 16),
    (UINT32)(gST->FirmwareRevision & 0xFFFF),
    gST->FirmwareRevision
    );

  Summary->CapsuleRuntimeServices = (BOOLEAN)((gRT->UpdateCapsule != NULL) && (gRT->QueryCapsuleCapabilities != NULL));
  Summary->CapsuleArchProtocol    = IsProtocolPresent (&gEfiCapsuleArchProtocolGuid);
  Summary->CapsuleReportPresent   = IsVariablePresent (L"Capsule0000", &gEfiCapsuleReportGuid);
  return EFI_SUCCESS;
}
