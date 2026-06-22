/** @file

  This library class defines a set of interfaces to customize Display module

Copyright (c) 2013-2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2025, Loongson Technology Corporation Limited. All rights reserved.<BR>
  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "CustomizedDisplayLibInternal.h"

EFI_SCREEN_DESCRIPTOR  gScreenDimensions;
CHAR16                 *mLibUnknownString;
extern EFI_HII_HANDLE  mCDLStringPackHandle;
CHAR16                 *mSpaceBuffer;
#define SPACE_BUFFER_SIZE  1000

STATIC MODERN_UI_RENDER_CONTEXT  mModernRenderContext;
STATIC BOOLEAN                   mModernRenderReady;
STATIC UINTN                     mModernCursorColumn;
STATIC UINTN                     mModernCursorRow;
//
// Text-grid row that most recently received row-level selection styling from
// ModernDisplayDrawStatementRow. The per-cell print path suppresses its flat
// highlight fill only on this row, so the styled selection bar shows through
// without affecting other EFI_RED-background text (e.g. highlighted popup
// options, which have no row-level styling underneath). (UINTN)-1 means none.
//
STATIC UINTN                     mModernStyledHighlightRow = (UINTN)-1;

#define MODERN_DISPLAY_HELP_LEFT_SKIPPED_COLUMNS  3

typedef enum {
  ModernDisplayPageStateLive,
  ModernDisplayPageStateLiveRefresh,
  ModernDisplayPageStateUnsaved,
  ModernDisplayPageStateRebootRequired,
  ModernDisplayPageStateModal
} MODERN_DISPLAY_PAGE_STATE;

STATIC
UINTN
ModernDisplayColumns (
  VOID
  );

STATIC
UINTN
ModernDisplayRows (
  VOID
  );

STATIC
VOID
ModernDisplayDrawStatementRowAccents (
  IN CONST MODERN_UI_RECT              *RowRect,
  IN CONST MODERN_DISPLAY_FORM_ROW     *FormRow,
  IN CONST MODERN_UI_THEME             *Theme
  );


STATIC
UINTN
ModernDisplayStatementTextInset (
  IN UINTN  Column,
  IN UINTN  Row,
  IN UINTN  Width,
  IN UINTN  CellWidth
  );

STATIC
UINTN
ModernDisplayRightHelpStartColumn (
  IN CONST MODERN_DISPLAY_LAYOUT  *Layout
  );

STATIC
VOID
ModernDisplayDrawRightHelpRailContext (
  IN CONST MODERN_DISPLAY_LAYOUT  *Layout,
  IN CONST MODERN_UI_THEME        *Theme,
  IN UINTN                        CellWidth,
  IN UINTN                        CellHeight
  );

STATIC
VOID
ModernDisplayDrawFormTitleContext (
  IN CONST MODERN_DISPLAY_LAYOUT  *Layout,
  IN CONST MODERN_UI_THEME        *Theme,
  IN UINTN                        CellWidth,
  IN UINTN                        CellHeight,
  IN CONST CHAR16                 *PrintableTitle
  );

STATIC
MODERN_DISPLAY_PAGE_STATE
ModernDisplayPageState (
  IN CONST FORM_DISPLAY_ENGINE_FORM  *FormData
  );

STATIC
CONST CHAR16 *
ModernDisplayPageStatusText (
  IN CONST FORM_DISPLAY_ENGINE_FORM  *FormData
  );

STATIC
UINTN
ModernDisplayFooterStatusReservedColumns (
  VOID
  );

/**
  Return GOP cell metrics that match the active text-mode grid.

  @param[out] CellWidth   Width in pixels for one text column. Must not be NULL.
  @param[out] CellHeight  Height in pixels for one text row. Must not be NULL.
**/
STATIC
VOID
ModernDisplayGetCellMetrics (
  OUT UINTN  *CellWidth,
  OUT UINTN  *CellHeight
  )
{
  UINTN  Columns;
  UINTN  Rows;

  ASSERT ((CellWidth != NULL) && (CellHeight != NULL));

  Columns     = ModernDisplayColumns ();
  Rows        = ModernDisplayRows ();
  *CellWidth  = MAX (1, mModernRenderContext.Width / Columns);
  *CellHeight = MAX (18, mModernRenderContext.Height / Rows);
}

/**
  Initialize the GOP-backed drawing context on first use.

  @retval EFI_SUCCESS  GOP and renderer state are available.
  @retval others       Status from ModernUiRendererInit().
**/
STATIC
EFI_STATUS
ModernDisplayEnsureRenderer (
  VOID
  )
{
  EFI_STATUS  Status;

  if (mModernRenderReady) {
    return EFI_SUCCESS;
  }

  Status = ModernUiRendererInit (&mModernRenderContext);
  if (!EFI_ERROR (Status)) {
    mModernRenderReady = TRUE;
  }

  return Status;
}

/**
  Clear the GOP-backed ModernSetup drawing surface and reset the emulated text
  cursor used by the display engine backend.
**/
VOID
ModernDisplayClearGop (
  VOID
  )
{
  if (!EFI_ERROR (ModernDisplayEnsureRenderer ())) {
    ModernUiClear (&mModernRenderContext, ModernUiGetTheme ()->Background);
  }

  mModernCursorColumn = 0;
  mModernCursorRow    = 0;
}

/**
  Return the current text-mode column count used as a layout grid.

  @return Number of columns. A conservative 80-column default is used if the
          text output mode cannot report dimensions.
**/
STATIC
UINTN
ModernDisplayColumns (
  VOID
  )
{
  UINTN  Columns;
  UINTN  Rows;

  Columns = 0;
  Rows    = 0;
  if ((gST != NULL) && (gST->ConOut != NULL) && (gST->ConOut->Mode != NULL)) {
    gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &Columns, &Rows);
  }

  return (Columns == 0) ? 80 : Columns;
}

/**
  Return the current text-mode row count used as a layout grid.

  @return Number of rows. A conservative 25-row default is used if the text
          output mode cannot report dimensions.
**/
STATIC
UINTN
ModernDisplayRows (
  VOID
  )
{
  UINTN  Columns;
  UINTN  Rows;

  Columns = 0;
  Rows    = 0;
  if ((gST != NULL) && (gST->ConOut != NULL) && (gST->ConOut->Mode != NULL)) {
    gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &Columns, &Rows);
  }

  return (Rows == 0) ? 25 : Rows;
}

/**
  Calculate the text-grid layout used by the GOP DisplayEngine chrome.

  The returned statement rectangle is the only area where native FormBrowser
  statements should be printed. Other fields describe the surrounding chrome.

  @param[out] Layout  Layout description to fill. Must not be NULL.

  @retval EFI_SUCCESS            Layout was calculated.
  @retval EFI_INVALID_PARAMETER  Layout is NULL.
**/
EFI_STATUS
ModernDisplayCalculateLayout (
  OUT MODERN_DISPLAY_LAYOUT  *Layout
  )
{
  UINTN  ScreenColumns;
  UINTN  HorizontalMargin;

  if (Layout == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Layout, sizeof (*Layout));

  ScreenColumns          = gScreenDimensions.RightColumn - gScreenDimensions.LeftColumn;
  HorizontalMargin       = (ScreenColumns > (2 * MODERN_SETUP_HORIZONTAL_MARGIN + 40)) ? MODERN_SETUP_HORIZONTAL_MARGIN : 0;
  Layout->HeaderRows     = (gClassOfVfr == FORMSET_CLASS_FRONT_PAGE) ? FRONT_PAGE_HEADER_HEIGHT : NONE_FRONT_PAGE_HEADER_HEIGHT;
  Layout->FooterTopRow   = gScreenDimensions.BottomRow - STATUS_BAR_HEIGHT - gFooterHeight;
  Layout->ContentTopRow  = gScreenDimensions.TopRow + Layout->HeaderRows;
  Layout->ContentBottomRow = Layout->FooterTopRow - MODERN_SETUP_CONTENT_BOTTOM_GAP;
  Layout->ContentLeftColumn = gScreenDimensions.LeftColumn + HorizontalMargin;
  Layout->ContentRightColumn = gScreenDimensions.RightColumn - HorizontalMargin;

  //
  // The decorative telemetry rail (CPU/Architecture/Memory/Voltage) is a static,
  // partly-placeholder panel that does not belong on a native FormBrowser form:
  // it shows no live data, steals horizontal width from the statements/help, and
  // reads as crude next to the real form content. Suppress it so the form
  // reclaims the full content width. The front-page App owns its own (real)
  // system summary; this engine only frames the native form. The layout fields
  // stay wired (rail rects zeroed) so help/watermark column math is unaffected.
  //
  Layout->RightRailVisible = FALSE;
  if (Layout->RightRailVisible) {
    Layout->RightRailRightColumn = Layout->ContentRightColumn;
    Layout->RightRailLeftColumn  = Layout->RightRailRightColumn - MODERN_SETUP_RIGHT_RAIL_COLUMNS;
    Layout->ContentRightColumn   = Layout->RightRailLeftColumn - 2;
  }

  Layout->Statement.TopRow    = Layout->ContentTopRow + MODERN_SETUP_CONTENT_TOP_GAP;
  Layout->Statement.BottomRow = Layout->ContentBottomRow;
  Layout->Statement.LeftColumn = Layout->ContentLeftColumn;
  Layout->Statement.RightColumn = Layout->ContentRightColumn;

  return EFI_SUCCESS;
}

/**
  Return a small GOP text inset for native FormBrowser text printed inside the
  modern statement list.

  The DisplayEngine still owns prompt/value text placement in text-grid cells.
  This helper only nudges the GOP glyph draw position inside the already assigned
  cells so the text does not visually collide with the FormModel accent rail or
  the rounded row surface. Cursor accounting and text-mode semantics stay
  unchanged.

  @param[in] Column     Text-grid column where the string starts.
  @param[in] Row        Text-grid row where the string is printed.
  @param[in] Width      Text-grid column count assigned to the print.
  @param[in] CellWidth  Pixel width for one text column.

  @return Pixel inset to add to the GOP text X coordinate.
**/
STATIC
UINTN
ModernDisplayStatementTextInset (
  IN UINTN  Column,
  IN UINTN  Row,
  IN UINTN  Width,
  IN UINTN  CellWidth
  )
{
  MODERN_DISPLAY_LAYOUT  Layout;
  UINTN                  EndColumn;

  if ((Width == 0) || EFI_ERROR (ModernDisplayCalculateLayout (&Layout))) {
    return 0;
  }

  EndColumn = Column + Width;
  if ((Row < Layout.Statement.TopRow) || (Row >= Layout.Statement.BottomRow) ||
      (EndColumn <= Layout.Statement.LeftColumn) || (Column >= Layout.Statement.RightColumn))
  {
    return 0;
  }

  if (Column <= (Layout.Statement.LeftColumn + 2)) {
    return MIN (10, MAX (4, CellWidth / 2));
  }

  return MIN (6, MAX (2, CellWidth / 3));
}

