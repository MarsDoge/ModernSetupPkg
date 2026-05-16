/** @file
  Runtime HII/IFR bridge for ModernSetupPkg.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
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

typedef enum {
  ModernUiHiiConditionNone = 0,
  ModernUiHiiConditionSuppress,
  ModernUiHiiConditionGrayOut,
  ModernUiHiiConditionDisable,
  ModernUiHiiConditionValidation
} MODERN_UI_HII_CONDITION_TYPE;

typedef struct {
  UINT8                         OpCode;
  MODERN_UI_HII_ITEM            *QuestionOwner;
  MODERN_UI_HII_CONDITION_TYPE  ConditionType;
  MODERN_UI_HII_EXPR            Condition;
  BOOLEAN                       CollectingExpression;
} MODERN_UI_HII_SCOPE;

typedef struct {
  MODERN_UI_HII_EXPR  Suppress;
  MODERN_UI_HII_EXPR  GrayOut;
  MODERN_UI_HII_EXPR  Disable;
} MODERN_UI_HII_ACTIVE_CONDITIONS;

/**
  Return TRUE when a formset GUID is allowed by an optional filter list.

  @param[in] Guid              Formset GUID to test. Must not be NULL.
  @param[in] FormSetGuidList   Optional list of allowed formset GUIDs.
  @param[in] FormSetGuidCount  Number of entries in FormSetGuidList.

  @retval TRUE   The filter is empty or Guid is present in the filter.
  @retval FALSE  Guid is not present in the filter.
**/
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
  STATIC MODERN_UI_HII_ITEM  DiscardedItem;
  MODERN_UI_HII_ITEM         *Item;

  if ((Form == NULL) || (Form->ItemCount >= MODERN_UI_HII_MAX_ITEMS)) {
    ZeroMem (&DiscardedItem, sizeof (DiscardedItem));
    DiscardedItem.Type        = Type;
    DiscardedItem.Visible     = FALSE;
    DiscardedItem.Unsupported = TRUE;
    return &DiscardedItem;
  }

  Item = &Form->Items[Form->ItemCount++];
  ZeroMem (Item, sizeof (*Item));
  Item->Type    = Type;
  Item->Visible = TRUE;
  return Item;
}

/**
  Return TRUE when an opcode can appear inside a boolean IFR expression.

  @param[in] OpCode  IFR opcode to classify.

  @retval TRUE   Opcode is expression bytecode consumed by the bridge evaluator.
  @retval FALSE  Opcode starts form structure or unsupported non-expression data.
**/
STATIC
BOOLEAN
IsExpressionOp (
  IN UINT8  OpCode
  )
{
  switch (OpCode) {
    case EFI_IFR_EQ_ID_VAL_OP:
    case EFI_IFR_EQ_ID_ID_OP:
    case EFI_IFR_EQ_ID_VAL_LIST_OP:
    case EFI_IFR_AND_OP:
    case EFI_IFR_OR_OP:
    case EFI_IFR_NOT_OP:
    case EFI_IFR_EQUAL_OP:
    case EFI_IFR_NOT_EQUAL_OP:
    case EFI_IFR_GREATER_THAN_OP:
    case EFI_IFR_GREATER_EQUAL_OP:
    case EFI_IFR_LESS_THAN_OP:
    case EFI_IFR_LESS_EQUAL_OP:
    case EFI_IFR_BITWISE_AND_OP:
    case EFI_IFR_SHIFT_RIGHT_OP:
    case EFI_IFR_ADD_OP:
    case EFI_IFR_QUESTION_REF1_OP:
    case EFI_IFR_UINT8_OP:
    case EFI_IFR_UINT16_OP:
    case EFI_IFR_UINT32_OP:
    case EFI_IFR_UINT64_OP:
    case EFI_IFR_TRUE_OP:
    case EFI_IFR_FALSE_OP:
    case EFI_IFR_ZERO_OP:
    case EFI_IFR_ONE_OP:
    case EFI_IFR_SECURITY_OP:
      return TRUE;
    default:
      return FALSE;
  }
}

/**
  Append one compact expression operation.

  @param[in,out] Expr    Expression to extend. Must not be NULL.
  @param[in]     Header  IFR opcode header. Must not be NULL.
**/
STATIC
VOID
AppendExpressionOp (
  IN OUT MODERN_UI_HII_EXPR  *Expr,
  IN     EFI_IFR_OP_HEADER   *Header
  )
{
  MODERN_UI_HII_EXPR_OP  *Op;
  UINTN                  Index;
  UINTN                  Count;
  EFI_IFR_EQ_ID_VAL_LIST *ListOp;

  if ((Expr == NULL) || (Header == NULL) || (Expr->OpCount >= MODERN_UI_HII_MAX_EXPR_OPS)) {
    if (Expr != NULL) {
      Expr->Unsupported = TRUE;
    }

    return;
  }

  Op = &Expr->Ops[Expr->OpCount++];
  ZeroMem (Op, sizeof (*Op));
  Op->OpCode = Header->OpCode;

  switch (Header->OpCode) {
    case EFI_IFR_EQ_ID_VAL_OP:
      Op->QuestionId = ((EFI_IFR_EQ_ID_VAL *)Header)->QuestionId;
      Op->Value      = ((EFI_IFR_EQ_ID_VAL *)Header)->Value;
      break;
    case EFI_IFR_EQ_ID_ID_OP:
      Op->QuestionId = ((EFI_IFR_EQ_ID_ID *)Header)->QuestionId1;
      Op->Value      = ((EFI_IFR_EQ_ID_ID *)Header)->QuestionId2;
      break;
    case EFI_IFR_EQ_ID_VAL_LIST_OP:
      ListOp         = (EFI_IFR_EQ_ID_VAL_LIST *)Header;
      Op->QuestionId = ListOp->QuestionId;
      Count          = MIN ((UINTN)ListOp->ListLength, ARRAY_SIZE (Op->ValueList));
      Op->ValueListCount = (UINT8)Count;
      for (Index = 0; Index < Count; Index++) {
        Op->ValueList[Index] = ListOp->ValueList[Index];
      }

      if ((UINTN)ListOp->ListLength > Count) {
        Expr->Unsupported = TRUE;
      }
      break;
    case EFI_IFR_QUESTION_REF1_OP:
      Op->QuestionId = ((EFI_IFR_QUESTION_REF1 *)Header)->QuestionId;
      break;
    case EFI_IFR_UINT8_OP:
      Op->Value = ((EFI_IFR_UINT8 *)Header)->Value;
      break;
    case EFI_IFR_UINT16_OP:
      Op->Value = ((EFI_IFR_UINT16 *)Header)->Value;
      break;
    case EFI_IFR_UINT32_OP:
      Op->Value = ((EFI_IFR_UINT32 *)Header)->Value;
      break;
    case EFI_IFR_UINT64_OP:
      Op->Value = ((EFI_IFR_UINT64 *)Header)->Value;
      break;
    case EFI_IFR_SECURITY_OP:
      Op->Value = 0;
      break;
    default:
      break;
  }
}

