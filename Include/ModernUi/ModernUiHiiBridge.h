/** @file
  HII/IFR bridge data model for ModernSetupPkg.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_HII_BRIDGE_H_
#define MODERN_UI_HII_BRIDGE_H_

#include <Uefi.h>
#include <Protocol/HiiConfigAccess.h>
#include <Uefi/UefiInternalFormRepresentation.h>

#define MODERN_UI_HII_MAX_FORMSETS  8
#define MODERN_UI_HII_MAX_FORMS     8
#define MODERN_UI_HII_MAX_ITEMS     80
#define MODERN_UI_HII_MAX_OPTIONS   10
#define MODERN_UI_HII_MAX_STORES    12

typedef enum {
  ModernUiHiiItemText = 0,
  ModernUiHiiItemSubtitle,
  ModernUiHiiItemRef,
  ModernUiHiiItemCheckbox,
  ModernUiHiiItemOneOf,
  ModernUiHiiItemNumeric,
  ModernUiHiiItemString,
  ModernUiHiiItemUnsupported
} MODERN_UI_HII_ITEM_TYPE;

typedef enum {
  ModernUiHiiStoreBuffer = 0,
  ModernUiHiiStoreEfi,
  ModernUiHiiStoreNameValue,
  ModernUiHiiStoreUnsupported
} MODERN_UI_HII_STORE_TYPE;

typedef struct {
  EFI_STRING_ID    PromptId;
  UINT8            ValueType;
  UINT64           Value;
} MODERN_UI_HII_OPTION;

typedef struct {
  MODERN_UI_HII_ITEM_TYPE    Type;
  EFI_STRING_ID              PromptId;
  EFI_STRING_ID              HelpId;
  EFI_QUESTION_ID            QuestionId;
  EFI_VARSTORE_ID            VarStoreId;
  UINT16                     VarOffset;
  UINT8                      QuestionFlags;
  UINT8                      NumericFlags;
  UINTN                      StorageWidth;
  UINT64                     Minimum;
  UINT64                     Maximum;
  UINT64                     Step;
  UINT64                     CurrentValue;
  EFI_FORM_ID                TargetFormId;
  EFI_GUID                   TargetFormSetGuid;
  UINTN                      OptionCount;
  MODERN_UI_HII_OPTION       Options[MODERN_UI_HII_MAX_OPTIONS];
  BOOLEAN                    HasValue;
  BOOLEAN                    ReadOnly;
  BOOLEAN                    Unsupported;
} MODERN_UI_HII_ITEM;

typedef struct {
  EFI_FORM_ID           FormId;
  EFI_STRING_ID         TitleId;
  UINTN                 ItemCount;
  MODERN_UI_HII_ITEM    Items[MODERN_UI_HII_MAX_ITEMS];
} MODERN_UI_HII_FORM;

typedef struct {
  MODERN_UI_HII_STORE_TYPE    Type;
  EFI_VARSTORE_ID             VarStoreId;
  EFI_GUID                    Guid;
  UINTN                       Size;
  CHAR16                      Name[64];
} MODERN_UI_HII_VARSTORE;

typedef struct {
  EFI_HII_HANDLE                    HiiHandle;
  EFI_HANDLE                        DriverHandle;
  EFI_HII_CONFIG_ACCESS_PROTOCOL    *ConfigAccess;
  EFI_GUID                          Guid;
  EFI_STRING_ID                     TitleId;
  EFI_STRING_ID                     HelpId;
  UINTN                             VarStoreCount;
  MODERN_UI_HII_VARSTORE            VarStores[MODERN_UI_HII_MAX_STORES];
  UINTN                             FormCount;
  MODERN_UI_HII_FORM                Forms[MODERN_UI_HII_MAX_FORMS];
} MODERN_UI_HII_FORMSET;

typedef struct {
  UINTN                     FormSetCount;
  MODERN_UI_HII_FORMSET     FormSets[MODERN_UI_HII_MAX_FORMSETS];
} MODERN_UI_HII_MODEL;

/**
  Enumerate the HII database and build a compact IFR model.

  @param[out] Model  Model storage to clear and populate. Must not be NULL.

  @retval EFI_SUCCESS            HII data was loaded. An empty model is valid.
  @retval EFI_INVALID_PARAMETER  Model is NULL.
  @retval EFI_NOT_FOUND          HII database protocol is unavailable.
  @retval EFI_OUT_OF_RESOURCES   A temporary package buffer allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiHiiBridgeLoad (
  OUT MODERN_UI_HII_MODEL  *Model
  );

/**
  Refresh readable question values through ConfigAccess.

  @param[in,out] Model  Populated HII model. Must not be NULL.

  @retval EFI_SUCCESS            Refresh completed. Individual unsupported
                                 questions may remain without values.
  @retval EFI_INVALID_PARAMETER  Model is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiHiiBridgeRefreshValues (
  IN OUT MODERN_UI_HII_MODEL  *Model
  );

/**
  Apply the next demo value for one supported question.

  Checkbox values are toggled, one-of values advance to the next option,
  and numeric values increment by step or one. String questions are rendered
  read-only in v1 until a text editor exists.

  @param[in,out] Model         Populated HII model. Must not be NULL.
  @param[in]     FormSetIndex  Zero-based formset index.
  @param[in]     FormIndex     Zero-based form index within the formset.
  @param[in]     ItemIndex     Zero-based item index within the form.

  @retval EFI_SUCCESS            Value was routed and model values refreshed.
  @retval EFI_INVALID_PARAMETER  Indices are invalid or Model is NULL.
  @retval EFI_ACCESS_DENIED      Item is read-only, callback-driven, or unsupported.
  @retval EFI_UNSUPPORTED        Item storage cannot be safely routed by v1.
  @retval others                 Status returned by ConfigAccess.RouteConfig().
**/
EFI_STATUS
EFIAPI
ModernUiHiiBridgeApplyNextValue (
  IN OUT MODERN_UI_HII_MODEL  *Model,
  IN     UINTN                FormSetIndex,
  IN     UINTN                FormIndex,
  IN     UINTN                ItemIndex
  );

#endif
