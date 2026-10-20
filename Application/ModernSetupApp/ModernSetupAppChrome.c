/** @file
  Modern graphical setup application prototype.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ModernSetupAppInternal.h"
#include "ModernSetupBuildStamp.generated.h"

#define MODERN_SETUP_MAX_VISIBLE_TABS       5
#define MODERN_SETUP_MIN_VISIBLE_TABS       3
#define MODERN_SETUP_TARGET_TAB_WIDTH       160
#define MODERN_SETUP_TAB_CHEVRON_GUTTER     18
#define MODERN_SETUP_SECONDARY_NAV_WIDTH     184
#define MODERN_SETUP_SECONDARY_NAV_GAP       16
#define MODERN_SETUP_SECONDARY_NAV_MIN_WIDTH 920

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
  { PageQuickSettings, ModernUiStringPageQuickSettings, ModernUiStringPageQuickSettingsHint },
  { PageServerInventory, ModernUiStringPageServerInventory, ModernUiStringPageServerInventoryHint },
  { PagePreferences, ModernUiStringPagePreferences, ModernUiStringPagePreferencesHint },
  { PageExit,      ModernUiStringPageExit,      ModernUiStringPageExitHint      }
};

STATIC CONST SETUP_PAGE  mTopLevelPages[] = {
  PageDashboard,
  PageDevices,
  PageBoot,
  PageSecurity,
  PageExit
};

STATIC CONST CHAR16  *mEnglishCompactTabLabels[] = {
  L"Main",
  L"System",
  L"Boot",
  L"Advanced",
  L"Security",
  L"Firmware",
  L"Status",
  L"Mgmt",
  L"Power",
  L"Perf",
  L"Quick",
  L"Assets",
  L"Prefs",
  L"Exit"
};

STATIC CONST CHAR16  *mChineseCompactTabLabels[] = {
  L"主页",
  L"系统",
  L"启动",
  L"高级",
  L"安全",
  L"固件",
  L"状态",
  L"管理",
  L"电源",
  L"性能",
  L"设置",
  L"资产",
  L"偏好",
  L"退出"
};

/**
  Map a concrete page back to the small top-level product IA used by the
  horizontal chrome.  Detailed summary pages stay reachable from the Dashboard
  quick-card directory, but they do not become first-row tabs.

  @param[in] Page  Concrete app page.

  @return One of mTopLevelPages.
**/
STATIC
SETUP_PAGE
ModernSetupGetTopLevelPage (
  IN SETUP_PAGE  Page
  )
{
  switch (Page) {
    case PageDashboard:
    case PageBoot:
    case PageSecurity:
    case PageExit:
      return Page;
    case PageSystemInfo:
      return PageDashboard;
    default:
      return PageDevices;
  }
}

/**
  Return the compact top-tab label for a page.

  The page title strings remain full length for the content title area; this
  keeps the first-row IBV-style navigation compact enough for 1280px captures.

  @param[in] Page  Page id.

  @return Non-NULL compact tab label.
**/
STATIC
CONST CHAR16 *
ModernSetupGetCompactTabLabel (
  IN SETUP_PAGE  Page
  )
{
  CONST CHAR8  *Language;

  if (Page >= ARRAY_SIZE (mEnglishCompactTabLabels)) {
    return L"";
  }

  Language = ModernUiGetLanguage ();
  if ((Language[0] == 'z') && (Language[1] == 'h') && (Page < ARRAY_SIZE (mChineseCompactTabLabels))) {
    return mChineseCompactTabLabels[Page];
  }

  return mEnglishCompactTabLabels[Page];
}