/**
  Return the approximate native FormBrowser help column start.

  The result mirrors the edk2 DisplayEngine prompt/option/help split only for
  presentation alignment. It must not alter FormBrowser widths, wrapping, HII
  ownership, or statement dimensions.

  @param[in] Layout  Calculated DisplayEngine layout. Must not be NULL.

  @return Text-grid column where the native help block starts, or Statement.RightColumn.
**/
STATIC
UINTN
ModernDisplayRightHelpStartColumn (
  IN CONST MODERN_DISPLAY_LAYOUT  *Layout
  )
{
  UINTN  StatementWidth;
  UINTN  OptionBlockWidth;
  UINTN  HelpBlockWidth;

  if ((Layout == NULL) || (Layout->Statement.RightColumn <= Layout->Statement.LeftColumn)) {
    return 0;
  }

  StatementWidth = Layout->Statement.RightColumn - Layout->Statement.LeftColumn;
  if (StatementWidth <= (MODERN_DISPLAY_HELP_LEFT_SKIPPED_COLUMNS + 4)) {
    return Layout->Statement.RightColumn;
  }

  OptionBlockWidth = (StatementWidth / 3) + 1;
  if (OptionBlockWidth <= (MODERN_DISPLAY_HELP_LEFT_SKIPPED_COLUMNS + 1)) {
    return Layout->Statement.RightColumn;
  }

  HelpBlockWidth = OptionBlockWidth - 1 - MODERN_DISPLAY_HELP_LEFT_SKIPPED_COLUMNS;
  if ((HelpBlockWidth == 0) || (HelpBlockWidth >= StatementWidth)) {
    return Layout->Statement.RightColumn;
  }

  return Layout->Statement.RightColumn - HelpBlockWidth;
}

/**
  Draw a presentation-only label and divider for FormBrowser contextual help.

  Native FormBrowser still owns the selected statement help text, wrapping, and
  print positions. This helper only marks the existing help column visually so
  the modern shell reads as statement list plus contextual-help region.

  @param[in] Layout      Calculated DisplayEngine layout. Must not be NULL.
  @param[in] Theme       Theme token table. Must not be NULL.
  @param[in] CellWidth   Pixel width for one text column.
  @param[in] CellHeight  Pixel height for one text row.
**/
STATIC
VOID
ModernDisplayDrawRightHelpRailContext (
  IN CONST MODERN_DISPLAY_LAYOUT  *Layout,
  IN CONST MODERN_UI_THEME        *Theme,
  IN UINTN                        CellWidth,
  IN UINTN                        CellHeight
  )
{
  UINTN  HelpLeftColumn;
  UINTN  LabelX;
  UINTN  LabelY;
  UINTN  LabelWidth;
  UINTN  DividerX;
  UINTN  DividerY;
  UINTN  DividerHeight;

  if ((Layout == NULL) || (Theme == NULL) || (CellWidth == 0) || (CellHeight == 0) ||
      (Layout->Statement.TopRow <= Layout->ContentTopRow) ||
      (Layout->Statement.RightColumn <= Layout->Statement.LeftColumn))
  {
    return;
  }

  HelpLeftColumn = ModernDisplayRightHelpStartColumn (Layout);
  if ((HelpLeftColumn <= Layout->Statement.LeftColumn) || (HelpLeftColumn >= Layout->Statement.RightColumn)) {
    return;
  }

  LabelX     = (HelpLeftColumn * CellWidth) + MIN (8, MAX (2, CellWidth / 2));
  LabelY     = (Layout->ContentTopRow * CellHeight) + MIN (6, MAX (2, CellHeight / 5));
  LabelWidth = (Layout->Statement.RightColumn - HelpLeftColumn) * CellWidth;

  //
  // Give the "CONTEXT HELP" label a soft accent (a muted gold, between the plain
  // muted body text and the full accent used by the primary CPU/Memory/Voltage
  // rail headers), so it reads as a styled section header while staying below the
  // telemetry rail in the visual hierarchy.
  //
  ModernUiDrawTextFit (
    &mModernRenderContext,
    LabelX,
    LabelY,
    LabelWidth,
    L"CONTEXT HELP",
    ModernUiBlendColor (Theme->AccentYellow, Theme->MutedText, 50),
    Theme->BackgroundBlack
    );

  DividerX      = (HelpLeftColumn * CellWidth > 6) ? (HelpLeftColumn * CellWidth - 6) : 0;
  DividerY      = Layout->Statement.TopRow * CellHeight;
  DividerHeight = (Layout->ContentBottomRow > Layout->Statement.TopRow) ?
                  ((Layout->ContentBottomRow - Layout->Statement.TopRow) * CellHeight) :
                  0;
  if (DividerHeight < 8) {
    return;
  }

  ModernUiFillRect (
    &mModernRenderContext,
    (MODERN_UI_RECT){ DividerX, DividerY + 6, 1, DividerHeight - 12 },
    ModernUiBlendColor (Theme->AccentOrange, Theme->Background, 24)
    );
}

/**
  Draw the current FormBrowser form title in the content-top context row.

  This is presentation-only: the title comes from FormBrowser-owned FormData and
  is already consumed by the chrome tab classifier. The helper only mirrors that
  existing title into the reserved gap above statement rows so screenshots and
  operators have visible page context without changing statement layout, HII
  routing, or storage semantics.

  @param[in] Layout          Calculated DisplayEngine layout. Must not be NULL.
  @param[in] Theme           Theme token table. Must not be NULL.
  @param[in] CellWidth       Pixel width for one text column.
  @param[in] CellHeight      Pixel height for one text row.
  @param[in] PrintableTitle  Printable form title text. May be NULL.
**/
STATIC
VOID
ModernDisplayDrawFormTitleContext (
  IN CONST MODERN_DISPLAY_LAYOUT  *Layout,
  IN CONST MODERN_UI_THEME        *Theme,
  IN UINTN                        CellWidth,
  IN UINTN                        CellHeight,
  IN CONST CHAR16                 *PrintableTitle
  )
{
  UINTN  TitleLeftColumn;
  UINTN  TitleRightColumn;
  UINTN  TitleX;
  UINTN  TitleY;
  UINTN  TitleWidth;

  if ((Layout == NULL) || (Theme == NULL) || (PrintableTitle == NULL) ||
      (PrintableTitle[0] == CHAR_NULL) || (CellWidth == 0) || (CellHeight == 0) ||
      (Layout->Statement.TopRow <= Layout->ContentTopRow))
  {
    return;
  }

  TitleLeftColumn  = Layout->ContentLeftColumn;
  TitleRightColumn = Layout->RightRailVisible ? Layout->RightRailLeftColumn - 2 : Layout->ContentRightColumn;
  if (TitleRightColumn <= TitleLeftColumn) {
    return;
  }

  TitleX     = TitleLeftColumn * CellWidth;
  TitleY     = (Layout->ContentTopRow * CellHeight) + MIN (6, MAX (2, CellHeight / 5));
  TitleWidth = (TitleRightColumn - TitleLeftColumn) * CellWidth;

  //
  // Render the form title as a prominent section header: brighter than plain
  // muted body text (kept as a blend toward MutedText so it still reads as a
  // heading, not a value), so each form is clearly anchored by its title.
  // Presentation only. (An accent underline below the title is intentionally
  // not drawn here: it sits in the content band the native FormBrowser repaints
  // after the chrome, which would wipe it.)
  //
  ModernUiDrawTextFit (
    &mModernRenderContext,
    TitleX,
    TitleY,
    TitleWidth,
    PrintableTitle,
    ModernUiBlendColor (Theme->Text, Theme->MutedText, 22),
    Theme->BackgroundBlack
    );
}

/**
  Draw an honest "<category> > <form title>" breadcrumb in the header tab band
  for native (non-front-page) FormBrowser forms.

  Replaces the decorative five-category tab strip, which on a native form read as
  clickable navigation but performed none -- FormBrowser owns navigation
  (Esc = back, arrows = move highlight). The breadcrumb instead states where the
  user is and makes the real form identity the prominent element. The category
  prefix is shown only for clearly classified forms (Devices/Boot/Security/Exit);
  an unclassified form shows just its title so no misleading label is attached.

  Presentation-only: the title comes from FormBrowser-owned FormData and the
  category is the same classifier the chrome already used. It does not alter
  form navigation, HII GUID binding, callbacks, or storage.

  @param[in] Layout          Calculated DisplayEngine layout. Must not be NULL.
  @param[in] Theme           Theme token table. Must not be NULL.
  @param[in] CellHeight      Pixel height for one text row.
  @param[in] CategoryIndex   Chrome tab classifier result (0..4).
  @param[in] PrintableTitle  Printable form title text. May be NULL.
**/
STATIC
VOID
ModernDisplayDrawFormBreadcrumb (
  IN CONST MODERN_DISPLAY_LAYOUT  *Layout,
  IN CONST MODERN_UI_THEME        *Theme,
  IN UINTN                        CellWidth,
  IN UINTN                        CellHeight,
  IN UINTN                        CategoryIndex,
  IN CONST CHAR16                 *PrintableTitle
  )
{
  UINTN         HeaderHeight;
  UINTN         BandY;
  UINTN         X;
  UINTN         Width;
  UINTN         PrefixWidth;
  CONST CHAR16  *Category;
  CHAR16        Prefix[64];

  if ((Layout == NULL) || (Theme == NULL) || (PrintableTitle == NULL) ||
      (PrintableTitle[0] == CHAR_NULL) || (CellWidth == 0) || (CellHeight == 0))
  {
    return;
  }

  HeaderHeight = Layout->HeaderRows * CellHeight;
  BandY        = (HeaderHeight > 52) ? (HeaderHeight - 52) : 0;
  X            = Layout->ContentLeftColumn * CellWidth;
  Width        = (Layout->ContentRightColumn > Layout->ContentLeftColumn) ?
                 ((Layout->ContentRightColumn - Layout->ContentLeftColumn) * CellWidth) : 0;
  if (Width == 0) {
    return;
  }

  //
  // Category prefix only for the clearly classified buckets; index 0 is both the
  // "Setup Categories" front bucket and the unmatched default, so prefixing it
  // would risk a wrong label -- show the bare title there.
  //
  Category = NULL;
  switch (CategoryIndex) {
    case 1:
      Category = ModernUiGetString (ModernUiStringPageDevices);
      break;
    case 2:
      Category = ModernUiGetString (ModernUiStringPageBoot);
      break;
    case 3:
      Category = ModernUiGetString (ModernUiStringPageSecurity);
      break;
    case 4:
      Category = ModernUiGetString (ModernUiStringPageExit);
      break;
    default:
      Category = NULL;
      break;
  }

  PrefixWidth = 0;
  if (Category != NULL) {
    UnicodeSPrint (Prefix, sizeof (Prefix), L"%s  >  ", Category);
    ModernUiDrawText (&mModernRenderContext, X, BandY + 8, Prefix, Theme->MutedText, Theme->BackgroundBlack);
    PrefixWidth = ModernUiMeasureText (Prefix);
  }

  //
  // The form title is the prominent element (bright Text), so the operator reads
  // the page identity at a glance instead of a faint sub-line.
  //
  ModernUiDrawTextFit (
    &mModernRenderContext,
    X + PrefixWidth,
    BandY + 8,
    (Width > PrefixWidth) ? (Width - PrefixWidth) : Width,
    PrintableTitle,
    Theme->Text,
    Theme->BackgroundBlack
    );

  //
  // A short accent underline keeps the modern bar affordance the tabs provided.
  //
  ModernUiFillRect (
    &mModernRenderContext,
    (MODERN_UI_RECT){ X, BandY + 34, MIN (Width, 240), 2 },
    Theme->AccentYellow
    );
}

/**
  Return normalized page-level state for the Modern DisplayEngine footer.

  This is intentionally presentation-only. It reflects FormBrowser-owned page
  state so future PEI/DXE/App data handoff and refresh flows have a stable UI
  place to surface "live", "changed", or "modal" state without moving policy or
  storage semantics into the renderer. `RebootRequired` is kept as an explicit
  UI state for a future platform/FormBrowser source; this helper does not infer
  it from generic changed state.

  @param[in] FormData  DisplayEngine form currently shown. May be NULL.

  @return Normalized page status state.
**/
STATIC
MODERN_DISPLAY_PAGE_STATE
ModernDisplayPageState (
  IN CONST FORM_DISPLAY_ENGINE_FORM  *FormData
  )
{
  if (FormData == NULL) {
    return ModernDisplayPageStateLive;
  }

  if ((FormData->Attribute & HII_DISPLAY_MODAL) != 0) {
    return ModernDisplayPageStateModal;
  }

  if (FormData->SettingChangedFlag) {
    return ModernDisplayPageStateUnsaved;
  }

  if (FormData->FormRefreshEvent != NULL) {
    return ModernDisplayPageStateLiveRefresh;
  }

  return ModernDisplayPageStateLive;
}