/**
  Merge one source expression into an active condition expression.

  Multiple enclosing scopes are treated as a logical OR for disable-style
  conditions because any active enclosing condition should affect the item.

  @param[in,out] Destination  Expression being built. Must not be NULL.
  @param[in]     Source       Expression to merge. Must not be NULL.
**/
STATIC
VOID
MergeConditionExpression (
  IN OUT MODERN_UI_HII_EXPR        *Destination,
  IN     CONST MODERN_UI_HII_EXPR  *Source
  )
{
  if ((Destination == NULL) || (Source == NULL) || (Source->OpCount == 0)) {
    return;
  }

  if (Destination->OpCount == 0) {
    CopyMem (Destination, Source, sizeof (*Destination));
    return;
  }

  if ((Destination->OpCount + Source->OpCount + 1) > MODERN_UI_HII_MAX_EXPR_OPS) {
    Destination->Unsupported = TRUE;
    return;
  }

  CopyMem (
    &Destination->Ops[Destination->OpCount],
    Source->Ops,
    Source->OpCount * sizeof (Source->Ops[0])
    );
  Destination->OpCount += Source->OpCount;
  Destination->Ops[Destination->OpCount++].OpCode = EFI_IFR_OR_OP;
  Destination->Unsupported = (BOOLEAN)(Destination->Unsupported || Source->Unsupported);
}

/**
  Copy the active condition set into a newly parsed item.

  @param[in,out] Item        Parsed item to annotate. Must not be NULL.
  @param[in]     Conditions  Active enclosing conditions. Must not be NULL.
**/
STATIC
VOID
ApplyActiveConditions (
  IN OUT MODERN_UI_HII_ITEM               *Item,
  IN     CONST MODERN_UI_HII_ACTIVE_CONDITIONS  *Conditions
  )
{
  if ((Item == NULL) || (Conditions == NULL)) {
    return;
  }

  CopyMem (&Item->SuppressExpr, &Conditions->Suppress, sizeof (Item->SuppressExpr));
  CopyMem (&Item->GrayOutExpr, &Conditions->GrayOut, sizeof (Item->GrayOutExpr));
  CopyMem (&Item->DisableExpr, &Conditions->Disable, sizeof (Item->DisableExpr));
}

/**
  Copy question flags into runtime metadata used by the modern renderer.

  @param[in,out] Item  Parsed question item. Must not be NULL.
**/
STATIC
VOID
ApplyQuestionFlags (
  IN OUT MODERN_UI_HII_ITEM  *Item
  )
{
  if (Item == NULL) {
    return;
  }

  Item->ReadOnly         = (BOOLEAN)((Item->QuestionFlags & EFI_IFR_FLAG_READ_ONLY) != 0);
  Item->CallbackRequired = (BOOLEAN)((Item->QuestionFlags & EFI_IFR_FLAG_CALLBACK) != 0);
  Item->ResetRequired    = (BOOLEAN)((Item->QuestionFlags & EFI_IFR_FLAG_RESET_REQUIRED) != 0);
  Item->RestStyle        = (BOOLEAN)((Item->QuestionFlags & EFI_IFR_FLAG_REST_STYLE) != 0);
  if (Item->ReadOnly) {
    Item->Reason = ModernUiHiiReasonReadOnly;
  } else if (Item->CallbackRequired) {
    Item->Reason = ModernUiHiiReasonCallback;
  }
}

/**
  Build the currently active visible/disabled condition expressions.

  @param[in]  ScopeStack  Active parse scopes. Must not be NULL.
  @param[in]  ScopeDepth  Number of entries in ScopeStack.
  @param[out] Conditions  Receives merged active conditions. Must not be NULL.
**/
STATIC
VOID
BuildActiveConditions (
  IN  MODERN_UI_HII_SCOPE              *ScopeStack,
  IN  UINTN                            ScopeDepth,
  OUT MODERN_UI_HII_ACTIVE_CONDITIONS  *Conditions
  )
{
  UINTN  Index;

  if ((ScopeStack == NULL) || (Conditions == NULL)) {
    return;
  }

  ZeroMem (Conditions, sizeof (*Conditions));
  for (Index = 0; Index < ScopeDepth; Index++) {
    switch (ScopeStack[Index].ConditionType) {
      case ModernUiHiiConditionSuppress:
        MergeConditionExpression (&Conditions->Suppress, &ScopeStack[Index].Condition);
        break;
      case ModernUiHiiConditionGrayOut:
        MergeConditionExpression (&Conditions->GrayOut, &ScopeStack[Index].Condition);
        break;
      case ModernUiHiiConditionDisable:
        MergeConditionExpression (&Conditions->Disable, &ScopeStack[Index].Condition);
        break;
      default:
        break;
    }
  }
}

/**
  Return the most recent open condition scope that is still collecting bytecode.

  @param[in,out] ScopeStack  Active parse scopes. Must not be NULL.
  @param[in]     ScopeDepth  Number of entries in ScopeStack.

  @return Collecting condition scope, or NULL when the current opcode is not
          part of a condition expression.
**/
STATIC
MODERN_UI_HII_SCOPE *
CurrentExpressionScope (
  IN OUT MODERN_UI_HII_SCOPE  *ScopeStack,
  IN     UINTN                ScopeDepth
  )
{
  UINTN  Index;

  if ((ScopeStack == NULL) || (ScopeDepth == 0)) {
    return NULL;
  }

  for (Index = ScopeDepth; Index > 0; Index--) {
    if ((ScopeStack[Index - 1].ConditionType != ModernUiHiiConditionNone) &&
        ScopeStack[Index - 1].CollectingExpression)
    {
      return &ScopeStack[Index - 1];
    }
  }

  return NULL;
}

