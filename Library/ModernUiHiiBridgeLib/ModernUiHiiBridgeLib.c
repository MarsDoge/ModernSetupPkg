/** @file
  Runtime HII/IFR bridge for ModernSetupPkg.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/DriverSampleHii.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/HiiConfigAccess.h>
#include <Protocol/HiiConfigRouting.h>
#include <Protocol/HiiDatabase.h>
#include <Uefi/UefiInternalFormRepresentation.h>

#include <ModernUi/ModernUiHiiBridge.h>

STATIC EFI_GUID  mDriverSampleFormSetGuid  = DRIVER_SAMPLE_FORMSET_GUID;
STATIC EFI_GUID  mDriverSampleInventoryGuid = DRIVER_SAMPLE_INVENTORY_GUID;

/**
  Return TRUE when a formset GUID belongs to the DriverSample demo.

  @param[in] Guid  Formset GUID to test. Must not be NULL.

  @retval TRUE   GUID is a DriverSample demo formset.
  @retval FALSE  GUID is another formset.
**/
STATIC
BOOLEAN
IsDriverSampleFormSet (
  IN CONST EFI_GUID  *Guid
  )
{
  return (BOOLEAN)(CompareGuid (Guid, &mDriverSampleFormSetGuid) || CompareGuid (Guid, &mDriverSampleInventoryGuid));
}

/**
  Convert a VFR ASCII storage name into a CHAR16 string.

  @param[out] Destination      Output buffer. Must not be NULL.
  @param[in]  DestinationSize  Number of CHAR16 elements in Destination.
  @param[in]  Source           ASCII source bytes. Must not be NULL.
  @param[in]  SourceSize       Maximum byte count available at Source.
**/
STATIC
VOID
CopyAsciiNameToUnicode (
  OUT CHAR16      *Destination,
  IN  UINTN       DestinationSize,
  IN  CONST UINT8 *Source,
  IN  UINTN       SourceSize
  )
{
  UINTN  Index;

  if ((Destination == NULL) || (DestinationSize == 0)) {
    return;
  }

  Destination[0] = L'\0';
  if (Source == NULL) {
    return;
  }

  for (Index = 0; (Index < SourceSize) && (Index < DestinationSize - 1) && (Source[Index] != '\0'); Index++) {
    Destination[Index] = (CHAR16)Source[Index];
  }

  Destination[Index] = L'\0';
}

/**
  Return a byte width for an IFR numeric or oneof flags field.

  @param[in] Flags  IFR flags containing EFI_IFR_NUMERIC_SIZE bits.

  @return Storage width in bytes. Unknown values default to one byte.
**/
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

/**
  Return the minimum value encoded in an IFR numeric payload.

  @param[in] Data   IFR min/max/step union. Must not be NULL.
  @param[in] Flags  IFR flags containing EFI_IFR_NUMERIC_SIZE bits.

  @return Minimum value extended to UINT64.
**/
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

/**
  Return the maximum value encoded in an IFR numeric payload.

  @param[in] Data   IFR min/max/step union. Must not be NULL.
  @param[in] Flags  IFR flags containing EFI_IFR_NUMERIC_SIZE bits.

  @return Maximum value extended to UINT64.
**/
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

/**
  Return the step value encoded in an IFR numeric payload.

  @param[in] Data   IFR min/max/step union. Must not be NULL.
  @param[in] Flags  IFR flags containing EFI_IFR_NUMERIC_SIZE bits.

  @return Step value extended to UINT64. Zero means manual input in IFR.
**/
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

/**
  Find a varstore by ID in a formset.

  @param[in] FormSet     Formset to inspect. Must not be NULL.
  @param[in] VarStoreId  IFR varstore identifier.

  @return Pointer to varstore metadata, or NULL when absent.
**/
STATIC
MODERN_UI_HII_VARSTORE *
FindVarStore (
  IN MODERN_UI_HII_FORMSET  *FormSet,
  IN EFI_VARSTORE_ID        VarStoreId
  )
{
  UINTN  Index;

  if (FormSet == NULL) {
    return NULL;
  }

  for (Index = 0; Index < FormSet->VarStoreCount; Index++) {
    if (FormSet->VarStores[Index].VarStoreId == VarStoreId) {
      return &FormSet->VarStores[Index];
    }
  }

  return NULL;
}

