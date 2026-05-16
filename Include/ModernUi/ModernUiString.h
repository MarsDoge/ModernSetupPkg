/** @file
  Localized string identifiers for ModernSetupPkg.

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
  ModernUiStringPageHii,
  ModernUiStringPageHiiHint,
  ModernUiStringPageExit,
  ModernUiStringPageExitHint,
  ModernUiStringFooterNav,
  ModernUiStringFooterContent,
  ModernUiStringFirmwareVendor,
  ModernUiStringFirmwareRevision,
  ModernUiStringDisplay,
  ModernUiStringBootOptions,
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
  ModernUiStringSecurityReadOnly,
  ModernUiStringExitInstruction,
  ModernUiStringExitContinue,
  ModernUiStringExitClassicUi,
  ModernUiStringExitReset,
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
  ModernUiStringMax
} MODERN_UI_STRING_ID;

/**
  Return the active language tag.

  The returned pointer is owned by the platform PCD database and must not be
  freed or modified by the caller.

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

#endif