/**
  Parse a forms package into the bridge model.

  @param[in,out] Model         Model to append to. Must not be NULL.
  @param[in]     HiiHandle     HII package-list handle.
  @param[in]     DriverHandle  Driver handle associated with HiiHandle.
  @param[in]     ConfigAccess  Optional ConfigAccess protocol for routing.
  @param[in]     Package       Forms package bytes. Must not be NULL.
  @param[in]     PackageSize   Forms package size in bytes.
  @param[in]     FormSetGuidList   Optional list of formset GUIDs to include.
  @param[in]     FormSetGuidCount  Number of entries in FormSetGuidList.
**/
STATIC
VOID
ParseFormsPackage (
  IN OUT MODERN_UI_HII_MODEL                  *Model,
  IN     EFI_HII_HANDLE                       HiiHandle,
  IN     EFI_HANDLE                           DriverHandle,
  IN     EFI_HII_CONFIG_ACCESS_PROTOCOL       *ConfigAccess,
  IN     CONST UINT8                          *Package,
  IN     UINTN                                PackageSize,
  IN     CONST EFI_GUID                       *FormSetGuidList OPTIONAL,
  IN     UINTN                                FormSetGuidCount
  )
{
  UINTN                            Offset;
  EFI_IFR_OP_HEADER                *Header;
  MODERN_UI_HII_FORMSET            *FormSet;
  MODERN_UI_HII_FORM               *Form;
  MODERN_UI_HII_ITEM               *Item;
  MODERN_UI_HII_ITEM               *OptionOwner;
  MODERN_UI_HII_VARSTORE           *Store;
  MODERN_UI_HII_SCOPE              ScopeStack[32];
  MODERN_UI_HII_SCOPE              *ExpressionScope;
  MODERN_UI_HII_ACTIVE_CONDITIONS  Conditions;
  UINTN                            ScopeDepth;
  MODERN_UI_HII_SCOPE              Popped;
  UINTN                            NameSize;
  UINT8                            OpCode;

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
        CopyMem (&Popped, &ScopeStack[--ScopeDepth], sizeof (Popped));
        if (Popped.OpCode == EFI_IFR_FORM_OP) {
          Form = NULL;
        } else if (((Popped.OpCode == EFI_IFR_ONE_OF_OP) ||
                    (Popped.OpCode == EFI_IFR_ORDERED_LIST_OP)) &&
                   (Popped.QuestionOwner == OptionOwner))
        {
          //
          // Expression opcodes such as DEFAULT/CONDITIONAL/VALUE/THIS can
          // also carry scope and close with END_OP.  They keep this stack
          // balanced, but must not close the active option owner.
          //
          OptionOwner = NULL;
        }
      }

      continue;
    }

    ExpressionScope = CurrentExpressionScope (ScopeStack, ScopeDepth);
    if ((ExpressionScope != NULL) && IsExpressionOp (Header->OpCode)) {
      AppendExpressionOp (&ExpressionScope->Condition, Header);
      if ((Header->Scope != 0) && (ScopeDepth < ARRAY_SIZE (ScopeStack))) {
        //
        // Scoped expression opcodes still need a stack entry so their END_OP
        // does not close the enclosing condition, form, or question scope.
        //
        ZeroMem (&ScopeStack[ScopeDepth], sizeof (ScopeStack[ScopeDepth]));
        ScopeStack[ScopeDepth].OpCode = Header->OpCode;
        ScopeDepth++;
      }

      continue;
    }

    if ((ExpressionScope != NULL) && !IsExpressionOp (Header->OpCode)) {
      ExpressionScope->CollectingExpression = FALSE;
    }

    Item = NULL;
    switch (Header->OpCode) {
      case EFI_IFR_FORM_SET_OP:
        if ((Header->Length >= sizeof (EFI_IFR_FORM_SET)) &&
            IsFormSetGuidAllowed (&((EFI_IFR_FORM_SET *)Header)->Guid, FormSetGuidList, FormSetGuidCount) &&
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
        } else {
          FormSet = NULL;
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
          BuildActiveConditions (ScopeStack, ScopeDepth, &Conditions);
          Item           = AppendItem (Form, ModernUiHiiItemSubtitle);
          Item->PromptId = ((EFI_IFR_SUBTITLE *)Header)->Statement.Prompt;
          Item->HelpId   = ((EFI_IFR_SUBTITLE *)Header)->Statement.Help;
          Item->ControlFlags = ((EFI_IFR_SUBTITLE *)Header)->Flags;
          ApplyActiveConditions (Item, &Conditions);
        }
        break;
      case EFI_IFR_TEXT_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_TEXT))) {
          BuildActiveConditions (ScopeStack, ScopeDepth, &Conditions);
          Item            = AppendItem (Form, ModernUiHiiItemText);
          Item->PromptId  = ((EFI_IFR_TEXT *)Header)->Statement.Prompt;
          Item->HelpId    = ((EFI_IFR_TEXT *)Header)->Statement.Help;
          Item->TextTwoId = ((EFI_IFR_TEXT *)Header)->TextTwo;
          ApplyActiveConditions (Item, &Conditions);
        }
        break;
      case EFI_IFR_REF_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_REF))) {
          BuildActiveConditions (ScopeStack, ScopeDepth, &Conditions);
          Item                = AppendItem (Form, ModernUiHiiItemRef);
          Item->PromptId      = ((EFI_IFR_REF *)Header)->Question.Header.Prompt;
          Item->HelpId        = ((EFI_IFR_REF *)Header)->Question.Header.Help;
          Item->QuestionId    = ((EFI_IFR_REF *)Header)->Question.QuestionId;
          Item->QuestionFlags = ((EFI_IFR_REF *)Header)->Question.Flags;
          Item->TargetFormId  = ((EFI_IFR_REF *)Header)->FormId;
          CopyGuid (&Item->TargetFormSetGuid, &FormSet->Guid);
          if (Header->Length >= sizeof (EFI_IFR_REF2)) {
            Item->TargetQuestionId = ((EFI_IFR_REF2 *)Header)->QuestionId;
          }

          if (Header->Length >= sizeof (EFI_IFR_REF3)) {
            CopyGuid (&Item->TargetFormSetGuid, &((EFI_IFR_REF3 *)Header)->FormSetId);
          }

          if (Header->Length >= sizeof (EFI_IFR_REF4)) {
            Item->TargetDevicePathId = ((EFI_IFR_REF4 *)Header)->DevicePath;
          }

          ApplyQuestionFlags (Item);
          ApplyActiveConditions (Item, &Conditions);
        }
        break;
      case EFI_IFR_CHECKBOX_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_CHECKBOX))) {
          BuildActiveConditions (ScopeStack, ScopeDepth, &Conditions);
          Item                = AppendItem (Form, ModernUiHiiItemCheckbox);
          Item->PromptId      = ((EFI_IFR_CHECKBOX *)Header)->Question.Header.Prompt;
          Item->HelpId        = ((EFI_IFR_CHECKBOX *)Header)->Question.Header.Help;
          Item->QuestionId    = ((EFI_IFR_CHECKBOX *)Header)->Question.QuestionId;
          Item->VarStoreId    = ((EFI_IFR_CHECKBOX *)Header)->Question.VarStoreId;
          Item->VarOffset     = ((EFI_IFR_CHECKBOX *)Header)->Question.VarStoreInfo.VarOffset;
          Item->QuestionFlags = ((EFI_IFR_CHECKBOX *)Header)->Question.Flags;
          Item->ControlFlags  = ((EFI_IFR_CHECKBOX *)Header)->Flags;
          Item->StorageWidth  = sizeof (UINT8);
          Item->CurrentType   = EFI_IFR_TYPE_BOOLEAN;
          ApplyQuestionFlags (Item);
          ApplyActiveConditions (Item, &Conditions);
        }
        break;
      case EFI_IFR_ONE_OF_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_ONE_OF))) {
          BuildActiveConditions (ScopeStack, ScopeDepth, &Conditions);
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
          Item->CurrentType   = (UINT8)(Item->NumericFlags & EFI_IFR_NUMERIC_SIZE);
          ApplyQuestionFlags (Item);
          ApplyActiveConditions (Item, &Conditions);
          OptionOwner = Item;
        }
        break;
      case EFI_IFR_ONE_OF_OPTION_OP:
        if ((OptionOwner != NULL) && (OptionOwner->OptionCount < MODERN_UI_HII_MAX_OPTIONS) && (Header->Length >= sizeof (EFI_IFR_ONE_OF_OPTION))) {
          OptionOwner->Options[OptionOwner->OptionCount].PromptId  = ((EFI_IFR_ONE_OF_OPTION *)Header)->Option;
          OptionOwner->Options[OptionOwner->OptionCount].Flags     = ((EFI_IFR_ONE_OF_OPTION *)Header)->Flags;
          OptionOwner->Options[OptionOwner->OptionCount].ValueType = ((EFI_IFR_ONE_OF_OPTION *)Header)->Type;
          OptionOwner->Options[OptionOwner->OptionCount].Value     = ((EFI_IFR_ONE_OF_OPTION *)Header)->Value.u64;
          OptionOwner->OptionCount++;
        }
        break;
      case EFI_IFR_NUMERIC_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_NUMERIC))) {
          BuildActiveConditions (ScopeStack, ScopeDepth, &Conditions);
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
          Item->CurrentType   = (UINT8)(Item->NumericFlags & EFI_IFR_NUMERIC_SIZE);
          ApplyQuestionFlags (Item);
          ApplyActiveConditions (Item, &Conditions);
        }
        break;
      case EFI_IFR_STRING_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_STRING))) {
          BuildActiveConditions (ScopeStack, ScopeDepth, &Conditions);
          Item                = AppendItem (Form, ModernUiHiiItemString);
          Item->PromptId      = ((EFI_IFR_STRING *)Header)->Question.Header.Prompt;
          Item->HelpId        = ((EFI_IFR_STRING *)Header)->Question.Header.Help;
          Item->QuestionId    = ((EFI_IFR_STRING *)Header)->Question.QuestionId;
          Item->VarStoreId    = ((EFI_IFR_STRING *)Header)->Question.VarStoreId;
          Item->VarOffset     = ((EFI_IFR_STRING *)Header)->Question.VarStoreInfo.VarOffset;
          Item->QuestionFlags = ((EFI_IFR_STRING *)Header)->Question.Flags;
          Item->StorageWidth  = ((EFI_IFR_STRING *)Header)->MaxSize * sizeof (CHAR16);
          Item->CurrentType   = EFI_IFR_TYPE_STRING;
          ApplyQuestionFlags (Item);
          ApplyActiveConditions (Item, &Conditions);
          Item->Reason = (Item->Reason == ModernUiHiiReasonNone) ? ModernUiHiiReasonUnsupportedControl : Item->Reason;
        }
        break;
      case EFI_IFR_PASSWORD_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_PASSWORD))) {
          BuildActiveConditions (ScopeStack, ScopeDepth, &Conditions);
          Item                = AppendItem (Form, ModernUiHiiItemPassword);
          Item->PromptId      = ((EFI_IFR_PASSWORD *)Header)->Question.Header.Prompt;
          Item->HelpId        = ((EFI_IFR_PASSWORD *)Header)->Question.Header.Help;
          Item->QuestionId    = ((EFI_IFR_PASSWORD *)Header)->Question.QuestionId;
          Item->VarStoreId    = ((EFI_IFR_PASSWORD *)Header)->Question.VarStoreId;
          Item->VarOffset     = ((EFI_IFR_PASSWORD *)Header)->Question.VarStoreInfo.VarOffset;
          Item->QuestionFlags = ((EFI_IFR_PASSWORD *)Header)->Question.Flags;
          Item->StorageWidth  = ((EFI_IFR_PASSWORD *)Header)->MaxSize * sizeof (CHAR16);
          Item->CurrentType   = EFI_IFR_TYPE_STRING;
          ApplyQuestionFlags (Item);
          ApplyActiveConditions (Item, &Conditions);
          Item->Reason = (Item->Reason == ModernUiHiiReasonNone) ? ModernUiHiiReasonUnsupportedControl : Item->Reason;
        }
        break;
      case EFI_IFR_ORDERED_LIST_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_ORDERED_LIST))) {
          BuildActiveConditions (ScopeStack, ScopeDepth, &Conditions);
          Item                = AppendItem (Form, ModernUiHiiItemOrderedList);
          Item->PromptId      = ((EFI_IFR_ORDERED_LIST *)Header)->Question.Header.Prompt;
          Item->HelpId        = ((EFI_IFR_ORDERED_LIST *)Header)->Question.Header.Help;
          Item->QuestionId    = ((EFI_IFR_ORDERED_LIST *)Header)->Question.QuestionId;
          Item->VarStoreId    = ((EFI_IFR_ORDERED_LIST *)Header)->Question.VarStoreId;
          Item->VarOffset     = ((EFI_IFR_ORDERED_LIST *)Header)->Question.VarStoreInfo.VarOffset;
          Item->QuestionFlags = ((EFI_IFR_ORDERED_LIST *)Header)->Question.Flags;
          Item->ControlFlags  = ((EFI_IFR_ORDERED_LIST *)Header)->Flags;
          Item->StorageWidth  = ((EFI_IFR_ORDERED_LIST *)Header)->MaxContainers;
          Item->CurrentType   = EFI_IFR_TYPE_BUFFER;
          ApplyQuestionFlags (Item);
          ApplyActiveConditions (Item, &Conditions);
          Item->Reason = (Item->Reason == ModernUiHiiReasonNone) ? ModernUiHiiReasonUnsupportedControl : Item->Reason;
          OptionOwner = Item;
        }
        break;
      case EFI_IFR_DATE_OP:
      case EFI_IFR_TIME_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_DATE))) {
          BuildActiveConditions (ScopeStack, ScopeDepth, &Conditions);
          Item                = AppendItem (Form, (Header->OpCode == EFI_IFR_DATE_OP) ? ModernUiHiiItemDate : ModernUiHiiItemTime);
          Item->PromptId      = ((EFI_IFR_DATE *)Header)->Question.Header.Prompt;
          Item->HelpId        = ((EFI_IFR_DATE *)Header)->Question.Header.Help;
          Item->QuestionId    = ((EFI_IFR_DATE *)Header)->Question.QuestionId;
          Item->QuestionFlags = ((EFI_IFR_DATE *)Header)->Question.Flags;
          Item->ControlFlags  = ((EFI_IFR_DATE *)Header)->Flags;
          Item->CurrentType   = (Header->OpCode == EFI_IFR_DATE_OP) ? EFI_IFR_TYPE_DATE : EFI_IFR_TYPE_TIME;
          ApplyQuestionFlags (Item);
          ApplyActiveConditions (Item, &Conditions);
          Item->Reason = (Item->Reason == ModernUiHiiReasonNone) ? ModernUiHiiReasonUnsupportedControl : Item->Reason;
        }
        break;
      case EFI_IFR_ACTION_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_ACTION_1))) {
          BuildActiveConditions (ScopeStack, ScopeDepth, &Conditions);
          Item                = AppendItem (Form, ModernUiHiiItemAction);
          Item->PromptId      = ((EFI_IFR_ACTION *)Header)->Question.Header.Prompt;
          Item->HelpId        = ((EFI_IFR_ACTION *)Header)->Question.Header.Help;
          Item->QuestionId    = ((EFI_IFR_ACTION *)Header)->Question.QuestionId;
          Item->QuestionFlags = ((EFI_IFR_ACTION *)Header)->Question.Flags;
          Item->CurrentType   = EFI_IFR_TYPE_ACTION;
          ApplyQuestionFlags (Item);
          ApplyActiveConditions (Item, &Conditions);
        }
        break;
      case EFI_IFR_RESET_BUTTON_OP:
        if ((Form != NULL) && (Header->Length >= sizeof (EFI_IFR_RESET_BUTTON))) {
          BuildActiveConditions (ScopeStack, ScopeDepth, &Conditions);
          Item              = AppendItem (Form, ModernUiHiiItemResetButton);
          Item->PromptId    = ((EFI_IFR_RESET_BUTTON *)Header)->Statement.Prompt;
          Item->HelpId      = ((EFI_IFR_RESET_BUTTON *)Header)->Statement.Help;
          Item->CurrentType = EFI_IFR_TYPE_ACTION;
          ApplyActiveConditions (Item, &Conditions);
          Item->Reason = ModernUiHiiReasonUnsupportedControl;
        }
        break;
      case EFI_IFR_SUPPRESS_IF_OP:
      case EFI_IFR_GRAY_OUT_IF_OP:
      case EFI_IFR_DISABLE_IF_OP:
      case EFI_IFR_INCONSISTENT_IF_OP:
      case EFI_IFR_NO_SUBMIT_IF_OP:
      case EFI_IFR_WARNING_IF_OP:
        break;
      default:
        break;
    }

    if ((Header->Scope != 0) && (ScopeDepth < ARRAY_SIZE (ScopeStack))) {
      OpCode = Header->OpCode;
      ZeroMem (&ScopeStack[ScopeDepth], sizeof (ScopeStack[ScopeDepth]));
      ScopeStack[ScopeDepth].OpCode = OpCode;
      if ((OpCode == EFI_IFR_ONE_OF_OP) || (OpCode == EFI_IFR_ORDERED_LIST_OP)) {
        ScopeStack[ScopeDepth].QuestionOwner = OptionOwner;
      } else if ((Item != NULL) &&
                 ((OpCode == EFI_IFR_CHECKBOX_OP) || (OpCode == EFI_IFR_NUMERIC_OP) ||
                  (OpCode == EFI_IFR_STRING_OP) || (OpCode == EFI_IFR_PASSWORD_OP) ||
                  (OpCode == EFI_IFR_REF_OP) || (OpCode == EFI_IFR_ACTION_OP)))
      {
        ScopeStack[ScopeDepth].QuestionOwner = Item;
      }

      switch (OpCode) {
        case EFI_IFR_SUPPRESS_IF_OP:
          ScopeStack[ScopeDepth].ConditionType        = ModernUiHiiConditionSuppress;
          ScopeStack[ScopeDepth].CollectingExpression = TRUE;
          break;
        case EFI_IFR_GRAY_OUT_IF_OP:
          ScopeStack[ScopeDepth].ConditionType        = ModernUiHiiConditionGrayOut;
          ScopeStack[ScopeDepth].CollectingExpression = TRUE;
          break;
        case EFI_IFR_DISABLE_IF_OP:
          ScopeStack[ScopeDepth].ConditionType        = ModernUiHiiConditionDisable;
          ScopeStack[ScopeDepth].CollectingExpression = TRUE;
          break;
        case EFI_IFR_INCONSISTENT_IF_OP:
        case EFI_IFR_NO_SUBMIT_IF_OP:
        case EFI_IFR_WARNING_IF_OP:
          ScopeStack[ScopeDepth].ConditionType        = ModernUiHiiConditionValidation;
          ScopeStack[ScopeDepth].CollectingExpression = TRUE;
          break;
        default:
          break;
      }

      ScopeDepth++;
    }
  }
}