/**
  Add a new item to a parsed form.

  @param[in,out] Form  Form to append to. Must not be NULL.
  @param[in]     Type  Modern UI item type.

  @return Pointer to the new item, or NULL when the form is full.
**/
STATIC
MODERN_UI_HII_ITEM *
AppendItem (
  IN OUT MODERN_UI_HII_FORM      *Form,
  IN     MODERN_UI_HII_ITEM_TYPE Type
  )
{
  MODERN_UI_HII_ITEM  *Item;

  if ((Form == NULL) || (Form->ItemCount >= MODERN_UI_HII_MAX_ITEMS)) {
    return NULL;
  }

  Item = &Form->Items[Form->ItemCount++];
  ZeroMem (Item, sizeof (*Item));
  Item->Type = Type;
  return Item;
}

/**
  Parse a DriverSample forms package into the model.

  @param[in,out] Model         Model to append to. Must not be NULL.
  @param[in]     HiiHandle     HII package-list handle.
  @param[in]     DriverHandle  Driver handle associated with HiiHandle.
  @param[in]     ConfigAccess  Optional ConfigAccess protocol for routing.
  @param[in]     Package       Forms package bytes. Must not be NULL.
  @param[in]     PackageSize   Forms package size in bytes.
**/
STATIC
VOID
ParseFormsPackage (
  IN OUT MODERN_UI_HII_MODEL                  *Model,
  IN     EFI_HII_HANDLE                       HiiHandle,
  IN     EFI_HANDLE                           DriverHandle,
  IN     EFI_HII_CONFIG_ACCESS_PROTOCOL       *ConfigAccess,
  IN     CONST UINT8                          *Package,
  IN     UINTN                                PackageSize
  )
{
  UINTN                       Offset;
  EFI_IFR_OP_HEADER           *Header;
  MODERN_UI_HII_FORMSET       *FormSet;
  MODERN_UI_HII_FORM          *Form;
  MODERN_UI_HII_ITEM          *Item;
  MODERN_UI_HII_ITEM          *OptionOwner;
  MODERN_UI_HII_VARSTORE      *Store;
  UINT8                       ScopeStack[32];
  UINTN                       ScopeDepth;
  UINT8                       Popped;
  UINTN                       NameSize;

  FormSet     = NULL;
  Form        = NULL;
  OptionOwner = NULL;
  ScopeDepth  = 0;

  for (Offset = sizeof (EFI_HII_PACKAGE_HEADER); Offset + sizeof (EFI_IFR_OP_HEADER) <= PackageSize; Offset += Header->Length) {
    Header = (EFI_IFR_OP_HEADER *)(UINTN)(Package + Offset);
    if ((Header->Length == 0) || (Offset + Header->Length > PackageSize)) {
      break;
    }

    if (Header->OpCode == EFI_IFR_END_OP) {
      if (ScopeDepth > 0) {
        Popped = ScopeStack[--ScopeDepth];
        if (Popped == EFI_IFR_FORM_OP) {
          Form = NULL;
        } else if (Popped == EFI_IFR_ONE_OF_OP) {
          OptionOwner = NULL;
        }
      }

      continue;
    }

    switch (Header->OpCode) {
      case EFI_IFR_FORM_SET_OP:
        if ((Header->Length >= sizeof (EFI_IFR_FORM_SET)) &&
            IsDriverSampleFormSet (&((EFI_IFR_FORM_SET *)Header)->Guid) &&
            (Model->FormSetCount < MODERN_UI_HII_MAX_FORMSETS))
        {
          FormSet = &Model->FormSets[Model->FormSetCount++];
          ZeroMem (FormSet, sizeof (*FormSet));
          FormSet->HiiHandle    = HiiHandle;
          FormSet->DriverHandle = DriverHandle;
          FormSet->ConfigAccess = ConfigAccess;
          CopyGuid (&FormSet->Guid, &((EFI_IFR_FORM_SET *)Header)->Guid);
          FormSet->TitleId = ((EFI_IFR_FORM_SET *)Header)->FormSetTitle;
          FormSet->HelpId  = ((EFI_IFR_FORM_SET *)Header)->Help;
        }
        break;
      case EFI_IFR_VARSTORE_OP:
        if ((FormSet != NULL) && (FormSet->VarStoreCount < MODERN_UI_HII_MAX_STORES) && (Header->Length >= sizeof (EFI_IFR_VARSTORE))) {
          Store             = &FormSet->VarStores[FormSet->VarStoreCount++];
          Store->Type       = ModernUiHiiStoreBuffer;
          Store->VarStoreId = ((EFI_IFR_VARSTORE *)Header)->VarStoreId;
          Store->Size       = ((EFI_IFR_VARSTORE *)Header)->Size;
          CopyGuid (&Store->Guid, &((EFI_IFR_VARSTORE *)Header)->Guid);
          NameSize = Header->Length - OFFSET_OF (EFI_IFR_VARSTORE, Name);
          CopyAsciiNameToUnicode (Store->Name, ARRAY_SIZE (Store->Name), ((EFI_IFR_VARSTORE *)Header)->Name, NameSize);
        }
        break;
      case EFI_IFR_VARSTORE_EFI_OP:
        if ((FormSet != NULL) && (FormSet->VarStoreCount < MODERN_UI_HII_MAX_STORES) && (Header->Length >= sizeof (EFI_IFR_VARSTORE_EFI))) {
          Store             = &FormSet->VarStores[FormSet->VarStoreCount++];
          Store->Type       = ModernUiHiiStoreEfi;
          Store->VarStoreId = ((EFI_IFR_VARSTORE_EFI *)Header)->VarStoreId;
          Store->Size       = ((EFI_IFR_VARSTORE_EFI *)Header)->Size;
          CopyGuid (&Store->Guid, &((EFI_IFR_VARSTORE_EFI *)Header)->Guid);
          NameSize = Header->Length - OFFSET_OF (EFI_IFR_VARSTORE_EFI, Name);
          CopyAsciiNameToUnicode (Store->Name, ARRAY_SIZE (Store->Name), ((EFI_IFR_VARSTORE_EFI *)Header)->Name, NameSize);
        }
        break;
      case EFI_IFR_VARSTORE_NAME_VALUE_OP:
        if ((FormSet != NULL) && (FormSet->VarStoreCount < MODERN_UI_HII_MAX_STORES) && (Header->Length >= sizeof (EFI_IFR_VARSTORE_NAME_VALUE))) {
          Store             = &FormSet->VarStores[FormSet->VarStoreCount++];
          Store->Type       = ModernUiHiiStoreNameValue;
          Store->VarStoreId = ((EFI_IFR_VARSTORE_NAME_VALUE *)Header)->VarStoreId;
          Store->Size       = 0;
          CopyGuid (&Store->Guid, &((EFI_IFR_VARSTORE_NAME_VALUE *)Header)->Guid);
        }
        break;
      case EFI_IFR_FORM_OP:
        if ((FormSet != NULL) && (FormSet->FormCount < MODERN_UI_HII_MAX_FORMS) && (Header->Length >= sizeof (EFI_IFR_FORM))) {
          Form = &FormSet->Forms[FormSet->FormCount++];
          ZeroMem (Form, sizeof (*Form));
          Form->FormId  = ((EFI_IFR_FORM *)Header)->FormId;
          Form->TitleId = ((EFI_IFR_FORM *)Header)->FormTitle;
        }
        break;
      case EFI_IFR_SUBTITLE_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_SUBTITLE))) {
          Item           = AppendItem (Form, ModernUiHiiItemSubtitle);
          Item->PromptId = ((EFI_IFR_SUBTITLE *)Header)->Statement.Prompt;
          Item->HelpId   = ((EFI_IFR_SUBTITLE *)Header)->Statement.Help;
        }
        break;
      case EFI_IFR_TEXT_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_TEXT))) {
          Item           = AppendItem (Form, ModernUiHiiItemText);
          Item->PromptId = ((EFI_IFR_TEXT *)Header)->Statement.Prompt;
          Item->HelpId   = ((EFI_IFR_TEXT *)Header)->Statement.Help;
        }
        break;
      case EFI_IFR_REF_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_REF))) {
          Item               = AppendItem (Form, ModernUiHiiItemRef);
          Item->PromptId     = ((EFI_IFR_REF *)Header)->Question.Header.Prompt;
          Item->HelpId       = ((EFI_IFR_REF *)Header)->Question.Header.Help;
          Item->QuestionId   = ((EFI_IFR_REF *)Header)->Question.QuestionId;
          Item->QuestionFlags = ((EFI_IFR_REF *)Header)->Question.Flags;
          Item->TargetFormId = ((EFI_IFR_REF *)Header)->FormId;
          CopyGuid (&Item->TargetFormSetGuid, &FormSet->Guid);
        }
        break;
      case EFI_IFR_CHECKBOX_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_CHECKBOX))) {
          Item                = AppendItem (Form, ModernUiHiiItemCheckbox);
          Item->PromptId      = ((EFI_IFR_CHECKBOX *)Header)->Question.Header.Prompt;
          Item->HelpId        = ((EFI_IFR_CHECKBOX *)Header)->Question.Header.Help;
          Item->QuestionId    = ((EFI_IFR_CHECKBOX *)Header)->Question.QuestionId;
          Item->VarStoreId    = ((EFI_IFR_CHECKBOX *)Header)->Question.VarStoreId;
          Item->VarOffset     = ((EFI_IFR_CHECKBOX *)Header)->Question.VarStoreInfo.VarOffset;
          Item->QuestionFlags = ((EFI_IFR_CHECKBOX *)Header)->Question.Flags;
          Item->StorageWidth  = sizeof (UINT8);
          Item->ReadOnly      = (BOOLEAN)((Item->QuestionFlags & EFI_IFR_FLAG_READ_ONLY) != 0);
        }
        break;
      case EFI_IFR_ONE_OF_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_ONE_OF))) {
          Item                = AppendItem (Form, ModernUiHiiItemOneOf);
          Item->PromptId      = ((EFI_IFR_ONE_OF *)Header)->Question.Header.Prompt;
          Item->HelpId        = ((EFI_IFR_ONE_OF *)Header)->Question.Header.Help;
          Item->QuestionId    = ((EFI_IFR_ONE_OF *)Header)->Question.QuestionId;
          Item->VarStoreId    = ((EFI_IFR_ONE_OF *)Header)->Question.VarStoreId;
          Item->VarOffset     = ((EFI_IFR_ONE_OF *)Header)->Question.VarStoreInfo.VarOffset;
          Item->QuestionFlags = ((EFI_IFR_ONE_OF *)Header)->Question.Flags;
          Item->NumericFlags  = ((EFI_IFR_ONE_OF *)Header)->Flags;
          Item->StorageWidth  = NumericStorageWidth (Item->NumericFlags);
          Item->Minimum       = NumericMinimum (&((EFI_IFR_ONE_OF *)Header)->data, Item->NumericFlags);
          Item->Maximum       = NumericMaximum (&((EFI_IFR_ONE_OF *)Header)->data, Item->NumericFlags);
          Item->Step          = NumericStep (&((EFI_IFR_ONE_OF *)Header)->data, Item->NumericFlags);
          Item->ReadOnly      = (BOOLEAN)((Item->QuestionFlags & EFI_IFR_FLAG_READ_ONLY) != 0);
          OptionOwner         = Item;
        }
        break;
      case EFI_IFR_ONE_OF_OPTION_OP:
        if ((OptionOwner != NULL) && (OptionOwner->OptionCount < MODERN_UI_HII_MAX_OPTIONS) && (Header->Length >= sizeof (EFI_IFR_ONE_OF_OPTION))) {
          OptionOwner->Options[OptionOwner->OptionCount].PromptId  = ((EFI_IFR_ONE_OF_OPTION *)Header)->Option;
          OptionOwner->Options[OptionOwner->OptionCount].ValueType = ((EFI_IFR_ONE_OF_OPTION *)Header)->Type;
          OptionOwner->Options[OptionOwner->OptionCount].Value     = ((EFI_IFR_ONE_OF_OPTION *)Header)->Value.u64;
          OptionOwner->OptionCount++;
        }
        break;
      case EFI_IFR_NUMERIC_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_NUMERIC))) {
          Item                = AppendItem (Form, ModernUiHiiItemNumeric);
          Item->PromptId      = ((EFI_IFR_NUMERIC *)Header)->Question.Header.Prompt;
          Item->HelpId        = ((EFI_IFR_NUMERIC *)Header)->Question.Header.Help;
          Item->QuestionId    = ((EFI_IFR_NUMERIC *)Header)->Question.QuestionId;
          Item->VarStoreId    = ((EFI_IFR_NUMERIC *)Header)->Question.VarStoreId;
          Item->VarOffset     = ((EFI_IFR_NUMERIC *)Header)->Question.VarStoreInfo.VarOffset;
          Item->QuestionFlags = ((EFI_IFR_NUMERIC *)Header)->Question.Flags;
          Item->NumericFlags  = ((EFI_IFR_NUMERIC *)Header)->Flags;
          Item->StorageWidth  = NumericStorageWidth (Item->NumericFlags);
          Item->Minimum       = NumericMinimum (&((EFI_IFR_NUMERIC *)Header)->data, Item->NumericFlags);
          Item->Maximum       = NumericMaximum (&((EFI_IFR_NUMERIC *)Header)->data, Item->NumericFlags);
          Item->Step          = NumericStep (&((EFI_IFR_NUMERIC *)Header)->data, Item->NumericFlags);
          Item->ReadOnly      = (BOOLEAN)((Item->QuestionFlags & EFI_IFR_FLAG_READ_ONLY) != 0);
        }
        break;
      case EFI_IFR_STRING_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_STRING))) {
          Item                = AppendItem (Form, ModernUiHiiItemString);
          Item->PromptId      = ((EFI_IFR_STRING *)Header)->Question.Header.Prompt;
          Item->HelpId        = ((EFI_IFR_STRING *)Header)->Question.Header.Help;
          Item->QuestionId    = ((EFI_IFR_STRING *)Header)->Question.QuestionId;
          Item->VarStoreId    = ((EFI_IFR_STRING *)Header)->Question.VarStoreId;
          Item->VarOffset     = ((EFI_IFR_STRING *)Header)->Question.VarStoreInfo.VarOffset;
          Item->QuestionFlags = ((EFI_IFR_STRING *)Header)->Question.Flags;
          Item->StorageWidth  = ((EFI_IFR_STRING *)Header)->MaxSize * sizeof (CHAR16);
          Item->ReadOnly      = (BOOLEAN)((Item->QuestionFlags & EFI_IFR_FLAG_READ_ONLY) != 0);
        }
        break;
      case EFI_IFR_ORDERED_LIST_OP:
      case EFI_IFR_DATE_OP:
      case EFI_IFR_TIME_OP:
      case EFI_IFR_ACTION_OP:
      case EFI_IFR_RESET_BUTTON_OP:
        if (Form != NULL) {
          Item              = AppendItem (Form, ModernUiHiiItemUnsupported);
          Item->Unsupported = TRUE;
          if (Header->Length >= sizeof (EFI_IFR_QUESTION_HEADER)) {
            Item->PromptId = ((EFI_IFR_ACTION *)Header)->Question.Header.Prompt;
            Item->HelpId   = ((EFI_IFR_ACTION *)Header)->Question.Header.Help;
          }
        }
        break;
      default:
        break;
    }

    if ((Header->Scope != 0) && (ScopeDepth < ARRAY_SIZE (ScopeStack))) {
      ScopeStack[ScopeDepth++] = Header->OpCode;
    }
  }
}