/**
  Build the displayed page hierarchy for the title area.

  The first-row top navigation intentionally stays small.  This breadcrumb is
  where second/third-level placement is shown without adding more horizontal
  first-level tabs.

  @param[in]  Page        Concrete app page.
  @param[out] Buffer      Receives a NUL-terminated hierarchy string.
  @param[in]  BufferSize  Size of Buffer in bytes.
**/
STATIC
VOID
ModernSetupBuildPageHierarchy (
  IN  SETUP_PAGE  Page,
  OUT CHAR16      *Buffer,
  IN  UINTN       BufferSize
  )
{
  CONST CHAR16  *Level1;
  CONST CHAR16  *Level2;
  CONST CHAR16  *Level3;

  if ((Buffer == NULL) || (BufferSize == 0)) {
    return;
  }

  Level1 = L"Advanced";
  Level2 = L"Platform";
  Level3 = ModernSetupGetCompactTabLabel (Page);

  switch (Page) {
    case PageDashboard:
      Level1 = L"Main";
      Level2 = L"Overview";
      Level3 = L"Dashboard";
      break;
    case PageSystemInfo:
      Level1 = L"Main";
      Level2 = L"System";
      Level3 = L"Inventory";
      break;
    case PageDevices:
      Level2 = L"Platform";
      Level3 = L"Devices";
      break;
    case PageFirmware:
      Level2 = L"Platform";
      Level3 = L"Firmware";
      break;
    case PageDiagnostics:
      Level2 = L"Service";
      Level3 = L"Diagnostics";
      break;
    case PageManagement:
      Level2 = L"Service";
      Level3 = L"Management";
      break;
    case PageServerInventory:
      Level2 = L"Service";
      Level3 = L"Assets";
      break;
    case PagePower:
      Level2 = L"Runtime";
      Level3 = L"Power";
      break;
    case PagePerformance:
      Level2 = L"Runtime";
      Level3 = L"Performance";
      break;
    case PageQuickSettings:
      Level2 = L"Runtime";
      Level3 = L"Quick";
      break;
    case PagePreferences:
      Level2 = L"UX";
      Level3 = L"Preferences";
      break;
    case PageBoot:
      Level1 = L"Boot";
      Level2 = L"Order";
      Level3 = L"Entries";
      break;
    case PageSecurity:
      Level1 = L"Security";
      Level2 = L"Posture";
      Level3 = L"Controls";
      break;
    case PageExit:
      Level1 = L"Exit";
      Level2 = L"Save";
      Level3 = L"Actions";
      break;
    default:
      break;
  }

  UnicodeSPrint (Buffer, BufferSize, L"%s > %s > %s", Level1, Level2, Level3);
}


/**
  Return the active second-level group label for the concrete page.
**/
STATIC
CONST CHAR16 *
ModernSetupGetSecondaryGroupLabel (
  IN SETUP_PAGE  Page
  )
{
  switch (Page) {
    case PageCatalog:
      return L"Setting Catalog";
    case PageDashboard:
      return L"Overview";
    case PageSystemInfo:
      return L"System";
    case PageBoot:
      return L"Order";
    case PageSecurity:
      return L"Posture";
    case PageExit:
      return L"Save";
    case PageDevices:
    case PageFirmware:
      return L"Platform";
    case PagePower:
    case PagePerformance:
    case PageQuickSettings:
      return L"Runtime";
    case PageDiagnostics:
    case PageManagement:
    case PageServerInventory:
      return L"Service";
    case PagePreferences:
      return L"UX";
    default:
      return L"Platform";
  }
}

