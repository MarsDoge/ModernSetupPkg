/** @file
  Modern graphical setup application prototype.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ModernSetupAppInternal.h"

STATIC CONST PAGE_DESCRIPTOR  mPages[] = {
  { PageDashboard, ModernUiStringPageDashboard, ModernUiStringPageDashboardHint },
  { PageSystemInfo, ModernUiStringPageSystemInfo, ModernUiStringPageSystemInfoHint },
  { PageBoot,      ModernUiStringPageBoot,      ModernUiStringPageBootHint      },
  { PageDevices,   ModernUiStringPageDevices,   ModernUiStringPageDevicesHint   },
  { PageSecurity,  ModernUiStringPageSecurity,  ModernUiStringPageSecurityHint  },
  { PageFirmware,  ModernUiStringPageFirmware,  ModernUiStringPageFirmwareHint  },
  { PageDiagnostics, ModernUiStringPageDiagnostics, ModernUiStringPageDiagnosticsHint },
  { PageManagement, ModernUiStringPageManagement, ModernUiStringPageManagementHint },
  { PagePower, ModernUiStringPagePower, ModernUiStringPagePowerHint },
  { PagePerformance, ModernUiStringPagePerformance, ModernUiStringPagePerformanceHint },
  { PageServerInventory, ModernUiStringPageServerInventory, ModernUiStringPageServerInventoryHint },
  { PagePreferences, ModernUiStringPagePreferences, ModernUiStringPagePreferencesHint },
  { PageExit,      ModernUiStringPageExit,      ModernUiStringPageExitHint      }
};

STATIC CONST CHAR16  *mEnglishCompactTabLabels[] = {
  L"Main",
  L"System",
  L"Boot",
  L"Devices",
  L"Security",
  L"Firmware",
  L"Status",
  L"Mgmt",
  L"Power",
  L"Perf",
  L"Assets",
  L"Prefs",
  L"Exit"
};

STATIC CONST CHAR16  *mChineseCompactTabLabels[] = {
  L"主页",
  L"系统",
  L"启动",
  L"设备",
  L"安全",
  L"固件",
  L"状态",
  L"管理",
  L"电源",
  L"性能",
  L"资产",
  L"偏好",
  L"退出"
};

/**
  Return the compact top-tab label for a page descriptor index.

  The page title strings remain full length for the content title area; this
  keeps the first-row IBV-style navigation compact enough for 1280px captures.

  @param[in] Index  Page descriptor index.

  @return Non-NULL compact tab label.
**/
STATIC
CONST CHAR16 *
ModernSetupGetCompactTabLabel (
  IN UINTN  Index
  )
{
  CONST CHAR8  *Language;

  if (Index >= ARRAY_SIZE (mEnglishCompactTabLabels)) {
    return L"";
  }

  Language = ModernUiGetLanguage ();
  if ((Language[0] == 'z') && (Language[1] == 'h') && (Index < ARRAY_SIZE (mChineseCompactTabLabels))) {
    return mChineseCompactTabLabels[Index];
  }

  return mEnglishCompactTabLabels[Index];
}

/**
  Draw the top status/header band.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
**/
VOID
ModernSetupDrawHeader (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  MODERN_UI_PAGE_MODEL  PageModel;

  ZeroMem (&PageModel, sizeof (PageModel));
  PageModel.Rect        = (MODERN_UI_RECT){ 0, 0, Ui->Width, TOP_BAR_HEIGHT };
  PageModel.ProductName = ModernUiGetString (ModernUiStringHeaderTitle);
  PageModel.ModeName    = ModernUiGetString (ModernUiStringHeaderMode);
  ModernUiEngineDrawPage (Ui, &PageModel, Theme);
}