/**
  Export one HII package list and parse its forms packages.

  @param[in,out] Model        Model to append to. Must not be NULL.
  @param[in]     HiiDatabase  HII database protocol. Must not be NULL.
  @param[in]     HiiHandle    HII handle to export.

  @retval EFI_SUCCESS           Package was inspected.
  @retval EFI_OUT_OF_RESOURCES  Export buffer allocation failed.
  @retval others                Status from ExportPackageLists().
**/
STATIC
EFI_STATUS
LoadOneHiiHandle (
  IN OUT MODERN_UI_HII_MODEL        *Model,
  IN     EFI_HII_DATABASE_PROTOCOL  *HiiDatabase,
  IN     EFI_HII_HANDLE             HiiHandle
  )
{
  EFI_STATUS                      Status;
  UINTN                           PackageListSize;
  EFI_HII_PACKAGE_LIST_HEADER     *PackageList;
  UINTN                           Offset;
  EFI_HII_PACKAGE_HEADER          PackageHeader;
  EFI_HANDLE                      DriverHandle;
  EFI_HII_CONFIG_ACCESS_PROTOCOL  *ConfigAccess;

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

  DriverHandle = NULL;
  ConfigAccess = NULL;
  Status = HiiDatabase->GetPackageListHandle (HiiDatabase, HiiHandle, &DriverHandle);
  if (!EFI_ERROR (Status) && (DriverHandle != NULL)) {
    Status = gBS->HandleProtocol (
                    DriverHandle,
                    &gEfiHiiConfigAccessProtocolGuid,
                    (VOID **)&ConfigAccess
                    );
    if (EFI_ERROR (Status)) {
      ConfigAccess = NULL;
    }
  }

  Offset = sizeof (EFI_HII_PACKAGE_LIST_HEADER);
  while (Offset + sizeof (EFI_HII_PACKAGE_HEADER) <= PackageList->PackageLength) {
    CopyMem (&PackageHeader, (UINT8 *)PackageList + Offset, sizeof (PackageHeader));
    if ((PackageHeader.Length < sizeof (EFI_HII_PACKAGE_HEADER)) || ((Offset + PackageHeader.Length) > PackageList->PackageLength)) {
      break;
    }

    if (PackageHeader.Type == EFI_HII_PACKAGE_FORMS) {
      ParseFormsPackage (
        Model,
        HiiHandle,
        DriverHandle,
        ConfigAccess,
        (UINT8 *)PackageList + Offset,
        PackageHeader.Length
        );
    }

    Offset += PackageHeader.Length;
  }

  FreePool (PackageList);
  return EFI_SUCCESS;
}

