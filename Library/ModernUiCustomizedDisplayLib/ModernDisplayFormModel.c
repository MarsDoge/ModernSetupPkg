/** @file
  Private FormBrowser DisplayEngine to Modern UI form view model helpers.

Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "CustomizedDisplayLibInternal.h"

/**
  Add a state bit when Condition is TRUE.

  @param[in,out] State      State bitset to update.
  @param[in]     Bit        Bit to add.
  @param[in]     Condition  TRUE to add Bit.
**/
STATIC
VOID
ModernDisplayAddRowState (
  IN OUT UINT32   *State,
  IN     UINT32   Bit,
  IN     BOOLEAN  Condition
  )
{
  if ((State != NULL) && Condition) {
    *State |= Bit;
  }
}

/**
  Return whether an opcode should present hexadecimal manual input help.

  @param[in] Statement  DisplayEngine statement. Must not be NULL.

  @retval TRUE   The opcode requests hexadecimal input display.
  @retval FALSE  Decimal/default input display is appropriate.
**/
STATIC
BOOLEAN
ModernDisplayStatementUsesHexInput (
  IN FORM_DISPLAY_ENGINE_STATEMENT  *Statement
  )
{
  EFI_IFR_NUMERIC  *NumericOp;
  EFI_IFR_DATE     *DateOp;
  EFI_IFR_TIME     *TimeOp;

  ASSERT ((Statement != NULL) && (Statement->OpCode != NULL));
  if ((Statement == NULL) || (Statement->OpCode == NULL)) {
    return FALSE;
  }

  switch (Statement->OpCode->OpCode) {
    case EFI_IFR_NUMERIC_OP:
      NumericOp = (EFI_IFR_NUMERIC *)Statement->OpCode;
      return (BOOLEAN)((NumericOp->Flags & EFI_IFR_DISPLAY_UINT_HEX) == EFI_IFR_DISPLAY_UINT_HEX);

    case EFI_IFR_DATE_OP:
      DateOp = (EFI_IFR_DATE *)Statement->OpCode;
      return (BOOLEAN)((DateOp->Flags & EFI_IFR_DISPLAY_UINT_HEX) == EFI_IFR_DISPLAY_UINT_HEX);

    case EFI_IFR_TIME_OP:
      TimeOp = (EFI_IFR_TIME *)Statement->OpCode;
      return (BOOLEAN)((TimeOp->Flags & EFI_IFR_DISPLAY_UINT_HEX) == EFI_IFR_DISPLAY_UINT_HEX);

    default:
      return FALSE;
  }
}

/**
  Classify a DisplayEngine statement into a private Modern UI row kind.

  @param[in] Statement  DisplayEngine statement. May be NULL.

  @return Derived row kind.
**/
STATIC
MODERN_DISPLAY_FORM_ROW_KIND
ModernDisplayGetStatementKind (
  IN FORM_DISPLAY_ENGINE_STATEMENT  *Statement OPTIONAL
  )
{
  if ((Statement == NULL) || (Statement->OpCode == NULL)) {
    return ModernDisplayFormRowUnknown;
  }

  switch (Statement->OpCode->OpCode) {
    case EFI_IFR_ORDERED_LIST_OP:
      return ModernDisplayFormRowOrderedList;

    case EFI_IFR_ONE_OF_OP:
      return ModernDisplayFormRowChoice;

    case EFI_IFR_NUMERIC_OP:
      return ModernDisplayFormRowNumeric;

    case EFI_IFR_TIME_OP:
      return ModernDisplayFormRowTime;

    case EFI_IFR_DATE_OP:
      return ModernDisplayFormRowDate;

    case EFI_IFR_CHECKBOX_OP:
      return ModernDisplayFormRowCheckbox;

    case EFI_IFR_REF_OP:
      return ModernDisplayFormRowReference;

    case EFI_IFR_PASSWORD_OP:
      return ModernDisplayFormRowPassword;

    case EFI_IFR_STRING_OP:
      return ModernDisplayFormRowString;

    case EFI_IFR_TEXT_OP:
      return ModernDisplayFormRowText;

    case EFI_IFR_ACTION_OP:
      return ModernDisplayFormRowAction;

    case EFI_IFR_RESET_BUTTON_OP:
      return ModernDisplayFormRowResetButton;

    case EFI_IFR_SUBTITLE_OP:
      return ModernDisplayFormRowSubtitle;

    default:
      return ModernDisplayFormRowUnknown;
  }
}