/**
  Repaint only the header clock in place, without redrawing the rest of the frame.

  The idle loop calls this once per timer tick so the clock's seconds field
  stays live while no key is pressed. Only the timestamp text is repainted: it
  sits on the solid HeaderPattern strip at the very top of the header, and the
  renderer fills each glyph cell with the text background before blending, so
  redrawing the fixed-width timestamp over itself fully erases the previous
  value with no flicker and no screen clear. The 6/26 insets mirror
  ModernUiEngineDrawPage()'s header layout so the refreshed clock lands on the
  same pixels the full redraw uses; the app always renders wide enough that the
  clock is right-aligned. The call is a no-op if either argument is NULL or the
  real-time clock cannot be read.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
**/
VOID
ModernSetupRefreshHeaderClock (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  EFI_TIME  Time;
  CHAR16    TimeText[40];
  UINTN     TimeWidth;
  UINTN     RightEdge;
  UINTN     TimeStart;

  if ((Ui == NULL) || (Theme == NULL)) {
    return;
  }

  if (EFI_ERROR (gRT->GetTime (&Time, NULL))) {
    return;
  }

  UnicodeSPrint (
    TimeText,
    sizeof (TimeText),
    L"%02d/%02d/%04d  %02d:%02d:%02d",
    Time.Month,
    Time.Day,
    Time.Year,
    Time.Hour,
    Time.Minute,
    Time.Second
    );

  TimeWidth = ModernUiMeasureText (TimeText);
  RightEdge = (Ui->Width > 52) ? (Ui->Width - 26) : Ui->Width;
  TimeStart = (RightEdge > TimeWidth) ? (RightEdge - TimeWidth) : 0;

  ModernUiDrawText (Ui, TimeStart, 6, TimeText, Theme->Text, Theme->HeaderPattern);
}

/**
  Compute the visible tab window and strip rectangles for the current page.

  Single source of layout truth shared by ModernSetupDrawTabs (painting) and
  ModernSetupHitTestTab (pointer routing): the scroll window selection and the
  chevron inset are identical in both, so click targets always match the
  painted tabs.

  @param[in]  Ui               Initialized render context. Must not be NULL.
  @param[in]  Page             Currently selected page.
  @param[out] SelectedTab      Receives the absolute selected tab index.
  @param[out] FirstVisibleTab  Receives the first visible tab index.
  @param[out] VisibleTabCount  Receives the visible tab count (>= 1).
  @param[out] TabRect          Receives the full strip rectangle.
  @param[out] DrawTabRect      Receives the strip rectangle after the scrolled
                               chevron inset (the rect tabs are painted in).
**/
STATIC
VOID
ModernSetupGetTabWindow (
  IN  MODERN_UI_RENDER_CONTEXT  *Ui,
  IN  SETUP_PAGE                Page,
  OUT UINTN                     *SelectedTab,
  OUT UINTN                     *FirstVisibleTab,
  OUT UINTN                     *VisibleTabCount,
  OUT MODERN_UI_RECT            *TabRect,
  OUT MODERN_UI_RECT            *DrawTabRect
  )
{
  UINTN  Index;
  UINTN  TabCapacity;

  *SelectedTab = 0;
  for (Index = 0; Index < ARRAY_SIZE (mPages); Index++) {
    if (mPages[Index].Page == Page) {
      *SelectedTab = Index;
    }
  }

  *TabRect         = (MODERN_UI_RECT){ SCREEN_MARGIN, TOP_BAR_HEIGHT, (Ui->Width > (SCREEN_MARGIN * 2)) ? (Ui->Width - (SCREEN_MARGIN * 2)) : Ui->Width, TAB_BAR_HEIGHT };
  *DrawTabRect     = *TabRect;
  *VisibleTabCount = ARRAY_SIZE (mPages);
  *FirstVisibleTab = 0;
  if ((*VisibleTabCount > 0) && ((TabRect->Width / *VisibleTabCount) < 118)) {
    TabCapacity = TabRect->Width / 132;
    if (TabCapacity < 5) {
      TabCapacity = 5;
    }

    if (TabCapacity < *VisibleTabCount) {
      *VisibleTabCount = TabCapacity;
      *FirstVisibleTab = (*SelectedTab > (*VisibleTabCount / 2)) ? (*SelectedTab - (*VisibleTabCount / 2)) : 0;
      if ((*FirstVisibleTab + *VisibleTabCount) > ARRAY_SIZE (mPages)) {
        *FirstVisibleTab = ARRAY_SIZE (mPages) - *VisibleTabCount;
      }
    }
  }

  if (((*FirstVisibleTab > 0) || ((*FirstVisibleTab + *VisibleTabCount) < ARRAY_SIZE (mPages))) && (DrawTabRect->Width > 48)) {
    DrawTabRect->X     += 18;
    DrawTabRect->Width -= 36;
  }
}