/**
  Parse a VALUE hex byte stream into an integer.

  @param[in]  ValueText  Hex bytes in config-response order. Must not be NULL.
  @param[in]  Width      Number of bytes to parse.
  @param[out] Value      Parsed little-endian integer. Must not be NULL.

  @retval EFI_SUCCESS            Value was parsed.
  @retval EFI_INVALID_PARAMETER  A parameter is invalid.
**/
STATIC
EFI_STATUS
ParseConfigValue (
  IN  CONST CHAR16  *ValueText,
  IN  UINTN         Width,
  OUT UINT64        *Value
  )
{
  UINTN   Index;
  CHAR16  ByteText[3];
  UINT8   Byte;

  if ((ValueText == NULL) || (Value == NULL) || (Width == 0) || (Width > sizeof (UINT64))) {
    return EFI_INVALID_PARAMETER;
  }

  *Value      = 0;
  ByteText[2] = L'\0';
  for (Index = 0; Index < Width; Index++) {
    ByteText[0] = ValueText[Index * 2];
    ByteText[1] = ValueText[Index * 2 + 1];
    Byte        = (UINT8)StrHexToUint64 (ByteText);
    *Value     |= LShiftU64 ((UINT64)Byte, (UINTN)(Index * 8));
  }

  return EFI_SUCCESS;
}