/**
  Return concise page-level status text for the Modern DisplayEngine footer.

  @param[in] FormData  DisplayEngine form currently shown. May be NULL.

  @return Static status string, or NULL when there is no status to surface.
**/
STATIC
CONST CHAR16 *
ModernDisplayPageStatusText (
  IN CONST FORM_DISPLAY_ENGINE_FORM  *FormData
  )
{
  switch (ModernDisplayPageState (FormData)) {
    case ModernDisplayPageStateModal:
      return L"MODAL VIEW";
    case ModernDisplayPageStateRebootRequired:
      return L"REBOOT REQUIRED";
    case ModernDisplayPageStateUnsaved:
      return L"UNSAVED CHANGES";
    case ModernDisplayPageStateLiveRefresh:
      return L"LIVE REFRESH";
    case ModernDisplayPageStateLive:
    default:
      //
      // The ordinary "nothing noteworthy" state shows no footer status pill: a
      // permanent "LIVE VIEW" badge added no information and rendered as a
      // clipped stub in the thin form footer. Only actionable states
      // (unsaved / reboot / modal / live-refresh) surface a pill now.
      //
      return NULL;
  }
}

/**
  Convert an EFI text attribute into a foreground pixel color.

  @param[in] Attribute  EFI text attribute from ConOut.

  @return Pixel color selected from the active ModernSetup theme.
**/
STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
ModernDisplayForeground (
  IN UINTN  Attribute
  )
{
  CONST MODERN_UI_THEME  *Theme;

  Theme = ModernUiGetTheme ();
  switch (Attribute & 0x0F) {
    case EFI_RED:
    case EFI_LIGHTRED:
      return Theme->WarningText;
    case EFI_GREEN:
    case EFI_LIGHTGREEN:
      return Theme->Success;
    case EFI_CYAN:
    case EFI_LIGHTCYAN:
    case EFI_BLUE:
    case EFI_LIGHTBLUE:
      return Theme->AccentOrange;
    case EFI_DARKGRAY:
    case EFI_LIGHTGRAY:
      return Theme->MutedText;
    case EFI_YELLOW:
      return Theme->AccentYellow;
    case EFI_BROWN:
    case EFI_WHITE:
    default:
      return Theme->Text;
  }
}

/**
  Convert an EFI text attribute into a background pixel color.

  @param[in] Attribute  EFI text attribute from ConOut.

  @return Pixel color selected from the active ModernSetup theme.
**/
STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
ModernDisplayBackground (
  IN UINTN  Attribute
  )
{
  CONST MODERN_UI_THEME  *Theme;

  Theme = ModernUiGetTheme ();
  switch ((Attribute >> 4) & 0x07) {
    case EFI_RED:
      return ModernUiGetSelectableRowBackground (TRUE, FALSE, FALSE, FALSE, Theme);
    case EFI_BLUE:
      return Theme->SurfaceRaised;
    case EFI_CYAN:
    case EFI_LIGHTBLUE:
      return ModernUiGetSelectableRowBackground (TRUE, FALSE, FALSE, FALSE, Theme);
    case EFI_BLACK:
      return Theme->BackgroundBlack;
    case EFI_LIGHTGRAY:
      return Theme->Surface;
    default:
      return Theme->Surface;
  }
}

/**
  Copy a formatted string into a printable buffer and remove HII width markers.

  @param[out] Output     Destination buffer. Must not be NULL.
  @param[in]  OutputLen  Number of CHAR16 entries in Output.
  @param[in]  Input      Source string. Must not be NULL.

  @return Number of visible characters copied.
**/
STATIC
UINTN
ModernDisplayCopyPrintable (
  OUT CHAR16        *Output,
  IN  UINTN         OutputLen,
  IN  CONST CHAR16  *Input
  )
{
  UINTN  Index;
  UINTN  OutIndex;

  if ((Output == NULL) || (OutputLen == 0) || (Input == NULL)) {
    return 0;
  }

  OutIndex = 0;
  for (Index = 0; (Input[Index] != CHAR_NULL) && ((OutIndex + 1) < OutputLen); Index++) {
    if ((Input[Index] == NARROW_CHAR) || (Input[Index] == WIDE_CHAR)) {
      continue;
    }

    //
    // Drop Unicode box-drawing glyphs (U+2500..U+257F). The native DisplayEngine
    // frames popups and multi-string boxes with these characters; under the
    // modern renderer the surrounding panel/surface already supplies the frame,
    // so rendering the glyphs only adds a retro dashed-border seam on top.
    //
    if ((Input[Index] >= 0x2500) && (Input[Index] <= 0x257F)) {
      continue;
    }

    Output[OutIndex++] = Input[Index];
  }

  Output[OutIndex] = CHAR_NULL;
  return OutIndex;
}

/**
  Pick the chrome tab that best matches the current form title.

  This is a visual hint only. Form navigation, HII GUID binding, callbacks, and
  storage behavior remain owned by edk2 SetupBrowser/FormBrowser.

  @param[in] Title  Printable form title. NULL selects the main tab.

  @return Zero-based chrome tab index.
**/
STATIC
UINTN
ModernDisplaySelectChromeTab (
  IN CONST CHAR16  *Title
  )
{
  if (Title == NULL) {
    return 0;
  }

  if ((StrStr (Title, L"Boot") != NULL) || (StrStr (Title, L"启动") != NULL)) {
    return 2;
  }

  if ((StrStr (Title, L"Device") != NULL) || (StrStr (Title, L"Driver") != NULL) ||
      (StrStr (Title, L"设备") != NULL))
  {
    return 1;
  }

  if ((StrStr (Title, L"Security") != NULL) || (StrStr (Title, L"安全") != NULL)) {
    return 3;
  }

  if ((StrStr (Title, L"Exit") != NULL) || (StrStr (Title, L"Save") != NULL) ||
      (StrStr (Title, L"退出") != NULL))
  {
    return 4;
  }

  return 0;
}

/**
  Draw the GOP surface behind a text-mode popup/dialog.

  @param[in] StartColumn  Left text-grid column of the popup.
  @param[in] EndColumn    Right text-grid column of the popup.
  @param[in] TopRow       Top text-grid row of the popup.
  @param[in] BottomRow    Bottom text-grid row of the popup.
**/
VOID
ModernDisplayDrawPopupSurface (
  IN UINTN  StartColumn,
  IN UINTN  EndColumn,
  IN UINTN  TopRow,
  IN UINTN  BottomRow
  )
{
  CONST MODERN_UI_THEME  *Theme;
  UINTN                  CellWidth;
  UINTN                  CellHeight;
  MODERN_UI_RECT         Rect;
  MODERN_UI_POPUP_MODEL  Popup;

  if ((EndColumn <= StartColumn) || (BottomRow <= TopRow) || EFI_ERROR (ModernDisplayEnsureRenderer ())) {
    return;
  }

  Theme = ModernUiGetTheme ();
  ModernDisplayGetCellMetrics (&CellWidth, &CellHeight);
  Rect = (MODERN_UI_RECT){
           StartColumn * CellWidth,
           TopRow * CellHeight,
           (EndColumn - StartColumn) * CellWidth,
           (BottomRow - TopRow) * CellHeight
         };

  Popup.Rect  = Rect;
  Popup.Title = NULL;
  ModernUiEngineDrawPopup (&mModernRenderContext, &Popup, Theme);
}

/**
  Select the accent color for a FormBrowser statement row.

  The color is a visual hint only. It is derived from the private FormModel row
  kind/state after FormBrowser has already materialized the statement. The helper
  does not inspect IFR packages, route storage, or change browser behavior.

  @param[in] FormRow  Private row model to inspect. May be NULL.
  @param[in] Theme    Theme token table. Must not be NULL.

  @return Accent color. NULL Theme returns zero.
**/
STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
ModernDisplayFormRowAccentColor (
  IN CONST MODERN_DISPLAY_FORM_ROW  *FormRow OPTIONAL,
  IN CONST MODERN_UI_THEME          *Theme
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Accent;

  ZeroMem (&Accent, sizeof (Accent));
  if (Theme == NULL) {
    return Accent;
  }

  if (FormRow == NULL) {
    return Theme->Border;
  }

  if ((FormRow->State & ModernDisplayFormRowStateInvalid) != 0) {
    return Theme->WarningText;
  }

  if ((FormRow->State & ModernDisplayFormRowStateDisabled) != 0) {
    return Theme->MutedText;
  }

  switch (FormRow->Kind) {
    case ModernDisplayFormRowChoice:
    case ModernDisplayFormRowOrderedList:
    case ModernDisplayFormRowNumeric:
    case ModernDisplayFormRowDate:
    case ModernDisplayFormRowTime:
      return Theme->AccentOrange;

    case ModernDisplayFormRowCheckbox:
    case ModernDisplayFormRowPassword:
    case ModernDisplayFormRowString:
      return Theme->AccentYellow;

    case ModernDisplayFormRowReference:
    case ModernDisplayFormRowAction:
    case ModernDisplayFormRowResetButton:
      return Theme->Success;

    case ModernDisplayFormRowSubtitle:
    case ModernDisplayFormRowText:
    case ModernDisplayFormRowUnknown:
    default:
      return Theme->Border;
  }
}

/**
  Map a DisplayEngine form-row kind to the shared engine control value type.

  This lets the in-setup DisplayEngine reuse the exact same per-control
  affordance vocabulary (ModernUiEngineDrawControlCue) that the front-page App
  value lane uses, so the same control type reads identically in both surfaces.
  Text-only / subtitle / unknown kinds map to ModernUiValueNone (no cue).

  @param[in] Kind  DisplayEngine form-row kind.

  @return The matching MODERN_UI_VALUE_TYPE, or ModernUiValueNone when the kind
          carries no control affordance.
**/
STATIC
MODERN_UI_VALUE_TYPE
ModernDisplayKindToValueType (
  IN MODERN_DISPLAY_FORM_ROW_KIND  Kind
  )
{
  switch (Kind) {
    case ModernDisplayFormRowCheckbox:
      return ModernUiValueCheckbox;
    case ModernDisplayFormRowChoice:
      return ModernUiValueOneOf;
    case ModernDisplayFormRowOrderedList:
      return ModernUiValueOrderedList;
    case ModernDisplayFormRowNumeric:
      return ModernUiValueNumeric;
    case ModernDisplayFormRowDate:
    case ModernDisplayFormRowTime:
      return ModernUiValueDateTime;
    case ModernDisplayFormRowPassword:
      return ModernUiValuePassword;
    case ModernDisplayFormRowString:
      return ModernUiValueString;
    case ModernDisplayFormRowReference:
    case ModernDisplayFormRowAction:
    case ModernDisplayFormRowResetButton:
      return ModernUiValueAction;
    default:
      return ModernUiValueNone;
  }
}

