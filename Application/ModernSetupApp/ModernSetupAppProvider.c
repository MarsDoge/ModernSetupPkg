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

STATIC MODERN_SETUP_PROVIDER_SNAPSHOT  mModernSetupProviderSnapshotCache;
STATIC BOOLEAN                         mModernSetupProviderSnapshotCacheValid;

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
  Snapshot->HardwareHealthStatus = EFI_NOT_READY;
  Snapshot->PerformanceStatus = EFI_NOT_READY;
  Snapshot->PcieStatus        = EFI_NOT_READY;

  SetUnknownText (Snapshot->Platform.FirmwareVendor, ARRAY_SIZE (Snapshot->Platform.FirmwareVendor));
  SetUnknownText (Snapshot->Platform.FirmwareRevision, ARRAY_SIZE (Snapshot->Platform.FirmwareRevision));
  SetUnknownText (Snapshot->Platform.Architecture, ARRAY_SIZE (Snapshot->Platform.Architecture));
  SetUnknownText (Snapshot->Platform.Platform, ARRAY_SIZE (Snapshot->Platform.Platform));
  SetUnknownText (Snapshot->Platform.Processor, ARRAY_SIZE (Snapshot->Platform.Processor));
  SetUnknownText (Snapshot->Platform.FormFactor, ARRAY_SIZE (Snapshot->Platform.FormFactor));
  SetUnknownText (Snapshot->Platform.BootMode, ARRAY_SIZE (Snapshot->Platform.BootMode));
  SetUnknownText (Snapshot->Firmware.Vendor, ARRAY_SIZE (Snapshot->Firmware.Vendor));
  SetUnknownText (Snapshot->Firmware.Revision, ARRAY_SIZE (Snapshot->Firmware.Revision));
  SetUnknownText (Snapshot->Power.ChassisThermalState, ARRAY_SIZE (Snapshot->Power.ChassisThermalState));
}

typedef struct {
  CONST CHAR16  *Name;
  EFI_STATUS    Status;
} MODERN_SETUP_PROVIDER_STATUS_ENTRY;

/**
  Return stable display text for an aggregate provider health state.

  @param[in] State  Aggregate health state.

  @return Non-NULL display text owned by the app.
**/
CONST CHAR16 *
ModernSetupGetProviderHealthStateText (
  IN MODERN_SETUP_PROVIDER_HEALTH_STATE  State
  )
{
  switch (State) {
    case ModernSetupProviderHealthReady:
      return L"Ready";
    case ModernSetupProviderHealthDegraded:
      return L"Degraded";
    case ModernSetupProviderHealthNotReady:
    default:
      return L"Not Ready";
  }
}