/**
  Return whether a row kind uses choice/list-style key help.

  @param[in] Kind  Row kind to test.

  @retval TRUE   The row uses choice/list-style key help.
  @retval FALSE  The row uses another key help path.
**/
BOOLEAN
ModernDisplayFormRowIsChoiceLike (
  IN MODERN_DISPLAY_FORM_ROW_KIND  Kind
  )
{
  return (BOOLEAN)(
                    (Kind == ModernDisplayFormRowOrderedList) ||
                    (Kind == ModernDisplayFormRowChoice) ||
                    (Kind == ModernDisplayFormRowNumeric) ||
                    (Kind == ModernDisplayFormRowTime) ||
                    (Kind == ModernDisplayFormRowDate)
                    );
}

/**
  Return whether a row kind uses action/text-style key help.

  @param[in] Kind  Row kind to test.

  @retval TRUE   The row uses action/text-style key help.
  @retval FALSE  The row uses another key help path.
**/
BOOLEAN
ModernDisplayFormRowIsActionLike (
  IN MODERN_DISPLAY_FORM_ROW_KIND  Kind
  )
{
  return (BOOLEAN)(
                    (Kind == ModernDisplayFormRowReference) ||
                    (Kind == ModernDisplayFormRowPassword) ||
                    (Kind == ModernDisplayFormRowString) ||
                    (Kind == ModernDisplayFormRowText) ||
                    (Kind == ModernDisplayFormRowAction) ||
                    (Kind == ModernDisplayFormRowResetButton) ||
                    (Kind == ModernDisplayFormRowSubtitle)
                    );
}

/**
  Return whether a row kind behaves as editable value content.

  @param[in] Kind  Row kind to test.

  @retval TRUE   The row represents editable value content.
  @retval FALSE  The row is text/action/chrome-like only.
**/
BOOLEAN
ModernDisplayFormRowIsEditable (
  IN MODERN_DISPLAY_FORM_ROW_KIND  Kind
  )
{
  return (BOOLEAN)(
                    ModernDisplayFormRowIsChoiceLike (Kind) ||
                    (Kind == ModernDisplayFormRowCheckbox) ||
                    (Kind == ModernDisplayFormRowPassword) ||
                    (Kind == ModernDisplayFormRowString)
                    );
}

/**
  Return whether a row kind has no value/action affordance.

  @param[in] Kind  Row kind to test.

  @retval TRUE   The row is display text only.
  @retval FALSE  The row has another affordance.
**/
BOOLEAN
ModernDisplayFormRowIsTextOnly (
  IN MODERN_DISPLAY_FORM_ROW_KIND  Kind
  )
{
  return (BOOLEAN)(
                    (Kind == ModernDisplayFormRowText) ||
                    (Kind == ModernDisplayFormRowSubtitle) ||
                    (Kind == ModernDisplayFormRowUnknown)
                    );
}

/**
  Return whether a BrowserStatus value indicates the current page has invalid
  or blocked input feedback to show.

  @param[in] BrowserStatus  DisplayEngine BrowserStatus value.

  @retval TRUE   The status represents invalid/warning/no-submit feedback.
  @retval FALSE  The status is success or non-row-specific action feedback.
**/
STATIC
BOOLEAN
ModernDisplayBrowserStatusIsInvalid (
  IN UINT32  BrowserStatus
  )
{
  switch (BrowserStatus) {
    case BROWSER_WARNING_IF:
    case BROWSER_NO_SUBMIT_IF:
    case BROWSER_INCONSISTENT_IF:
    case BROWSER_SUBMIT_FAIL:
      return TRUE;
    default:
      return FALSE;
  }
}