/**
  Hit-test the top tab strip for a pointer click. See ModernSetupAppInternal.h.

  @param[in]  Ui    Initialized render context. Must not be NULL.
  @param[in]  Page  Currently selected page (determines the scroll window).
  @param[in]  X     Pointer X in pixels.
  @param[in]  Y     Pointer Y in pixels.
  @param[out] Hit   Receives the page of the clicked tab on success.

  @retval TRUE   (X,Y) lies on a visible tab; *Hit is set.
  @retval FALSE  No tab at this position.
**/
BOOLEAN
ModernSetupHitTestTab (
  IN  MODERN_UI_RENDER_CONTEXT  *Ui,
  IN  SETUP_PAGE                Page,
  IN  UINTN                     X,
  IN  UINTN                     Y,
  OUT SETUP_PAGE                *Hit
  )
{
  UINTN           SelectedTab;
  UINTN           FirstVisibleTab;
  UINTN           VisibleTabCount;
  MODERN_UI_RECT  TabRect;
  MODERN_UI_RECT  DrawTabRect;
  UINTN           TabWidth;
  UINTN           Index;

  if ((Ui == NULL) || (Hit == NULL)) {
    return FALSE;
  }

  if ((Y < TOP_BAR_HEIGHT) || (Y >= (TOP_BAR_HEIGHT + TAB_BAR_HEIGHT))) {
    return FALSE;
  }

  ModernSetupGetTabWindow (Ui, Page, &SelectedTab, &FirstVisibleTab, &VisibleTabCount, &TabRect, &DrawTabRect);
  if ((VisibleTabCount == 0) || (DrawTabRect.Width == 0) ||
      (X < DrawTabRect.X) || (X >= (DrawTabRect.X + DrawTabRect.Width)))
  {
    return FALSE;
  }

  TabWidth = DrawTabRect.Width / VisibleTabCount;
  if (TabWidth == 0) {
    return FALSE;
  }

  Index = (X - DrawTabRect.X) / TabWidth;
  if (Index >= VisibleTabCount) {
    Index = VisibleTabCount - 1;
  }

  *Hit = mPages[FirstVisibleTab + Index].Page;
  return TRUE;
}

//
// Save-under state for the pointer cursor: the pixels beneath the cursor are
// captured before the arrow is drawn and restored when it moves, so pointer
// motion repaints only this small rectangle instead of the whole frame.
//
#define MODERN_SETUP_CURSOR_SIZE  16

STATIC EFI_GRAPHICS_OUTPUT_BLT_PIXEL  mCursorSave[MODERN_SETUP_CURSOR_SIZE * MODERN_SETUP_CURSOR_SIZE];
STATIC BOOLEAN                        mCursorSaveValid = FALSE;
STATIC UINTN                          mCursorSaveX;
STATIC UINTN                          mCursorSaveY;

/**
  Forget the saved under-cursor pixels. See ModernSetupAppInternal.h.

  Call after any full-frame repaint: the saved pixels describe the old frame
  and must not be restored on the next cursor move.
**/
VOID
ModernSetupInvalidatePointerCursor (
  VOID
  )
{
  mCursorSaveValid = FALSE;
}