/**
  Draw lightweight FormModel-driven accents over one statement row surface.

  The native DisplayEngine still prints the prompt/value text. This function only
  paints non-semantic GOP hints: editable/action accent rails, changed markers,
  invalid borders, and disabled/read-only separators. Any draw failure leaves the
  already-painted row surface in place and is intentionally ignored by callers.

  @param[in] RowRect  Pixel row rectangle. Must not be NULL.
  @param[in] FormRow  Private row model. Must not be NULL.
  @param[in] Theme    Theme token table. Must not be NULL.
**/
STATIC
VOID
ModernDisplayDrawStatementRowAccents (
  IN CONST MODERN_UI_RECT           *RowRect,
  IN CONST MODERN_DISPLAY_FORM_ROW  *FormRow,
  IN CONST MODERN_UI_THEME          *Theme
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Accent;
  UINTN                          AccentWidth;
  UINTN                          MarkerSize;

  if ((RowRect == NULL) || (FormRow == NULL) || (Theme == NULL) || (RowRect->Width == 0) || (RowRect->Height == 0)) {
    return;
  }

  Accent      = ModernDisplayFormRowAccentColor (FormRow, Theme);
  AccentWidth = ((FormRow->State & ModernDisplayFormRowStateHighlighted) != 0) ? 6 : 3;

  if (!ModernDisplayFormRowIsTextOnly (FormRow->Kind) && (RowRect->Width > (AccentWidth + 4)) && (RowRect->Height > 6)) {
    ModernUiFillRect (
      &mModernRenderContext,
      (MODERN_UI_RECT){ RowRect->X, RowRect->Y + 2, AccentWidth, RowRect->Height - 4 },
      Accent
      );
  }

  if ((FormRow->State & ModernDisplayFormRowStateChanged) != 0) {
    MarkerSize = (RowRect->Height > 18) ? 6 : 4;
    if ((RowRect->Width > (MarkerSize + 8)) && (RowRect->Height > (MarkerSize + 6))) {
      ModernUiFillRect (
        &mModernRenderContext,
        (MODERN_UI_RECT){ RowRect->X + RowRect->Width - MarkerSize - 6, RowRect->Y + 4, MarkerSize, MarkerSize },
        Theme->AccentYellow
        );
    }
  }

  if ((FormRow->State & ModernDisplayFormRowStateInvalid) != 0) {
    ModernUiStrokeRect (&mModernRenderContext, *RowRect, Theme->WarningText);
    return;
  }

  if (((FormRow->State & ModernDisplayFormRowStateDisabled) != 0) || ((FormRow->State & ModernDisplayFormRowStateReadOnly) != 0)) {
    ModernUiFillRect (
      &mModernRenderContext,
      (MODERN_UI_RECT){ RowRect->X, RowRect->Y + RowRect->Height - 1, RowRect->Width, 1 },
      ModernUiBlendColor (Theme->Border, Theme->Background, 50)
      );
  }
}

/**
  Draw a ModernSetup row background for one FormBrowser statement.

  The caller still prints all statement text through the native DisplayEngine
  flow. This hook classifies the already-materialized statement with the private
  form row model and paints the GOP row surface beneath that text.

  @param[in] FormData   DisplayEngine form that owns Statement. May be NULL.
  @param[in] Statement  Statement to classify for row rendering. May be NULL.
  @param[in] Column     Text-grid column where the row starts.
  @param[in] Row        Text-grid row to paint.
  @param[in] Width      Text-grid column count to paint.
  @param[in] Highlight  TRUE when the row has keyboard highlight.
  @param[in] Selected   TRUE when the row is in edit/selection mode.
**/
VOID
EFIAPI
ModernDisplayDrawStatementRow (
  IN FORM_DISPLAY_ENGINE_FORM       *FormData OPTIONAL,
  IN FORM_DISPLAY_ENGINE_STATEMENT  *Statement OPTIONAL,
  IN UINTN    Column,
  IN UINTN    Row,
  IN UINTN    Width,
  IN BOOLEAN  Highlight,
  IN BOOLEAN  Selected
  )
{
  CONST MODERN_UI_THEME            *Theme;
  UINTN                            CellWidth;
  UINTN                            CellHeight;
  UINTN                            X;
  UINTN                            Y;
  UINTN                            PixelWidth;
  MODERN_UI_RECT                   RowRect;
  MODERN_UI_ROW_MODEL              RowModel;
  MODERN_DISPLAY_FORM_ROW          FormRow;

  if ((Width == 0) || EFI_ERROR (ModernDisplayEnsureRenderer ())) {
    return;
  }

  Theme = ModernUiGetTheme ();
  ModernDisplayGetCellMetrics (&CellWidth, &CellHeight);

  X          = Column * CellWidth;
  Y          = Row * CellHeight;
  PixelWidth = Width * CellWidth;
  RowRect    = (MODERN_UI_RECT){ X, Y, PixelWidth, CellHeight };
  RowModel.Rect      = RowRect;
  RowModel.Prompt    = NULL;
  RowModel.Value     = NULL;
  RowModel.ValueType = ModernUiValueNone;
  RowModel.Role      = ModernUiRowNormal;
  if (!EFI_ERROR (ModernDisplayClassifyStatementForForm (FormData, Statement, Highlight, Selected, &FormRow))) {
    RowModel.Role = ModernDisplayFormRowGetVisualRole (&FormRow);
  }

  ModernUiEngineDrawRows (&mModernRenderContext, &RowModel, 1, Theme);
  ModernDisplayDrawStatementRowAccents (&RowRect, &FormRow, Theme);

  //
  // Record the row that just received selected styling so the per-cell print
  // path knows to let it show through (and clear the record when this row is no
  // longer the selected one).
  //
  if (RowModel.Role == ModernUiRowSelected) {
    mModernStyledHighlightRow = Row;
  } else if (mModernStyledHighlightRow == Row) {
    mModernStyledHighlightRow = (UINTN)-1;
  }
}

/**
  Forget any tracked selection-styled row.

  The per-cell print path suppresses the highlight fill on the row recorded by
  ModernDisplayDrawStatementRow. That record is only valid while a form's
  statement rows are being drawn; a popup drawn on top prints its own
  EFI_RED-background text (e.g. a highlighted selectable option) without going
  through ModernDisplayDrawStatementRow, and could share that grid row. Callers
  invoke this at popup entry so a popup line is never mistaken for the styled
  statement row and left without its background fill.
**/
VOID
EFIAPI
ModernDisplayResetHighlightRowTracking (
  VOID
  )
{
  mModernStyledHighlightRow = (UINTN)-1;
}

/**
  Draw a per-opcode control affordance over an already-painted statement row.

  This runs AFTER native FormBrowser has printed the row's prompt/value text
  (and its highlight background), so the affordance is composited on top rather
  than being overpainted. It paints only a small non-semantic cue glyph at the
  row's right edge (clear of the value text) to make each control type read
  distinctly. It classifies already-materialized statement data and never reads,
  writes, or owns any HII/FormBrowser value or semantics.

  @param[in] FormData   DisplayEngine form that owns Statement. May be NULL.
  @param[in] Statement  Statement to classify. May be NULL (then no cue).
  @param[in] Column     Text-grid column where the row starts.
  @param[in] Row        Text-grid row of the statement.
  @param[in] Width      Text-grid column count of the row.
  @param[in] Highlight  TRUE when the row currently has keyboard highlight.
  @param[in] Selected   TRUE when the row is in edit/selection mode.
**/
VOID
EFIAPI
ModernDisplayDrawStatementRowCue (
  IN FORM_DISPLAY_ENGINE_FORM       *FormData OPTIONAL,
  IN FORM_DISPLAY_ENGINE_STATEMENT  *Statement OPTIONAL,
  IN UINTN    Column,
  IN UINTN    Row,
  IN UINTN    Width,
  IN BOOLEAN  Highlight,
  IN BOOLEAN  Selected
  )
{
  CONST MODERN_UI_THEME          *Theme;
  UINTN                          CellWidth;
  UINTN                          CellHeight;
  UINTN                          X;
  UINTN                          Y;
  UINTN                          PixelWidth;
  UINTN                          CueSide;
  MODERN_DISPLAY_FORM_ROW        FormRow;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  CueColor;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  FillColor;

  if ((Statement == NULL) || (Width == 0) || EFI_ERROR (ModernDisplayEnsureRenderer ())) {
    return;
  }

  if (EFI_ERROR (ModernDisplayClassifyStatementForForm (FormData, Statement, Highlight, Selected, &FormRow))) {
    return;
  }

  //
  // Only editable controls get a type affordance; text/subtitle and
  // disabled/read-only rows do not.
  //
  if (ModernDisplayFormRowIsTextOnly (FormRow.Kind) ||
      ((FormRow.State & (ModernDisplayFormRowStateDisabled | ModernDisplayFormRowStateReadOnly)) != 0))
  {
    return;
  }

  //
  // Widget-mapped rows (one-of/checkbox/numeric/string/password/ordered-list)
  // render as real controls via ModernDisplayDrawValueWidget, which carry their
  // own affordance, so skip the separate cue for them. Date/time (native
  // per-segment rendering) and action keep the cue.
  //
  if ((FormRow.Kind == ModernDisplayFormRowChoice) ||
      (FormRow.Kind == ModernDisplayFormRowCheckbox) ||
      (FormRow.Kind == ModernDisplayFormRowNumeric) ||
      (FormRow.Kind == ModernDisplayFormRowString) ||
      (FormRow.Kind == ModernDisplayFormRowPassword) ||
      (FormRow.Kind == ModernDisplayFormRowOrderedList))
  {
    return;
  }

  Theme = ModernUiGetTheme ();
  ModernDisplayGetCellMetrics (&CellWidth, &CellHeight);

  X          = Column * CellWidth;
  Y          = Row * CellHeight;
  PixelWidth = Width * CellWidth;
  if ((PixelWidth < 48) || (CellHeight < 10)) {
    return;
  }

  CueSide = MIN (CellHeight - 6, 14);

  //
  // High-contrast cue: dark on the bright selected band, bright yellow on the
  // dark surface. The cue sits at the row's right edge, clear of value text.
  //
  CueColor  = (Highlight || Selected) ? Theme->BackgroundBlack : Theme->AccentYellow;
  FillColor = ModernUiBlendColor (Theme->AccentOrange, Theme->BackgroundBlack, 70);

  ModernUiEngineDrawControlCue (
    &mModernRenderContext,
    (MODERN_UI_RECT){ X + PixelWidth - CueSide - 8, Y + (CellHeight - CueSide) / 2, CueSide, CueSide },
    ModernDisplayKindToValueType (FormRow.Kind),
    CueColor,
    FillColor
    );
}

/**
  Draw the text-input edit caret at a text-grid cell through the Modern renderer.

  Paints a thin vertical accent bar at the left edge of cell (Column, Row), using
  the same cell metrics as the themed text printer so it lands exactly on the
  character the caller is about to write. `ReadString` suppresses the native
  EFI_SIMPLE_TEXT_OUTPUT cursor (which would draw straight to the GraphicsConsole
  framebuffer and be invisible behind an off-screen canvas such as the LVGL
  backend) and calls this each keystroke after redrawing the field, so the caret
  renders identically on every backend. No-op (no error) when no renderer is
  available or the cell geometry is degenerate.

  @param[in] Column  Text-grid column of the caret cell.
  @param[in] Row     Text-grid row of the caret cell.
**/
VOID
EFIAPI
ModernDisplayDrawTextCaret (
  IN UINTN  Column,
  IN UINTN  Row
  )
{
  CONST MODERN_UI_THEME  *Theme;
  UINTN                  CellWidth;
  UINTN                  CellHeight;
  UINTN                  CaretWidth;
  UINTN                  Inset;

  if (EFI_ERROR (ModernDisplayEnsureRenderer ())) {
    return;
  }

  ModernDisplayGetCellMetrics (&CellWidth, &CellHeight);
  if ((CellWidth == 0) || (CellHeight < 6)) {
    return;
  }

  Theme      = ModernUiGetTheme ();
  CaretWidth = MAX (2, CellWidth / 8);
  Inset      = (CellHeight > 8) ? 2 : 1;

  ModernUiFillRect (
    &mModernRenderContext,
    (MODERN_UI_RECT){ Column * CellWidth, Row * CellHeight + Inset, CaretWidth, CellHeight - (2 * Inset) },
    Theme->AccentYellow
    );
}