/**
  Map a private form row model to the shared Modern UI renderer row role.

  @param[in] Row  Row model to inspect. May be NULL.

  @return Renderer row role for conservative DisplayEngine row painting.
**/
MODERN_UI_ROW_ROLE
ModernDisplayFormRowGetVisualRole (
  IN CONST MODERN_DISPLAY_FORM_ROW  *Row OPTIONAL
  )
{
  if (Row == NULL) {
    return ModernUiRowNormal;
  }

  if ((Row->State & ModernDisplayFormRowStateInvalid) != 0) {
    return ModernUiRowWarning;
  }

  if ((Row->State & ModernDisplayFormRowStateHighlighted) != 0) {
    return ModernUiRowSelected;
  }

  if ((Row->State & ModernDisplayFormRowStateDisabled) != 0) {
    return ModernUiRowDisabled;
  }

  if ((Row->State & ModernDisplayFormRowStateReadOnly) != 0) {
    return ModernUiRowReadOnly;
  }

  if (Row->Kind == ModernDisplayFormRowSubtitle) {
    return ModernUiRowSubtitle;
  }

  if (ModernDisplayFormRowIsActionLike (Row->Kind) && !ModernDisplayFormRowIsTextOnly (Row->Kind)) {
    return ModernUiRowAction;
  }

  return ModernUiRowNormal;
}

/**
  Classify a DisplayEngine statement into a private Modern UI row model.

  @param[in]  Statement  DisplayEngine statement to classify. May be NULL.
  @param[in]  Selected   TRUE when the row is in edit/selection mode.
  @param[out] Row        Row model to fill. Must not be NULL.

  @retval EFI_SUCCESS            Row was filled. A NULL Statement produces an
                                  Unknown row.
  @retval EFI_INVALID_PARAMETER  Row is NULL.
**/
EFI_STATUS
ModernDisplayClassifyStatement (
  IN  FORM_DISPLAY_ENGINE_STATEMENT  *Statement OPTIONAL,
  IN  BOOLEAN                        Selected,
  OUT MODERN_DISPLAY_FORM_ROW        *Row
  )
{
  return ModernDisplayClassifyStatementForForm (NULL, Statement, FALSE, Selected, Row);
}

/**
  Classify a DisplayEngine statement into a private Modern UI row model with
  optional page/form context.

  @param[in]  FormData   DisplayEngine form that owns the statement. May be
                         NULL when only statement-local state is available.
  @param[in]  Statement  DisplayEngine statement to classify. May be NULL.
  @param[in]  Highlight  TRUE when the row has keyboard highlight.
  @param[in]  Selected   TRUE when the row is in edit/selection mode.
  @param[out] Row        Row model to fill. Must not be NULL.

  @retval EFI_SUCCESS            Row was filled. A NULL Statement produces an
                                  Unknown row.
  @retval EFI_INVALID_PARAMETER  Row is NULL.
**/
EFI_STATUS
ModernDisplayClassifyStatementForForm (
  IN  FORM_DISPLAY_ENGINE_FORM       *FormData OPTIONAL,
  IN  FORM_DISPLAY_ENGINE_STATEMENT  *Statement OPTIONAL,
  IN  BOOLEAN                        Highlight,
  IN  BOOLEAN                        Selected,
  OUT MODERN_DISPLAY_FORM_ROW        *Row
  )
{
  UINT32  State;

  if (Row == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Row, sizeof (*Row));
  Row->Statement = Statement;
  Row->Kind      = ModernDisplayGetStatementKind (Statement);
  Row->OpCode    = ((Statement != NULL) && (Statement->OpCode != NULL)) ? Statement->OpCode->OpCode : 0;

  State = 0;
  ModernDisplayAddRowState (&State, ModernDisplayFormRowStateHighlighted, Highlight);
  ModernDisplayAddRowState (&State, ModernDisplayFormRowStateSelected, Selected);
  if (FormData != NULL) {
    ModernDisplayAddRowState (&State, ModernDisplayFormRowStateModal, (BOOLEAN)((FormData->Attribute & HII_DISPLAY_MODAL) != 0));
    ModernDisplayAddRowState (&State, ModernDisplayFormRowStatePageChanged, FormData->SettingChangedFlag);
    ModernDisplayAddRowState (
      &State,
      ModernDisplayFormRowStateInvalid,
      (BOOLEAN)(Highlight && ModernDisplayBrowserStatusIsInvalid (FormData->BrowserStatus))
      );
  }

  if (Statement != NULL) {
    ModernDisplayAddRowState (&State, ModernDisplayFormRowStateDisabled, (BOOLEAN)((Statement->Attribute & (HII_DISPLAY_GRAYOUT | HII_DISPLAY_LOCK)) != 0));
    ModernDisplayAddRowState (&State, ModernDisplayFormRowStateReadOnly, (BOOLEAN)((Statement->Attribute & HII_DISPLAY_READONLY) != 0));
    ModernDisplayAddRowState (&State, ModernDisplayFormRowStateChanged, Statement->SettingChangedFlag);
    ModernDisplayAddRowState (&State, ModernDisplayFormRowStateHexInput, ModernDisplayStatementUsesHexInput (Statement));
    ModernDisplayAddRowState (
      &State,
      ModernDisplayFormRowStateAdjustable,
      (BOOLEAN)((Row->Kind == ModernDisplayFormRowNumeric) && (LibGetFieldFromNum (Statement->OpCode) != 0))
      );
  }

  Row->State = State;
  return EFI_SUCCESS;
}

