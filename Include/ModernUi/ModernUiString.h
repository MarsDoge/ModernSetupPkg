/** @file
  Localized string identifiers for ModernSetupPkg.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_STRING_H_
#define MODERN_UI_STRING_H_

#include <Uefi.h>

typedef enum {
  ModernUiStringHeaderTitle = 0,
  ModernUiStringHeaderMode,
  ModernUiStringPageDashboard,
  ModernUiStringPageDashboardHint,
  ModernUiStringPageBoot,
  ModernUiStringPageBootHint,
  ModernUiStringPageDevices,
  ModernUiStringPageDevicesHint,
  ModernUiStringPageSecurity,
  ModernUiStringPageSecurityHint,
  ModernUiStringPageFirmware,
  ModernUiStringPageFirmwareHint,
  ModernUiStringPageDiagnostics,
  ModernUiStringPageDiagnosticsHint,
  ModernUiStringPageManagement,
  ModernUiStringPageManagementHint,
  ModernUiStringPagePower,
  ModernUiStringPagePowerHint,
  ModernUiStringPagePerformance,
  ModernUiStringPagePerformanceHint,
  ModernUiStringPageHii,
  ModernUiStringPageHiiHint,
  ModernUiStringPageExit,
  ModernUiStringPageExitHint,
  ModernUiStringFooterNav,
  ModernUiStringFooterContent,
  ModernUiStringFirmwareVendor,
  ModernUiStringFirmwareRevision,
  ModernUiStringFormFactor,
  ModernUiStringBootMode,
  ModernUiStringDisplay,
  ModernUiStringBootOptions,
  ModernUiStringBootCategory,
  ModernUiStringBootPath,
  ModernUiStringBootCountFormat,
  ModernUiStringPrototypeStatus,
  ModernUiStringPrototypeStatusValue,
  ModernUiStringBootInstruction,
  ModernUiStringNoBootOptions,
  ModernUiStringNoDescription,
  ModernUiStringActive,
  ModernUiStringInactive,
  ModernUiStringUnableEnumerateHandles,
  ModernUiStringHandleCountFormat,
  ModernUiStringSecureBoot,
  ModernUiStringEnabled,
  ModernUiStringDisabled,
  ModernUiStringAvailable,
  ModernUiStringNotAvailable,
  ModernUiStringPresent,
  ModernUiStringAbsent,
  ModernUiStringUnknown,
  ModernUiStringFirmwareUpdate,
  ModernUiStringCapsuleRuntime,
  ModernUiStringCapsuleProtocol,
  ModernUiStringCapsuleReport,
  ModernUiStringDiagnosticsLogs,
  ModernUiStringAcpiTables,
  ModernUiStringSmbiosTables,
  ModernUiStringMemoryMap,
  ModernUiStringDxeHandles,
  ModernUiStringConfigurationTables,
  ModernUiStringManagement,
  ModernUiStringIpmi,
  ModernUiStringRedfish,
  ModernUiStringManagementInterface,
  ModernUiStringPowerThermal,
  ModernUiStringAcpiTablesProvider,
  ModernUiStringAcpiSdtProtocol,
  ModernUiStringChassisThermalState,
  ModernUiStringPowerSupply,
  ModernUiStringPerformanceTuning,
  ModernUiStringProcessorInventory,
  ModernUiStringMemoryInventory,
  ModernUiStringCpuIo2,
  ModernUiStringVirtualizationPolicy,
  ModernUiStringRasPolicy,
  ModernUiStringTcg2,
  ModernUiStringTree,
  ModernUiStringSecurityReadOnly,
  ModernUiStringExitInstruction,
  ModernUiStringExitContinue,
  ModernUiStringExitClassicUi,
  ModernUiStringExitReset,
  ModernUiStringExitLanguageFormat,
  ModernUiStringLanguageLabel,
  ModernUiStringLanguageChinese,
  ModernUiStringLanguageEnglish,
  ModernUiStringLanguageChangedFormat,
  ModernUiStringHiiNoFormsets,
  ModernUiStringHiiFormsets,
  ModernUiStringHiiForms,
  ModernUiStringHiiItems,
  ModernUiStringHiiUnsupported,
  ModernUiStringHiiReadOnly,
  ModernUiStringHiiEnterForm,
  ModernUiStringHiiRouteReturnedFormat,
  ModernUiStringBootReturnedFormat,
  ModernUiStringClassicReturnedFormat,
  ModernUiStringGraphicsInitFailedFormat,
  ModernUiStringGroupBootDevices,
  ModernUiStringGroupPlatformHealth,
  ModernUiStringGroupPowerPerformance,
  ModernUiStringGroupFirmware,
  ModernUiStringGroupDiagnostics,
  ModernUiStringGroupManagement,
  ModernUiStringGroupPower,
  ModernUiStringGroupPerformance,
  ModernUiStringMax
} MODERN_UI_STRING_ID;

/**
  Return the active language tag.

  Runtime variable ModernSetupLanguage is preferred when present. The fixed PCD
  language is used as the fallback.

  @return Non-NULL ASCII language tag. The default is "zh-Hans".
**/
CONST CHAR8 *
EFIAPI
ModernUiGetLanguage (
  VOID
  );

/**
  Return one localized string for the active language.

  If the active language is unknown or the localized string is absent, English
  text is returned as the fallback.

  @param[in] Id  String identifier to resolve.

  @return Non-NULL UCS-2 string owned by this library.
**/
CONST CHAR16 *
EFIAPI
ModernUiGetString (
  IN MODERN_UI_STRING_ID  Id
  );

/**
  Set the active ModernSetup language.

  Supported language families are "zh" and "en". Other language tags are
  rejected so callers do not persist an unsupported UI state.

  @param[in] Language  ASCII language tag. Must not be NULL.
  @param[in] Persist   TRUE writes the language to non-volatile variables.

  @retval EFI_SUCCESS            Active language was changed.
  @retval EFI_INVALID_PARAMETER  Language is NULL or unsupported.
  @retval others                 Variable write failed after the in-memory
                                 language was changed.
**/
EFI_STATUS
EFIAPI
ModernUiSetLanguage (
  IN CONST CHAR8  *Language,
  IN BOOLEAN      Persist
  );

#endif