/**
  Move within the active category's second-level rail using keyboard navigation.

  The mouse path already hit-tests the painted rail.  This helper keeps keyboard
  Up/Down on the navigation focus aligned with the same painted groups so the
  second-level rail is not pointer-only.
**/
SETUP_PAGE
ModernSetupMoveSecondaryNavPage (
  IN SETUP_PAGE  Page,
  IN BOOLEAN     Forward
  )
{
  STATIC CONST SETUP_PAGE  MainPages[]     = { PageDashboard, PageSystemInfo };
  STATIC CONST SETUP_PAGE  AdvancedPages[] = { PageDevices, PageCatalog, PageQuickSettings, PageDiagnostics, PagePreferences };
  STATIC CONST SETUP_PAGE  BootPages[]     = { PageBoot, PageBoot };
  STATIC CONST SETUP_PAGE  SecurityPages[] = { PageSecurity, PageSecurity, PageSecurity };
  STATIC CONST SETUP_PAGE  ExitPages[]     = { PageExit, PageExit, PageExit };
  CONST SETUP_PAGE         *Pages;
  SETUP_PAGE               TopLevelPage;
  SETUP_PAGE               CurrentGroupPage;
  UINTN                    Count;
  UINTN                    Index;

  if (Page == PageDashboard) {
    return Page;
  }

  TopLevelPage = ModernSetupGetTopLevelPage (Page);
  Pages        = AdvancedPages;
  Count        = ARRAY_SIZE (AdvancedPages);

  switch (TopLevelPage) {
    case PageDashboard:
      Pages = MainPages;
      Count = ARRAY_SIZE (MainPages);
      break;
    case PageBoot:
      Pages = BootPages;
      Count = ARRAY_SIZE (BootPages);
      break;
    case PageSecurity:
      Pages = SecurityPages;
      Count = ARRAY_SIZE (SecurityPages);
      break;
    case PageExit:
      Pages = ExitPages;
      Count = ARRAY_SIZE (ExitPages);
      break;
    case PageDevices:
    default:
      break;
  }

  CurrentGroupPage = Page;
  switch (Page) {
    case PageSystemInfo:
      CurrentGroupPage = PageSystemInfo;
      break;
    case PageFirmware:
      CurrentGroupPage = PageDevices;
      break;
    case PagePower:
    case PagePerformance:
    case PageQuickSettings:
      CurrentGroupPage = PageQuickSettings;
      break;
    case PageManagement:
    case PageServerInventory:
      CurrentGroupPage = PageDiagnostics;
      break;
    default:
      break;
  }

  for (Index = 0; Index < Count; Index++) {
    if (Pages[Index] == CurrentGroupPage) {
      if (Forward) {
        return Pages[(Index + 1) % Count];
      }

      return Pages[(Index == 0) ? (Count - 1) : (Index - 1)];
    }
  }

  return Pages[0];
}

/**
  Return whether the current resolution has room for the vertical second-level rail.
**/
STATIC
BOOLEAN
ModernSetupSecondaryNavCanFit (
  IN MODERN_UI_RENDER_CONTEXT  *Ui
  )
{
  return (BOOLEAN)((Ui != NULL) && (Ui->Width >= MODERN_SETUP_SECONDARY_NAV_MIN_WIDTH));
}

/**
  Return whether the current page should show the vertical second-level rail.
**/
STATIC
BOOLEAN
ModernSetupSecondaryNavVisible (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN SETUP_PAGE                Page
  )
{
  //
  // The dashboard is already the full entry directory. Drawing a secondary rail
  // there creates a dead visual gutter and compresses the overview cards.
  //
  return (BOOLEAN)(ModernSetupSecondaryNavCanFit (Ui) && (Page != PageDashboard));
}

/**
  Return the far-left X position for the vertical second-level rail.

  The rail is intentionally a left-side detail-page selector. Dashboard keeps a
  full-width directory and does not draw this rail.
**/
STATIC
UINTN
ModernSetupSecondaryNavX (
  IN MODERN_UI_RENDER_CONTEXT  *Ui
  )
{
  return SCREEN_MARGIN;
}