/**
  Build a private Modern UI view model from DisplayEngine form data.

  The model borrows FormData and statement pointers. It does not allocate row
  storage and does not copy strings. ModernDisplayFormModelClear() resets only
  the caller-provided model storage.

  @param[in]  FormData              DisplayEngine form data. Must not be NULL.
  @param[in]  HighlightedStatement  Current highlighted statement. May be NULL.
  @param[in]  Selected              TRUE when the highlighted row is selected.
  @param[in]  Layout                Calculated Modern DisplayEngine layout. Must
                                    not be NULL.
  @param[out] Model                 Model to fill. Must not be NULL.

  @retval EFI_SUCCESS            Model was built.
  @retval EFI_INVALID_PARAMETER  A required parameter is NULL.
**/
EFI_STATUS
ModernDisplayFormModelBuild (
  IN  FORM_DISPLAY_ENGINE_FORM       *FormData,
  IN  FORM_DISPLAY_ENGINE_STATEMENT  *HighlightedStatement OPTIONAL,
  IN  BOOLEAN                        Selected,
  IN  CONST MODERN_DISPLAY_LAYOUT    *Layout,
  OUT MODERN_DISPLAY_FORM_MODEL      *Model
  )
{
  EFI_STATUS                     Status;
  LIST_ENTRY                     *Link;
  FORM_DISPLAY_ENGINE_STATEMENT  *Statement;

  if ((FormData == NULL) || (Layout == NULL) || (Model == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ModernDisplayFormModelClear (Model);
  Model->FormData              = FormData;
  Model->HighlightedStatement  = HighlightedStatement;
  Model->Modal                 = (BOOLEAN)((FormData->Attribute & HII_DISPLAY_MODAL) != 0);
  Model->SettingChanged        = FormData->SettingChangedFlag;
  CopyMem (&Model->Layout, Layout, sizeof (Model->Layout));

  Link = GetFirstNode (&FormData->StatementListHead);
  while (!IsNull (&FormData->StatementListHead, Link)) {
    Statement = FORM_DISPLAY_ENGINE_STATEMENT_FROM_LINK (Link);
    Link      = GetNextNode (&FormData->StatementListHead, Link);

    if (Statement == HighlightedStatement) {
      Model->HighlightedRowIndex = Model->RowCount;
      Model->HasHighlightedRow   = TRUE;
    }

    Model->RowCount++;
  }

  Status = ModernDisplayClassifyStatementForForm (
             FormData,
             HighlightedStatement,
             Model->HasHighlightedRow,
             Selected,
             &Model->HighlightedRow
             );
  if (EFI_ERROR (Status)) {
    ModernDisplayFormModelClear (Model);
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Clear a private Modern UI form model.

  Because the model stores borrowed pointers only, this helper zeroes the model
  and does not free any FormBrowser-owned data.

  @param[in,out] Model  Model to clear. May be NULL.
**/
VOID
ModernDisplayFormModelClear (
  IN OUT MODERN_DISPLAY_FORM_MODEL  *Model OPTIONAL
  )
{
  if (Model != NULL) {
    ZeroMem (Model, sizeof (*Model));
  }
}