/**
  Move (or first-draw) the pointer cursor using save-under compositing. See
  ModernSetupAppInternal.h.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] X      Cursor hotspot X in pixels (clamped to keep the arrow
                    fully on screen).
  @param[in] Y      Cursor hotspot Y in pixels (clamped likewise).
**/
VOID
ModernSetupMovePointerCursor (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN UINTN                     X,
  IN UINTN                     Y
  )
{
  MODERN_UI_RECT  Rect;

  if ((Ui == NULL) || (Theme == NULL) ||
      (Ui->Width < MODERN_SETUP_CURSOR_SIZE) || (Ui->Height < MODERN_SETUP_CURSOR_SIZE))
  {
    return;
  }

  //
  // Clamp so the full save rectangle stays on screen (fixed-size capture).
  //
  if (X > (Ui->Width - MODERN_SETUP_CURSOR_SIZE)) {
    X = Ui->Width - MODERN_SETUP_CURSOR_SIZE;
  }

  if (Y > (Ui->Height - MODERN_SETUP_CURSOR_SIZE)) {
    Y = Ui->Height - MODERN_SETUP_CURSOR_SIZE;
  }

  if (mCursorSaveValid) {
    if ((X == mCursorSaveX) && (Y == mCursorSaveY)) {
      return;
    }

    Rect = (MODERN_UI_RECT){ mCursorSaveX, mCursorSaveY, MODERN_SETUP_CURSOR_SIZE, MODERN_SETUP_CURSOR_SIZE };
    ModernUiRestoreRect (Ui, Rect, mCursorSave);
    mCursorSaveValid = FALSE;
  }

  Rect = (MODERN_UI_RECT){ X, Y, MODERN_SETUP_CURSOR_SIZE, MODERN_SETUP_CURSOR_SIZE };
  if (!EFI_ERROR (ModernUiCaptureRect (Ui, Rect, mCursorSave))) {
    mCursorSaveValid = TRUE;
    mCursorSaveX     = X;
    mCursorSaveY     = Y;
  }

  //
  // Simple high-contrast arrow: a dark outline triangle with a lighter accent
  // triangle inset, apex at the hotspot pointing right-down. Original artwork
  // built from the shared primitive vocabulary (no bitmap asset).
  //
  ModernUiFillTriangle (Ui, (MODERN_UI_RECT){ X, Y, 16, 16 }, ModernUiTriRight, Theme->BackgroundBlack);
  ModernUiFillTriangle (Ui, (MODERN_UI_RECT){ X + 1, Y + 2, 12, 12 }, ModernUiTriRight, Theme->AccentYellow);
}

/**
  Draw the top page tab bar.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Page   Currently selected page.
  @param[in] Focus  Current focus area.
**/
VOID
ModernSetupDrawTabs (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_PAGE                Page,
  IN SETUP_FOCUS               Focus
  )
{
  UINTN                          Index;
  MODERN_UI_TAB_MODEL            Tabs[ARRAY_SIZE (mPages)];
  UINTN                          SelectedTab;
  UINTN                          FirstVisibleTab;
  UINTN                          VisibleTabCount;
  UINTN                          LocalSelectedTab;
  MODERN_UI_RECT                 TabRect;
  MODERN_UI_RECT                 DrawTabRect;

  ModernSetupGetTabWindow (Ui, Page, &SelectedTab, &FirstVisibleTab, &VisibleTabCount, &TabRect, &DrawTabRect);

  for (Index = 0; Index < VisibleTabCount; Index++) {
    Tabs[Index].Text = ModernSetupGetCompactTabLabel (FirstVisibleTab + Index);
  }

  LocalSelectedTab = SelectedTab - FirstVisibleTab;

  ModernUiEngineDrawTabs (
    Ui,
    DrawTabRect,
    Tabs,
    VisibleTabCount,
    LocalSelectedTab,
    Theme
    );

  if (FirstVisibleTab > 0) {
    ModernUiDrawText (Ui, TabRect.X + 4, TOP_BAR_HEIGHT + 10, L"<", Theme->AccentYellow, Theme->BackgroundBlack);
  }

  if ((FirstVisibleTab + VisibleTabCount) < ARRAY_SIZE (mPages)) {
    ModernUiDrawText (Ui, TabRect.X + TabRect.Width - 12, TOP_BAR_HEIGHT + 10, L">", Theme->AccentYellow, Theme->BackgroundBlack);
  }

  if (Focus == SetupFocusNav) {
    ModernUiFillRect (Ui, (MODERN_UI_RECT){ TabRect.X, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT - 3, TabRect.Width, 2 }, Theme->Accent);
  }
}

