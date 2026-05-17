/** @file
  Security state provider for ModernSetupApp.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_SECURITY_DATA_H_
#define MODERN_UI_SECURITY_DATA_H_

#include <Uefi.h>

typedef enum {
  ModernUiSecurityStateUnknown = 0,
  ModernUiSecurityStateAbsent,
  ModernUiSecurityStatePresent,
  ModernUiSecurityStateEnabled,
  ModernUiSecurityStateDisabled
} MODERN_UI_SECURITY_STATE;

typedef struct {
  MODERN_UI_SECURITY_STATE    SecureBoot;
  MODERN_UI_SECURITY_STATE    SetupMode;
  MODERN_UI_SECURITY_STATE    PlatformKey;
  MODERN_UI_SECURITY_STATE    KeyExchangeKey;
  MODERN_UI_SECURITY_STATE    SignatureDb;
  MODERN_UI_SECURITY_STATE    ForbiddenSignatureDb;
} MODERN_UI_SECURITY_SUMMARY;

/**
  Collect read-only Secure Boot and key database state.

  @param[out] Summary  Security summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiSecurityDataGetSummary (
  OUT MODERN_UI_SECURITY_SUMMARY  *Summary
  );

#endif
