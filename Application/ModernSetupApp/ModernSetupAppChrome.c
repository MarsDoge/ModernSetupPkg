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
  UINTN                          TabCapacity;
  MODERN_UI_RECT                 TabRect;
  MODERN_UI_RECT                 DrawTabRect;

  SelectedTab = 0;
  for (Index = 0; Index < ARRAY_SIZE (mPages); Index++) {
    if (mPages[Index].Page == Page) {
      SelectedTab = Index;
    }
  }

  TabRect         = (MODERN_UI_RECT){ SCREEN_MARGIN, TOP_BAR_HEIGHT, (Ui->Width > (SCREEN_MARGIN * 2)) ? (Ui->Width - (SCREEN_MARGIN * 2)) : Ui->Width, TAB_BAR_HEIGHT };
  DrawTabRect     = TabRect;
  VisibleTabCount = ARRAY_SIZE (mPages);
  FirstVisibleTab = 0;
  if ((VisibleTabCount > 0) && ((TabRect.Width / VisibleTabCount) < 118)) {
    TabCapacity = TabRect.Width / 132;
    if (TabCapacity < 5) {
      TabCapacity = 5;
    }

    if (TabCapacity < VisibleTabCount) {
      VisibleTabCount = TabCapacity;
      FirstVisibleTab = (SelectedTab > (VisibleTabCount / 2)) ? (SelectedTab - (VisibleTabCount / 2)) : 0;
      if ((FirstVisibleTab + VisibleTabCount) > ARRAY_SIZE (mPages)) {
        FirstVisibleTab = ARRAY_SIZE (mPages) - VisibleTabCount;
      }
    }
  }

  for (Index = 0; Index < VisibleTabCount; Index++) {
    Tabs[Index].Text = ModernSetupGetCompactTabLabel (FirstVisibleTab + Index);
  }

  LocalSelectedTab = SelectedTab - FirstVisibleTab;
  if (((FirstVisibleTab > 0) || ((FirstVisibleTab + VisibleTabCount) < ARRAY_SIZE (mPages))) && (DrawTabRect.Width > 48)) {
    DrawTabRect.X     += 18;
    DrawTabRect.Width -= 36;
  }

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