/**
  Draw the bottom hotkey/status strip.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
  @param[in] StatusMessage Optional status text. May be NULL.
**/
VOID
ModernSetupDrawFooter (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN CONST CHAR16              *StatusMessage
  )
{
  UINTN                         Y;
  CONST CHAR16                   *HelpText;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  HelpBackground;

  Y = Ui->Height - FOOTER_HEIGHT;
  ModernUiEngineDrawFooter (Ui, (MODERN_UI_RECT){ 0, Y, Ui->Width, FOOTER_HEIGHT }, StatusMessage, Theme);
  if ((StatusMessage != NULL) && (StatusMessage[0] != L'\0')) {
    return;
  }

  HelpText       = (Focus == SetupFocusNav) ? ModernUiGetString (ModernUiStringFooterNav) : ModernUiGetString (ModernUiStringFooterContent);
  HelpBackground = ModernUiBlendColor (Theme->BackgroundBlack, Theme->SelectedBand, 28);
  if ((Ui->Width > (SCREEN_MARGIN * 2)) && (FOOTER_HEIGHT >= 28)) {
    ModernUiFillRect (
      Ui,
      (MODERN_UI_RECT){ SCREEN_MARGIN - 6, Y + 6, Ui->Width - ((SCREEN_MARGIN - 6) * 2), 24 },
      HelpBackground
      );
    ModernUiFillRect (
      Ui,
      (MODERN_UI_RECT){ SCREEN_MARGIN - 6, Y + 6, 4, 24 },
      (Focus == SetupFocusNav) ? Theme->AccentYellow : Theme->AccentOrange
      );
  }

  ModernUiDrawText (
    Ui,
    SCREEN_MARGIN + 8,
    Y + 10,
    HelpText,
    (Focus == SetupFocusNav) ? Theme->AccentYellow : Theme->Text,
    HelpBackground
    );
}

/**
  Calculate the main content rectangle for the current resolution.

  @param[in] Ui  Initialized render context. Must not be NULL.

  @return Content rectangle in screen coordinates.
**/
MODERN_UI_RECT
ModernSetupContentRect (
  IN MODERN_UI_RENDER_CONTEXT  *Ui
  )
{
  return (MODERN_UI_RECT){
           SCREEN_MARGIN,
           TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + PAGE_TITLE_HEIGHT,
           Ui->Width - (SCREEN_MARGIN * 2),
           Ui->Height - TOP_BAR_HEIGHT - TAB_BAR_HEIGHT - PAGE_TITLE_HEIGHT - FOOTER_HEIGHT - SCREEN_MARGIN
         };
}

/**
  Draw the current page title and hint text.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Page   Page descriptor index to draw.
**/
VOID
ModernSetupDrawPageTitle (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_PAGE                Page
  )
{
  ModernUiDrawText (Ui, SCREEN_MARGIN, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 16, ModernUiGetString (mPages[Page].Title), Theme->Text, Theme->Background);
  ModernUiDrawText (Ui, SCREEN_MARGIN, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 40, ModernUiGetString (mPages[Page].Hint), Theme->MutedText, Theme->Background);
}
