/** @file
  Private FormBrowser DisplayEngine to Modern UI form view model helpers.

  This file is internal to ModernUiCustomizedDisplayLib.  It consumes only the
  already-materialized FORM_DISPLAY_ENGINE_* data and opcode metadata supplied by
  edk2 FormBrowser/DisplayEngine.  It does not own browser policy, storage, or
  form routing semantics.

  The model stores borrowed pointers into FormData/Statement data.  Callers must
  keep the source FormData alive for the lifetime of the model and must call
  ModernDisplayFormModelClear() before reusing the model storage.

Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

/**
  High-level Modern UI row kind derived from a DisplayEngine statement opcode.

  The values are UX classifications only.  The owning FormBrowser keeps all HII
  and browser behavior.
**/
typedef enum {
  ModernDisplayFormRowUnknown,
  ModernDisplayFormRowChoice,
  ModernDisplayFormRowOrderedList,
  ModernDisplayFormRowNumeric,
  ModernDisplayFormRowDate,
  ModernDisplayFormRowTime,
  ModernDisplayFormRowCheckbox,
  ModernDisplayFormRowReference,
  ModernDisplayFormRowPassword,
  ModernDisplayFormRowString,
  ModernDisplayFormRowText,
  ModernDisplayFormRowAction,
  ModernDisplayFormRowResetButton,
  ModernDisplayFormRowSubtitle
} MODERN_DISPLAY_FORM_ROW_KIND;

/**
  UX state bits for a Modern UI form row.
**/
typedef enum {
  ModernDisplayFormRowStateNone       = 0,
  ModernDisplayFormRowStateHighlighted = BIT0,
  ModernDisplayFormRowStateSelected   = BIT1,
  ModernDisplayFormRowStateDisabled   = BIT2,
  ModernDisplayFormRowStateReadOnly   = BIT3,
  ModernDisplayFormRowStateChanged    = BIT4,
  ModernDisplayFormRowStateHexInput   = BIT5,
  ModernDisplayFormRowStateAdjustable = BIT6,
  ModernDisplayFormRowStateInvalid    = BIT7,
  ModernDisplayFormRowStateModal      = BIT8,
  ModernDisplayFormRowStatePageChanged = BIT9
} MODERN_DISPLAY_FORM_ROW_STATE;

/**
  One row in the private Modern UI form model.

  Statement and string-like fields are borrowed.  This foundation currently keeps
  only the statement pointer and derived UX metadata to avoid taking ownership of
  FormBrowser data.
**/
typedef struct {
  FORM_DISPLAY_ENGINE_STATEMENT     *Statement;
  MODERN_DISPLAY_FORM_ROW_KIND      Kind;
  UINT32                            State;
  UINT8                             OpCode;
} MODERN_DISPLAY_FORM_ROW;

/**
  Private page/form model used by ModernUiCustomizedDisplayLib.

  FormData and HighlightedStatement are borrowed.  Layout is copied from the
  caller to centralize the DisplayEngine-to-Modern UI view description without
  adding a public API surface.
**/
typedef struct {
  FORM_DISPLAY_ENGINE_FORM          *FormData;
  FORM_DISPLAY_ENGINE_STATEMENT     *HighlightedStatement;
  MODERN_DISPLAY_LAYOUT             Layout;
  UINTN                             RowCount;
  UINTN                             HighlightedRowIndex;
  BOOLEAN                           HasHighlightedRow;
  BOOLEAN                           Modal;
  BOOLEAN                           SettingChanged;
  MODERN_DISPLAY_FORM_ROW           HighlightedRow;
} MODERN_DISPLAY_FORM_MODEL;

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
  );

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
  );

/**
  Return whether a row kind uses choice/list-style key help.

  @param[in] Kind  Row kind to test.

  @retval TRUE   The row uses choice/list-style key help.
  @retval FALSE  The row uses another key help path.
**/
BOOLEAN
ModernDisplayFormRowIsChoiceLike (
  IN MODERN_DISPLAY_FORM_ROW_KIND  Kind
  );

/**
  Return whether a row kind uses action/text-style key help.

  @param[in] Kind  Row kind to test.

  @retval TRUE   The row uses action/text-style key help.
  @retval FALSE  The row uses another key help path.
**/
BOOLEAN
ModernDisplayFormRowIsActionLike (
  IN MODERN_DISPLAY_FORM_ROW_KIND  Kind
  );

/**
  Return whether a row kind behaves as editable value content.

  @param[in] Kind  Row kind to test.

  @retval TRUE   The row represents editable value content.
  @retval FALSE  The row is text/action/chrome-like only.
**/
BOOLEAN
ModernDisplayFormRowIsEditable (
  IN MODERN_DISPLAY_FORM_ROW_KIND  Kind
  );

/**
  Return whether a row kind has no value/action affordance.

  @param[in] Kind  Row kind to test.

  @retval TRUE   The row is display text only.
  @retval FALSE  The row has another affordance.
**/
BOOLEAN
ModernDisplayFormRowIsTextOnly (
  IN MODERN_DISPLAY_FORM_ROW_KIND  Kind
  );

/**
  Map a private form row model to the shared Modern UI renderer row role.

  @param[in] Row  Row model to inspect. May be NULL.

  @return Renderer row role for conservative DisplayEngine row painting.
**/
MODERN_UI_ROW_ROLE
ModernDisplayFormRowGetVisualRole (
  IN CONST MODERN_DISPLAY_FORM_ROW  *Row OPTIONAL
  );

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
  );

/**
  Clear a private Modern UI form model.

  Because the model stores borrowed pointers only, this helper zeroes the model
  and does not free any FormBrowser-owned data.

  @param[in,out] Model  Model to clear. May be NULL.
**/
VOID
ModernDisplayFormModelClear (
  IN OUT MODERN_DISPLAY_FORM_MODEL  *Model OPTIONAL
  );
