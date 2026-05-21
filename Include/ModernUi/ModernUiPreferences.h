/** @file
  Typed app-owned preferences for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_PREFERENCES_H_
#define MODERN_UI_PREFERENCES_H_

#include <Uefi.h>

#define MODERN_UI_PREFERENCES_VARIABLE_NAME  L"ModernSetupAppPreferences"
#define MODERN_UI_PREFERENCES_SIGNATURE      SIGNATURE_32 ('M', 'S', 'P', 'F')
#define MODERN_UI_PREFERENCES_VERSION        2

#define MODERN_UI_PREFERENCES_BOOT_TIMEOUT_MIN      0
#define MODERN_UI_PREFERENCES_BOOT_TIMEOUT_MAX      30
#define MODERN_UI_PREFERENCES_BOOT_TIMEOUT_DEFAULT  5
#define MODERN_UI_PREFERENCES_PROFILE_NAME_CHARS    32

#define MODERN_UI_PREFERENCES_THEME_SYSTEM   0
#define MODERN_UI_PREFERENCES_THEME_DARK     1
#define MODERN_UI_PREFERENCES_THEME_RED      2
#define MODERN_UI_PREFERENCES_THEME_MAX      MODERN_UI_PREFERENCES_THEME_RED

typedef enum {
  ModernUiDashboardDensityComfortable = 0,
  ModernUiDashboardDensityCompact,
  ModernUiDashboardDensityMax
} MODERN_UI_DASHBOARD_DENSITY;

#pragma pack(1)
typedef struct {
  UINT32  Signature;
  UINT16  Version;
  UINT16  Size;
  UINT8   ThemeId;
  UINT8   DashboardDensity;
  UINT8   RememberLastPage;
  UINT8   ShowAdvancedHints;
  UINT8   ConfirmReset;
  UINT8   BootTimeoutSeconds;
  CHAR16  ProfileName[MODERN_UI_PREFERENCES_PROFILE_NAME_CHARS];
  UINT8   Reserved[2];
} MODERN_UI_PREFERENCES;
#pragma pack()

/**
  Fill a preferences structure with built-in defaults.

  @param[out] Preferences  Preferences structure to initialize. Must not be NULL.

  @retval EFI_SUCCESS            Defaults were written to Preferences.
  @retval EFI_INVALID_PARAMETER  Preferences is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiPreferencesResetToDefaults (
  OUT MODERN_UI_PREFERENCES  *Preferences
  );

/**
  Load ModernSetupApp app-owned preferences from NVRAM.

  Missing or invalid variables return defaults in Preferences and do not write
  NVRAM. Callers may use EFI_NOT_FOUND/EFI_COMPROMISED_DATA to explain fallback.

  @param[out] Preferences  Receives loaded or default preferences. Must not be NULL.

  @retval EFI_SUCCESS           Preferences were loaded and validated.
  @retval EFI_NOT_FOUND         Variable was absent; defaults were returned.
  @retval EFI_COMPROMISED_DATA  Variable existed but was invalid; defaults were returned.
  @retval others                GetVariable status; defaults were returned.
**/
EFI_STATUS
EFIAPI
ModernUiPreferencesLoad (
  OUT MODERN_UI_PREFERENCES  *Preferences
  );

/**
  Save ModernSetupApp app-owned preferences to NVRAM.

  Preferences are validated and normalized before writing. Only the app-owned
  ModernSetupAppPreferences variable is written.

  @param[in] Preferences  Preferences to persist. Must not be NULL.

  @retval EFI_SUCCESS            Preferences were saved.
  @retval EFI_INVALID_PARAMETER  Preferences is NULL or structurally invalid.
  @retval others                 SetVariable status.
**/
EFI_STATUS
EFIAPI
ModernUiPreferencesSave (
  IN CONST MODERN_UI_PREFERENCES  *Preferences
  );

#endif
