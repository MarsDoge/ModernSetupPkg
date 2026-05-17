/** @file
  Shared ModernSetup visual engine interfaces.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_ENGINE_H_
#define MODERN_UI_ENGINE_H_

#include <Uefi.h>

#include <ModernUi/ModernUiRenderer.h>
#include <ModernUi/ModernUiTheme.h>

typedef enum {
  ModernUiRowNormal,
  ModernUiRowSelected,
  ModernUiRowDisabled,
  ModernUiRowReadOnly,
  ModernUiRowSubtitle,
  ModernUiRowAction,
  ModernUiRowWarning
} MODERN_UI_ROW_ROLE;

typedef enum {
  ModernUiValueNone,
  ModernUiValueText,
  ModernUiValueCheckbox,
  ModernUiValueOneOf,
  ModernUiValueNumeric,
  ModernUiValueString,
  ModernUiValuePassword,
  ModernUiValueDateTime,
  ModernUiValueOrderedList,
  ModernUiValueAction
} MODERN_UI_VALUE_TYPE;

typedef struct {
  MODERN_UI_RECT    Header;
  MODERN_UI_RECT    TabBar;
  MODERN_UI_RECT    Content;
  MODERN_UI_RECT    Footer;
  MODERN_UI_RECT    RightRail;
  BOOLEAN           RightRailVisible;
} MODERN_UI_LAYOUT;

typedef struct {
  CONST CHAR16    *Text;
} MODERN_UI_TAB_MODEL;

typedef struct {
  MODERN_UI_LAYOUT                Layout;
  MODERN_UI_RECT                  Rect;
  CONST MODERN_UI_TAB_MODEL       *Tabs;
  UINTN                           TabCount;
  UINTN                           SelectedTab;
  CONST CHAR16                    *ProductName;
  CONST CHAR16                    *ModeName;
  CONST CHAR16                    *StatusText;
  BOOLEAN                         DrawRightRail;
} MODERN_UI_PAGE_MODEL;

typedef struct {
  MODERN_UI_RECT         Rect;
  CONST CHAR16           *Prompt;
  CONST CHAR16           *Value;
  MODERN_UI_ROW_ROLE     Role;
  MODERN_UI_VALUE_TYPE   ValueType;
} MODERN_UI_ROW_MODEL;

typedef struct {
  MODERN_UI_RECT         Rect;
  CONST CHAR16           *Text;
  MODERN_UI_VALUE_TYPE   Type;
  BOOLEAN                Selected;
} MODERN_UI_VALUE_MODEL;

typedef struct {
  MODERN_UI_RECT    Rect;
  CONST CHAR16      *Title;
} MODERN_UI_POPUP_MODEL;

/**
  Initialize a shared engine render context.

  @param[out] Context  Render context to initialize. Must not be NULL.

  @retval EFI_SUCCESS            Context was initialized.
  @retval EFI_INVALID_PARAMETER  Context is NULL.
  @retval others                 Status returned by ModernUiRendererInit().
**/
EFI_STATUS
EFIAPI
ModernUiEngineInit (
  OUT MODERN_UI_RENDER_CONTEXT  *Context
  );

/**
  Compute the standard ModernSetup page layout in pixels.

  @param[in]  Context         Initialized render context. Must not be NULL.
  @param[in]  HeaderHeight    Header height in pixels.
  @param[in]  FooterHeight    Footer height in pixels.
  @param[in]  Margin          Horizontal margin in pixels.
  @param[in]  RightRailWidth  Preferred right rail width in pixels.
  @param[out] Layout          Layout to fill. Must not be NULL.

  @retval EFI_SUCCESS            Layout was computed.
  @retval EFI_INVALID_PARAMETER  Context or Layout is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiEngineComputeLayout (
  IN  CONST MODERN_UI_RENDER_CONTEXT  *Context,
  IN  UINTN                           HeaderHeight,
  IN  UINTN                           FooterHeight,
  IN  UINTN                           Margin,
  IN  UINTN                           RightRailWidth,
  OUT MODERN_UI_LAYOUT                *Layout
  );

/**
  Draw the complete page chrome and content background.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Model    Page model describing chrome and selected tab. Must not be NULL.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Page chrome was drawn.
  @retval EFI_INVALID_PARAMETER  Context, Model, or Theme is NULL.
  @retval others                 Status returned by renderer primitives.
**/
EFI_STATUS
EFIAPI
ModernUiEngineDrawPage (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN CONST MODERN_UI_PAGE_MODEL *Model,
  IN CONST MODERN_UI_THEME      *Theme
  );

