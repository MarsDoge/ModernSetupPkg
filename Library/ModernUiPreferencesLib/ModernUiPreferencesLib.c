/** @file
  ModernSetupApp app-owned preference variable access.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <ModernUi/ModernUiPreferences.h>

/**
  Validate and normalize app-owned preferences in place.

  @param[in,out] Preferences  Preferences to validate. Must not be NULL.

  @retval TRUE   Preferences are structurally valid and were normalized.
  @retval FALSE  Preferences signature/version/size is invalid.
**/
STATIC
BOOLEAN
ValidatePreferences (
  IN OUT MODERN_UI_PREFERENCES  *Preferences
  )
{
  if (Preferences == NULL) {
    return FALSE;
  }

  if ((Preferences->Signature != MODERN_UI_PREFERENCES_SIGNATURE) ||
      (Preferences->Version != MODERN_UI_PREFERENCES_VERSION) ||
      (Preferences->Size != sizeof (*Preferences)))
  {
    return FALSE;
  }

  if (Preferences->ThemeId > MODERN_UI_PREFERENCES_THEME_MAX) {
    Preferences->ThemeId = MODERN_UI_PREFERENCES_THEME_SYSTEM;
  }

  if (Preferences->DashboardDensity >= ModernUiDashboardDensityMax) {
    Preferences->DashboardDensity = ModernUiDashboardDensityComfortable;
  }

  Preferences->RememberLastPage  = (Preferences->RememberLastPage != 0) ? 1 : 0;
  Preferences->ShowAdvancedHints = (Preferences->ShowAdvancedHints != 0) ? 1 : 0;
  Preferences->ConfirmReset      = (Preferences->ConfirmReset != 0) ? 1 : 0;
  ZeroMem (Preferences->Reserved, sizeof (Preferences->Reserved));
  return TRUE;
}

EFI_STATUS
EFIAPI
ModernUiPreferencesResetToDefaults (
  OUT MODERN_UI_PREFERENCES  *Preferences
  )
{
  if (Preferences == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Preferences, sizeof (*Preferences));
  Preferences->Signature         = MODERN_UI_PREFERENCES_SIGNATURE;
  Preferences->Version           = MODERN_UI_PREFERENCES_VERSION;
  Preferences->Size              = sizeof (*Preferences);
  Preferences->ThemeId           = MODERN_UI_PREFERENCES_THEME_SYSTEM;
  Preferences->DashboardDensity  = ModernUiDashboardDensityComfortable;
  Preferences->RememberLastPage  = TRUE;
  Preferences->ShowAdvancedHints = TRUE;
  Preferences->ConfirmReset      = TRUE;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ModernUiPreferencesLoad (
  OUT MODERN_UI_PREFERENCES  *Preferences
  )
{
  EFI_STATUS             Status;
  UINTN                  Size;
  UINT32                 Attributes;
  MODERN_UI_PREFERENCES  Candidate;

  if (Preferences == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ModernUiPreferencesResetToDefaults (Preferences);

  Size = sizeof (Candidate);
  Attributes = 0;
  ZeroMem (&Candidate, sizeof (Candidate));
  Status = gRT->GetVariable (
                  MODERN_UI_PREFERENCES_VARIABLE_NAME,
                  &gModernSetupAppPreferencesGuid,
                  &Attributes,
                  &Size,
                  &Candidate
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Attributes != (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS)) {
    return EFI_SECURITY_VIOLATION;
  }

  if ((Size != sizeof (Candidate)) || !ValidatePreferences (&Candidate)) {
    return EFI_COMPROMISED_DATA;
  }

  CopyMem (Preferences, &Candidate, sizeof (*Preferences));
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ModernUiPreferencesSave (
  IN CONST MODERN_UI_PREFERENCES  *Preferences
  )
{
  MODERN_UI_PREFERENCES  Candidate;
  UINT32                 Attributes;

  if (Preferences == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (&Candidate, Preferences, sizeof (Candidate));
  if (!ValidatePreferences (&Candidate)) {
    return EFI_INVALID_PARAMETER;
  }

  Attributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS;
  return gRT->SetVariable (
                MODERN_UI_PREFERENCES_VARIABLE_NAME,
                &gModernSetupAppPreferencesGuid,
                Attributes,
                sizeof (Candidate),
                &Candidate
                );
}
