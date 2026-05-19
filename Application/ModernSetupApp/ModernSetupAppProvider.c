/** @file
  ModernSetupApp private provider contract helpers.

  This module is the app-owned boundary between read-only provider libraries and
  presentation code. It normalizes provider failures into safe fallback values so
  Dashboard and provider summary pages can render without duplicating recovery
  policy or reaching below the provider LibraryClass surface.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ModernSetupAppInternal.h"

/**
  Copy the localized Unknown text into a fixed provider text buffer.

  @param[out] Buffer  Destination buffer. Must not be NULL.
  @param[in]  Count   Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
SetUnknownText (
  OUT CHAR16  *Buffer,
  IN  UINTN   Count
  )
{
  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  StrCpyS (Buffer, Count, ModernUiGetString (ModernUiStringUnknown));
}

/**
  Initialize a provider snapshot with safe read-only fallback values.

  @param[out] Snapshot  Snapshot to initialize. Must not be NULL.
**/
STATIC
VOID
InitializeProviderSnapshotDefaults (
  OUT MODERN_SETUP_PROVIDER_SNAPSHOT  *Snapshot
  )
{
  ZeroMem (Snapshot, sizeof (*Snapshot));

  Snapshot->PlatformStatus    = EFI_NOT_READY;
  Snapshot->SecurityStatus    = EFI_NOT_READY;
  Snapshot->FirmwareStatus    = EFI_NOT_READY;
  Snapshot->DiagnosticsStatus = EFI_NOT_READY;
  Snapshot->ManagementStatus  = EFI_NOT_READY;
  Snapshot->PowerStatus       = EFI_NOT_READY;
  Snapshot->PerformanceStatus = EFI_NOT_READY;

  SetUnknownText (Snapshot->Platform.FirmwareVendor, ARRAY_SIZE (Snapshot->Platform.FirmwareVendor));
  SetUnknownText (Snapshot->Platform.FirmwareRevision, ARRAY_SIZE (Snapshot->Platform.FirmwareRevision));
  SetUnknownText (Snapshot->Platform.Architecture, ARRAY_SIZE (Snapshot->Platform.Architecture));
  SetUnknownText (Snapshot->Platform.Platform, ARRAY_SIZE (Snapshot->Platform.Platform));
  SetUnknownText (Snapshot->Platform.FormFactor, ARRAY_SIZE (Snapshot->Platform.FormFactor));
  SetUnknownText (Snapshot->Platform.BootMode, ARRAY_SIZE (Snapshot->Platform.BootMode));
  SetUnknownText (Snapshot->Firmware.Vendor, ARRAY_SIZE (Snapshot->Firmware.Vendor));
  SetUnknownText (Snapshot->Firmware.Revision, ARRAY_SIZE (Snapshot->Firmware.Revision));
  SetUnknownText (Snapshot->Power.ChassisThermalState, ARRAY_SIZE (Snapshot->Power.ChassisThermalState));
}

/**
  Collect one normalized read-only app provider snapshot.

  Provider LibraryClasses remain the sole data collection surface. The app uses
  this private snapshot to centralize failure defaults and to keep pages from
  duplicating provider recovery policy.

  @param[out] Snapshot  Provider snapshot to fill. Must not be NULL.

  @retval EFI_SUCCESS            Snapshot was filled. Individual provider status
                                  fields describe partial collection failures.
  @retval EFI_INVALID_PARAMETER  Snapshot is NULL.
**/
EFI_STATUS
ModernSetupGetProviderSnapshot (
  OUT MODERN_SETUP_PROVIDER_SNAPSHOT  *Snapshot
  )
{
  MODERN_UI_PLATFORM_SUMMARY      Platform;
  MODERN_UI_SECURITY_SUMMARY      Security;
  MODERN_UI_FIRMWARE_SUMMARY      Firmware;
  MODERN_UI_DIAGNOSTICS_SUMMARY   Diagnostics;
  MODERN_UI_MANAGEMENT_SUMMARY    Management;
  MODERN_UI_POWER_SUMMARY         Power;
  MODERN_UI_PERFORMANCE_SUMMARY   Performance;

  if (Snapshot == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  InitializeProviderSnapshotDefaults (Snapshot);

  Snapshot->PlatformStatus = ModernUiPlatformDataGetSummary (&Platform);
  if (!EFI_ERROR (Snapshot->PlatformStatus)) {
    CopyMem (&Snapshot->Platform, &Platform, sizeof (Snapshot->Platform));
  }

  Snapshot->SecurityStatus = ModernUiSecurityDataGetSummary (&Security);
  if (!EFI_ERROR (Snapshot->SecurityStatus)) {
    CopyMem (&Snapshot->Security, &Security, sizeof (Snapshot->Security));
  }

  Snapshot->FirmwareStatus = ModernUiFirmwareDataGetSummary (&Firmware);
  if (!EFI_ERROR (Snapshot->FirmwareStatus)) {
    CopyMem (&Snapshot->Firmware, &Firmware, sizeof (Snapshot->Firmware));
  }

  Snapshot->DiagnosticsStatus = ModernUiDiagnosticsDataGetSummary (&Diagnostics);
  if (!EFI_ERROR (Snapshot->DiagnosticsStatus)) {
    CopyMem (&Snapshot->Diagnostics, &Diagnostics, sizeof (Snapshot->Diagnostics));
  }

  Snapshot->ManagementStatus = ModernUiManagementDataGetSummary (&Management);
  if (!EFI_ERROR (Snapshot->ManagementStatus)) {
    CopyMem (&Snapshot->Management, &Management, sizeof (Snapshot->Management));
  }

  Snapshot->PowerStatus = ModernUiPowerDataGetSummary (&Power);
  if (!EFI_ERROR (Snapshot->PowerStatus)) {
    CopyMem (&Snapshot->Power, &Power, sizeof (Snapshot->Power));
  }

  Snapshot->PerformanceStatus = ModernUiPerformanceDataGetSummary (&Performance);
  if (!EFI_ERROR (Snapshot->PerformanceStatus)) {
    CopyMem (&Snapshot->Performance, &Performance, sizeof (Snapshot->Performance));
  }

  return EFI_SUCCESS;
}