/**
  Overlay a one-of row's value lane with the backend's best drop-down.

  Converts the text-grid value lane to pixels using the same cell metrics as the
  themed printer, then delegates to the shared ModernUiRenderOneOf, which renders
  a real lv_dropdown on the LVGL backend and a composed value box on GOP. See the
  contract on ModernDisplayDrawOneOfWidget in FormDisplay.h. No-op (no error) when
  no renderer is available, the value text is NULL, or the lane is degenerate.

  @param[in] Column     Text-grid column where the value lane starts.
  @param[in] Row        Text-grid row of the statement.
  @param[in] Width      Text-grid column count of the value lane.
  @param[in] ValueText  Selected option text. May be NULL.
  @param[in] Highlight  TRUE when the row currently has keyboard highlight.
  @param[in] Selected   TRUE when the row is in edit/selection mode.
**/
VOID
EFIAPI
ModernDisplayDrawValueWidget (
  IN UINT8         OpCode,
  IN UINTN         Column,
  IN UINTN         Row,
  IN UINTN         Width,
  IN CONST CHAR16  *ValueText OPTIONAL,
  IN BOOLEAN       Highlight,
  IN BOOLEAN       Selected
  )
{
  CONST MODERN_UI_THEME  *Theme;
  UINTN                  CellWidth;
  UINTN                  CellHeight;
  MODERN_UI_RECT         Rect;
  CHAR16                 Clean[128];
  UINTN                  Src;
  UINTN                  Dst;
  BOOLEAN                Sel;

  if ((ValueText == NULL) || (Width == 0) || EFI_ERROR (ModernDisplayEnsureRenderer ())) {
    return;
  }

  //
  // Only the widget-mapped opcodes are overlaid; everything else keeps its
  // native text plus the cue overlay.
  //
  if ((OpCode != EFI_IFR_ONE_OF_OP) && (OpCode != EFI_IFR_CHECKBOX_OP) &&
      (OpCode != EFI_IFR_NUMERIC_OP) && (OpCode != EFI_IFR_STRING_OP) &&
      (OpCode != EFI_IFR_PASSWORD_OP) && (OpCode != EFI_IFR_ORDERED_LIST_OP))
  {
    return;
  }

  ModernDisplayGetCellMetrics (&CellWidth, &CellHeight);
  if ((CellWidth == 0) || (CellHeight < 10)) {
    return;
  }

  //
  // FormBrowser embeds glyph-width markers (NARROW_CHAR/WIDE_CHAR, >= 0xFFF0) in
  // option strings; they are layout hints, not printable text. Strip them so the
  // widget shows clean text on both backends.
  //
  for (Src = 0, Dst = 0; (ValueText[Src] != CHAR_NULL) && (Dst < (ARRAY_SIZE (Clean) - 1)); Src++) {
    if (ValueText[Src] < 0xFFF0) {
      Clean[Dst++] = ValueText[Src];
    }
  }

  Clean[Dst] = CHAR_NULL;

  Theme       = ModernUiGetTheme ();
  Rect.X      = Column * CellWidth;
  Rect.Y      = Row * CellHeight;
  Rect.Width  = Width * CellWidth;
  Rect.Height = CellHeight;
  Sel         = (BOOLEAN)(Highlight || Selected);

  switch (OpCode) {
    case EFI_IFR_ONE_OF_OP:
      ModernUiRenderOneOf (&mModernRenderContext, Rect, Clean, Sel, Theme);
      break;
    case EFI_IFR_CHECKBOX_OP:
      ModernUiRenderCheckbox (&mModernRenderContext, Rect, Clean, Sel, Theme);
      break;
    case EFI_IFR_NUMERIC_OP:
      ModernUiRenderNumeric (&mModernRenderContext, Rect, Clean, Sel, Theme);
      break;
    case EFI_IFR_STRING_OP:
      ModernUiRenderString (&mModernRenderContext, Rect, Clean, Sel, Theme);
      break;
    case EFI_IFR_PASSWORD_OP:
      ModernUiRenderPassword (&mModernRenderContext, Rect, Clean, Sel, Theme);
      break;
    case EFI_IFR_ORDERED_LIST_OP:
      ModernUiRenderOrderedList (&mModernRenderContext, Rect, Clean, Sel, Theme);
      break;
    default:
      break;
  }
}

/**
  Clear a statement's value lane to the field background before native editing.

  See the contract on ModernDisplayClearValueLane in FormDisplay.h.

  @param[in] Column  Text-grid column where the value lane starts.
  @param[in] Row     Text-grid row of the statement.
  @param[in] Width   Text-grid column count of the value lane.
**/
VOID
EFIAPI
ModernDisplayClearValueLane (
  IN UINTN  Column,
  IN UINTN  Row,
  IN UINTN  Width
  )
{
  CONST MODERN_UI_THEME  *Theme;
  UINTN                  CellWidth;
  UINTN                  CellHeight;

  if ((Width == 0) || EFI_ERROR (ModernDisplayEnsureRenderer ())) {
    return;
  }

  ModernDisplayGetCellMetrics (&CellWidth, &CellHeight);
  if ((CellWidth == 0) || (CellHeight == 0)) {
    return;
  }

  Theme = ModernUiGetTheme ();
  ModernUiFillRect (
    &mModernRenderContext,
    (MODERN_UI_RECT){ Column * CellWidth, Row * CellHeight, Width * CellWidth, CellHeight },
    Theme->Surface
    );
}

/**
  Draw the modern DisplayEngine shell behind the native FormBrowser content.

  This function does not parse HII or own any FormBrowser semantics. It only
  paints the GOP chrome used by the customized display backend.

  @param[in] FormData  Form currently being displayed. Must not be NULL.
**/
STATIC
VOID
ModernDisplayDrawPageChrome (
  IN FORM_DISPLAY_ENGINE_FORM  *FormData
  )
{
  CONST MODERN_UI_THEME  *Theme;
  CHAR16                 *Title;
  CHAR16                 *PrintableTitle;
  UINTN                  CellWidth;
  UINTN                  CellHeight;
  MODERN_DISPLAY_LAYOUT  Layout;
  MODERN_UI_LAYOUT       EngineLayout;
  MODERN_UI_PAGE_MODEL   PageModel;
  MODERN_UI_TAB_MODEL    Tabs[5];
  UINTN                  HeaderHeight;
  UINTN                  CategoryIndex;
  BOOLEAN                IsFrontPage;

  ASSERT (FormData != NULL);
  if ((FormData == NULL) || EFI_ERROR (ModernDisplayEnsureRenderer ())) {
    return;
  }

  Theme = ModernUiGetTheme ();
  ModernDisplayGetCellMetrics (&CellWidth, &CellHeight);
  if (EFI_ERROR (ModernDisplayCalculateLayout (&Layout))) {
    return;
  }

  HeaderHeight = Layout.HeaderRows * CellHeight;

  Title          = LibGetToken (FormData->FormTitle, FormData->HiiHandle);
  PrintableTitle = NULL;
  if (Title != NULL) {
    PrintableTitle = AllocateZeroPool ((StrLen (Title) + 1) * sizeof (CHAR16));
    if (PrintableTitle != NULL) {
      ModernDisplayCopyPrintable (PrintableTitle, StrLen (Title) + 1, Title);
    }
  }

  EngineLayout.Header = (MODERN_UI_RECT){ 0, 0, mModernRenderContext.Width, HeaderHeight };
  EngineLayout.TabBar = EngineLayout.Header;
  EngineLayout.Footer = (MODERN_UI_RECT){
                         0,
                         Layout.FooterTopRow * CellHeight,
                         mModernRenderContext.Width,
                         mModernRenderContext.Height - (Layout.FooterTopRow * CellHeight)
                       };
  EngineLayout.Content = (MODERN_UI_RECT){
                          Layout.ContentLeftColumn * CellWidth,
                          Layout.ContentTopRow * CellHeight,
                          (Layout.ContentRightColumn - Layout.ContentLeftColumn) * CellWidth,
                          (Layout.ContentBottomRow - Layout.ContentTopRow) * CellHeight
                        };
  EngineLayout.RightRailVisible = Layout.RightRailVisible;
  if (Layout.RightRailVisible) {
    EngineLayout.RightRail = (MODERN_UI_RECT){
                              Layout.RightRailLeftColumn * CellWidth,
                              Layout.ContentTopRow * CellHeight,
                              (Layout.RightRailRightColumn - Layout.RightRailLeftColumn) * CellWidth,
                              (Layout.ContentBottomRow - Layout.ContentTopRow) * CellHeight
                            };
  } else {
    ZeroMem (&EngineLayout.RightRail, sizeof (EngineLayout.RightRail));
  }

  //
  // Localize the chrome through ModernUiStringLib so the in-setup header and
  // tab hints follow the active language (matching the front-page app), instead
  // of being pinned to English. The label set is a visual hint only; it does not
  // alter form navigation, HII GUID binding, callbacks, or storage.
  //
  Tabs[0].Text = ModernUiGetString (ModernUiStringPageDashboard);
  Tabs[1].Text = ModernUiGetString (ModernUiStringPageDevices);
  Tabs[2].Text = ModernUiGetString (ModernUiStringPageBoot);
  Tabs[3].Text = ModernUiGetString (ModernUiStringPageSecurity);
  Tabs[4].Text = ModernUiGetString (ModernUiStringPageExit);

  CategoryIndex = ModernDisplaySelectChromeTab (PrintableTitle);
  //
  // The native front page is a real menu, so it keeps the category tab strip.
  // Every other form reached via SendForm gets an honest breadcrumb title bar
  // instead: the five tabs there were decorative (they performed no navigation)
  // and read as clickable, while the real form title was only a faint sub-line.
  //
  IsFrontPage = (BOOLEAN)(gClassOfVfr == FORMSET_CLASS_FRONT_PAGE);

  CopyMem (&PageModel.Layout, &EngineLayout, sizeof (PageModel.Layout));
  PageModel.Rect          = EngineLayout.Header;
  PageModel.Tabs          = Tabs;
  PageModel.TabCount      = IsFrontPage ? ARRAY_SIZE (Tabs) : 0;
  PageModel.SelectedTab   = CategoryIndex;
  PageModel.ProductName   = ModernUiGetString (ModernUiStringHeaderTitle);
  PageModel.ModeName      = ModernUiGetString (ModernUiStringHeaderMode);
  PageModel.StatusText    = ModernDisplayPageStatusText (FormData);
  PageModel.DrawRightRail = FALSE;
  ModernUiEngineDrawPage (&mModernRenderContext, &PageModel, Theme);
  if (IsFrontPage) {
    ModernDisplayDrawFormTitleContext (&Layout, Theme, CellWidth, CellHeight, PrintableTitle);
  } else {
    ModernDisplayDrawFormBreadcrumb (&Layout, Theme, CellWidth, CellHeight, CategoryIndex, PrintableTitle);
  }

  ModernDisplayDrawRightHelpRailContext (&Layout, Theme, CellWidth, CellHeight);

  if (PrintableTitle != NULL) {
    FreePool (PrintableTitle);
  }

  if (Title != NULL) {
    FreePool (Title);
  }
}