/**
  Draw a page tab strip.

  @param[in] Context      Initialized render context. Must not be NULL.
  @param[in] Rect         Tab strip rectangle.
  @param[in] Tabs         Tab model array. Must not be NULL when TabCount is nonzero.
  @param[in] TabCount     Number of entries in Tabs.
  @param[in] SelectedTab  Selected tab index.
  @param[in] Theme        Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Tabs were drawn.
  @retval EFI_INVALID_PARAMETER  Context, Tabs, or Theme is invalid.
  @retval others                 Status returned by renderer primitives.
**/
EFI_STATUS
EFIAPI
ModernUiEngineDrawTabs (
  IN MODERN_UI_RENDER_CONTEXT     *Context,
  IN MODERN_UI_RECT               Rect,
  IN CONST MODERN_UI_TAB_MODEL    *Tabs,
  IN UINTN                        TabCount,
  IN UINTN                        SelectedTab,
  IN CONST MODERN_UI_THEME        *Theme
  );

/**
  Draw a row array using shared ModernSetup row styling.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rows      Row model array. Must not be NULL when RowCount is nonzero.
  @param[in] RowCount  Number of entries in Rows.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Rows were drawn.
  @retval EFI_INVALID_PARAMETER  Context, Rows, or Theme is invalid.
  @retval others                 Status returned by renderer primitives.
**/
EFI_STATUS
EFIAPI
ModernUiEngineDrawRows (
  IN MODERN_UI_RENDER_CONTEXT   *Context,
  IN CONST MODERN_UI_ROW_MODEL  *Rows,
  IN UINTN                      RowCount,
  IN CONST MODERN_UI_THEME      *Theme
  );

/**
  Draw one visual value control.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Value    Value model to draw. Must not be NULL.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Value was drawn.
  @retval EFI_INVALID_PARAMETER  Context, Value, or Theme is NULL.
  @retval others                 Status returned by renderer primitives.
**/
EFI_STATUS
EFIAPI
ModernUiEngineDrawValue (
  IN MODERN_UI_RENDER_CONTEXT    *Context,
  IN CONST MODERN_UI_VALUE_MODEL *Value,
  IN CONST MODERN_UI_THEME       *Theme
  );

/**
  Draw a popup surface.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Popup    Popup model to draw. Must not be NULL.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Popup was drawn.
  @retval EFI_INVALID_PARAMETER  Context, Popup, or Theme is NULL.
  @retval others                 Status returned by renderer primitives.
**/
EFI_STATUS
EFIAPI
ModernUiEngineDrawPopup (
  IN MODERN_UI_RENDER_CONTEXT    *Context,
  IN CONST MODERN_UI_POPUP_MODEL *Popup,
  IN CONST MODERN_UI_THEME       *Theme
  );

/**
  Draw the shared footer/status strip.

  @param[in] Context     Initialized render context. Must not be NULL.
  @param[in] Rect        Footer rectangle.
  @param[in] StatusText  Optional status text. May be NULL.
  @param[in] Theme       Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Footer was drawn.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL, or Rect is empty.
  @retval others                 Status returned by renderer primitives.
**/
EFI_STATUS
EFIAPI
ModernUiEngineDrawFooter (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN CONST CHAR16              *StatusText,
  IN CONST MODERN_UI_THEME     *Theme
  );

#endif