/**
  Export one HII package list and parse its forms packages.

  @param[in,out] Model        Model to append to. Must not be NULL.
  @param[in]     HiiDatabase  HII database protocol. Must not be NULL.
  @param[in]     HiiHandle    HII handle to export.
  @param[in]     FormSetGuidList   Optional list of formset GUIDs to include.
  @param[in]     FormSetGuidCount  Number of entries in FormSetGuidList.

  @retval EFI_SUCCESS           Package was inspected.
  @retval EFI_OUT_OF_RESOURCES  Export buffer allocation failed.
  @retval others                Status from ExportPackageLists().
**/
STATIC
EFI_STATUS
LoadOneHiiHandle (
  IN OUT MODERN_UI_HII_MODEL        *Model,
  IN     EFI_HII_DATABASE_PROTOCOL  *HiiDatabase,
  IN     EFI_HII_HANDLE             HiiHandle,
  IN     CONST EFI_GUID             *FormSetGuidList OPTIONAL,
  IN     UINTN                      FormSetGuidCount
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
  Find a parsed question by IFR question ID.

  @param[in] FormSet     Formset to search. Must not be NULL.
  @param[in] QuestionId  Question ID to locate.

  @return Matching item, or NULL when not found.
**/
STATIC
MODERN_UI_HII_ITEM *
FindQuestionById (
  IN MODERN_UI_HII_FORMSET  *FormSet,
  IN EFI_QUESTION_ID        QuestionId
  )
{
  UINTN  FormIndex;
  UINTN  ItemIndex;

  if ((FormSet == NULL) || (QuestionId == 0)) {
    return NULL;
  }

  for (FormIndex = 0; FormIndex < FormSet->FormCount; FormIndex++) {
    for (ItemIndex = 0; ItemIndex < FormSet->Forms[FormIndex].ItemCount; ItemIndex++) {
      if (FormSet->Forms[FormIndex].Items[ItemIndex].QuestionId == QuestionId) {
        return &FormSet->Forms[FormIndex].Items[ItemIndex];
      }
    }
  }

  return NULL;
}

/**
  Return a question value for expression evaluation.

  @param[in]  FormSet     Formset to search. Must not be NULL.
  @param[in]  QuestionId  Question ID to read.
  @param[out] Value       Receives the current integer value. Must not be NULL.

  @retval EFI_SUCCESS    Value is available.
  @retval EFI_NOT_FOUND  Question is absent or has no readable value.
**/
STATIC
EFI_STATUS
GetExpressionQuestionValue (
  IN  MODERN_UI_HII_FORMSET  *FormSet,
  IN  EFI_QUESTION_ID        QuestionId,
  OUT UINT64                 *Value
  )
{
  MODERN_UI_HII_ITEM  *Item;

  if ((FormSet == NULL) || (Value == NULL)) {
    return EFI_NOT_FOUND;
  }

  Item = FindQuestionById (FormSet, QuestionId);
  if ((Item == NULL) || !Item->HasValue) {
    return EFI_NOT_FOUND;
  }

  *Value = Item->CurrentValue;
  return EFI_SUCCESS;
}

/**
  Pop one expression stack value.

  @param[in,out] Stack       Stack storage. Must not be NULL.
  @param[in,out] StackDepth  Current stack depth. Must not be NULL.
  @param[out]    Value       Receives popped value. Must not be NULL.

  @retval EFI_SUCCESS       Value was popped.
  @retval EFI_UNSUPPORTED   Stack was empty.
**/
STATIC
EFI_STATUS
PopExpressionValue (
  IN OUT UINT64  *Stack,
  IN OUT UINTN   *StackDepth,
  OUT    UINT64  *Value
  )
{
  if ((Stack == NULL) || (StackDepth == NULL) || (Value == NULL) || (*StackDepth == 0)) {
    return EFI_UNSUPPORTED;
  }

  *Value = Stack[--(*StackDepth)];
  return EFI_SUCCESS;
}

/**
  Push one expression stack value.

  @param[in,out] Stack       Stack storage. Must not be NULL.
  @param[in,out] StackDepth  Current stack depth. Must not be NULL.
  @param[in]     Value       Value to push.

  @retval EFI_SUCCESS       Value was pushed.
  @retval EFI_UNSUPPORTED   Stack is full.
**/
STATIC
EFI_STATUS
PushExpressionValue (
  IN OUT UINT64  *Stack,
  IN OUT UINTN   *StackDepth,
  IN     UINT64  Value
  )
{
  if ((Stack == NULL) || (StackDepth == NULL) || (*StackDepth >= MODERN_UI_HII_MAX_EXPR_OPS)) {
    return EFI_UNSUPPORTED;
  }

  Stack[(*StackDepth)++] = Value;
  return EFI_SUCCESS;
}

/**
  Evaluate one compact IFR expression as a boolean.

  @param[in]  FormSet  Formset containing referenced question values.
                      Must not be NULL.
  @param[in]  Expr     Expression to evaluate. Must not be NULL.
  @param[out] Result   Receives TRUE or FALSE. Must not be NULL.

  @retval EFI_SUCCESS      Expression was evaluated.
  @retval EFI_UNSUPPORTED  Expression contains unsupported or unavailable data.
**/
STATIC
EFI_STATUS
EvaluateExpression (
  IN  MODERN_UI_HII_FORMSET        *FormSet,
  IN  CONST MODERN_UI_HII_EXPR     *Expr,
  OUT BOOLEAN                      *Result
  )
{
  UINT64                      Stack[MODERN_UI_HII_MAX_EXPR_OPS];
  UINTN                       StackDepth;
  UINTN                       Index;
  UINTN                       ListIndex;
  UINT64                      Left;
  UINT64                      Right;
  UINT64                      Value;
  CONST MODERN_UI_HII_EXPR_OP *Op;
  EFI_STATUS                  Status;

  if ((FormSet == NULL) || (Expr == NULL) || (Result == NULL)) {
    return EFI_UNSUPPORTED;
  }

  if (Expr->OpCount == 0) {
    *Result = FALSE;
    return EFI_SUCCESS;
  }

  if (Expr->Unsupported) {
    return EFI_UNSUPPORTED;
  }

  StackDepth = 0;
  for (Index = 0; Index < Expr->OpCount; Index++) {
    Op = &Expr->Ops[Index];
    switch (Op->OpCode) {
      case EFI_IFR_EQ_ID_VAL_OP:
        Status = GetExpressionQuestionValue (FormSet, Op->QuestionId, &Value);
        if (EFI_ERROR (Status)) {
          return EFI_UNSUPPORTED;
        }
        Status = PushExpressionValue (Stack, &StackDepth, (Value == Op->Value) ? 1 : 0);
        break;
      case EFI_IFR_EQ_ID_ID_OP:
        Status = GetExpressionQuestionValue (FormSet, Op->QuestionId, &Left);
        if (EFI_ERROR (Status)) {
          return EFI_UNSUPPORTED;
        }
        Status = GetExpressionQuestionValue (FormSet, (EFI_QUESTION_ID)Op->Value, &Right);
        if (EFI_ERROR (Status)) {
          return EFI_UNSUPPORTED;
        }
        Status = PushExpressionValue (Stack, &StackDepth, (Left == Right) ? 1 : 0);
        break;
      case EFI_IFR_EQ_ID_VAL_LIST_OP:
        Status = GetExpressionQuestionValue (FormSet, Op->QuestionId, &Value);
        if (EFI_ERROR (Status)) {
          return EFI_UNSUPPORTED;
        }

        Right = 0;
        for (ListIndex = 0; ListIndex < Op->ValueListCount; ListIndex++) {
          if (Value == Op->ValueList[ListIndex]) {
            Right = 1;
            break;
          }
        }

        Status = PushExpressionValue (Stack, &StackDepth, Right);
        break;
      case EFI_IFR_QUESTION_REF1_OP:
        Status = GetExpressionQuestionValue (FormSet, Op->QuestionId, &Value);
        if (EFI_ERROR (Status)) {
          return EFI_UNSUPPORTED;
        }
        Status = PushExpressionValue (Stack, &StackDepth, Value);
        break;
      case EFI_IFR_UINT8_OP:
      case EFI_IFR_UINT16_OP:
      case EFI_IFR_UINT32_OP:
      case EFI_IFR_UINT64_OP:
        Status = PushExpressionValue (Stack, &StackDepth, Op->Value);
        break;
      case EFI_IFR_TRUE_OP:
      case EFI_IFR_ONE_OP:
        Status = PushExpressionValue (Stack, &StackDepth, 1);
        break;
      case EFI_IFR_FALSE_OP:
      case EFI_IFR_ZERO_OP:
      case EFI_IFR_SECURITY_OP:
        Status = PushExpressionValue (Stack, &StackDepth, 0);
        break;
      case EFI_IFR_AND_OP:
      case EFI_IFR_OR_OP:
      case EFI_IFR_EQUAL_OP:
      case EFI_IFR_NOT_EQUAL_OP:
      case EFI_IFR_GREATER_THAN_OP:
      case EFI_IFR_GREATER_EQUAL_OP:
      case EFI_IFR_LESS_THAN_OP:
      case EFI_IFR_LESS_EQUAL_OP:
      case EFI_IFR_BITWISE_AND_OP:
      case EFI_IFR_SHIFT_RIGHT_OP:
      case EFI_IFR_ADD_OP:
        Status = PopExpressionValue (Stack, &StackDepth, &Right);
        if (EFI_ERROR (Status)) {
          return Status;
        }

        Status = PopExpressionValue (Stack, &StackDepth, &Left);
        if (EFI_ERROR (Status)) {
          return Status;
        }

        switch (Op->OpCode) {
          case EFI_IFR_AND_OP:
            Value = ((Left != 0) && (Right != 0)) ? 1 : 0;
            break;
          case EFI_IFR_OR_OP:
            Value = ((Left != 0) || (Right != 0)) ? 1 : 0;
            break;
          case EFI_IFR_EQUAL_OP:
            Value = (Left == Right) ? 1 : 0;
            break;
          case EFI_IFR_NOT_EQUAL_OP:
            Value = (Left != Right) ? 1 : 0;
            break;
          case EFI_IFR_GREATER_THAN_OP:
            Value = (Left > Right) ? 1 : 0;
            break;
          case EFI_IFR_GREATER_EQUAL_OP:
            Value = (Left >= Right) ? 1 : 0;
            break;
          case EFI_IFR_LESS_THAN_OP:
            Value = (Left < Right) ? 1 : 0;
            break;
          case EFI_IFR_LESS_EQUAL_OP:
            Value = (Left <= Right) ? 1 : 0;
            break;
          case EFI_IFR_BITWISE_AND_OP:
            Value = Left & Right;
            break;
          case EFI_IFR_SHIFT_RIGHT_OP:
            Value = RShiftU64 (Left, (UINTN)Right);
            break;
          case EFI_IFR_ADD_OP:
          default:
            Value = Left + Right;
            break;
        }

        Status = PushExpressionValue (Stack, &StackDepth, Value);
        break;
      case EFI_IFR_NOT_OP:
        Status = PopExpressionValue (Stack, &StackDepth, &Value);
        if (EFI_ERROR (Status)) {
          return Status;
        }
        Status = PushExpressionValue (Stack, &StackDepth, (Value == 0) ? 1 : 0);
        break;
      default:
        return EFI_UNSUPPORTED;
    }

    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (StackDepth == 0) {
    return EFI_UNSUPPORTED;
  }

  *Result = (BOOLEAN)(Stack[StackDepth - 1] != 0);
  return EFI_SUCCESS;
}

/**
  Recompute display and edit state for all parsed HII items.

  @param[in,out] Model  Populated model. Must not be NULL.
**/
STATIC
VOID
EvaluateModelConditions (
  IN OUT MODERN_UI_HII_MODEL  *Model
  )
{
  UINTN                  FormSetIndex;
  UINTN                  FormIndex;
  UINTN                  ItemIndex;
  MODERN_UI_HII_FORMSET  *FormSet;
  MODERN_UI_HII_ITEM     *Item;
  BOOLEAN                Result;
  EFI_STATUS             Status;

  if (Model == NULL) {
    return;
  }

  for (FormSetIndex = 0; FormSetIndex < Model->FormSetCount; FormSetIndex++) {
    FormSet = &Model->FormSets[FormSetIndex];
    for (FormIndex = 0; FormIndex < FormSet->FormCount; FormIndex++) {
      for (ItemIndex = 0; ItemIndex < FormSet->Forms[FormIndex].ItemCount; ItemIndex++) {
        Item = &FormSet->Forms[FormIndex].Items[ItemIndex];
        Item->Visible  = TRUE;
        Item->GrayOut  = FALSE;
        Item->Disabled = FALSE;

        Status = EvaluateExpression (FormSet, &Item->SuppressExpr, &Result);
        if (EFI_ERROR (Status)) {
          if (Item->SuppressExpr.OpCount > 0) {
            Item->Disabled = TRUE;
            Item->Reason   = ModernUiHiiReasonUnsupportedCondition;
          }
        } else if (Result) {
          Item->Visible = FALSE;
        }

        Status = EvaluateExpression (FormSet, &Item->GrayOutExpr, &Result);
        if (EFI_ERROR (Status)) {
          if (Item->GrayOutExpr.OpCount > 0) {
            Item->Disabled = TRUE;
            Item->Reason   = ModernUiHiiReasonUnsupportedCondition;
          }
        } else if (Result) {
          Item->GrayOut  = TRUE;
          Item->Disabled = TRUE;
          Item->Reason   = ModernUiHiiReasonGrayOut;
        }

        Status = EvaluateExpression (FormSet, &Item->DisableExpr, &Result);
        if (EFI_ERROR (Status)) {
          if (Item->DisableExpr.OpCount > 0) {
            Item->Disabled = TRUE;
            Item->Reason   = ModernUiHiiReasonUnsupportedCondition;
          }
        } else if (Result) {
          Item->Disabled = TRUE;
          Item->Reason   = ModernUiHiiReasonDisable;
        }
      }
    }
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
    Item->Reason = ModernUiHiiReasonUnsupportedStorage;
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
  return ModernUiHiiBridgeLoadFiltered (Model, NULL, 0);
}

/**
  Enumerate the HII database and build a compact IFR model for selected formsets.

  @param[out] Model            Model storage to clear and populate. Must not be NULL.
  @param[in]  FormSetGuidList  Optional list of formset GUIDs to include.
  @param[in]  FormSetGuidCount Number of entries in FormSetGuidList.

  @retval EFI_SUCCESS            HII data was loaded. An empty model is valid.
  @retval EFI_INVALID_PARAMETER  Model is NULL, or count is nonzero with NULL list.
  @retval EFI_NOT_FOUND          HII database protocol is unavailable.
  @retval EFI_OUT_OF_RESOURCES   A temporary package buffer allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiHiiBridgeLoadFiltered (
  OUT MODERN_UI_HII_MODEL  *Model,
  IN  CONST EFI_GUID       *FormSetGuidList OPTIONAL,
  IN  UINTN                FormSetGuidCount
  )
{
  EFI_STATUS                 Status;
  EFI_HII_DATABASE_PROTOCOL  *HiiDatabase;
  EFI_HII_HANDLE             *HiiHandles;
  UINTN                      Index;

  if (Model == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((FormSetGuidList == NULL) && (FormSetGuidCount != 0)) {
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
    Status = LoadOneHiiHandle (Model, HiiDatabase, HiiHandles[Index], FormSetGuidList, FormSetGuidCount);
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

  EvaluateModelConditions (Model);
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
  if (Item->Unsupported || Item->ReadOnly || Item->Disabled || ((Item->QuestionFlags & EFI_IFR_FLAG_CALLBACK) != 0)) {
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

/**
  Convert an item into a callback value and type.

  @param[in]  Item   Item to convert. Must not be NULL.
  @param[out] Type   Receives EFI_IFR_TYPE_* value. Must not be NULL.
  @param[out] Value  Receives callback value. Must not be NULL.

  @retval EFI_SUCCESS            Value was prepared.
  @retval EFI_INVALID_PARAMETER  A parameter is NULL.
**/
STATIC
EFI_STATUS
BuildCallbackValue (
  IN  MODERN_UI_HII_ITEM     *Item,
  OUT UINT8                  *Type,
  OUT EFI_IFR_TYPE_VALUE     *Value
  )
{
  if ((Item == NULL) || (Type == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Value, sizeof (*Value));
  switch (Item->Type) {
    case ModernUiHiiItemRef:
      *Type                 = EFI_IFR_TYPE_REF;
      Value->ref.QuestionId = Item->TargetQuestionId;
      Value->ref.FormId     = Item->TargetFormId;
      CopyGuid (&Value->ref.FormSetGuid, &Item->TargetFormSetGuid);
      Value->ref.DevicePath = Item->TargetDevicePathId;
      break;
    case ModernUiHiiItemAction:
    case ModernUiHiiItemResetButton:
      *Type         = EFI_IFR_TYPE_ACTION;
      Value->string = 0;
      break;
    case ModernUiHiiItemCheckbox:
      *Type    = EFI_IFR_TYPE_BOOLEAN;
      Value->b = (BOOLEAN)(Item->CurrentValue != 0);
      break;
    case ModernUiHiiItemOneOf:
    case ModernUiHiiItemNumeric:
      *Type      = Item->CurrentType;
      Value->u64 = Item->CurrentValue;
      break;
    case ModernUiHiiItemString:
    case ModernUiHiiItemPassword:
      *Type         = EFI_IFR_TYPE_STRING;
      Value->string = 0;
      break;
    case ModernUiHiiItemDate:
      *Type = EFI_IFR_TYPE_DATE;
      break;
    case ModernUiHiiItemTime:
      *Type = EFI_IFR_TYPE_TIME;
      break;
    default:
      *Type      = EFI_IFR_TYPE_UNDEFINED;
      Value->u64 = 0;
      break;
  }

  return EFI_SUCCESS;
}

/**
  Invoke a ConfigAccess callback for one parsed item.

  @param[in]  FormSet  Formset that owns the item. Must not be NULL.
  @param[in]  Item     Item to pass to the driver. Must not be NULL.
  @param[in]  Action   Browser action being simulated.
  @param[out] Value    Optional callback value, updated by the driver.
  @param[out] Request  Optional browser action request.

  @retval EFI_SUCCESS            Callback completed or was unsupported.
  @retval EFI_INVALID_PARAMETER  A required parameter is NULL.
  @retval others                 Status returned by ConfigAccess.Callback().
**/
STATIC
EFI_STATUS
InvokeItemCallback (
  IN  MODERN_UI_HII_FORMSET        *FormSet,
  IN  MODERN_UI_HII_ITEM           *Item,
  IN  EFI_BROWSER_ACTION           Action,
  OUT EFI_IFR_TYPE_VALUE           *Value OPTIONAL,
  OUT EFI_BROWSER_ACTION_REQUEST   *Request OPTIONAL
  )
{
  EFI_IFR_TYPE_VALUE          LocalValue;
  EFI_IFR_TYPE_VALUE          *CallbackValue;
  EFI_BROWSER_ACTION_REQUEST  LocalRequest;
  UINT8                       Type;
  EFI_STATUS                  Status;

  if ((FormSet == NULL) || (Item == NULL) || (FormSet->ConfigAccess == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  CallbackValue = NULL;
  if ((Action != EFI_BROWSER_ACTION_FORM_OPEN) && (Action != EFI_BROWSER_ACTION_FORM_CLOSE)) {
    Status = BuildCallbackValue (Item, &Type, &LocalValue);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    CallbackValue = &LocalValue;
  } else {
    Type = EFI_IFR_TYPE_UNDEFINED;
  }

  LocalRequest = EFI_BROWSER_ACTION_REQUEST_NONE;
  Status = FormSet->ConfigAccess->Callback (
                                    FormSet->ConfigAccess,
                                    Action,
                                    Item->QuestionId,
                                    Type,
                                    CallbackValue,
                                    &LocalRequest
                                    );
  if ((Status == EFI_UNSUPPORTED) &&
      ((Action == EFI_BROWSER_ACTION_FORM_OPEN) || (Action == EFI_BROWSER_ACTION_FORM_CLOSE) ||
       (Action == EFI_BROWSER_ACTION_CHANGING) || (Action == EFI_BROWSER_ACTION_CHANGED)))
  {
    Status = EFI_SUCCESS;
  }

  if ((Value != NULL) && (CallbackValue != NULL)) {
    CopyMem (Value, CallbackValue, sizeof (*Value));
  }

  if (Request != NULL) {
    *Request = LocalRequest;
  }

  return Status;
}

/**
  Notify a HII driver callback that a form is opening or closing.

  @param[in,out] Model         Populated HII model. Must not be NULL.
  @param[in]     FormSetIndex  Zero-based formset index.
  @param[in]     FormIndex     Zero-based form index within the formset.
  @param[in]     Opening       TRUE sends FORM_OPEN, FALSE sends FORM_CLOSE.
  @param[out]    Request       Optional browser action request returned by the callback.

  @retval EFI_SUCCESS            Callback was not needed or completed.
  @retval EFI_INVALID_PARAMETER  Model or indices are invalid.
  @retval others                 Status returned by ConfigAccess.Callback().
**/
EFI_STATUS
EFIAPI
ModernUiHiiBridgeNotifyForm (
  IN OUT MODERN_UI_HII_MODEL         *Model,
  IN     UINTN                       FormSetIndex,
  IN     UINTN                       FormIndex,
  IN     BOOLEAN                     Opening,
  OUT    EFI_BROWSER_ACTION_REQUEST  *Request OPTIONAL
  )
{
  MODERN_UI_HII_FORMSET        *FormSet;
  MODERN_UI_HII_FORM           *Form;
  MODERN_UI_HII_ITEM           *Item;
  EFI_BROWSER_ACTION_REQUEST   LocalRequest;
  EFI_BROWSER_ACTION_REQUEST   ItemRequest;
  EFI_STATUS                   Status;
  UINTN                        ItemIndex;

  if ((Model == NULL) || (FormSetIndex >= Model->FormSetCount)) {
    return EFI_INVALID_PARAMETER;
  }

  FormSet = &Model->FormSets[FormSetIndex];
  if (FormIndex >= FormSet->FormCount) {
    return EFI_INVALID_PARAMETER;
  }

  Form         = &FormSet->Forms[FormIndex];
  LocalRequest = EFI_BROWSER_ACTION_REQUEST_NONE;
  for (ItemIndex = 0; ItemIndex < Form->ItemCount; ItemIndex++) {
    Item = &Form->Items[ItemIndex];
    if (!Item->CallbackRequired || (Item->QuestionId == 0)) {
      continue;
    }

    ItemRequest = EFI_BROWSER_ACTION_REQUEST_NONE;
    Status = InvokeItemCallback (
               FormSet,
               Item,
               Opening ? EFI_BROWSER_ACTION_FORM_OPEN : EFI_BROWSER_ACTION_FORM_CLOSE,
               NULL,
               &ItemRequest
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (ItemRequest != EFI_BROWSER_ACTION_REQUEST_NONE) {
      LocalRequest = ItemRequest;
    }
  }

  if (Request != NULL) {
    *Request = LocalRequest;
  }

  return EFI_SUCCESS;
}

/**
  Run callback processing for the selected HII item.

  @param[in,out] Model         Populated HII model. Must not be NULL.
  @param[in]     FormSetIndex  Zero-based formset index.
  @param[in]     FormIndex     Zero-based form index within the formset.
  @param[in]     ItemIndex     Zero-based item index within the form.
  @param[out]    Value         Optional callback value, updated by the driver.
  @param[out]    Request       Optional browser action request returned by the callback.

  @retval EFI_SUCCESS            Callback was not needed or completed.
  @retval EFI_INVALID_PARAMETER  Model or indices are invalid.
  @retval others                 Status returned by ConfigAccess.Callback().
**/
EFI_STATUS
EFIAPI
ModernUiHiiBridgeRunCallback (
  IN OUT MODERN_UI_HII_MODEL         *Model,
  IN     UINTN                       FormSetIndex,
  IN     UINTN                       FormIndex,
  IN     UINTN                       ItemIndex,
  OUT    EFI_IFR_TYPE_VALUE          *Value OPTIONAL,
  OUT    EFI_BROWSER_ACTION_REQUEST  *Request OPTIONAL
  )
{
  MODERN_UI_HII_FORMSET        *FormSet;
  MODERN_UI_HII_FORM           *Form;
  MODERN_UI_HII_ITEM           *Item;
  EFI_BROWSER_ACTION_REQUEST   LocalRequest;
  EFI_BROWSER_ACTION_REQUEST   ChangedRequest;
  EFI_IFR_TYPE_VALUE           LocalValue;
  EFI_STATUS                   Status;

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
  if ((FormSet->ConfigAccess == NULL) || (Item->QuestionId == 0) || !Item->CallbackRequired) {
    if (Request != NULL) {
      *Request = EFI_BROWSER_ACTION_REQUEST_NONE;
    }

    if (Value != NULL) {
      ZeroMem (Value, sizeof (*Value));
    }

    return EFI_SUCCESS;
  }

  LocalRequest = EFI_BROWSER_ACTION_REQUEST_NONE;
  ZeroMem (&LocalValue, sizeof (LocalValue));
  Status = InvokeItemCallback (
             FormSet,
             Item,
             EFI_BROWSER_ACTION_CHANGING,
             &LocalValue,
             &LocalRequest
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ChangedRequest = EFI_BROWSER_ACTION_REQUEST_NONE;
  Status = InvokeItemCallback (
             FormSet,
             Item,
             EFI_BROWSER_ACTION_CHANGED,
             &LocalValue,
             &ChangedRequest
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (ChangedRequest != EFI_BROWSER_ACTION_REQUEST_NONE) {
    LocalRequest = ChangedRequest;
  }

  if (Value != NULL) {
    CopyMem (Value, &LocalValue, sizeof (*Value));
  }

  if (Request != NULL) {
    *Request = LocalRequest;
  }

  return EFI_SUCCESS;
}