/**
  Composite the OEM brand watermark into the content-area whitespace.

  Unlike the chrome (which the native FormBrowser repaints over via the
  statement rows and the empty-row clear loop), this overlay is meant to be
  invoked *after* the form content has been painted, so the mark lands on top of
  the freshly cleared whitespace rather than being wiped. The region handed to
  the renderer is the empty band from FirstEmptyRow down to the content bottom,
  so on a form whose rows fill the content area the band is too short and the
  renderer no-ops -- the mark never tints over a statement row.

  Display-only; parses no HII and owns no FormBrowser state. Safe to call on
  every form refresh.

  @param[in] FirstEmptyRow  Text-grid row where the empty area below the menu
                            begins.
**/
VOID
EFIAPI
ModernDisplayDrawOemWatermarkOverlay (
  IN UINTN  FirstEmptyRow
  )
{
  CONST MODERN_UI_THEME  *Theme;
  UINTN                  CellWidth;
  UINTN                  CellHeight;
  UINTN                  HelpStartColumn;
  UINTN                  WhitespaceTopRow;
  MODERN_DISPLAY_LAYOUT  Layout;
  MODERN_UI_RECT         Content;

  if (EFI_ERROR (ModernDisplayEnsureRenderer ())) {
    return;
  }

  Theme = ModernUiGetTheme ();
  ModernDisplayGetCellMetrics (&CellWidth, &CellHeight);
  if (EFI_ERROR (ModernDisplayCalculateLayout (&Layout))) {
    return;
  }

  //
  // Confine the mark to the statement value area (left of the native help
  // block). The help/right-rail columns are repainted by later control-flow
  // states in the same form pass, which would clip any mark that strayed into
  // them.
  //
  HelpStartColumn = ModernDisplayRightHelpStartColumn (&Layout);
  if (HelpStartColumn <= Layout.ContentLeftColumn) {
    return;
  }

  //
  // Vertically confine the mark to the empty band below the last menu row.
  // Clamp FirstEmptyRow into the content area; if the rows reach (or pass) the
  // content bottom there is no whitespace and the band height collapses, so the
  // renderer's minimum-size guard skips the mark instead of tinting a row.
  //
  WhitespaceTopRow = FirstEmptyRow;
  if (WhitespaceTopRow < Layout.ContentTopRow) {
    WhitespaceTopRow = Layout.ContentTopRow;
  }

  if (WhitespaceTopRow >= Layout.ContentBottomRow) {
    return;
  }

  Content.X      = Layout.ContentLeftColumn * CellWidth;
  Content.Y      = WhitespaceTopRow * CellHeight;
  Content.Width  = (HelpStartColumn - Layout.ContentLeftColumn) * CellWidth;
  Content.Height = (Layout.ContentBottomRow - WhitespaceTopRow) * CellHeight;

  ModernUiDrawOemWatermark (&mModernRenderContext, Content, Theme);
}

//
// Browser Global Strings
//
CHAR16  *gEnterString;
CHAR16  *gEnterCommitString;
CHAR16  *gEnterEscapeString;
CHAR16  *gEscapeString;
CHAR16  *gMoveHighlight;
CHAR16  *gDecNumericInput;
CHAR16  *gHexNumericInput;
CHAR16  *gToggleCheckBox;
CHAR16  *gLibEmptyString;
CHAR16  *gAreYouSure;
CHAR16  *gYesResponse;
CHAR16  *gNoResponse;
CHAR16  *gPlusString;
CHAR16  *gMinusString;
CHAR16  *gAdjustNumber;
CHAR16  *gSaveChanges;
CHAR16  *gNvUpdateMessage;
CHAR16  *gInputErrorMessage;

/**

  Print banner info for front page.

  @param[in]  FormData             Form Data to be shown in Page

**/
VOID
PrintBannerInfo (
  IN FORM_DISPLAY_ENGINE_FORM  *FormData
  )
{
  UINT8   Line;
  UINT8   Alignment;
  CHAR16  *StrFrontPageBanner;
  UINT8   RowIdx;
  UINT8   ColumnIdx;

  //
  //    ClearLines(0, LocalScreen.RightColumn, 0, BANNER_HEIGHT-1, BANNER_TEXT | BANNER_BACKGROUND);
  //
  ClearLines (
    gScreenDimensions.LeftColumn,
    gScreenDimensions.RightColumn,
    gScreenDimensions.TopRow,
    FRONT_PAGE_HEADER_HEIGHT - 1 + gScreenDimensions.TopRow,
    BANNER_TEXT | BANNER_BACKGROUND
    );

  //
  //    for (Line = 0; Line < BANNER_HEIGHT; Line++) {
  //
  for (Line = (UINT8)gScreenDimensions.TopRow; Line < BANNER_HEIGHT + (UINT8)gScreenDimensions.TopRow; Line++) {
    //
    //      for (Alignment = 0; Alignment < BANNER_COLUMNS; Alignment++) {
    //
    for (Alignment = (UINT8)gScreenDimensions.LeftColumn;
         Alignment < BANNER_COLUMNS + (UINT8)gScreenDimensions.LeftColumn;
         Alignment++
         )
    {
      RowIdx    = (UINT8)(Line - (UINT8)gScreenDimensions.TopRow);
      ColumnIdx = (UINT8)(Alignment - (UINT8)gScreenDimensions.LeftColumn);

      ASSERT (RowIdx < BANNER_HEIGHT && ColumnIdx < BANNER_COLUMNS);

      if ((gBannerData != NULL) && (gBannerData->Banner[RowIdx][ColumnIdx] != 0x0000)) {
        StrFrontPageBanner = LibGetToken (gBannerData->Banner[RowIdx][ColumnIdx], FormData->HiiHandle);
      } else {
        continue;
      }

      switch (Alignment - gScreenDimensions.LeftColumn) {
        case 0:
          //
          // Handle left column
          //
          PrintStringAt (gScreenDimensions.LeftColumn + BANNER_LEFT_COLUMN_INDENT, Line, StrFrontPageBanner);
          break;

        case 1:
          //
          // Handle center column
          //
          PrintStringAt (
            gScreenDimensions.LeftColumn + (gScreenDimensions.RightColumn - gScreenDimensions.LeftColumn) / 3,
            Line,
            StrFrontPageBanner
            );
          break;

        case 2:
          //
          // Handle right column
          //
          PrintStringAt (
            gScreenDimensions.LeftColumn + (gScreenDimensions.RightColumn - gScreenDimensions.LeftColumn) * 2 / 3,
            Line,
            StrFrontPageBanner
            );
          break;
      }

      FreePool (StrFrontPageBanner);
    }
  }
}

/**
  Print framework and form title for a page.

  @param[in]  FormData             Form Data to be shown in Page
**/
VOID
PrintFramework (
  IN FORM_DISPLAY_ENGINE_FORM  *FormData
  )
{
  if (gClassOfVfr != FORMSET_CLASS_PLATFORM_SETUP) {
    //
    // Only Setup page needs Framework
    //
    ClearLines (
      gScreenDimensions.LeftColumn,
      gScreenDimensions.RightColumn,
      gScreenDimensions.BottomRow - STATUS_BAR_HEIGHT - gFooterHeight,
      gScreenDimensions.BottomRow - STATUS_BAR_HEIGHT - 1,
      KEYHELP_TEXT | KEYHELP_BACKGROUND
      );
    return;
  }

  ClearLines (
    gScreenDimensions.LeftColumn,
    gScreenDimensions.RightColumn,
    gScreenDimensions.TopRow,
    gScreenDimensions.TopRow + NONE_FRONT_PAGE_HEADER_HEIGHT - 1,
    TITLE_TEXT | TITLE_BACKGROUND
    );

  ClearLines (
    gScreenDimensions.LeftColumn,
    gScreenDimensions.RightColumn,
    gScreenDimensions.TopRow + NONE_FRONT_PAGE_HEADER_HEIGHT,
    gScreenDimensions.BottomRow - STATUS_BAR_HEIGHT - gFooterHeight - 1,
    PcdGet8 (PcdBrowserFieldTextColor) | FIELD_BACKGROUND
    );

  ClearLines (
    gScreenDimensions.LeftColumn,
    gScreenDimensions.RightColumn,
    gScreenDimensions.BottomRow - STATUS_BAR_HEIGHT - gFooterHeight,
    gScreenDimensions.BottomRow - STATUS_BAR_HEIGHT - 1,
    KEYHELP_TEXT | KEYHELP_BACKGROUND
    );

  ModernDisplayDrawPageChrome (FormData);
}

/**
  Process some op code which is not recognized by browser core.

  @param OpCodeData                  The pointer to the op code buffer.

  @return EFI_SUCCESS            Pass the statement success.

**/
VOID
ProcessUserOpcode (
  IN  EFI_IFR_OP_HEADER  *OpCodeData
  )
{
  EFI_GUID  *ClassGuid;
  UINT8     ClassGuidNum;

  ClassGuid    = NULL;
  ClassGuidNum = 0;

  switch (OpCodeData->OpCode) {
    case EFI_IFR_FORM_SET_OP:
      //
      // process the statement outside of form,if it is formset op, get its formsetguid or classguid and compared with gFrontPageFormSetGuid
      //
      if (CompareMem (PcdGetPtr (PcdFrontPageFormSetGuid), &((EFI_IFR_FORM_SET *)OpCodeData)->Guid, sizeof (EFI_GUID)) == 0) {
        gClassOfVfr = FORMSET_CLASS_FRONT_PAGE;
      } else {
        ClassGuidNum = (UINT8)(((EFI_IFR_FORM_SET *)OpCodeData)->Flags & 0x3);
        ClassGuid    = (EFI_GUID *)(VOID *)((UINT8 *)OpCodeData + sizeof (EFI_IFR_FORM_SET));
        while (ClassGuidNum-- > 0) {
          if (CompareGuid ((EFI_GUID *)PcdGetPtr (PcdFrontPageFormSetGuid), ClassGuid)) {
            gClassOfVfr = FORMSET_CLASS_FRONT_PAGE;
            break;
          }

          ClassGuid++;
        }
      }

      break;

    case EFI_IFR_GUID_OP:
      if (CompareGuid (&gEfiIfrTianoGuid, (EFI_GUID *)((CHAR8 *)OpCodeData + sizeof (EFI_IFR_OP_HEADER)))) {
        //
        // Tiano specific GUIDed opcodes
        //
        switch (((EFI_IFR_GUID_LABEL *)OpCodeData)->ExtendOpCode) {
          case EFI_IFR_EXTEND_OP_LABEL:
            //
            // just ignore label
            //
            break;

          case EFI_IFR_EXTEND_OP_BANNER:
            //
            // Only in front page form set, we care about the banner data.
            //
            if (gClassOfVfr == FORMSET_CLASS_FRONT_PAGE) {
              //
              // Initialize Driver private data
              //
              if (gBannerData == NULL) {
                gBannerData = AllocateZeroPool (sizeof (BANNER_DATA));
                ASSERT (gBannerData != NULL);
              }

              CopyMem (
                &gBannerData->Banner[((EFI_IFR_GUID_BANNER *)OpCodeData)->LineNumber][
                                                                                      ((EFI_IFR_GUID_BANNER *)OpCodeData)->Alignment],
                &((EFI_IFR_GUID_BANNER *)OpCodeData)->Title,
                sizeof (EFI_STRING_ID)
                );
            }

            break;

          case EFI_IFR_EXTEND_OP_SUBCLASS:
            if (((EFI_IFR_GUID_SUBCLASS *)OpCodeData)->SubClass == EFI_FRONT_PAGE_SUBCLASS) {
              gClassOfVfr = FORMSET_CLASS_FRONT_PAGE;
            }

            break;

          default:
            break;
        }
      }

      break;

    default:
      break;
  }
}