/**
  Append one little-endian integer as config-response hex bytes.

  @param[out] Buffer      Destination string. Must not be NULL.
  @param[in]  BufferSize  Size of Buffer in bytes.
  @param[in]  Value       Integer value to encode.
  @param[in]  Width       Number of bytes to emit.
**/
STATIC
VOID
AppendValueHex (
  OUT CHAR16  *Buffer,
  IN  UINTN   BufferSize,
  IN  UINT64  Value,
  IN  UINTN   Width
  )
{
  UINTN   Index;
  CHAR16  ByteText[3];

  for (Index = 0; Index < Width; Index++) {
    UnicodeSPrint (ByteText, sizeof (ByteText), L"%02x", (UINT8)RShiftU64 (Value, (UINTN)(Index * 8)));
    StrCatS (Buffer, BufferSize / sizeof (CHAR16), ByteText);
  }
}

/**
  Read one numeric-sized item value through ConfigAccess.ExtractConfig().

  @param[in]     FormSet  Formset that owns the item. Must not be NULL.
  @param[in,out] Item     Item whose CurrentValue is updated. Must not be NULL.

  @retval EFI_SUCCESS       Value was read.
  @retval EFI_UNSUPPORTED   The item is not backed by buffer varstore routing.
  @retval others            Status from ExtractConfig().
**/
STATIC
EFI_STATUS
ReadItemValue (
  IN     MODERN_UI_HII_FORMSET  *FormSet,
  IN OUT MODERN_UI_HII_ITEM     *Item
  )
{
  MODERN_UI_HII_VARSTORE  *Store;
  CHAR16                  *ConfigHdr;
  CHAR16                  Request[256];
  EFI_STRING              Results;
  EFI_STRING              Progress;
  CHAR16                  *ValueText;
  EFI_STATUS              Status;

  if ((FormSet == NULL) || (Item == NULL) || (FormSet->ConfigAccess == NULL) || (Item->StorageWidth == 0)) {
    return EFI_UNSUPPORTED;
  }

  Store = FindVarStore (FormSet, Item->VarStoreId);
  if ((Store == NULL) || (Store->Type != ModernUiHiiStoreBuffer)) {
    Item->Unsupported = TRUE;
    return EFI_UNSUPPORTED;
  }

  ConfigHdr = HiiConstructConfigHdr (&Store->Guid, Store->Name, FormSet->DriverHandle);
  if (ConfigHdr == NULL) {
    return EFI_UNSUPPORTED;
  }

  UnicodeSPrint (Request, sizeof (Request), L"%s&OFFSET=%x&WIDTH=%016lx", ConfigHdr, Item->VarOffset, (UINT64)Item->StorageWidth);
  FreePool (ConfigHdr);

  Results  = NULL;
  Progress = NULL;
  Status = FormSet->ConfigAccess->ExtractConfig (FormSet->ConfigAccess, Request, &Progress, &Results);
  if (EFI_ERROR (Status) || (Results == NULL)) {
    return Status;
  }

  ValueText = StrStr (Results, L"VALUE=");
  if (ValueText == NULL) {
    FreePool (Results);
    return EFI_NOT_FOUND;
  }

  ValueText += StrLen (L"VALUE=");
  Status = ParseConfigValue (ValueText, MIN (Item->StorageWidth, sizeof (UINT64)), &Item->CurrentValue);
  if (!EFI_ERROR (Status)) {
    Item->HasValue = TRUE;
  }

  FreePool (Results);
  return Status;
}