/**
  Draw a Colorful/IBV-style vertical second-level navigation rail.

  The first row remains a small top-level IA.  The second level is a vertical
  rail in the left-side content area, scoped to the selected top-level
  category.  Concrete pages remain third-level destinations in the main content
  area and page title hierarchy.
**/
STATIC
VOID
ModernSetupDrawSecondaryNav (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_PAGE                Page
  )
{
  STATIC CONST CHAR16  *MainGroups[]     = { L"Overview", L"System" };
  STATIC CONST CHAR16  *AdvancedGroups[] = { L"Platform", L"Setting Catalog", L"Runtime", L"Service", L"UX" };
  STATIC CONST CHAR16  *BootGroups[]     = { L"Order", L"Native Tools" };
  STATIC CONST CHAR16  *SecurityGroups[] = { L"Posture", L"Secure Boot", L"TPM" };
  STATIC CONST CHAR16  *ExitGroups[]     = { L"Save", L"Reset", L"Language" };
  CONST CHAR16         **Groups;
  CONST CHAR16         *ActiveGroup;
  SETUP_PAGE           TopLevelPage;
  MODERN_UI_RECT       Rail;
  UINTN                GroupCount;
  UINTN                Index;
  UINTN                RowY;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  RailBackground;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  RowBackground;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  RowText;

  if ((Ui == NULL) || (Theme == NULL) || !ModernSetupSecondaryNavVisible (Ui, Page)) {
    return;
  }

  TopLevelPage = ModernSetupGetTopLevelPage (Page);
  ActiveGroup  = ModernSetupGetSecondaryGroupLabel (Page);
  Groups       = AdvancedGroups;
  GroupCount   = ARRAY_SIZE (AdvancedGroups);

  switch (TopLevelPage) {
    case PageDashboard:
      Groups     = MainGroups;
      GroupCount = ARRAY_SIZE (MainGroups);
      break;
    case PageBoot:
      Groups     = BootGroups;
      GroupCount = ARRAY_SIZE (BootGroups);
      break;
    case PageSecurity:
      Groups     = SecurityGroups;
      GroupCount = ARRAY_SIZE (SecurityGroups);
      break;
    case PageExit:
      Groups     = ExitGroups;
      GroupCount = ARRAY_SIZE (ExitGroups);
      break;
    case PageDevices:
    default:
      break;
  }

  Rail = (MODERN_UI_RECT){
           ModernSetupSecondaryNavX (Ui),
           TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + PAGE_TITLE_HEIGHT,
           MODERN_SETUP_SECONDARY_NAV_WIDTH,
           (Ui->Height > (TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + PAGE_TITLE_HEIGHT + FOOTER_HEIGHT + SCREEN_MARGIN)) ?
           (Ui->Height - TOP_BAR_HEIGHT - TAB_BAR_HEIGHT - PAGE_TITLE_HEIGHT - FOOTER_HEIGHT - SCREEN_MARGIN) : 0
         };
  if (Rail.Height == 0) {
    return;
  }

  RailBackground = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 36);
  ModernUiFillRect (Ui, Rail, RailBackground);
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ Rail.X, Rail.Y, 4, Rail.Height }, Theme->Accent);

  RowY = Rail.Y + 16;
  for (Index = 0; Index < GroupCount; Index++) {
    if ((RowY + 34) > (Rail.Y + Rail.Height)) {
      break;
    }

    if (StrCmp (Groups[Index], ActiveGroup) == 0) {
      RowBackground = ModernUiBlendColor (Theme->Accent, Theme->BackgroundBlack, 32);
      RowText       = Theme->AccentYellow;
      ModernUiFillRect (Ui, (MODERN_UI_RECT){ Rail.X + 12, RowY, Rail.Width - 24, 34 }, RowBackground);
      ModernUiFillRect (Ui, (MODERN_UI_RECT){ Rail.X + 12, RowY, 4, 34 }, Theme->AccentYellow);
    } else {
      RowBackground = RailBackground;
      RowText       = Theme->MutedText;
    }

    ModernUiDrawText (Ui, Rail.X + 28, RowY + 9,
      (Groups == AdvancedGroups && Index == 1) ? ModernSetupCatalogUi (L"Setting Catalog", L"配置目录") : Groups[Index],
      RowText, RowBackground);
    RowY += 42;
  }
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
  CHAR16                TitleWithVersion[96];

  //
  // Append the UI release version to the product title so a running UI is
  // identifiable on screen, e.g. "Modern UEFI Setup  v1.1.0".
  //
  UnicodeSPrint (
    TitleWithVersion,
    sizeof (TitleWithVersion),
    L"%s  v%s",
    ModernUiGetString (ModernUiStringHeaderTitle),
    MODERN_SETUP_VERSION_STRING
    );

  ZeroMem (&PageModel, sizeof (PageModel));
  PageModel.Rect        = (MODERN_UI_RECT){ 0, 0, Ui->Width, TOP_BAR_HEIGHT };
  PageModel.ProductName = TitleWithVersion;
  PageModel.ModeName    = ModernUiGetString (ModernUiStringHeaderMode);
  ModernUiEngineDrawPage (Ui, &PageModel, Theme);
  // Frozen build identity, separate from both product version and RTC clock.
  ModernUiDrawText (Ui, SCREEN_MARGIN, 30, MODERN_SETUP_BUILD_STAMP, Theme->MutedText, Theme->HeaderPattern);
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

  Page = ModernSetupGetTopLevelPage (Page);

  *SelectedTab = 0;
  for (Index = 0; Index < ARRAY_SIZE (mTopLevelPages); Index++) {
    if (mTopLevelPages[Index] == Page) {
      *SelectedTab = Index;
    }
  }

  *TabRect         = (MODERN_UI_RECT){ SCREEN_MARGIN, TOP_BAR_HEIGHT, (Ui->Width > (SCREEN_MARGIN * 2)) ? (Ui->Width - (SCREEN_MARGIN * 2)) : Ui->Width, TAB_BAR_HEIGHT };
  *DrawTabRect     = *TabRect;
  *VisibleTabCount = ARRAY_SIZE (mTopLevelPages);
  *FirstVisibleTab = 0;
  if (*VisibleTabCount > MODERN_SETUP_MAX_VISIBLE_TABS) {
    TabCapacity = TabRect->Width / MODERN_SETUP_TARGET_TAB_WIDTH;
    if (TabCapacity > MODERN_SETUP_MAX_VISIBLE_TABS) {
      TabCapacity = MODERN_SETUP_MAX_VISIBLE_TABS;
    }

    if (TabCapacity < MODERN_SETUP_MIN_VISIBLE_TABS) {
      TabCapacity = MODERN_SETUP_MIN_VISIBLE_TABS;
    }

    if (TabCapacity > ARRAY_SIZE (mTopLevelPages)) {
      TabCapacity = ARRAY_SIZE (mTopLevelPages);
    }

    *VisibleTabCount = TabCapacity;
    *FirstVisibleTab = (*SelectedTab > (*VisibleTabCount / 2)) ? (*SelectedTab - (*VisibleTabCount / 2)) : 0;
    if ((*FirstVisibleTab + *VisibleTabCount) > ARRAY_SIZE (mTopLevelPages)) {
      *FirstVisibleTab = ARRAY_SIZE (mTopLevelPages) - *VisibleTabCount;
    }
  }

  if (((*FirstVisibleTab > 0) || ((*FirstVisibleTab + *VisibleTabCount) < ARRAY_SIZE (mTopLevelPages))) && (DrawTabRect->Width > (MODERN_SETUP_TAB_CHEVRON_GUTTER * 2 + 12))) {
    DrawTabRect->X     += MODERN_SETUP_TAB_CHEVRON_GUTTER;
    DrawTabRect->Width -= (MODERN_SETUP_TAB_CHEVRON_GUTTER * 2);
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

  *Hit = mTopLevelPages[FirstVisibleTab + Index];
  return TRUE;
}