/**
  Derive a compact app-private readiness summary from provider snapshot statuses.

  The summary is intentionally read-only and derived only from app-owned snapshot
  status fields. It lets presentation code show whether provider-backed data is
  complete, partially degraded, or unavailable without bypassing the provider
  snapshot boundary.

  @param[in]  Snapshot  Provider snapshot to summarize. Must not be NULL.
  @param[out] Health    Health summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Health summary was filled.
  @retval EFI_INVALID_PARAMETER  Snapshot or Health is NULL.
**/
EFI_STATUS
ModernSetupGetProviderHealthSummary (
  IN  CONST MODERN_SETUP_PROVIDER_SNAPSHOT  *Snapshot,
  OUT MODERN_SETUP_PROVIDER_HEALTH_SUMMARY  *Health
  )
{
  MODERN_SETUP_PROVIDER_STATUS_ENTRY  Entries[9];
  UINTN                               Index;

  if ((Snapshot == NULL) || (Health == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Entries[0] = (MODERN_SETUP_PROVIDER_STATUS_ENTRY){ L"Platform",    Snapshot->PlatformStatus };
  Entries[1] = (MODERN_SETUP_PROVIDER_STATUS_ENTRY){ L"Security",    Snapshot->SecurityStatus };
  Entries[2] = (MODERN_SETUP_PROVIDER_STATUS_ENTRY){ L"Firmware",    Snapshot->FirmwareStatus };
  Entries[3] = (MODERN_SETUP_PROVIDER_STATUS_ENTRY){ L"Diagnostics", Snapshot->DiagnosticsStatus };
  Entries[4] = (MODERN_SETUP_PROVIDER_STATUS_ENTRY){ L"Management",  Snapshot->ManagementStatus };
  Entries[5] = (MODERN_SETUP_PROVIDER_STATUS_ENTRY){ L"Power",       Snapshot->PowerStatus };
  Entries[6] = (MODERN_SETUP_PROVIDER_STATUS_ENTRY){ L"Hardware Health", Snapshot->HardwareHealthStatus };
  Entries[7] = (MODERN_SETUP_PROVIDER_STATUS_ENTRY){ L"Performance", Snapshot->PerformanceStatus };
  Entries[8] = (MODERN_SETUP_PROVIDER_STATUS_ENTRY){ L"PCIe Policy", Snapshot->PcieStatus };

  ZeroMem (Health, sizeof (*Health));
  Health->TotalProviders       = ARRAY_SIZE (Entries);
  Health->FirstIssueStatus     = EFI_SUCCESS;
  Health->FirstIssueName       = L"None";

  for (Index = 0; Index < ARRAY_SIZE (Entries); Index++) {
    if (EFI_ERROR (Entries[Index].Status)) {
      Health->UnavailableProviders++;
      if (Health->FirstIssueStatus == EFI_SUCCESS) {
        Health->FirstIssueStatus = Entries[Index].Status;
        Health->FirstIssueName   = Entries[Index].Name;
      }
    } else {
      Health->ReadyProviders++;
    }
  }

  if (Health->ReadyProviders == Health->TotalProviders) {
    Health->State = ModernSetupProviderHealthReady;
  } else if (Health->ReadyProviders == 0) {
    Health->State = ModernSetupProviderHealthNotReady;
  } else {
    Health->State = ModernSetupProviderHealthDegraded;
  }

  return EFI_SUCCESS;
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
  MODERN_UI_HARDWARE_HEALTH_SUMMARY HardwareHealth;
  MODERN_UI_PERFORMANCE_SUMMARY   Performance;
  MODERN_UI_PCIE_SUMMARY          Pcie;

  if (Snapshot == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  InitializeProviderSnapshotDefaults (Snapshot);

  Snapshot->PlatformStatus = ModernUiPlatformDataGetSummary (&Platform);
  if (!EFI_ERROR (Snapshot->PlatformStatus)) {
    CopyMem (&Snapshot->Platform, &Platform, sizeof (Snapshot->Platform));
    //
    // The platform provider leaves the form factor empty when SMBIOS Type 3 is
    // unavailable. Surface the localized Unknown text instead of an empty value
    // so it does not duplicate the generic platform name.
    //
    if (Snapshot->Platform.FormFactor[0] == L'\0') {
      SetUnknownText (Snapshot->Platform.FormFactor, ARRAY_SIZE (Snapshot->Platform.FormFactor));
    }
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

  Snapshot->HardwareHealthStatus = ModernUiHardwareHealthDataGetSummary (&HardwareHealth);
  if (!EFI_ERROR (Snapshot->HardwareHealthStatus)) {
    CopyMem (&Snapshot->HardwareHealth, &HardwareHealth, sizeof (Snapshot->HardwareHealth));
  }

  Snapshot->PerformanceStatus = ModernUiPerformanceDataGetSummary (&Performance);
  if (!EFI_ERROR (Snapshot->PerformanceStatus)) {
    CopyMem (&Snapshot->Performance, &Performance, sizeof (Snapshot->Performance));
  }

  Snapshot->PcieStatus = ModernUiPcieDataGetSummary (&Pcie);
  if (!EFI_ERROR (Snapshot->PcieStatus)) {
    CopyMem (&Snapshot->Pcie, &Pcie, sizeof (Snapshot->Pcie));
  }

  return EFI_SUCCESS;
}

/**
  Return a cached provider snapshot for app-session redraws.

  Provider data is read-only presentation state. Caching avoids repeating
  platform/provider enumeration on every page redraw while preserving an
  explicit invalidation boundary for native handoffs.

  @param[out] Snapshot  Receives the cached snapshot. Must not be NULL.

  @retval EFI_SUCCESS            Snapshot was filled.
  @retval EFI_INVALID_PARAMETER  Snapshot is NULL.
  @retval others                 Provider collection failed before cache fill.
**/
EFI_STATUS
ModernSetupGetCachedProviderSnapshot (
  OUT MODERN_SETUP_PROVIDER_SNAPSHOT  *Snapshot
  )
{
  EFI_STATUS  Status;

  if (Snapshot == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (!mModernSetupProviderSnapshotCacheValid) {
    Status = ModernSetupGetProviderSnapshot (&mModernSetupProviderSnapshotCache);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    mModernSetupProviderSnapshotCacheValid = TRUE;
  }

  CopyMem (Snapshot, &mModernSetupProviderSnapshotCache, sizeof (*Snapshot));
  return EFI_SUCCESS;
}

/**
  Invalidate app-session provider snapshot cache.
**/
VOID
ModernSetupInvalidateProviderSnapshotCache (
  VOID
  )
{
  ZeroMem (&mModernSetupProviderSnapshotCache, sizeof (mModernSetupProviderSnapshotCache));
  mModernSetupProviderSnapshotCacheValid = FALSE;
}