/**
  Process some op codes which is out side of current form.

  @param FormData                Pointer to the form data.

  @return EFI_SUCCESS            Pass the statement success.

**/
VOID
ProcessExternedOpcode (
  IN FORM_DISPLAY_ENGINE_FORM  *FormData
  )
{
  LIST_ENTRY                     *Link;
  LIST_ENTRY                     *NestLink;
  FORM_DISPLAY_ENGINE_STATEMENT  *Statement;
  FORM_DISPLAY_ENGINE_STATEMENT  *NestStatement;

  Link = GetFirstNode (&FormData->StatementListOSF);
  while (!IsNull (&FormData->StatementListOSF, Link)) {
    Statement = FORM_DISPLAY_ENGINE_STATEMENT_FROM_LINK (Link);
    Link      = GetNextNode (&FormData->StatementListOSF, Link);

    ProcessUserOpcode (Statement->OpCode);
  }

  Link = GetFirstNode (&FormData->StatementListHead);
  while (!IsNull (&FormData->StatementListHead, Link)) {
    Statement = FORM_DISPLAY_ENGINE_STATEMENT_FROM_LINK (Link);
    Link      = GetNextNode (&FormData->StatementListHead, Link);

    ProcessUserOpcode (Statement->OpCode);

    NestLink = GetFirstNode (&Statement->NestStatementList);
    while (!IsNull (&Statement->NestStatementList, NestLink)) {
      NestStatement = FORM_DISPLAY_ENGINE_STATEMENT_FROM_LINK (NestLink);
      NestLink      = GetNextNode (&Statement->NestStatementList, NestLink);

      ProcessUserOpcode (NestStatement->OpCode);
    }
  }
}

