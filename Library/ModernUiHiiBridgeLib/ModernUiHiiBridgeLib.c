/** @file
  View-only HII/IFR bridge for ModernSetupPkg.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/HiiDatabase.h>
#include <Uefi/UefiInternalFormRepresentation.h>

#include <ModernUi/ModernUiHiiBridge.h>

STATIC
VOID
SetHiiTextRef (
  OUT MODERN_UI_TEXT_REF  *TextRef,
  IN  EFI_HII_HANDLE      HiiHandle,
  IN  EFI_STRING_ID       StringId
  )
{
  if (TextRef == NULL) {
    return;
  }

  ZeroMem (TextRef, sizeof (*TextRef));
  if (StringId == 0) {
    TextRef->Kind = ModernUiTextRefNone;
    return;
  }

  TextRef->Kind      = ModernUiTextRefHiiString;
  TextRef->HiiHandle = HiiHandle;
  TextRef->StringId  = StringId;
}

STATIC
BOOLEAN
IsFormSetGuidAllowed (
  IN CONST EFI_GUID  *Guid,
  IN CONST EFI_GUID  *FormSetGuidList OPTIONAL,
  IN UINTN           FormSetGuidCount
  )
{
  UINTN  Index;

  if (Guid == NULL) {
    return FALSE;
  }

  if ((FormSetGuidList == NULL) || (FormSetGuidCount == 0)) {
    return TRUE;
  }

  for (Index = 0; Index < FormSetGuidCount; Index++) {
    if (CompareGuid (Guid, &FormSetGuidList[Index])) {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
UINTN
NumericStorageWidth (
  IN UINT8  Flags
  )
{
  switch (Flags & EFI_IFR_NUMERIC_SIZE) {
    case EFI_IFR_NUMERIC_SIZE_2:
      return sizeof (UINT16);
    case EFI_IFR_NUMERIC_SIZE_4:
      return sizeof (UINT32);
    case EFI_IFR_NUMERIC_SIZE_8:
      return sizeof (UINT64);
    case EFI_IFR_NUMERIC_SIZE_1:
    default:
      return sizeof (UINT8);
  }
}

STATIC
UINT64
NumericMinimum (
  IN CONST MINMAXSTEP_DATA  *Data,
  IN UINT8                  Flags
  )
{
  switch (Flags & EFI_IFR_NUMERIC_SIZE) {
    case EFI_IFR_NUMERIC_SIZE_2:
      return Data->u16.MinValue;
    case EFI_IFR_NUMERIC_SIZE_4:
      return Data->u32.MinValue;
    case EFI_IFR_NUMERIC_SIZE_8:
      return Data->u64.MinValue;
    case EFI_IFR_NUMERIC_SIZE_1:
    default:
      return Data->u8.MinValue;
  }
}

STATIC
UINT64
NumericMaximum (
  IN CONST MINMAXSTEP_DATA  *Data,
  IN UINT8                  Flags
  )
{
  switch (Flags & EFI_IFR_NUMERIC_SIZE) {
    case EFI_IFR_NUMERIC_SIZE_2:
      return Data->u16.MaxValue;
    case EFI_IFR_NUMERIC_SIZE_4:
      return Data->u32.MaxValue;
    case EFI_IFR_NUMERIC_SIZE_8:
      return Data->u64.MaxValue;
    case EFI_IFR_NUMERIC_SIZE_1:
    default:
      return Data->u8.MaxValue;
  }
}

STATIC
UINT64
NumericStep (
  IN CONST MINMAXSTEP_DATA  *Data,
  IN UINT8                  Flags
  )
{
  switch (Flags & EFI_IFR_NUMERIC_SIZE) {
    case EFI_IFR_NUMERIC_SIZE_2:
      return Data->u16.Step;
    case EFI_IFR_NUMERIC_SIZE_4:
      return Data->u32.Step;
    case EFI_IFR_NUMERIC_SIZE_8:
      return Data->u64.Step;
    case EFI_IFR_NUMERIC_SIZE_1:
    default:
      return Data->u8.Step;
  }
}

STATIC
VOID
SetPolicy (
  OUT MODERN_UI_DISPLAY_POLICY  *Policy,
  IN  MODERN_UI_DISPLAY_KIND    DisplayKind,
  IN  MODERN_UI_EDIT_POLICY     EditPolicy,
  IN  BOOLEAN                   ReadOnly,
  IN  BOOLEAN                   NativeOnly,
  IN  BOOLEAN                   Unsupported
  )
{
  if (Policy == NULL) {
    return;
  }

  ZeroMem (Policy, sizeof (*Policy));
  Policy->DisplayKind            = DisplayKind;
  Policy->EditPolicy             = NativeOnly ? ModernUiEditNativeOnly : (ReadOnly ? ModernUiEditReadOnly : EditPolicy);
  Policy->VisibleByDefault       = TRUE;
  Policy->EnabledByDefault       = (BOOLEAN)(!ReadOnly && !NativeOnly && !Unsupported);
  Policy->ReadOnly               = ReadOnly;
  Policy->RequiresNativeFallback = (BOOLEAN)(NativeOnly || Unsupported);
  Policy->NativeOnly             = NativeOnly;
  Policy->Unsupported            = Unsupported;
}

STATIC
VOID
MarkItemNativeOnly (
  IN OUT MODERN_UI_HII_ITEM  *Item,
  IN     UINT8               OpCode
  )
{
  if (Item == NULL) {
    return;
  }

  Item->Source.IfrOpCode = OpCode;
  SetPolicy (&Item->Policy, ModernUiDisplayNativeOnly, ModernUiEditNativeOnly, TRUE, TRUE, FALSE);
}

STATIC
MODERN_UI_HII_ITEM *
AppendItem (
  IN OUT MODERN_UI_HII_PAGE     *Page,
  IN     EFI_HII_HANDLE         HiiHandle,
  IN     CONST EFI_GUID         *FormSetGuid,
  IN     EFI_FORM_ID            FormId,
  IN     EFI_QUESTION_ID        QuestionId,
  IN     UINT8                  OpCode,
  IN     MODERN_UI_DISPLAY_KIND DisplayKind,
  IN     MODERN_UI_EDIT_POLICY  EditPolicy,
  IN     BOOLEAN                ReadOnly
  )
{
  STATIC MODERN_UI_HII_ITEM  DiscardedItem;
  MODERN_UI_HII_ITEM         *Item;

  if ((Page == NULL) || (Page->ItemCount >= MODERN_UI_HII_MAX_ITEMS)) {
    ZeroMem (&DiscardedItem, sizeof (DiscardedItem));
    MarkItemNativeOnly (&DiscardedItem, OpCode);
    return &DiscardedItem;
  }

  Item = &Page->Items[Page->ItemCount++];
  ZeroMem (Item, sizeof (*Item));
  Item->Source.HiiHandle  = HiiHandle;
  Item->Source.FormId     = FormId;
  Item->Source.QuestionId = QuestionId;
  Item->Source.IfrOpCode  = OpCode;
  if (FormSetGuid != NULL) {
    CopyGuid (&Item->Source.FormSetGuid, FormSetGuid);
  }

  SetPolicy (&Item->Policy, DisplayKind, EditPolicy, ReadOnly, FALSE, FALSE);
  return Item;
}

STATIC
BOOLEAN
QuestionIsReadOnly (
  IN UINT8  QuestionFlags
  )
{
  return (BOOLEAN)((QuestionFlags & EFI_IFR_FLAG_READ_ONLY) != 0);
}

STATIC
BOOLEAN
QuestionRequiresNativeFallback (
  IN UINT8  QuestionFlags
  )
{
  return (BOOLEAN)((QuestionFlags & EFI_IFR_FLAG_CALLBACK) != 0);
}

STATIC
VOID
ApplyQuestionFallbackPolicy (
  IN OUT MODERN_UI_HII_ITEM  *Item,
  IN     UINT8               QuestionFlags
  )
{
  if ((Item == NULL) || !QuestionRequiresNativeFallback (QuestionFlags)) {
    return;
  }

  SetPolicy (
    &Item->Policy,
    Item->Policy.DisplayKind,
    ModernUiEditNativeOnly,
    QuestionIsReadOnly (QuestionFlags),
    TRUE,
    FALSE
    );
}

STATIC
VOID
ParseFormsPackage (
  IN OUT MODERN_UI_HII_VIEW  *View,
  IN     EFI_HII_HANDLE      HiiHandle,
  IN     CONST UINT8         *Package,
  IN     UINTN               PackageSize,
  IN     CONST EFI_GUID      *FormSetGuidList OPTIONAL,
  IN     UINTN               FormSetGuidCount
  )
{
  UINTN                   Offset;
  EFI_IFR_OP_HEADER       *Header;
  MODERN_UI_HII_FORMSET   *FormSet;
  MODERN_UI_HII_PAGE      *Page;
  MODERN_UI_HII_ITEM      *Item;
  MODERN_UI_HII_ITEM      *OptionOwner;
  EFI_GUID                CurrentFormSetGuid;
  EFI_FORM_ID             CurrentFormId;
  EFI_IFR_FORM_SET        *IfrFormSet;
  EFI_IFR_FORM            *IfrForm;
  EFI_IFR_REF             *IfrRef;
  EFI_IFR_CHECKBOX        *IfrCheckbox;
  EFI_IFR_ONE_OF          *IfrOneOf;
  EFI_IFR_NUMERIC         *IfrNumeric;
  EFI_IFR_STRING          *IfrString;
  EFI_IFR_PASSWORD        *IfrPassword;
  EFI_IFR_DATE            *IfrDate;
  EFI_IFR_ACTION          *IfrAction;
  EFI_IFR_ONE_OF_OPTION   *IfrOption;

  FormSet      = NULL;
  Page         = NULL;
  OptionOwner  = NULL;
  CurrentFormId = 0;
  ZeroMem (&CurrentFormSetGuid, sizeof (CurrentFormSetGuid));

  for (Offset = sizeof (EFI_HII_PACKAGE_HEADER); Offset + sizeof (EFI_IFR_OP_HEADER) <= PackageSize; Offset += Header->Length) {
    Header = (EFI_IFR_OP_HEADER *)(UINTN)(Package + Offset);
    if ((Header->Length == 0) || (Offset + Header->Length > PackageSize)) {
      break;
    }

    if (Header->OpCode == EFI_IFR_END_OP) {
      continue;
    }

    switch (Header->OpCode) {
      case EFI_IFR_FORM_SET_OP:
        if ((Header->Length >= sizeof (EFI_IFR_FORM_SET)) &&
            (View->FormSetCount < MODERN_UI_HII_MAX_FORMSETS))
        {
          IfrFormSet = (EFI_IFR_FORM_SET *)Header;
          if (!IsFormSetGuidAllowed (&IfrFormSet->Guid, FormSetGuidList, FormSetGuidCount)) {
            FormSet = NULL;
            Page    = NULL;
            break;
          }

          FormSet = &View->FormSets[View->FormSetCount++];
          ZeroMem (FormSet, sizeof (*FormSet));
          FormSet->Source.HiiHandle = HiiHandle;
          FormSet->Source.IfrOpCode = Header->OpCode;
          CopyGuid (&FormSet->Source.FormSetGuid, &IfrFormSet->Guid);
          CopyGuid (&CurrentFormSetGuid, &IfrFormSet->Guid);
          SetHiiTextRef (&FormSet->Title, HiiHandle, IfrFormSet->FormSetTitle);
          SetHiiTextRef (&FormSet->Help, HiiHandle, IfrFormSet->Help);
        } else {
          FormSet = NULL;
          Page    = NULL;
        }
        break;

      case EFI_IFR_FORM_OP:
        if ((FormSet != NULL) && (Header->Length >= sizeof (EFI_IFR_FORM)) &&
            (FormSet->PageCount < MODERN_UI_HII_MAX_PAGES))
        {
          IfrForm = (EFI_IFR_FORM *)Header;
          Page = &FormSet->Pages[FormSet->PageCount++];
          ZeroMem (Page, sizeof (*Page));
          CurrentFormId = IfrForm->FormId;
          Page->Source.HiiHandle = HiiHandle;
          Page->Source.FormId    = IfrForm->FormId;
          Page->Source.IfrOpCode = Header->OpCode;
          CopyGuid (&Page->Source.FormSetGuid, &CurrentFormSetGuid);
          SetHiiTextRef (&Page->Title, HiiHandle, IfrForm->FormTitle);
        }
        break;

      case EFI_IFR_SUBTITLE_OP:
        if ((Page != NULL) && (Header->Length >= sizeof (EFI_IFR_SUBTITLE))) {
          Item = AppendItem (Page, HiiHandle, &CurrentFormSetGuid, CurrentFormId, 0, Header->OpCode, ModernUiDisplaySubtitle, ModernUiEditNone, TRUE);
          SetHiiTextRef (&Item->Prompt, HiiHandle, ((EFI_IFR_SUBTITLE *)Header)->Statement.Prompt);
          SetHiiTextRef (&Item->Help, HiiHandle, ((EFI_IFR_SUBTITLE *)Header)->Statement.Help);
          Item->ControlFlags = ((EFI_IFR_SUBTITLE *)Header)->Flags;
        }
        break;

      case EFI_IFR_TEXT_OP:
        if ((Page != NULL) && (Header->Length >= sizeof (EFI_IFR_TEXT))) {
          Item = AppendItem (Page, HiiHandle, &CurrentFormSetGuid, CurrentFormId, 0, Header->OpCode, ModernUiDisplayText, ModernUiEditNone, TRUE);
          SetHiiTextRef (&Item->Prompt, HiiHandle, ((EFI_IFR_TEXT *)Header)->Statement.Prompt);
          SetHiiTextRef (&Item->Help, HiiHandle, ((EFI_IFR_TEXT *)Header)->Statement.Help);
          SetHiiTextRef (&Item->ValueText, HiiHandle, ((EFI_IFR_TEXT *)Header)->TextTwo);
        }
        break;

      case EFI_IFR_REF_OP:
        if ((Page != NULL) && (Header->Length >= sizeof (EFI_IFR_REF))) {
          IfrRef = (EFI_IFR_REF *)Header;
          Item = AppendItem (Page, HiiHandle, &CurrentFormSetGuid, CurrentFormId, IfrRef->Question.QuestionId, Header->OpCode, ModernUiDisplayLink, ModernUiEditNavigate, QuestionIsReadOnly (IfrRef->Question.Flags));
          SetHiiTextRef (&Item->Prompt, HiiHandle, IfrRef->Question.Header.Prompt);
          SetHiiTextRef (&Item->Help, HiiHandle, IfrRef->Question.Header.Help);
          Item->QuestionFlags = IfrRef->Question.Flags;
          ApplyQuestionFallbackPolicy (Item, Item->QuestionFlags);
          Item->TargetFormId  = IfrRef->FormId;
          CopyGuid (&Item->TargetFormSetGuid, &CurrentFormSetGuid);
          if (Header->Length >= sizeof (EFI_IFR_REF2)) {
            Item->TargetQuestionId = ((EFI_IFR_REF2 *)Header)->QuestionId;
          }
          if (Header->Length >= sizeof (EFI_IFR_REF3)) {
            CopyGuid (&Item->TargetFormSetGuid, &((EFI_IFR_REF3 *)Header)->FormSetId);
          }
          if (Header->Length >= sizeof (EFI_IFR_REF4)) {
            Item->TargetDevicePathId = ((EFI_IFR_REF4 *)Header)->DevicePath;
          }
        }
        break;

      case EFI_IFR_CHECKBOX_OP:
        if ((Page != NULL) && (Header->Length >= sizeof (EFI_IFR_CHECKBOX))) {
          IfrCheckbox = (EFI_IFR_CHECKBOX *)Header;
          Item = AppendItem (Page, HiiHandle, &CurrentFormSetGuid, CurrentFormId, IfrCheckbox->Question.QuestionId, Header->OpCode, ModernUiDisplayToggle, ModernUiEditToggle, QuestionIsReadOnly (IfrCheckbox->Question.Flags));
          SetHiiTextRef (&Item->Prompt, HiiHandle, IfrCheckbox->Question.Header.Prompt);
          SetHiiTextRef (&Item->Help, HiiHandle, IfrCheckbox->Question.Header.Help);
          Item->QuestionFlags = IfrCheckbox->Question.Flags;
          ApplyQuestionFallbackPolicy (Item, Item->QuestionFlags);
          Item->ControlFlags  = IfrCheckbox->Flags;
        }
        break;

      case EFI_IFR_ONE_OF_OP:
        if ((Page != NULL) && (Header->Length >= sizeof (EFI_IFR_ONE_OF))) {
          IfrOneOf = (EFI_IFR_ONE_OF *)Header;
          Item = AppendItem (Page, HiiHandle, &CurrentFormSetGuid, CurrentFormId, IfrOneOf->Question.QuestionId, Header->OpCode, ModernUiDisplayChoice, ModernUiEditChoose, QuestionIsReadOnly (IfrOneOf->Question.Flags));
          SetHiiTextRef (&Item->Prompt, HiiHandle, IfrOneOf->Question.Header.Prompt);
          SetHiiTextRef (&Item->Help, HiiHandle, IfrOneOf->Question.Header.Help);
          Item->QuestionFlags = IfrOneOf->Question.Flags;
          ApplyQuestionFallbackPolicy (Item, Item->QuestionFlags);
          Item->NumericFlags  = IfrOneOf->Flags;
          Item->Minimum       = NumericMinimum (&IfrOneOf->data, IfrOneOf->Flags);
          Item->Maximum       = NumericMaximum (&IfrOneOf->data, IfrOneOf->Flags);
          Item->Step          = NumericStep (&IfrOneOf->data, IfrOneOf->Flags);
          OptionOwner = Item;
        }
        break;

      case EFI_IFR_ONE_OF_OPTION_OP:
        if ((OptionOwner != NULL) && (OptionOwner->OptionCount < MODERN_UI_HII_MAX_OPTIONS) && (Header->Length >= sizeof (EFI_IFR_ONE_OF_OPTION))) {
          IfrOption = (EFI_IFR_ONE_OF_OPTION *)Header;
          SetHiiTextRef (&OptionOwner->Options[OptionOwner->OptionCount].Text, HiiHandle, IfrOption->Option);
          OptionOwner->Options[OptionOwner->OptionCount].Flags     = IfrOption->Flags;
          OptionOwner->Options[OptionOwner->OptionCount].ValueType = IfrOption->Type;
          OptionOwner->Options[OptionOwner->OptionCount].Value     = IfrOption->Value.u64;
          OptionOwner->OptionCount++;
        }
        break;

      case EFI_IFR_NUMERIC_OP:
        if ((Page != NULL) && (Header->Length >= sizeof (EFI_IFR_NUMERIC))) {
          IfrNumeric = (EFI_IFR_NUMERIC *)Header;
          Item = AppendItem (Page, HiiHandle, &CurrentFormSetGuid, CurrentFormId, IfrNumeric->Question.QuestionId, Header->OpCode, ModernUiDisplayNumeric, ModernUiEditInput, QuestionIsReadOnly (IfrNumeric->Question.Flags));
          SetHiiTextRef (&Item->Prompt, HiiHandle, IfrNumeric->Question.Header.Prompt);
          SetHiiTextRef (&Item->Help, HiiHandle, IfrNumeric->Question.Header.Help);
          Item->QuestionFlags = IfrNumeric->Question.Flags;
          ApplyQuestionFallbackPolicy (Item, Item->QuestionFlags);
          Item->NumericFlags  = IfrNumeric->Flags;
          Item->Minimum       = NumericMinimum (&IfrNumeric->data, IfrNumeric->Flags);
          Item->Maximum       = NumericMaximum (&IfrNumeric->data, IfrNumeric->Flags);
          Item->Step          = NumericStep (&IfrNumeric->data, IfrNumeric->Flags);
          (VOID)NumericStorageWidth (IfrNumeric->Flags);
        }
        break;

      case EFI_IFR_STRING_OP:
        if ((Page != NULL) && (Header->Length >= sizeof (EFI_IFR_STRING))) {
          IfrString = (EFI_IFR_STRING *)Header;
          Item = AppendItem (Page, HiiHandle, &CurrentFormSetGuid, CurrentFormId, IfrString->Question.QuestionId, Header->OpCode, ModernUiDisplayString, ModernUiEditInput, TRUE);
          SetHiiTextRef (&Item->Prompt, HiiHandle, IfrString->Question.Header.Prompt);
          SetHiiTextRef (&Item->Help, HiiHandle, IfrString->Question.Header.Help);
          Item->QuestionFlags = IfrString->Question.Flags;
          ApplyQuestionFallbackPolicy (Item, Item->QuestionFlags);
        }
        break;

      case EFI_IFR_PASSWORD_OP:
        if ((Page != NULL) && (Header->Length >= sizeof (EFI_IFR_PASSWORD))) {
          IfrPassword = (EFI_IFR_PASSWORD *)Header;
          Item = AppendItem (Page, HiiHandle, &CurrentFormSetGuid, CurrentFormId, IfrPassword->Question.QuestionId, Header->OpCode, ModernUiDisplayPassword, ModernUiEditInput, TRUE);
          SetHiiTextRef (&Item->Prompt, HiiHandle, IfrPassword->Question.Header.Prompt);
          SetHiiTextRef (&Item->Help, HiiHandle, IfrPassword->Question.Header.Help);
          Item->QuestionFlags = IfrPassword->Question.Flags;
          ApplyQuestionFallbackPolicy (Item, Item->QuestionFlags);
        }
        break;

      case EFI_IFR_DATE_OP:
      case EFI_IFR_TIME_OP:
        if ((Page != NULL) && (Header->Length >= sizeof (EFI_IFR_DATE))) {
          IfrDate = (EFI_IFR_DATE *)Header;
          Item = AppendItem (Page, HiiHandle, &CurrentFormSetGuid, CurrentFormId, IfrDate->Question.QuestionId, Header->OpCode, (Header->OpCode == EFI_IFR_DATE_OP) ? ModernUiDisplayDate : ModernUiDisplayTime, ModernUiEditInput, TRUE);
          SetHiiTextRef (&Item->Prompt, HiiHandle, IfrDate->Question.Header.Prompt);
          SetHiiTextRef (&Item->Help, HiiHandle, IfrDate->Question.Header.Help);
          Item->QuestionFlags = IfrDate->Question.Flags;
          ApplyQuestionFallbackPolicy (Item, Item->QuestionFlags);
          Item->ControlFlags  = IfrDate->Flags;
        }
        break;

      case EFI_IFR_ACTION_OP:
        if ((Page != NULL) && (Header->Length >= sizeof (EFI_IFR_ACTION))) {
          IfrAction = (EFI_IFR_ACTION *)Header;
          Item = AppendItem (Page, HiiHandle, &CurrentFormSetGuid, CurrentFormId, IfrAction->Question.QuestionId, Header->OpCode, ModernUiDisplayAction, ModernUiEditActivate, QuestionIsReadOnly (IfrAction->Question.Flags));
          SetHiiTextRef (&Item->Prompt, HiiHandle, IfrAction->Question.Header.Prompt);
          SetHiiTextRef (&Item->Help, HiiHandle, IfrAction->Question.Header.Help);
          Item->QuestionFlags = IfrAction->Question.Flags;
          ApplyQuestionFallbackPolicy (Item, Item->QuestionFlags);
        }
        break;

      case EFI_IFR_RESET_BUTTON_OP:
        if ((Page != NULL) && (Header->Length >= sizeof (EFI_IFR_RESET_BUTTON))) {
          Item = AppendItem (Page, HiiHandle, &CurrentFormSetGuid, CurrentFormId, 0, Header->OpCode, ModernUiDisplayNativeOnly, ModernUiEditNativeOnly, TRUE);
          SetHiiTextRef (&Item->Prompt, HiiHandle, ((EFI_IFR_RESET_BUTTON *)Header)->Statement.Prompt);
          SetHiiTextRef (&Item->Help, HiiHandle, ((EFI_IFR_RESET_BUTTON *)Header)->Statement.Help);
          Item->Policy.NativeOnly             = TRUE;
          Item->Policy.RequiresNativeFallback = TRUE;
        }
        break;

      default:
        if ((Page != NULL) && (Header->Scope != 0) &&
            ((Header->OpCode == EFI_IFR_ORDERED_LIST_OP) || (Header->OpCode == EFI_IFR_GUID_OP)))
        {
          Item = AppendItem (Page, HiiHandle, &CurrentFormSetGuid, CurrentFormId, 0, Header->OpCode, ModernUiDisplayNativeOnly, ModernUiEditNativeOnly, TRUE);
          MarkItemNativeOnly (Item, Header->OpCode);
        }
        break;
    }
  }
}

STATIC
EFI_STATUS
LoadOneHiiHandle (
  IN OUT MODERN_UI_HII_VIEW       *View,
  IN     EFI_HII_DATABASE_PROTOCOL *HiiDatabase,
  IN     EFI_HII_HANDLE           HiiHandle,
  IN     CONST EFI_GUID           *FormSetGuidList OPTIONAL,
  IN     UINTN                    FormSetGuidCount
  )
{
  EFI_STATUS                   Status;
  UINTN                        PackageListSize;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;
  UINTN                        Offset;
  EFI_HII_PACKAGE_HEADER       PackageHeader;

  PackageListSize = 0;
  Status = HiiDatabase->ExportPackageLists (HiiDatabase, HiiHandle, &PackageListSize, NULL);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return EFI_SUCCESS;
  }

  PackageList = AllocateZeroPool (PackageListSize);
  if (PackageList == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = HiiDatabase->ExportPackageLists (HiiDatabase, HiiHandle, &PackageListSize, PackageList);
  if (EFI_ERROR (Status)) {
    FreePool (PackageList);
    return Status;
  }

  Offset = sizeof (EFI_HII_PACKAGE_LIST_HEADER);
  while (Offset + sizeof (EFI_HII_PACKAGE_HEADER) <= PackageList->PackageLength) {
    CopyMem (&PackageHeader, (UINT8 *)PackageList + Offset, sizeof (PackageHeader));
    if ((PackageHeader.Length < sizeof (EFI_HII_PACKAGE_HEADER)) || ((Offset + PackageHeader.Length) > PackageList->PackageLength)) {
      break;
    }

    if (PackageHeader.Type == EFI_HII_PACKAGE_FORMS) {
      ParseFormsPackage (
        View,
        HiiHandle,
        (UINT8 *)PackageList + Offset,
        PackageHeader.Length,
        FormSetGuidList,
        FormSetGuidCount
        );
    }

    Offset += PackageHeader.Length;
  }

  FreePool (PackageList);
  return EFI_SUCCESS;
}

VOID
EFIAPI
ModernUiHiiBridgeClearView (
  OUT MODERN_UI_HII_VIEW  *View
  )
{
  if (View != NULL) {
    ZeroMem (View, sizeof (*View));
  }
}

EFI_STATUS
EFIAPI
ModernUiHiiBridgeBuildView (
  OUT MODERN_UI_HII_VIEW  *View,
  IN  CONST EFI_GUID      *FormSetGuidList OPTIONAL,
  IN  UINTN               FormSetGuidCount
  )
{
  EFI_STATUS                 Status;
  EFI_HII_DATABASE_PROTOCOL  *HiiDatabase;
  EFI_HII_HANDLE             *HiiHandles;
  UINTN                      Index;

  if (View == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((FormSetGuidList == NULL) && (FormSetGuidCount != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  ModernUiHiiBridgeClearView (View);

  Status = gBS->LocateProtocol (&gEfiHiiDatabaseProtocolGuid, NULL, (VOID **)&HiiDatabase);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  HiiHandles = HiiGetHiiHandles (NULL);
  if (HiiHandles == NULL) {
    return EFI_SUCCESS;
  }

  for (Index = 0; (HiiHandles[Index] != NULL) && (View->FormSetCount < MODERN_UI_HII_MAX_FORMSETS); Index++) {
    Status = LoadOneHiiHandle (View, HiiDatabase, HiiHandles[Index], FormSetGuidList, FormSetGuidCount);
    if (EFI_ERROR (Status)) {
      FreePool (HiiHandles);
      return Status;
    }
  }

  FreePool (HiiHandles);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ModernUiHiiBridgeResolveText (
  IN  CONST MODERN_UI_TEXT_REF  *TextRef,
  OUT CHAR16                    *Buffer,
  IN  UINTN                     BufferChars
  )
{
  EFI_STRING  HiiString;

  if ((TextRef == NULL) || (Buffer == NULL) || (BufferChars == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Buffer[0] = L'\0';

  switch (TextRef->Kind) {
    case ModernUiTextRefNone:
      return EFI_NOT_FOUND;

    case ModernUiTextRefInline:
      StrnCpyS (Buffer, BufferChars, TextRef->Inline, BufferChars - 1);
      return EFI_SUCCESS;

    case ModernUiTextRefAppString:
      if (TextRef->AppString == NULL) {
        return EFI_NOT_FOUND;
      }
      StrnCpyS (Buffer, BufferChars, TextRef->AppString, BufferChars - 1);
      return EFI_SUCCESS;

    case ModernUiTextRefHiiString:
      if ((TextRef->HiiHandle == NULL) || (TextRef->StringId == 0)) {
        return EFI_NOT_FOUND;
      }
      HiiString = HiiGetString (TextRef->HiiHandle, TextRef->StringId, NULL);
      if (HiiString == NULL) {
        return EFI_NOT_FOUND;
      }
      StrnCpyS (Buffer, BufferChars, HiiString, BufferChars - 1);
      FreePool (HiiString);
      return EFI_SUCCESS;

    default:
      return EFI_UNSUPPORTED;
  }
}
