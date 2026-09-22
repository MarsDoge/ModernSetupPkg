/** @file
  Immutable catalog contract. Metadata does not grant runtime write access.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef MODERN_SETUP_SETTING_H_
#define MODERN_SETUP_SETTING_H_

#include <Uefi.h>

#define MODERN_SETUP_CATALOG_VERSION  1

typedef enum {
  ModernSettingUnbound,
  ModernSettingUnsupported,
  ModernSettingNotReported,
  ModernSettingReadError,
  ModernSettingAvailable
} MODERN_SETTING_STATUS;

typedef struct {
  CONST CHAR8  *Id;
  CONST CHAR8  *Category;
  CONST CHAR8  *TopNav;
  CONST CHAR8  *LabelEn;
  CONST CHAR8  *LabelZh;
  CONST CHAR8  *HelpEn;
  CONST CHAR8  *HelpZh;
  CONST CHAR8  *Control;
  CONST CHAR8  *ValueType;
  CONST CHAR8  *AccessIntent;
  CONST CHAR8  *BindingKind;
  CONST CHAR8  *IntendedOwner;
  CONST CHAR8  *Apply;
  CONST CHAR8  *Risk;
  BOOLEAN      Sensitive;
  // Complete versioned descriptor, UTF-8 JSON. Not a variable write payload.
  CONST CHAR8  *DescriptorJson;
} MODERN_SETUP_SETTING_DESCRIPTOR;

typedef struct {
  BOOLEAN  Visible;
  BOOLEAN  ShowNa;
  BOOLEAN  CanEdit;
  BOOLEAN  CanInvoke;
  BOOLEAN  CanSubmit;
} MODERN_SETUP_SETTING_PRESENTATION;

// Provider values remain separate from metadata. Zero and Disabled are values,
// never absence markers. Native permissions/suppression always take precedence.
STATIC inline MODERN_SETUP_SETTING_PRESENTATION
ModernSetupSettingPresentation (
  IN MODERN_SETTING_STATUS  Status,
  IN BOOLEAN                NativeSuppressed,
  IN BOOLEAN                BackendAllowsWrite,
  IN BOOLEAN                IsAction,
  IN BOOLEAN                IsStaged
  )
{
  MODERN_SETUP_SETTING_PRESENTATION  Result;
  BOOLEAN                           Available;

  Available        = (BOOLEAN)(Status == ModernSettingAvailable);
  Result.Visible   = (BOOLEAN)!NativeSuppressed;
  Result.ShowNa    = (BOOLEAN)(Result.Visible && !Available);
  Result.CanEdit   = (BOOLEAN)(Result.Visible && Available && BackendAllowsWrite && !IsAction);
  Result.CanInvoke = (BOOLEAN)(Result.Visible && Available && BackendAllowsWrite && IsAction);
  Result.CanSubmit = (BOOLEAN)(Result.CanEdit && IsStaged);
  return Result;
}

#endif