/**
  Validate the input screen dimension info.

  @param  FormData               The input form data info.

  @return EFI_SUCCESS            The input screen info is acceptable.
  @return EFI_INVALID_PARAMETER  The input screen info is not acceptable.

**/
EFI_STATUS
ScreenDimensionInfoValidate (
  IN FORM_DISPLAY_ENGINE_FORM  *FormData
  )
{
  LIST_ENTRY  *Link;
  UINTN       Index;

  //
  // Calculate total number of Register HotKeys.
  //
  Index = 0;
  if (!IsListEmpty (&FormData->HotKeyListHead)) {
    Link = GetFirstNode (&FormData->HotKeyListHead);
    while (!IsNull (&FormData->HotKeyListHead, Link)) {
      Link = GetNextNode (&FormData->HotKeyListHead, Link);
      Index++;
    }
  }

  //
  // Show three HotKeys help information on one row.
  //
  gFooterHeight = FOOTER_HEIGHT + (Index / 3);

  ZeroMem (&gScreenDimensions, sizeof (EFI_SCREEN_DESCRIPTOR));
  gST->ConOut->QueryMode (
                 gST->ConOut,
                 gST->ConOut->Mode->Mode,
                 &gScreenDimensions.RightColumn,
                 &gScreenDimensions.BottomRow
                 );

  //
  // Check local dimension vs. global dimension.
  //
  if (FormData->ScreenDimensions != NULL) {
    if ((gScreenDimensions.RightColumn < FormData->ScreenDimensions->RightColumn) ||
        (gScreenDimensions.BottomRow < FormData->ScreenDimensions->BottomRow)
        )
    {
      return EFI_INVALID_PARAMETER;
    } else {
      //
      // Local dimension validation.
      //
      if ((FormData->ScreenDimensions->RightColumn > FormData->ScreenDimensions->LeftColumn) &&
          (FormData->ScreenDimensions->BottomRow > FormData->ScreenDimensions->TopRow) &&
          ((FormData->ScreenDimensions->RightColumn - FormData->ScreenDimensions->LeftColumn) > 2) &&
          ((FormData->ScreenDimensions->BottomRow - FormData->ScreenDimensions->TopRow) > STATUS_BAR_HEIGHT +
           FRONT_PAGE_HEADER_HEIGHT + gFooterHeight + 3))
      {
        CopyMem (&gScreenDimensions, (VOID *)FormData->ScreenDimensions, sizeof (EFI_SCREEN_DESCRIPTOR));
      } else {
        return EFI_INVALID_PARAMETER;
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Get the string based on the StringId and HII Package List Handle.

  @param  Token                  The String's ID.
  @param  HiiHandle              The package list in the HII database to search for
                                 the specified string.

  @return The output string.

**/
CHAR16 *
LibGetToken (
  IN  EFI_STRING_ID   Token,
  IN  EFI_HII_HANDLE  HiiHandle
  )
{
  EFI_STRING  String;

  String = HiiGetString (HiiHandle, Token, NULL);
  if (String == NULL) {
    String = AllocateCopyPool (StrSize (mLibUnknownString), mLibUnknownString);
    ASSERT (String != NULL);
  }

  return (CHAR16 *)String;
}

/**
  Count the storage space of a Unicode string.

  This function handles the Unicode string with NARROW_CHAR
  and WIDE_CHAR control characters. NARROW_HCAR and WIDE_CHAR
  does not count in the resultant output. If a WIDE_CHAR is
  hit, then 2 Unicode character will consume an output storage
  space with size of CHAR16 till a NARROW_CHAR is hit.

  If String is NULL, then ASSERT ().

  @param String          The input string to be counted.

  @return Storage space for the input string.

**/
UINTN
LibGetStringWidth (
  IN CHAR16  *String
  )
{
  UINTN  Index;
  UINTN  Count;
  UINTN  IncrementValue;

  ASSERT (String != NULL);
  if (String == NULL) {
    return 0;
  }

  Index          = 0;
  Count          = 0;
  IncrementValue = 1;

  do {
    //
    // Advance to the null-terminator or to the first width directive
    //
    for ( ;
          (String[Index] != NARROW_CHAR) && (String[Index] != WIDE_CHAR) && (String[Index] != 0);
          Index++, Count = Count + IncrementValue
          )
    {
    }

    //
    // We hit the null-terminator, we now have a count
    //
    if (String[Index] == 0) {
      break;
    }

    //
    // We encountered a narrow directive - strip it from the size calculation since it doesn't get printed
    // and also set the flag that determines what we increment by.(if narrow, increment by 1, if wide increment by 2)
    //
    if (String[Index] == NARROW_CHAR) {
      //
      // Skip to the next character
      //
      Index++;
      IncrementValue = 1;
    } else {
      //
      // Skip to the next character
      //
      Index++;
      IncrementValue = 2;
    }
  } while (String[Index] != 0);

  //
  // Increment by one to include the null-terminator in the size
  //
  Count++;

  return Count * sizeof (CHAR16);
}

/**
  Reserve text columns used by the Modern footer status chip.

  Native hotkey help still owns its text output. When the GOP Modern renderer is
  active, keep the left-most hotkey column from colliding with the status chip.

  @return Number of text columns reserved at the left of the footer.
**/
STATIC
UINTN
ModernDisplayFooterStatusReservedColumns (
  VOID
  )
{
  if (!mModernRenderReady) {
    return 0;
  }

  return 18;
}

/**
  Show all registered HotKey help strings on bottom Rows.

  @param FormData          The curent input form data info.
  @param SetState          Set HotKey or Clear HotKey

**/
VOID
PrintHotKeyHelpString (
  IN FORM_DISPLAY_ENGINE_FORM  *FormData,
  IN BOOLEAN                   SetState
  )
{
  UINTN                  CurrentCol;
  UINTN                  CurrentRow;
  UINTN                  BottomRowOfHotKeyHelp;
  UINTN                  ColumnIndexWidth;
  UINTN                  ColumnWidth;
  UINTN                  ColumnIndex;
  UINTN                  FooterReservedColumns;
  UINTN                  Index;
  EFI_SCREEN_DESCRIPTOR  LocalScreen;
  LIST_ENTRY             *Link;
  BROWSER_HOT_KEY        *HotKey;
  CHAR16                 BakChar;
  CHAR16                 *ColumnStr;

  CopyMem (&LocalScreen, &gScreenDimensions, sizeof (EFI_SCREEN_DESCRIPTOR));
  ColumnWidth           = (LocalScreen.RightColumn - LocalScreen.LeftColumn) / 3;
  BottomRowOfHotKeyHelp = LocalScreen.BottomRow - STATUS_BAR_HEIGHT - 3;
  FooterReservedColumns = ModernDisplayFooterStatusReservedColumns ();
  ColumnStr             = gLibEmptyString;

  //
  // Calculate total number of Register HotKeys.
  //
  Index = 0;
  Link  = GetFirstNode (&FormData->HotKeyListHead);
  while (!IsNull (&FormData->HotKeyListHead, Link)) {
    HotKey = BROWSER_HOT_KEY_FROM_LINK (Link);
    //
    // Calculate help information Column and Row.
    //
    ColumnIndex = Index % 3;
    if (ColumnIndex == 0) {
      CurrentCol       = LocalScreen.LeftColumn + 2 * ColumnWidth;
      ColumnIndexWidth = LocalScreen.RightColumn - CurrentCol - 1;
    } else if (ColumnIndex == 1) {
      CurrentCol       = LocalScreen.LeftColumn + ColumnWidth;
      ColumnIndexWidth = ColumnWidth;
    } else {
      CurrentCol       = LocalScreen.LeftColumn + 2 + FooterReservedColumns;
      ColumnIndexWidth = (ColumnWidth > (FooterReservedColumns + 2)) ? (ColumnWidth - FooterReservedColumns - 2) : 0;
    }

    if (ColumnIndexWidth == 0) {
      Link = GetNextNode (&FormData->HotKeyListHead, Link);
      Index++;
      continue;
    }

    CurrentRow = BottomRowOfHotKeyHelp - Index / 3;

    //
    // Help string can't exceed ColumnWidth. One Row will show three Help information.
    //
    BakChar = L'\0';
    if (StrLen (HotKey->HelpString) > ColumnIndexWidth) {
      BakChar                              = HotKey->HelpString[ColumnIndexWidth];
      HotKey->HelpString[ColumnIndexWidth] = L'\0';
    }

    //
    // Print HotKey help string on bottom Row.
    //
    if (SetState) {
      ColumnStr = HotKey->HelpString;
    }

    PrintStringAtWithWidth (CurrentCol, CurrentRow, ColumnStr, ColumnIndexWidth);

    if (BakChar != L'\0') {
      HotKey->HelpString[ColumnIndexWidth] = BakChar;
    }

    //
    // Get Next Hot Key.
    //
    Link = GetNextNode (&FormData->HotKeyListHead, Link);
    Index++;
  }

  if (SetState) {
    //
    // Clear KeyHelp
    //
    CurrentRow  = BottomRowOfHotKeyHelp - Index / 3;
    ColumnIndex = Index % 3;
    if (ColumnIndex == 0) {
      CurrentCol       = LocalScreen.LeftColumn + 2 * ColumnWidth;
      ColumnIndexWidth = LocalScreen.RightColumn - CurrentCol - 1;
      ColumnIndex++;
      PrintStringAtWithWidth (CurrentCol, CurrentRow, gLibEmptyString, ColumnIndexWidth);
    }

    if (ColumnIndex == 1) {
      CurrentCol       = LocalScreen.LeftColumn + ColumnWidth;
      ColumnIndexWidth = ColumnWidth;
      PrintStringAtWithWidth (CurrentCol, CurrentRow, gLibEmptyString, ColumnIndexWidth);
    }
  }

  return;
}

/**
  Get step info from numeric opcode.

  @param[in] OpCode     The input numeric op code.

  @return step info for this opcode.
**/
UINT64
LibGetFieldFromNum (
  IN  EFI_IFR_OP_HEADER  *OpCode
  )
{
  EFI_IFR_NUMERIC  *NumericOp;
  UINT64           Step;

  NumericOp = (EFI_IFR_NUMERIC *)OpCode;

  switch (NumericOp->Flags & EFI_IFR_NUMERIC_SIZE) {
    case EFI_IFR_NUMERIC_SIZE_1:
      Step = NumericOp->data.u8.Step;
      break;

    case EFI_IFR_NUMERIC_SIZE_2:
      Step = NumericOp->data.u16.Step;
      break;

    case EFI_IFR_NUMERIC_SIZE_4:
      Step = NumericOp->data.u32.Step;
      break;

    case EFI_IFR_NUMERIC_SIZE_8:
      Step = NumericOp->data.u64.Step;
      break;

    default:
      Step = 0;
      break;
  }

  return Step;
}

/**
  Initialize the HII String Token to the correct values.

**/
VOID
InitializeLibStrings (
  VOID
  )
{
  mLibUnknownString = L"!";

  gEnterString       = LibGetToken (STRING_TOKEN (ENTER_STRING), mCDLStringPackHandle);
  gEnterCommitString = LibGetToken (STRING_TOKEN (ENTER_COMMIT_STRING), mCDLStringPackHandle);
  gEnterEscapeString = LibGetToken (STRING_TOKEN (ENTER_ESCAPE_STRING), mCDLStringPackHandle);
  gEscapeString      = LibGetToken (STRING_TOKEN (ESCAPE_STRING), mCDLStringPackHandle);
  gMoveHighlight     = LibGetToken (STRING_TOKEN (MOVE_HIGHLIGHT), mCDLStringPackHandle);
  gDecNumericInput   = LibGetToken (STRING_TOKEN (DEC_NUMERIC_INPUT), mCDLStringPackHandle);
  gHexNumericInput   = LibGetToken (STRING_TOKEN (HEX_NUMERIC_INPUT), mCDLStringPackHandle);
  gToggleCheckBox    = LibGetToken (STRING_TOKEN (TOGGLE_CHECK_BOX), mCDLStringPackHandle);

  gAreYouSure   = LibGetToken (STRING_TOKEN (ARE_YOU_SURE), mCDLStringPackHandle);
  gYesResponse  = LibGetToken (STRING_TOKEN (ARE_YOU_SURE_YES), mCDLStringPackHandle);
  gNoResponse   = LibGetToken (STRING_TOKEN (ARE_YOU_SURE_NO), mCDLStringPackHandle);
  gPlusString   = LibGetToken (STRING_TOKEN (PLUS_STRING), mCDLStringPackHandle);
  gMinusString  = LibGetToken (STRING_TOKEN (MINUS_STRING), mCDLStringPackHandle);
  gAdjustNumber = LibGetToken (STRING_TOKEN (ADJUST_NUMBER), mCDLStringPackHandle);
  gSaveChanges  = LibGetToken (STRING_TOKEN (SAVE_CHANGES), mCDLStringPackHandle);

  gLibEmptyString = LibGetToken (STRING_TOKEN (EMPTY_STRING), mCDLStringPackHandle);

  gNvUpdateMessage   = LibGetToken (STRING_TOKEN (NV_UPDATE_MESSAGE), mCDLStringPackHandle);
  gInputErrorMessage = LibGetToken (STRING_TOKEN (INPUT_ERROR_MESSAGE), mCDLStringPackHandle);

  //
  // SpaceBuffer;
  //
  mSpaceBuffer = AllocatePool ((SPACE_BUFFER_SIZE + 1) * sizeof (CHAR16));
  ASSERT (mSpaceBuffer != NULL);
  LibSetUnicodeMem (mSpaceBuffer, SPACE_BUFFER_SIZE, L' ');
  mSpaceBuffer[SPACE_BUFFER_SIZE] = L'\0';
}

/**
  Free the HII String.

**/
VOID
FreeLibStrings (
  VOID
  )
{
  FreePool (gEnterString);
  FreePool (gEnterCommitString);
  FreePool (gEnterEscapeString);
  FreePool (gEscapeString);
  FreePool (gMoveHighlight);
  FreePool (gDecNumericInput);
  FreePool (gHexNumericInput);
  FreePool (gToggleCheckBox);

  FreePool (gAreYouSure);
  FreePool (gYesResponse);
  FreePool (gNoResponse);
  FreePool (gPlusString);
  FreePool (gMinusString);
  FreePool (gAdjustNumber);
  FreePool (gSaveChanges);

  FreePool (gLibEmptyString);

  FreePool (gNvUpdateMessage);
  FreePool (gInputErrorMessage);

  FreePool (mSpaceBuffer);
}

/**
  Wait for a key to be pressed by user.

  @param Key         The key which is pressed by user.

  @retval EFI_SUCCESS The function always completed successfully.

**/
EFI_STATUS
WaitForKeyStroke (
  OUT  EFI_INPUT_KEY  *Key
  )
{
  EFI_STATUS  Status;
  UINTN       Index;

  while (TRUE) {
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, Key);
    if (!EFI_ERROR (Status)) {
      break;
    }

    if (Status != EFI_NOT_READY) {
      continue;
    }

    gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Index);
  }

  return Status;
}

/**
  Set Buffer to Value for Size bytes.

  @param  Buffer                 Memory to set.
  @param  Size                   Number of bytes to set
  @param  Value                  Value of the set operation.

**/
VOID
LibSetUnicodeMem (
  IN VOID    *Buffer,
  IN UINTN   Size,
  IN CHAR16  Value
  )
{
  CHAR16  *Ptr;

  Ptr = Buffer;
  while ((Size--)  != 0) {
    *(Ptr++) = Value;
  }
}

/**
  The internal function prints to the EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
  protocol instance.

  @param Width           Width of string to be print.
  @param Column          The position of the output string.
  @param Row             The position of the output string.
  @param Out             The EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL instance.
  @param Fmt             The format string.
  @param Args            The additional argument for the variables in the format string.

  @return Number of Unicode character printed.

**/
UINTN
PrintInternal (
  IN UINTN                            Width,
  IN UINTN                            Column,
  IN UINTN                            Row,
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *Out,
  IN CHAR16                           *Fmt,
  IN VA_LIST                          Args
  )
{
  CHAR16  *Buffer;
  UINTN   Index;
  UINTN   TotalCount;
  UINTN   PrintWidth;
  UINTN   CharWidth;
  UINTN   Attribute;
  UINTN   CellWidth;
  UINTN   CellHeight;
  UINTN   DrawColumn;
  UINTN   DrawRow;
  UINTN   DrawWidth;
  UINTN   TextX;
  UINTN   TextY;
  UINTN   TextMaxWidth;
  UINTN   TextInset;
  UINTN   MeasuredWidth;
  UINTN   Emitted;
  CHAR16  *Printable;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Foreground;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background;

  //
  // For now, allocate an arbitrarily long buffer
  //
  Buffer = AllocateZeroPool (0x10000);
  ASSERT (Buffer);

  if (Column != (UINTN)-1) {
    Out->SetCursorPosition (Out, Column, Row);
    mModernCursorColumn = Column;
    mModernCursorRow    = Row;
  }

  UnicodeVSPrint (Buffer, 0x10000, Fmt, Args);

  Out->Mode->Attribute = Out->Mode->Attribute & 0x7f;

  Out->SetAttribute (Out, Out->Mode->Attribute);

  Index      = 0;
  TotalCount = 0;
  PrintWidth = 0;
  CharWidth  = 1;

  do {
    if (Buffer[Index] == 0) {
      break;
    }

    switch (Buffer[Index]) {
      case NARROW_CHAR:
        CharWidth = 1;
        break;
      case WIDE_CHAR:
        CharWidth = 2;
        break;
      default:
        PrintWidth += CharWidth;
        TotalCount += 1;
        break;
    }

    Index++;
  } while (Buffer[Index] != 0);

  //
  // We hit the end of the string - fill remaining space with SPACE.
  //
  if (PrintWidth < Width) {
    PrintWidth = Width;
  }

  if (!EFI_ERROR (ModernDisplayEnsureRenderer ())) {
    ModernDisplayGetCellMetrics (&CellWidth, &CellHeight);
    DrawColumn = (Column == (UINTN)-1) ? mModernCursorColumn : Column;
    DrawRow    = (Row == (UINTN)-1) ? mModernCursorRow : Row;
    DrawWidth  = (Width == 0) ? MAX (1, PrintWidth) : Width;
    Attribute  = Out->Mode->Attribute;
    Foreground = ModernDisplayForeground (Attribute);
    Background = ModernDisplayBackground (Attribute);

    //
    // For the highlighted statement row (EFI_RED background nibble on the row
    // that ModernDisplayDrawStatementRow just styled) skip the flat per-cell
    // fill: the row-level selection styling (inset bar, top/bottom sheen, left
    // accent) is already painted underneath, and a flat fill would bury it under
    // a solid band. The guard is scoped to that exact row so other EFI_RED text
    // (e.g. a highlighted popup option, which has no row styling) still fills
    // normally. The text below keeps Background for anti-alias blending.
    //
    if ((((Attribute >> 4) & 0x07) != EFI_RED) || (DrawRow != mModernStyledHighlightRow)) {
      ModernUiFillRect (
        &mModernRenderContext,
        (MODERN_UI_RECT){ DrawColumn * CellWidth, DrawRow * CellHeight, DrawWidth * CellWidth, CellHeight },
        Background
        );
    }

    Printable = AllocateZeroPool ((StrLen (Buffer) + 1) * sizeof (CHAR16));
    if (Printable != NULL) {
      ModernDisplayCopyPrintable (Printable, StrLen (Buffer) + 1, Buffer);
      TextInset    = ModernDisplayStatementTextInset (DrawColumn, DrawRow, DrawWidth, CellWidth);
      TextX        = DrawColumn * CellWidth + 2 + TextInset;
      TextY        = DrawRow * CellHeight + ((CellHeight > 18) ? ((CellHeight - 18) / 2) : 0);
      TextMaxWidth = (DrawWidth * CellWidth > (4 + TextInset)) ?
                     (DrawWidth * CellWidth - 4 - TextInset) :
                     DrawWidth * CellWidth;
      //
      // Width == 0 means the caller imposed no column constraint. The modern
      // proportional font advances wider than a text-grid cell, so a budget
      // derived from the character count would clip the caller's own string
      // (popups size themselves exactly to the text and were truncated as
      // "..."). Grow the budget to the measured text width so unconstrained
      // prints render in full.
      //
      if (Width == 0) {
        MeasuredWidth = ModernUiMeasureText (Printable);
        if ((MeasuredWidth + 4) > TextMaxWidth) {
          TextMaxWidth = MeasuredWidth + 4;
        }
      }

      ModernUiDrawTextFit (
        &mModernRenderContext,
        TextX,
        TextY,
        TextMaxWidth,
        Printable,
        Foreground,
        Background
        );
      FreePool (Printable);
    }

    mModernCursorColumn = DrawColumn + TotalCount;
    mModernCursorRow    = DrawRow;
  } else {
    //
    // Renderer unavailable (GOP absent, or a mode below the usable minimum):
    // fall back to plain text-console output so the form stays readable instead
    // of blanking. SetCursorPosition/SetAttribute above already positioned and
    // themed the cell; pad to the caller's field width so a previously longer
    // string at this position is overwritten, matching the native text grid.
    //
    Out->OutputString (Out, Buffer);
    for (Emitted = TotalCount; Emitted < Width; Emitted++) {
      Out->OutputString (Out, L" ");
    }

    mModernCursorColumn = ((Column == (UINTN)-1) ? mModernCursorColumn : Column) + MAX (TotalCount, Width);
    mModernCursorRow    = (Row == (UINTN)-1) ? mModernCursorRow : Row;
  }

  FreePool (Buffer);
  return TotalCount;
}

/**
  Prints a formatted unicode string to the default console, at
  the supplied cursor position.

  @param  Width      Width of String to be printed.
  @param  Column     The cursor position to print the string at.
  @param  Row        The cursor position to print the string at.
  @param  Fmt        Format string.
  @param  ...        Variable argument list for format string.

  @return Length of string printed to the console

**/
UINTN
EFIAPI
PrintAt (
  IN UINTN   Width,
  IN UINTN   Column,
  IN UINTN   Row,
  IN CHAR16  *Fmt,
  ...
  )
{
  VA_LIST  Args;
  UINTN    LengthOfPrinted;

  VA_START (Args, Fmt);
  LengthOfPrinted = PrintInternal (Width, Column, Row, gST->ConOut, Fmt, Args);
  VA_END (Args);
  return LengthOfPrinted;
}