/**
  Route one numeric-sized item value through ConfigAccess.RouteConfig().

  @param[in] FormSet  Formset that owns the item. Must not be NULL.
  @param[in] Item     Item whose varstore location is routed. Must not be NULL.
  @param[in] Value    New integer value.

  @retval EFI_SUCCESS       Value was routed.
  @retval EFI_UNSUPPORTED   The item is not backed by buffer varstore routing.
  @retval others            Status from RouteConfig().
**/
STATIC
EFI_STATUS
WriteItemValue (
  IN MODERN_UI_HII_FORMSET  *FormSet,
  IN MODERN_UI_HII_ITEM     *Item,
  IN UINT64                 Value
  )
{
  MODERN_UI_HII_VARSTORE  *Store;
  CHAR16                  *ConfigHdr;
  CHAR16                  ConfigResp[320];
  EFI_STRING              Progress;
  EFI_STATUS              Status;

  if ((FormSet == NULL) || (Item == NULL) || (FormSet->ConfigAccess == NULL) || (Item->StorageWidth == 0)) {
    return EFI_UNSUPPORTED;
  }

  Store = FindVarStore (FormSet, Item->VarStoreId);
  if ((Store == NULL) || (Store->Type != ModernUiHiiStoreBuffer)) {
    return EFI_UNSUPPORTED;
  }

  ConfigHdr = HiiConstructConfigHdr (&Store->Guid, Store->Name, FormSet->DriverHandle);
  if (ConfigHdr == NULL) {
    return EFI_UNSUPPORTED;
  }

  UnicodeSPrint (ConfigResp, sizeof (ConfigResp), L"%s&OFFSET=%x&WIDTH=%016lx&VALUE=", ConfigHdr, Item->VarOffset, (UINT64)Item->StorageWidth);
  FreePool (ConfigHdr);
  AppendValueHex (ConfigResp, sizeof (ConfigResp), Value, MIN (Item->StorageWidth, sizeof (UINT64)));

  Progress = NULL;
  Status = FormSet->ConfigAccess->RouteConfig (FormSet->ConfigAccess, ConfigResp, &Progress);
  return Status;
}

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
  )
{
  EFI_STATUS                 Status;
  EFI_HII_DATABASE_PROTOCOL  *HiiDatabase;
  EFI_HII_HANDLE             *HiiHandles;
  UINTN                      Index;

  if (Model == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Model, sizeof (*Model));

  Status = gBS->LocateProtocol (&gEfiHiiDatabaseProtocolGuid, NULL, (VOID **)&HiiDatabase);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  HiiHandles = HiiGetHiiHandles (NULL);
  if (HiiHandles == NULL) {
    return EFI_SUCCESS;
  }

  for (Index = 0; (HiiHandles[Index] != NULL) && (Model->FormSetCount < MODERN_UI_HII_MAX_FORMSETS); Index++) {
    Status = LoadOneHiiHandle (Model, HiiDatabase, HiiHandles[Index]);
    if (EFI_ERROR (Status)) {
      FreePool (HiiHandles);
      return Status;
    }
  }

  FreePool (HiiHandles);
  return ModernUiHiiBridgeRefreshValues (Model);
}

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
  )
{
  UINTN                   FormSetIndex;
  UINTN                   FormIndex;
  UINTN                   ItemIndex;
  MODERN_UI_HII_FORMSET   *FormSet;
  MODERN_UI_HII_FORM      *Form;
  MODERN_UI_HII_ITEM      *Item;

  if (Model == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  for (FormSetIndex = 0; FormSetIndex < Model->FormSetCount; FormSetIndex++) {
    FormSet = &Model->FormSets[FormSetIndex];
    for (FormIndex = 0; FormIndex < FormSet->FormCount; FormIndex++) {
      Form = &FormSet->Forms[FormIndex];
      for (ItemIndex = 0; ItemIndex < Form->ItemCount; ItemIndex++) {
        Item = &Form->Items[ItemIndex];
        if ((Item->Type == ModernUiHiiItemCheckbox) || (Item->Type == ModernUiHiiItemOneOf) || (Item->Type == ModernUiHiiItemNumeric)) {
          ReadItemValue (FormSet, Item);
        }
      }
    }
  }

  return EFI_SUCCESS;
}

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
  )
{
  MODERN_UI_HII_FORMSET  *FormSet;
  MODERN_UI_HII_FORM     *Form;
  MODERN_UI_HII_ITEM     *Item;
  UINT64                 NewValue;
  UINTN                  Index;
  EFI_STATUS             Status;

  if ((Model == NULL) || (FormSetIndex >= Model->FormSetCount)) {
    return EFI_INVALID_PARAMETER;
  }

  FormSet = &Model->FormSets[FormSetIndex];
  if (FormIndex >= FormSet->FormCount) {
    return EFI_INVALID_PARAMETER;
  }

  Form = &FormSet->Forms[FormIndex];
  if (ItemIndex >= Form->ItemCount) {
    return EFI_INVALID_PARAMETER;
  }

  Item = &Form->Items[ItemIndex];
  if (Item->Unsupported || Item->ReadOnly || ((Item->QuestionFlags & EFI_IFR_FLAG_CALLBACK) != 0)) {
    return EFI_ACCESS_DENIED;
  }

  switch (Item->Type) {
    case ModernUiHiiItemCheckbox:
      NewValue = (Item->CurrentValue == 0) ? 1 : 0;
      break;
    case ModernUiHiiItemOneOf:
      if (Item->OptionCount == 0) {
        return EFI_UNSUPPORTED;
      }

      NewValue = Item->Options[0].Value;
      for (Index = 0; Index < Item->OptionCount; Index++) {
        if (Item->Options[Index].Value == Item->CurrentValue) {
          NewValue = Item->Options[(Index + 1) % Item->OptionCount].Value;
          break;
        }
      }
      break;
    case ModernUiHiiItemNumeric:
      NewValue = Item->CurrentValue + ((Item->Step == 0) ? 1 : Item->Step);
      if (NewValue > Item->Maximum) {
        NewValue = Item->Minimum;
      }
      break;
    default:
      return EFI_UNSUPPORTED;
  }

  Status = WriteItemValue (FormSet, Item, NewValue);
  if (!EFI_ERROR (Status)) {
    ModernUiHiiBridgeRefreshValues (Model);
  }

  return Status;
}
