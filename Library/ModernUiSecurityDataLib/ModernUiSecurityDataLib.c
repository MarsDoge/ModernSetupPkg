/** @file
  Security state provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/GlobalVariable.h>
#include <Guid/ImageAuthentication.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/Tcg2Protocol.h>
#include <Protocol/TrEEProtocol.h>
#include <ModernUi/ModernUiSecurityData.h>

/**
  Read a one-byte global variable as enabled/disabled state.

  @param[in] Name  Global variable name. Must not be NULL.

  @return Enabled, disabled, or unknown state.
**/
STATIC
MODERN_UI_SECURITY_STATE
ReadGlobalBooleanState (
  IN CONST CHAR16  *Name
  )
{
  EFI_STATUS  Status;
  UINT8       Value;
  UINTN       Size;

  if (Name == NULL) {
    return ModernUiSecurityStateUnknown;
  }

  Value = 0;
  Size  = sizeof (Value);
  Status = gRT->GetVariable ((CHAR16 *)Name, &gEfiGlobalVariableGuid, NULL, &Size, &Value);
  if (EFI_ERROR (Status) || (Size < sizeof (Value))) {
    return ModernUiSecurityStateUnknown;
  }

  return (Value == 0) ? ModernUiSecurityStateDisabled : ModernUiSecurityStateEnabled;
}

/**
  Return whether a variable exists and has data.

  @param[in] Name        Variable name. Must not be NULL.
  @param[in] VendorGuid  Vendor GUID. Must not be NULL.

  @return Present, absent, or unknown state.
**/
STATIC
MODERN_UI_SECURITY_STATE
ReadVariablePresence (
  IN CONST CHAR16    *Name,
  IN CONST EFI_GUID  *VendorGuid
  )
{
  EFI_STATUS  Status;
  UINTN       Size;

  if ((Name == NULL) || (VendorGuid == NULL)) {
    return ModernUiSecurityStateUnknown;
  }

  Size = 0;
  Status = gRT->GetVariable ((CHAR16 *)Name, (EFI_GUID *)VendorGuid, NULL, &Size, NULL);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    return ModernUiSecurityStatePresent;
  }

  if (Status == EFI_NOT_FOUND) {
    return ModernUiSecurityStateAbsent;
  }

  return ModernUiSecurityStateUnknown;
}

/**
  Return whether a protocol is present on any handle.

  @param[in] ProtocolGuid  Protocol GUID to locate. Must not be NULL.

  @return Present, absent, or unknown state.
**/
STATIC
MODERN_UI_SECURITY_STATE
ReadProtocolPresence (
  IN CONST EFI_GUID  *ProtocolGuid
  )
{
  EFI_STATUS  Status;
  VOID        *Protocol;

  if (ProtocolGuid == NULL) {
    return ModernUiSecurityStateUnknown;
  }

  Protocol = NULL;
  Status = gBS->LocateProtocol ((EFI_GUID *)ProtocolGuid, NULL, &Protocol);
  if (!EFI_ERROR (Status)) {
    return ModernUiSecurityStatePresent;
  }

  return (Status == EFI_NOT_FOUND) ? ModernUiSecurityStateAbsent : ModernUiSecurityStateUnknown;
}

EFI_STATUS
EFIAPI
ModernUiSecurityDataGetSummary (
  OUT MODERN_UI_SECURITY_SUMMARY  *Summary
  )
{
  if (Summary == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Summary, sizeof (*Summary));
  Summary->SecureBoot           = ReadGlobalBooleanState (EFI_SECURE_BOOT_MODE_NAME);
  Summary->SetupMode            = ReadGlobalBooleanState (EFI_SETUP_MODE_NAME);
  Summary->PlatformKey          = ReadVariablePresence (EFI_PLATFORM_KEY_NAME, &gEfiGlobalVariableGuid);
  Summary->KeyExchangeKey       = ReadVariablePresence (EFI_KEY_EXCHANGE_KEY_NAME, &gEfiGlobalVariableGuid);
  Summary->SignatureDb          = ReadVariablePresence (EFI_IMAGE_SECURITY_DATABASE, &gEfiImageSecurityDatabaseGuid);
  Summary->ForbiddenSignatureDb = ReadVariablePresence (EFI_IMAGE_SECURITY_DATABASE1, &gEfiImageSecurityDatabaseGuid);
  Summary->Tcg2Protocol         = ReadProtocolPresence (&gEfiTcg2ProtocolGuid);
  Summary->TreeProtocol         = ReadProtocolPresence (&gEfiTrEEProtocolGuid);
  return EFI_SUCCESS;
}
