/** @file
  View-only HII/IFR bridge model for ModernSetupPkg.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_HII_BRIDGE_H_
#define MODERN_UI_HII_BRIDGE_H_

#include <Uefi.h>
#include <Protocol/HiiDatabase.h>
#include <Uefi/UefiInternalFormRepresentation.h>

#define MODERN_UI_HII_MAX_FORMSETS     3
#define MODERN_UI_HII_MAX_PAGES        8
#define MODERN_UI_HII_MAX_ITEMS        48
#define MODERN_UI_HII_MAX_OPTIONS      8
#define MODERN_UI_HII_INLINE_TEXT_MAX  96

/** Text reference used by the bridge view model. */
typedef enum {
  ModernUiTextRefNone = 0,
  ModernUiTextRefAppString,
  ModernUiTextRefHiiString,
  ModernUiTextRefInline
} MODERN_UI_TEXT_REF_KIND;

typedef struct {
  MODERN_UI_TEXT_REF_KIND  Kind;
  EFI_HII_HANDLE           HiiHandle;
  EFI_STRING_ID            StringId;
  CONST CHAR16             *AppString;
  CHAR16                   Inline[MODERN_UI_HII_INLINE_TEXT_MAX];
} MODERN_UI_TEXT_REF;

/** Opaque source coordinates for native FormBrowser fallback and diagnostics. */
typedef struct {
  EFI_HII_HANDLE   HiiHandle;
  EFI_GUID         FormSetGuid;
  EFI_FORM_ID      FormId;
  EFI_QUESTION_ID  QuestionId;
  UINT8            IfrOpCode;
} MODERN_UI_SETUP_SOURCE_REF;

typedef enum {
  ModernUiDisplayNone = 0,
  ModernUiDisplayText,
  ModernUiDisplaySubtitle,
  ModernUiDisplayLink,
  ModernUiDisplayToggle,
  ModernUiDisplayChoice,
  ModernUiDisplayNumeric,
  ModernUiDisplayString,
  ModernUiDisplayPassword,
  ModernUiDisplayAction,
  ModernUiDisplayDate,
  ModernUiDisplayTime,
  ModernUiDisplayNativeOnly,
  ModernUiDisplayUnsupported
} MODERN_UI_DISPLAY_KIND;

typedef enum {
  ModernUiEditNone = 0,
  ModernUiEditReadOnly,
  ModernUiEditNavigate,
  ModernUiEditToggle,
  ModernUiEditChoose,
  ModernUiEditInput,
  ModernUiEditActivate,
  ModernUiEditNativeOnly
} MODERN_UI_EDIT_POLICY;

typedef struct {
  MODERN_UI_DISPLAY_KIND  DisplayKind;
  MODERN_UI_EDIT_POLICY   EditPolicy;
  BOOLEAN                 VisibleByDefault;
  BOOLEAN                 EnabledByDefault;
  BOOLEAN                 ReadOnly;
  BOOLEAN                 RequiresNativeFallback;
  BOOLEAN                 NativeOnly;
  BOOLEAN                 Unsupported;
} MODERN_UI_DISPLAY_POLICY;

typedef struct {
  MODERN_UI_TEXT_REF  Text;
  UINT8               Flags;
  UINT8               ValueType;
  UINT64              Value;
} MODERN_UI_HII_OPTION;

typedef struct {
  MODERN_UI_SETUP_SOURCE_REF  Source;
  MODERN_UI_DISPLAY_POLICY    Policy;
  MODERN_UI_TEXT_REF          Title;
  MODERN_UI_TEXT_REF          Prompt;
  MODERN_UI_TEXT_REF          Help;
  MODERN_UI_TEXT_REF          ValueText;
  EFI_FORM_ID                 TargetFormId;
  EFI_QUESTION_ID             TargetQuestionId;
  EFI_GUID                    TargetFormSetGuid;
  EFI_STRING_ID               TargetDevicePathId;
  UINT8                       QuestionFlags;
  UINT8                       ControlFlags;
  UINT8                       NumericFlags;
  UINT64                      Minimum;
  UINT64                      Maximum;
  UINT64                      Step;
  UINTN                       OptionCount;
  MODERN_UI_HII_OPTION        Options[MODERN_UI_HII_MAX_OPTIONS];
} MODERN_UI_HII_ITEM;

typedef struct {
  MODERN_UI_SETUP_SOURCE_REF  Source;
  MODERN_UI_TEXT_REF          Title;
  UINTN                       ItemCount;
  MODERN_UI_HII_ITEM          Items[MODERN_UI_HII_MAX_ITEMS];
} MODERN_UI_HII_PAGE;

typedef struct {
  MODERN_UI_SETUP_SOURCE_REF  Source;
  MODERN_UI_TEXT_REF          Title;
  MODERN_UI_TEXT_REF          Help;
  UINTN                       PageCount;
  MODERN_UI_HII_PAGE          Pages[MODERN_UI_HII_MAX_PAGES];
} MODERN_UI_HII_FORMSET;

typedef struct {
  UINTN                  FormSetCount;
  MODERN_UI_HII_FORMSET  FormSets[MODERN_UI_HII_MAX_FORMSETS];
} MODERN_UI_HII_VIEW;

VOID
EFIAPI
ModernUiHiiBridgeClearView (
  OUT MODERN_UI_HII_VIEW  *View
  );

EFI_STATUS
EFIAPI
ModernUiHiiBridgeBuildView (
  OUT MODERN_UI_HII_VIEW  *View,
  IN  CONST EFI_GUID      *FormSetGuidList OPTIONAL,
  IN  UINTN               FormSetGuidCount
  );

EFI_STATUS
EFIAPI
ModernUiHiiBridgeResolveText (
  IN  CONST MODERN_UI_TEXT_REF  *TextRef,
  OUT CHAR16                    *Buffer,
  IN  UINTN                     BufferChars
  );

#endif