/**
  Hit-test the fixed second-level vertical navigation rail for a pointer click.

  The row geometry mirrors ModernSetupDrawSecondaryNav() exactly.  The
  destination is a representative third-level page for the clicked second-level
  group.

  @param[in]  Ui    Initialized render context. Must not be NULL.
  @param[in]  Page  Currently selected page.
  @param[in]  X     Pointer X in pixels.
  @param[in]  Y     Pointer Y in pixels.
  @param[out] Hit   Receives the representative page for the clicked group.

  @retval TRUE   (X,Y) lies on a visible second-level row; *Hit is set.
  @retval FALSE  No row at this position.
**/
BOOLEAN
ModernSetupHitTestSecondaryNav (
  IN  MODERN_UI_RENDER_CONTEXT  *Ui,
  IN  SETUP_PAGE                Page,
  IN  UINTN                     X,
  IN  UINTN                     Y,
  OUT SETUP_PAGE                *Hit
  )
{
  STATIC CONST SETUP_PAGE  MainDestinations[]     = { PageDashboard, PageSystemInfo };
  STATIC CONST SETUP_PAGE  AdvancedDestinations[] = { PageDevices, PageCatalog, PageQuickSettings, PageDiagnostics, PagePreferences };
  STATIC CONST SETUP_PAGE  BootDestinations[]     = { PageBoot, PageBoot };
  STATIC CONST SETUP_PAGE  SecurityDestinations[] = { PageSecurity, PageSecurity, PageSecurity };
  STATIC CONST SETUP_PAGE  ExitDestinations[]     = { PageExit, PageExit, PageExit };
  CONST SETUP_PAGE         *Destinations;
  SETUP_PAGE               TopLevelPage;
  UINTN                    GroupCount;
  UINTN                    Index;
  MODERN_UI_RECT           Rail;
  UINTN                    RowY;

  if ((Ui != NULL) && (Hit != NULL) &&
      !ModernSetupSecondaryNavCanFit (Ui) &&
      (ModernSetupGetTopLevelPage (Page) == PageDevices) &&
      (Ui->Width >= 400) && (X >= Ui->Width - 200) && (X < Ui->Width - SCREEN_MARGIN) &&
      (Y >= TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 8) && (Y < TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 36)) {
    *Hit = (Page == PageCatalog) ? PageDevices : PageCatalog;
    return TRUE;
  }

  if ((Ui == NULL) || (Hit == NULL) || !ModernSetupSecondaryNavVisible (Ui, Page)) {
    return FALSE;
  }

  Rail = (MODERN_UI_RECT){
           ModernSetupSecondaryNavX (Ui),
           TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + PAGE_TITLE_HEIGHT,
           MODERN_SETUP_SECONDARY_NAV_WIDTH,
           (Ui->Height > (TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + PAGE_TITLE_HEIGHT + FOOTER_HEIGHT + SCREEN_MARGIN)) ?
           (Ui->Height - TOP_BAR_HEIGHT - TAB_BAR_HEIGHT - PAGE_TITLE_HEIGHT - FOOTER_HEIGHT - SCREEN_MARGIN) : 0
         };
  if ((Rail.Height == 0) || (X < Rail.X) || (X >= (Rail.X + Rail.Width)) || (Y < Rail.Y) || (Y >= (Rail.Y + Rail.Height))) {
    return FALSE;
  }

  TopLevelPage = ModernSetupGetTopLevelPage (Page);
  Destinations = AdvancedDestinations;
  GroupCount   = ARRAY_SIZE (AdvancedDestinations);

  switch (TopLevelPage) {
    case PageDashboard:
      Destinations = MainDestinations;
      GroupCount   = ARRAY_SIZE (MainDestinations);
      break;
    case PageBoot:
      Destinations = BootDestinations;
      GroupCount   = ARRAY_SIZE (BootDestinations);
      break;
    case PageSecurity:
      Destinations = SecurityDestinations;
      GroupCount   = ARRAY_SIZE (SecurityDestinations);
      break;
    case PageExit:
      Destinations = ExitDestinations;
      GroupCount   = ARRAY_SIZE (ExitDestinations);
      break;
    case PageDevices:
    default:
      break;
  }

  RowY = Rail.Y + 16;
  for (Index = 0; Index < GroupCount; Index++) {
    if ((RowY + 34) > (Rail.Y + Rail.Height)) {
      break;
    }

    if ((Y >= RowY) && (Y < (RowY + 34))) {
      *Hit = Destinations[Index];
      return TRUE;
    }

    RowY += 42;
  }

  return FALSE;
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
    Tabs[Index].Text = ModernSetupGetCompactTabLabel (mTopLevelPages[FirstVisibleTab + Index]);
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

  if ((FirstVisibleTab + VisibleTabCount) < ARRAY_SIZE (mTopLevelPages)) {
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
  UINTN  X;
  UINTN  Width;

  X     = SCREEN_MARGIN;
  Width = Ui->Width - (SCREEN_MARGIN * 2);
  if (ModernSetupSecondaryNavCanFit (Ui) && (Width > (MODERN_SETUP_SECONDARY_NAV_WIDTH + MODERN_SETUP_SECONDARY_NAV_GAP + 320))) {
    X     = ModernSetupSecondaryNavX (Ui) + MODERN_SETUP_SECONDARY_NAV_WIDTH + MODERN_SETUP_SECONDARY_NAV_GAP;
    Width = (Ui->Width > (X + SCREEN_MARGIN)) ? (Ui->Width - X - SCREEN_MARGIN) : 0;
  }

  return (MODERN_UI_RECT){
           X,
           TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + PAGE_TITLE_HEIGHT,
           Width,
           Ui->Height - TOP_BAR_HEIGHT - TAB_BAR_HEIGHT - PAGE_TITLE_HEIGHT - FOOTER_HEIGHT - SCREEN_MARGIN
         };
}

/**
  Calculate the full-width dashboard content rectangle.

  Dashboard is the entry directory and should not reserve space for the
  second-level rail; otherwise the home page looks like a compressed detail page.
**/
MODERN_UI_RECT
ModernSetupDashboardContentRect (
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
  CHAR16  Hierarchy[96];

  ModernSetupBuildPageHierarchy (Page, Hierarchy, sizeof (Hierarchy));
  ModernSetupDrawSecondaryNav (Ui, Theme, Page);
  if (!ModernSetupSecondaryNavCanFit (Ui) && (Ui->Width >= 400) &&
      (ModernSetupGetTopLevelPage (Page) == PageDevices)) {
    ModernUiFillRect (Ui, (MODERN_UI_RECT){ Ui->Width - 200, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 8, 176, 28 }, Theme->SelectedBand);
    ModernUiDrawText (Ui, Ui->Width - 192, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 14,
      (Page == PageCatalog) ? ModernSetupCatalogUi (L"< Platform", L"< 平台") : ModernSetupCatalogUi (L"Setting Catalog >", L"配置目录 >"), Theme->AccentYellow, Theme->SelectedBand);
  }
  if (Page == PageCatalog) {
    ModernUiDrawText (Ui, SCREEN_MARGIN, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 12, ModernSetupCatalogUi (L"Universal Setting Catalog", L"通用配置目录"), Theme->Text, Theme->Background);
    ModernUiDrawText (Ui, SCREEN_MARGIN, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 36, ModernSetupCatalogUi (L"Advanced > Setting Catalog", L"高级 > 配置目录"), Theme->AccentYellow, Theme->Background);
    ModernUiDrawText (Ui, SCREEN_MARGIN, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 58, ModernSetupCatalogUi (L"English reference catalog | Unbound: N/A | No edits or submission", L"配置参考目录 | 未绑定：N/A | 不可编辑或提交"), Theme->MutedText, Theme->Background);
    return;
  }
  ModernUiDrawText (Ui, SCREEN_MARGIN, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 12, ModernUiGetString (mPages[Page].Title), Theme->Text, Theme->Background);
  ModernUiDrawText (Ui, SCREEN_MARGIN, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 36, Hierarchy, Theme->AccentYellow, Theme->Background);
  ModernUiDrawText (Ui, SCREEN_MARGIN, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 58, ModernUiGetString (mPages[Page].Hint), Theme->MutedText, Theme->Background);
}
