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
  { PageBoot,      ModernUiStringPageBoot,      ModernUiStringPageBootHint      },
  { PageDevices,   ModernUiStringPageDevices,   ModernUiStringPageDevicesHint   },
  { PageSecurity,  ModernUiStringPageSecurity,  ModernUiStringPageSecurityHint  },
  { PageFirmware,  ModernUiStringPageFirmware,  ModernUiStringPageFirmwareHint  },
  { PageDiagnostics, ModernUiStringPageDiagnostics, ModernUiStringPageDiagnosticsHint },
  { PageManagement, ModernUiStringPageManagement, ModernUiStringPageManagementHint },
  { PagePower, ModernUiStringPagePower, ModernUiStringPagePowerHint },
  { PagePerformance, ModernUiStringPagePerformance, ModernUiStringPagePerformanceHint },
  { PageExit,      ModernUiStringPageExit,      ModernUiStringPageExitHint      }
};

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

  SelectedTab = 0;
  for (Index = 0; Index < ARRAY_SIZE (mPages); Index++) {
    if (mPages[Index].Page == Page) {
      SelectedTab = Index;
    }
    Tabs[Index].Text = ModernUiGetString (mPages[Index].Title);
  }

  ModernUiEngineDrawTabs (
    Ui,
    (MODERN_UI_RECT){ SCREEN_MARGIN, TOP_BAR_HEIGHT, Ui->Width - (SCREEN_MARGIN * 2), TAB_BAR_HEIGHT },
    Tabs,
    ARRAY_SIZE (Tabs),
    SelectedTab,
    Theme
    );

  if (Focus == SetupFocusNav) {
    ModernUiFillRect (Ui, (MODERN_UI_RECT){ SCREEN_MARGIN, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT - 3, Ui->Width - (SCREEN_MARGIN * 2), 2 }, Theme->Accent);
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
  UINTN  Y;

  Y = Ui->Height - FOOTER_HEIGHT;
  ModernUiEngineDrawFooter (Ui, (MODERN_UI_RECT){ 0, Y, Ui->Width, FOOTER_HEIGHT }, StatusMessage, Theme);
  if ((StatusMessage != NULL) && (StatusMessage[0] != L'\0')) {
    return;
  } else if (Focus == SetupFocusNav) {
    ModernUiDrawText (Ui, SCREEN_MARGIN, Y + 10, ModernUiGetString (ModernUiStringFooterNav), Theme->MutedText, Theme->SurfaceRaised);
  } else {
    ModernUiDrawText (Ui, SCREEN_MARGIN, Y + 10, ModernUiGetString (ModernUiStringFooterContent), Theme->MutedText, Theme->SurfaceRaised);
  }
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
