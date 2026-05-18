/** @file
  Power and thermal summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/Acpi.h>
#include <IndustryStandard/SmBios.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Protocol/Smbios.h>

#include <ModernUi/ModernUiPowerData.h>

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
  Convert SMBIOS chassis thermal state into user-facing text.

  @param[in]  State   SMBIOS MISC_CHASSIS_STATE value.
  @param[out] Buffer  Destination buffer. Must not be NULL.
  @param[in]  Count   Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
FormatThermalState (
  IN  UINT8   State,
  OUT CHAR16  *Buffer,
  IN  UINTN   Count
  )
{
  CONST CHAR16  *Text;

  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  switch (State) {
    case ChassisStateSafe:
      Text = L"Safe";
      break;
    case ChassisStateWarning:
      Text = L"Warning";
      break;
    case ChassisStateCritical:
      Text = L"Critical";
      break;
    case ChassisStateNonRecoverable:
      Text = L"Non-recoverable";
      break;
    default:
      Text = L"Unknown";
      break;
  }

  UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%s", Text);
}

/**
  Collect SMBIOS chassis and power supply presence.

  @param[out] Summary  Power summary to update. Must not be NULL.
**/
STATIC
VOID
CollectSmbiosPowerState (
  OUT MODERN_UI_POWER_SUMMARY  *Summary
  )
{
  EFI_STATUS              Status;
  EFI_SMBIOS_PROTOCOL     *Smbios;
  EFI_SMBIOS_HANDLE       Handle;
  EFI_SMBIOS_TABLE_HEADER *Record;
  EFI_SMBIOS_TYPE         Type;

  if (Summary == NULL) {
    return;
  }

  Smbios = NULL;
  Status = gBS->LocateProtocol (&gEfiSmbiosProtocolGuid, NULL, (VOID **)&Smbios);
  if (EFI_ERROR (Status) || (Smbios == NULL)) {
    return;
  }

  Handle = SMBIOS_HANDLE_PI_RESERVED;
  Type   = SMBIOS_TYPE_SYSTEM_ENCLOSURE;
  Status = Smbios->GetNext (Smbios, &Handle, &Type, &Record, NULL);
  if (!EFI_ERROR (Status) && (Record != NULL)) {
    Summary->SmbiosChassisPresent = TRUE;
    FormatThermalState (
      ((SMBIOS_TABLE_TYPE3 *)Record)->ThermalState,
      Summary->ChassisThermalState,
      ARRAY_SIZE (Summary->ChassisThermalState)
      );
  }

  Handle = SMBIOS_HANDLE_PI_RESERVED;
  Type   = SMBIOS_TYPE_SYSTEM_POWER_SUPPLY;
  Status = Smbios->GetNext (Smbios, &Handle, &Type, &Record, NULL);
  Summary->SmbiosPowerSupplyPresent = (BOOLEAN)!EFI_ERROR (Status);
}

/**
  Collect read-only power and thermal capability state.

  @param[out] Summary  Power summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiPowerDataGetSummary (
  OUT MODERN_UI_POWER_SUMMARY  *Summary
  )
{
  if (Summary == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Summary, sizeof (*Summary));
  UnicodeSPrint (Summary->ChassisThermalState, sizeof (Summary->ChassisThermalState), L"Unknown");
  Summary->AcpiTablePresent       = IsConfigurationTablePresent (&gEfiAcpi20TableGuid);
  Summary->AcpiSdtProtocolPresent = IsProtocolPresent (&gEfiAcpiSdtProtocolGuid);
  CollectSmbiosPowerState (Summary);
  return EFI_SUCCESS;
}
