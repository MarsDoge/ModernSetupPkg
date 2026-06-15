/** @file
  Server and remote management summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <IndustryStandard/SmBios.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/IpmiProtocol.h>
#include <Protocol/RedfishDiscover.h>

#include <ModernUi/ModernUiPlatformTables.h>
#include <ModernUi/ModernUiManagementData.h>

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
  Return whether SMBIOS exposes a management controller host interface table.

  @retval TRUE   SMBIOS Type 38 or Type 42 is present.
  @retval FALSE  SMBIOS protocol is absent or no management table was found.
**/
STATIC
BOOLEAN
HasSmbiosManagementInterface (
  VOID
  )
{
  return (BOOLEAN)(ModernUiSmbiosTypePresent (SMBIOS_TYPE_IPMI_DEVICE_INFORMATION) ||
                   ModernUiSmbiosTypePresent (SMBIOS_TYPE_MANAGEMENT_CONTROLLER_HOST_INTERFACE));
}

/**
  Collect read-only management capability state.

  @param[out] Summary  Management summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiManagementDataGetSummary (
  OUT MODERN_UI_MANAGEMENT_SUMMARY  *Summary
  )
{
  if (Summary == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Summary, sizeof (*Summary));
  Summary->IpmiProtocolPresent              = IsProtocolPresent (&gIpmiProtocolGuid);
  Summary->RedfishDiscoverPresent           = IsProtocolPresent (&gEfiRedfishDiscoverProtocolGuid);
  Summary->SmbiosManagementInterfacePresent = HasSmbiosManagementInterface ();
  return EFI_SUCCESS;
}
