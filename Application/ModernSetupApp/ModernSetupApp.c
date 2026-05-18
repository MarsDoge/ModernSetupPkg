/** @file
  Modern graphical setup application prototype.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/LoadedImage.h>

#include <ModernUi/ModernUiBootData.h>
#include <ModernUi/ModernUiDeviceData.h>
#include <ModernUi/ModernUiDiagnosticsData.h>
#include <ModernUi/ModernUiFirmwareData.h>
#include <ModernUi/ModernUiInput.h>
#include <ModernUi/ModernUiEngine.h>
#include <ModernUi/ModernUiManagementData.h>
#include <ModernUi/ModernUiPerformanceData.h>
#include <ModernUi/ModernUiPlatformData.h>
#include <ModernUi/ModernUiPowerData.h>
#include <ModernUi/ModernUiRenderer.h>
#include <ModernUi/ModernUiSecurityData.h>
#include <ModernUi/ModernUiString.h>
#include <ModernUi/ModernUiTheme.h>

#define CARD_GAP           16
#define TOP_BAR_HEIGHT     54
#define TAB_BAR_HEIGHT     54
#define PAGE_TITLE_HEIGHT  64
#define FOOTER_HEIGHT      36
#define SCREEN_MARGIN      24
#define MAX_BOOT_ROWS      9
#define MAX_DEVICE_ROWS    9

STATIC CONST EFI_GUID  mUiAppGuid = { 0x462CAA21, 0x7614, 0x4503, { 0x83, 0x6E, 0x8A, 0xB6, 0xF4, 0x66, 0x23, 0x31 } };
STATIC EFI_HANDLE      mImageHandle;

typedef enum {
  PageDashboard = 0,
  PageBoot,
  PageDevices,
  PageSecurity,
  PageFirmware,
  PageDiagnostics,
  PageManagement,
  PagePower,
  PagePerformance,
  PageExit,
  PageMax
} SETUP_PAGE;

typedef enum {
  SetupFocusNav = 0,
  SetupFocusContent
} SETUP_FOCUS;

typedef struct {
  SETUP_PAGE           Page;
  MODERN_UI_STRING_ID  Title;
  MODERN_UI_STRING_ID  Hint;
} PAGE_DESCRIPTOR;

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

STATIC BOOLEAN         mLanguageDropdownOpen;
STATIC UINTN           mLanguageDropdownSelection;

/**
  Calculate the main content rectangle for the current resolution.

  @param[in] Ui  Initialized render context. Must not be NULL.

  @return Content rectangle in screen coordinates.
**/
STATIC
MODERN_UI_RECT
ContentRect (
  IN MODERN_UI_RENDER_CONTEXT  *Ui
  );

/**
  Return TRUE when the Dashboard has enough vertical space for Quick Access.

  @param[in] Ui  Initialized render context. Must not be NULL.

  @retval TRUE   Quick Access cards are visible and selectable.
  @retval FALSE  Quick Access is hidden for the current resolution.
**/
STATIC
BOOLEAN
DashboardQuickAccessVisible (
  IN MODERN_UI_RENDER_CONTEXT  *Ui
  )
{
  MODERN_UI_RECT  Content;
  UINTN           TopHeight;
  UINTN           QuickHeight;

  if (Ui == NULL) {
    return FALSE;
  }

  Content     = (MODERN_UI_RECT){
                  SCREEN_MARGIN,
                  TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + PAGE_TITLE_HEIGHT,
                  Ui->Width - (SCREEN_MARGIN * 2),
                  Ui->Height - TOP_BAR_HEIGHT - TAB_BAR_HEIGHT - PAGE_TITLE_HEIGHT - FOOTER_HEIGHT - SCREEN_MARGIN
                };
  TopHeight   = (Content.Height >= 460) ? 300 : 232;
  QuickHeight = (Content.Height > (TopHeight + 16)) ? (Content.Height - TopHeight - 16) : 0;
  return (BOOLEAN)(QuickHeight > 110);
}

/**
  Return TRUE when the active UI language is Simplified Chinese.

  @retval TRUE   Active language starts with "zh".
  @retval FALSE  Active language is another supported language.
**/
STATIC
BOOLEAN
IsChineseLanguage (
  VOID
  )
{
  CONST CHAR8  *Language;

  Language = ModernUiGetLanguage ();
  return (BOOLEAN)((Language[0] == 'z') && (Language[1] == 'h'));
}

/**
  Return the display name for one language selector option.

  @param[in] Selection  Language selector index. Zero is Chinese, one is English.

  @return Non-NULL localized language display name.
**/
STATIC
CONST CHAR16 *
GetLanguageOptionName (
  IN UINTN  Selection
  )
{
  return (Selection == 0) ?
         ModernUiGetString (ModernUiStringLanguageChinese) :
         ModernUiGetString (ModernUiStringLanguageEnglish);
}

/**
  Return the selector index for the active language.

  @return Zero for Chinese, one for English.
**/
STATIC
UINTN
GetActiveLanguageSelection (
  VOID
  )
{
  return IsChineseLanguage () ? 0 : 1;
}

/**
  Get the selected item value for a page.

  @param[in] Page             Page whose selected item is requested.
  @param[in] DashboardSelection Current Dashboard Quick Access selection.
  @param[in] BootSelection    Current Boot page selection.
  @param[in] DeviceSelection  Current Devices page selection.
  @param[in] ExitSelection    Current Exit page selection.

  @return Selected item index for Page. Pages without selectable rows return 0.
**/
STATIC
UINTN
GetPageSelection (
  IN SETUP_PAGE  Page,
  IN UINTN       DashboardSelection,
  IN UINTN       BootSelection,
  IN UINTN       DeviceSelection,
  IN UINTN       ExitSelection
  )
{
  switch (Page) {
    case PageDashboard:
      return DashboardSelection;
    case PageBoot:
      return BootSelection;
    case PageDevices:
      return DeviceSelection;
    case PageExit:
      return ExitSelection;
    default:
      return 0;
  }
}

/**
  Store the selected item value for a page.

  @param[in]     Page             Page whose selected item is updated.
  @param[in]     Selection        New selected item index.
  @param[in,out] DashboardSelection Dashboard Quick Access selection storage.
                                      Must not be NULL.
  @param[in,out] BootSelection    Boot page selection storage. Must not be NULL.
  @param[in,out] DeviceSelection  Devices page selection storage. Must not be NULL.
  @param[in,out] ExitSelection    Exit page selection storage. Must not be NULL.
**/
STATIC
VOID
SetPageSelection (
  IN     SETUP_PAGE  Page,
  IN     UINTN       Selection,
  IN OUT UINTN       *DashboardSelection,
  IN OUT UINTN       *BootSelection,
  IN OUT UINTN       *DeviceSelection,
  IN OUT UINTN       *ExitSelection
  )
{
  switch (Page) {
    case PageDashboard:
      *DashboardSelection = Selection;
      break;
    case PageBoot:
      *BootSelection = Selection;
      break;
    case PageDevices:
      *DeviceSelection = Selection;
      break;
    case PageExit:
      *ExitSelection = Selection;
      break;
    default:
      break;
  }
}


/**
  Count visible boot options from UefiBootManagerLib.

  @return Number of visible Boot#### entries, or 0 when none are available.
**/
STATIC
UINTN
GetBootCount (
  VOID
  )
{
  EFI_STATUS             Status;
  MODERN_UI_BOOT_OPTION  *Options;
  UINTN                  OptionCount;

  Options = NULL;
  Status = ModernUiBootDataGetOptions (mImageHandle, &Options, &OptionCount);
  if (EFI_ERROR (Status)) {
    return 0;
  }

  ModernUiBootDataFreeOptions (Options, OptionCount);
  return OptionCount;
}

/**
  Count visible device-path rows for the Devices page.

  @return Number of device-path rows that can be selected in the current v1
          Devices page, capped at 8 rows.
**/
STATIC
UINTN
GetVisibleDeviceCount (
  VOID
  )
{
  EFI_STATUS              Status;
  MODERN_UI_DEVICE_ENTRY  *Entries;
  UINTN                   EntryCount;

  Entries = NULL;
  Status = ModernUiDeviceDataGetEntries (&Entries, &EntryCount);
  if (EFI_ERROR (Status)) {
    return 0;
  }

  ModernUiDeviceDataFreeEntries (Entries, EntryCount);
  return MIN (EntryCount, MAX_DEVICE_ROWS);
}


/**
  Return the selectable row count for one page.

  @param[in] Ui    Initialized render context. Must not be NULL.
  @param[in] Page  Page whose selectable count is requested.

  @return Number of selectable rows or actions available on Page.
**/
STATIC
UINTN
GetPageSelectableCount (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN SETUP_PAGE  Page
  )
{
  switch (Page) {
    case PageDashboard:
      return DashboardQuickAccessVisible (Ui) ? 3 : 0;
    case PageBoot:
      {
        MODERN_UI_RECT  Panel;
        UINTN           MaxRows;

        Panel   = ContentRect (Ui);
        MaxRows = (Panel.Height > 96) ? ((Panel.Height - 92) / 58) : 0;
        return MIN (GetBootCount (), MIN (MaxRows, MAX_BOOT_ROWS));
      }
    case PageDevices:
      return GetVisibleDeviceCount ();
    case PageExit:
      return 4;
    default:
      return 0;
  }
}

/**
  Boot one visible Boot page row through UefiBootManagerLib.

  @param[in] Selection  Zero-based visible Boot page row index.

  @retval EFI_SUCCESS            Boot option was launched and returned.
  @retval EFI_NOT_FOUND          The selected Boot#### option could not be found.
  @retval others                 Status from boot option decoding or launch.
**/
STATIC
EFI_STATUS
LaunchSelectedBootOption (
  IN UINTN  Selection
  )
{
  EFI_STATUS             Status;
  MODERN_UI_BOOT_OPTION  *Options;
  UINTN                  OptionCount;
  UINT16                 OptionNumber;

  Options = NULL;
  Status = ModernUiBootDataGetOptions (mImageHandle, &Options, &OptionCount);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((Options == NULL) || (Selection >= OptionCount)) {
    ModernUiBootDataFreeOptions (Options, OptionCount);
    return EFI_NOT_FOUND;
  }

  OptionNumber = Options[Selection].OptionNumber;
  ModernUiBootDataFreeOptions (Options, OptionCount);
  Status = ModernUiBootDataBootOption (OptionNumber);
  return Status;
}

/**
  Open one visible device/HII entry through native FormBrowser2.

  @param[in] Selection  Zero-based Devices page row index.

  @retval EFI_SUCCESS    FormBrowser returned successfully.
  @retval EFI_NOT_FOUND  Selection was out of range or no HII formset exists.
  @retval others         Status returned by FormBrowser2->SendForm().
**/
STATIC
EFI_STATUS
OpenSelectedDeviceEntry (
  IN UINTN  Selection
  )
{
  EFI_STATUS              Status;
  MODERN_UI_DEVICE_ENTRY  *Entries;
  UINTN                   EntryCount;
  MODERN_UI_DEVICE_ENTRY  Entry;

  Entries = NULL;
  Status = ModernUiDeviceDataGetEntries (&Entries, &EntryCount);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((Entries == NULL) || (Selection >= EntryCount)) {
    ModernUiDeviceDataFreeEntries (Entries, EntryCount);
    return EFI_NOT_FOUND;
  }

  CopyMem (&Entry, &Entries[Selection], sizeof (Entry));
  ModernUiDeviceDataFreeEntries (Entries, EntryCount);
  return ModernUiDeviceDataOpenEntry (&Entry);
}


/**
  Draw the top status/header band.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
**/
STATIC
VOID
DrawHeader (
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
STATIC
VOID
DrawTabs (
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
STATIC
VOID
DrawFooter (
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
STATIC
MODERN_UI_RECT
ContentRect (
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
STATIC
VOID
DrawPageTitle (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_PAGE                Page
  )
{
  ModernUiDrawText (Ui, SCREEN_MARGIN, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 16, ModernUiGetString (mPages[Page].Title), Theme->Text, Theme->Background);
  ModernUiDrawText (Ui, SCREEN_MARGIN, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT + 40, ModernUiGetString (mPages[Page].Hint), Theme->MutedText, Theme->Background);
}

/**
  Draw one dashboard label/value row.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] X      Left coordinate in pixels.
  @param[in] Y      Top coordinate in pixels.
  @param[in] Width  Available row width in pixels.
  @param[in] Label  Label text. Must not be NULL.
  @param[in] Value  Value text. Must not be NULL.
**/
STATIC
VOID
DrawDashboardInfoRow (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN UINTN                     X,
  IN UINTN                     Y,
  IN UINTN                     Width,
  IN CONST CHAR16              *Label,
  IN CONST CHAR16              *Value
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background;
  UINTN  LabelWidth;
  UINTN  ValueX;

  if (Width < 32) {
    return;
  }

  Background = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30);
  LabelWidth = (Width > 240) ? 180 : (Width / 2);
  ValueX     = X + LabelWidth;
  ModernUiDrawTextFit (Ui, X, Y, LabelWidth - 8, Label, Theme->MutedText, Background);
  ModernUiDrawTextFit (Ui, ValueX, Y, (Width > LabelWidth) ? (Width - LabelWidth) : Width, Value, Theme->Text, Background);
}

/**
  Draw a subtle Dashboard section surface.

  @param[in] Ui      Initialized render context. Must not be NULL.
  @param[in] Theme   Theme token table. Must not be NULL.
  @param[in] Rect    Section rectangle in pixels.
  @param[in] Title   Section title text. Must not be NULL.
  @param[in] Accent  TRUE to draw a stronger top accent line.
**/
STATIC
VOID
DrawDashboardSection (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN MODERN_UI_RECT            Rect,
  IN CONST CHAR16              *Title,
  IN BOOLEAN                   Accent
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  PanelColor;

  PanelColor = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30);
  ModernUiFillRect (Ui, Rect, PanelColor);
  ModernUiStrokeRect (Ui, Rect, Theme->Border);
  ModernUiFillRect (
    Ui,
    (MODERN_UI_RECT){ Rect.X, Rect.Y, Rect.Width, Accent ? 2 : 1 },
    Accent ? Theme->AccentOrange : ModernUiBlendColor (Theme->Border, Theme->BackgroundBlack, 40)
    );
  ModernUiDrawTextFit (Ui, Rect.X + 18, Rect.Y + 16, Rect.Width - 36, Title, Accent ? Theme->AccentYellow : Theme->MutedText, PanelColor);
}

/**
  Draw one compact Dashboard status card.

  @param[in] Ui       Initialized render context. Must not be NULL.
  @param[in] Theme    Theme token table. Must not be NULL.
  @param[in] Rect     Tile rectangle in pixels.
  @param[in] Title    Tile title text. Must not be NULL.
  @param[in] Value    Tile value text. Must not be NULL.
  @param[in] Emphasis TRUE to draw the value as a highlighted state.
  @param[in] Selected TRUE to draw the tile as the active Quick Access entry.
**/
STATIC
VOID
DrawDashboardTile (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN MODERN_UI_RECT            Rect,
  IN CONST CHAR16              *Title,
  IN CONST CHAR16              *Value,
  IN BOOLEAN                   Emphasis,
  IN BOOLEAN                   Selected
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  TileColor;

  TileColor = Selected ?
              ModernUiBlendColor (Theme->SelectedBand, Theme->BackgroundBlack, 42) :
              ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 24);
  ModernUiFillRect (Ui, Rect, TileColor);
  ModernUiStrokeRect (Ui, Rect, Selected ? Theme->PopupBorder : Theme->Border);
  if (Selected) {
    ModernUiFillRect (Ui, (MODERN_UI_RECT){ Rect.X, Rect.Y, Rect.Width, 2 }, Theme->GlowOrange);
    ModernUiFillRect (Ui, (MODERN_UI_RECT){ Rect.X, Rect.Y + Rect.Height - 3, Rect.Width, 2 }, Theme->AccentOrange);
  }

  ModernUiFillRect (Ui, (MODERN_UI_RECT){ Rect.X, Rect.Y, Selected ? 7 : 4, Rect.Height }, (Emphasis || Selected) ? Theme->AccentYellow : Theme->AccentSoft);
  ModernUiDrawTextFit (Ui, Rect.X + 18, Rect.Y + 12, Rect.Width - 36, Title, Theme->MutedText, TileColor);
  ModernUiDrawTextFit (Ui, Rect.X + 18, Rect.Y + 40, Rect.Width - 36, Value, (Emphasis || Selected) ? Theme->AccentYellow : Theme->Text, TileColor);
}

/**
  Return localized capability text for a boolean provider state.

  @param[in] Present  TRUE when the capability is available.

  @return Non-NULL localized capability text.
**/
STATIC
CONST CHAR16 *
CapabilityText (
  IN BOOLEAN  Present
  )
{
  return Present ? ModernUiGetString (ModernUiStringAvailable) : ModernUiGetString (ModernUiStringNotAvailable);
}

/**
  Return localized text for one security provider state.

  @param[in] State  Security state to render.

  @return Non-NULL localized status text.
**/
STATIC
CONST CHAR16 *
SecurityStateText (
  IN MODERN_UI_SECURITY_STATE  State
  )
{
  switch (State) {
    case ModernUiSecurityStatePresent:
      return ModernUiGetString (ModernUiStringPresent);
    case ModernUiSecurityStateAbsent:
      return ModernUiGetString (ModernUiStringAbsent);
    case ModernUiSecurityStateEnabled:
      return ModernUiGetString (ModernUiStringEnabled);
    case ModernUiSecurityStateDisabled:
      return ModernUiGetString (ModernUiStringDisabled);
    default:
      return ModernUiGetString (ModernUiStringUnknown);
  }
}

/**
  Draw a provider summary page with one section and a row list.

  @param[in] Ui        Initialized render context. Must not be NULL.
  @param[in] Theme     Theme token table. Must not be NULL.
  @param[in] Focus     Current focus area.
  @param[in] Section   Section title text. Must not be NULL.
  @param[in] Labels    Row label array. Must not be NULL when RowCount is nonzero.
  @param[in] Values    Row value array. Must not be NULL when RowCount is nonzero.
  @param[in] RowCount  Number of rows in Labels and Values.
**/
STATIC
VOID
DrawProviderSummaryPage (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN CONST CHAR16              *Section,
  IN CONST CHAR16              **Labels,
  IN CONST CHAR16              **Values,
  IN UINTN                     RowCount
  )
{
  MODERN_UI_RECT  Content;
  MODERN_UI_RECT  Panel;
  UINTN           Index;
  UINTN           RowY;

  Content = ContentRect (Ui);
  Panel   = (MODERN_UI_RECT){ Content.X, Content.Y, Content.Width, MIN (Content.Height, 340) };

  DrawDashboardSection (Ui, Theme, Panel, Section, TRUE);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);

  for (Index = 0; Index < RowCount; Index++) {
    RowY = Panel.Y + 58 + (Index * 34);
    if ((RowY + 24) > (Panel.Y + Panel.Height)) {
      break;
    }

    DrawDashboardInfoRow (
      Ui,
      Theme,
      Panel.X + 22,
      RowY,
      Panel.Width - 44,
      Labels[Index],
      Values[Index]
      );
  }
}

/**
  Draw the Dashboard page.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus      Current focus area.
  @param[in] Selection  Selected Quick Access entry.
**/
STATIC
VOID
DrawDashboard (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     Selection
  )
{
  CHAR16  Resolution[48];
  CHAR16  BootCount[48];
  CHAR16  DeviceCount[48];
  CHAR16  MemoryText[48];
  CHAR16  SecurityText[48];
  CHAR16  ArchitectureText[96];
  MODERN_UI_RECT  Content;
  MODERN_UI_RECT  SystemPanel;
  MODERN_UI_RECT  MonitorPanel;
  MODERN_UI_RECT  QuickPanel;
  MODERN_UI_RECT  QuickCard;
  UINTN           MonitorWidth;
  UINTN           QuickY;
  UINTN           QuickHeight;
  UINTN           CardWidth;
  UINTN           CardGap;
  UINTN           TopHeight;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  PanelBackground;
  MODERN_UI_PLATFORM_SUMMARY  Platform;
  MODERN_UI_SECURITY_SUMMARY  Security;
  MODERN_UI_FIRMWARE_SUMMARY  Firmware;
  MODERN_UI_DIAGNOSTICS_SUMMARY  Diagnostics;
  MODERN_UI_MANAGEMENT_SUMMARY  Management;

  Content = ContentRect (Ui);
  PanelBackground = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30);
  UnicodeSPrint (Resolution, sizeof (Resolution), L"%u x %u", Ui->Width, Ui->Height);
  UnicodeSPrint (BootCount, sizeof (BootCount), ModernUiGetString (ModernUiStringBootCountFormat), GetBootCount ());
  UnicodeSPrint (DeviceCount, sizeof (DeviceCount), L"%u entries", GetVisibleDeviceCount ());
  if (EFI_ERROR (ModernUiPlatformDataGetSummary (&Platform))) {
    ZeroMem (&Platform, sizeof (Platform));
    StrCpyS (Platform.FirmwareVendor, ARRAY_SIZE (Platform.FirmwareVendor), L"Unknown");
    StrCpyS (Platform.FirmwareRevision, ARRAY_SIZE (Platform.FirmwareRevision), L"Unknown");
    StrCpyS (Platform.Architecture, ARRAY_SIZE (Platform.Architecture), L"Unknown");
    StrCpyS (Platform.Platform, ARRAY_SIZE (Platform.Platform), L"Unknown");
    StrCpyS (Platform.FormFactor, ARRAY_SIZE (Platform.FormFactor), L"Unknown");
    StrCpyS (Platform.BootMode, ARRAY_SIZE (Platform.BootMode), L"Unknown");
  }

  ModernUiSecurityDataGetSummary (&Security);
  if (EFI_ERROR (ModernUiFirmwareDataGetSummary (&Firmware))) {
    ZeroMem (&Firmware, sizeof (Firmware));
  }

  if (EFI_ERROR (ModernUiDiagnosticsDataGetSummary (&Diagnostics))) {
    ZeroMem (&Diagnostics, sizeof (Diagnostics));
  }

  if (EFI_ERROR (ModernUiManagementDataGetSummary (&Management))) {
    ZeroMem (&Management, sizeof (Management));
  }

  UnicodeSPrint (MemoryText, sizeof (MemoryText), L"%lu MB", Platform.MemorySizeMb);
  UnicodeSPrint (ArchitectureText, sizeof (ArchitectureText), L"%s", Platform.Architecture);
  UnicodeSPrint (
    SecurityText,
    sizeof (SecurityText),
    L"%s",
    (Security.SecureBoot == ModernUiSecurityStateEnabled) ? ModernUiGetString (ModernUiStringEnabled) : ModernUiGetString (ModernUiStringDisabled)
    );

  TopHeight   = (Content.Height >= 460) ? 300 : 232;
  QuickY      = Content.Y + TopHeight + 16;
  QuickHeight = (Content.Height > (TopHeight + 16)) ? (Content.Height - TopHeight - 16) : 0;
  MonitorWidth = (Content.Width >= 760) ? ((Content.Width * 31) / 100) : 0;
  if ((MonitorWidth > 0) && (Content.Width > (MonitorWidth + CARD_GAP))) {
    SystemPanel  = (MODERN_UI_RECT){ Content.X, Content.Y, Content.Width - MonitorWidth - CARD_GAP, TopHeight };
    MonitorPanel = (MODERN_UI_RECT){ SystemPanel.X + SystemPanel.Width + CARD_GAP, Content.Y, MonitorWidth, TopHeight };
  } else {
    SystemPanel  = (MODERN_UI_RECT){ Content.X, Content.Y, Content.Width, TopHeight };
    MonitorPanel = (MODERN_UI_RECT){ 0, 0, 0, 0 };
  }

  QuickPanel = (MODERN_UI_RECT){ Content.X, QuickY, Content.Width, QuickHeight };
  DrawDashboardSection (Ui, Theme, SystemPanel, L"System Information", TRUE);
  ModernUiDrawFocusFrame (Ui, SystemPanel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 58, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringFirmwareVendor), Platform.FirmwareVendor);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 90, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringFirmwareRevision), Platform.FirmwareRevision);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 122, SystemPanel.Width - 44, L"Platform", Platform.Platform);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 154, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringFormFactor), Platform.FormFactor);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 186, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringBootMode), Platform.BootMode);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 218, SystemPanel.Width - 44, L"Memory", MemoryText);
  if (TopHeight >= 260) {
    DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 250, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringDisplay), Resolution);
  }

  if (MonitorPanel.Width > 0) {
    DrawDashboardSection (Ui, Theme, MonitorPanel, L"Hardware Monitor", FALSE);
    ModernUiDrawText (Ui, MonitorPanel.X + 22, MonitorPanel.Y + 58, L"CPU", Theme->WarningText, PanelBackground);
    DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 88, MonitorPanel.Width - 44, L"Architecture", ArchitectureText);
    DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 120, MonitorPanel.Width - 44, L"Provider", L"UEFI");
    ModernUiFillRect (Ui, (MODERN_UI_RECT){ MonitorPanel.X + 22, MonitorPanel.Y + 154, MonitorPanel.Width - 44, 1 }, Theme->Border);
    ModernUiDrawText (Ui, MonitorPanel.X + 22, MonitorPanel.Y + 178, L"Providers", Theme->WarningText, PanelBackground);
    DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 208, MonitorPanel.Width - 44, ModernUiGetString (ModernUiStringFirmwareUpdate), CapabilityText (Firmware.CapsuleRuntimeServices || Firmware.CapsuleArchProtocol));
    DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 240, MonitorPanel.Width - 44, ModernUiGetString (ModernUiStringDiagnosticsLogs), CapabilityText (Diagnostics.AcpiPresent || Diagnostics.SmbiosPresent));
    if (TopHeight >= 300) {
      DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 272, MonitorPanel.Width - 44, ModernUiGetString (ModernUiStringManagement), CapabilityText (Management.IpmiProtocolPresent || Management.RedfishDiscoverPresent || Management.SmbiosManagementInterfacePresent));
    }
  }

  if (QuickPanel.Height > 110) {
    DrawDashboardSection (Ui, Theme, QuickPanel, L"Quick Access", FALSE);
    CardGap   = 14;
    CardWidth = (QuickPanel.Width > ((CardGap * 2) + 40)) ? ((QuickPanel.Width - (CardGap * 2) - 40) / 3) : QuickPanel.Width;
    QuickCard = (MODERN_UI_RECT){ QuickPanel.X + 20, QuickPanel.Y + 54, CardWidth, QuickPanel.Height - 74 };
    DrawDashboardTile (Ui, Theme, QuickCard, ModernUiGetString (ModernUiStringBootOptions), BootCount, TRUE, (BOOLEAN)((Focus == SetupFocusContent) && (Selection == 0)));
    QuickCard.X += CardWidth + CardGap;
    DrawDashboardTile (Ui, Theme, QuickCard, L"Devices / HII", DeviceCount, TRUE, (BOOLEAN)((Focus == SetupFocusContent) && (Selection == 1)));
    QuickCard.X += CardWidth + CardGap;
    DrawDashboardTile (Ui, Theme, QuickCard, ModernUiGetString (ModernUiStringSecureBoot), SecurityText, (BOOLEAN)(Security.SecureBoot == ModernUiSecurityStateEnabled), (BOOLEAN)((Focus == SetupFocusContent) && (Selection == 2)));
  }
}

/**
  Draw the Boot page with launchable BootOrder entries.

  @param[in] Ui        Initialized render context. Must not be NULL.
  @param[in] Theme     Theme token table. Must not be NULL.
  @param[in] Focus     Current focus area.
  @param[in] Selected  Selected BootOrder row.
**/
STATIC
VOID
DrawBoot (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     Selected
  )
{
  EFI_STATUS                    Status;
  MODERN_UI_BOOT_OPTION         *BootOptions;
  UINTN                         BootOptionCount;
  UINTN                         Index;
  UINTN                         Y;
  CHAR16                        Line[160];
  CHAR16                        Value[96];
  CONST CHAR16                  *State;
  BOOLEAN                       IsSelected;
  MODERN_UI_RECT                Panel;
  UINTN                         RowX;
  UINTN                         RowWidth;
  UINTN                         MaxRows;
  MODERN_UI_ROW_MODEL           RowModel;

  Panel = ContentRect (Ui);
  RowX = Panel.X + 20;
  RowWidth = Panel.Width - 40;
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 20, ModernUiGetString (ModernUiStringBootInstruction), Theme->MutedText, Theme->Surface);

  BootOptions = NULL;
  Status = ModernUiBootDataGetOptions (mImageHandle, &BootOptions, &BootOptionCount);
  if (EFI_ERROR (Status) || (BootOptions == NULL)) {
    ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 66, ModernUiGetString (ModernUiStringNoBootOptions), Theme->Warning, Theme->Surface);
    return;
  }

  MaxRows = (Panel.Height > 96) ? ((Panel.Height - 92) / 58) : 0;
  MaxRows = MIN (MaxRows, MAX_BOOT_ROWS);
  for (Index = 0; (Index < BootOptionCount) && (Index < MaxRows); Index++) {
    Y           = Panel.Y + 62 + Index * 58;
    State       = BootOptions[Index].Active ? ModernUiGetString (ModernUiStringActive) : ModernUiGetString (ModernUiStringInactive);
    IsSelected  = (BOOLEAN)((Focus == SetupFocusContent) && (Index == Selected));
    UnicodeSPrint (
      Line,
      sizeof (Line),
      L"%02u  Boot%04x  %s",
      Index + 1,
      BootOptions[Index].OptionNumber,
      BootOptions[Index].Description
      );
    UnicodeSPrint (
      Value,
      sizeof (Value),
      L"%s%s%s",
      State,
      BootOptions[Index].Hidden ? L" / Hidden / " : L" / ",
      BootOptions[Index].Category
      );
    RowModel.Rect      = (MODERN_UI_RECT){ RowX, Y - 8, RowWidth, 42 };
    RowModel.Prompt    = Line;
    RowModel.Value     = Value;
    RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowNormal;
    RowModel.ValueType = ModernUiValueText;
    ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
    ModernUiDrawTextFit (
      Ui,
      RowX + 20,
      Y + 18,
      RowWidth - 40,
      BootOptions[Index].FilePathSummary,
      Theme->MutedText,
      IsSelected ? Theme->SelectedBand : Theme->Surface
      );
  }

  if (BootOptionCount == 0) {
    ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 66, ModernUiGetString (ModernUiStringNoBootOptions), Theme->Warning, Theme->Surface);
  }

  ModernUiBootDataFreeOptions (BootOptions, BootOptionCount);
}

/**
  Draw the Devices page with a small handle/device-path inventory.

  @param[in] Ui        Initialized render context. Must not be NULL.
  @param[in] Theme     Theme token table. Must not be NULL.
  @param[in] Focus     Current focus area.
  @param[in] Selected  Selected device-path row.
**/
STATIC
VOID
DrawDevices (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     Selected
  )
{
  EFI_STATUS                Status;
  MODERN_UI_DEVICE_ENTRY    *Entries;
  UINTN                     EntryCount;
  UINTN                     Index;
  UINTN                     HiiCount;
  CHAR16                    Line[168];
  CHAR16                    Summary[96];
  BOOLEAN                   IsSelected;
  MODERN_UI_RECT            Panel;
  UINTN                     RowX;
  UINTN                     RowWidth;
  MODERN_UI_ROW_MODEL     RowModel;

  Panel = ContentRect (Ui);
  RowX = Panel.X + 20;
  RowWidth = Panel.Width - 40;
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);

  Entries = NULL;
  Status = ModernUiDeviceDataGetEntries (&Entries, &EntryCount);
  if (EFI_ERROR (Status)) {
    ModernUiDrawText (Ui, 280, 150, ModernUiGetString (ModernUiStringUnableEnumerateHandles), Theme->Warning, Theme->Surface);
    return;
  }

  HiiCount = 0;
  for (Index = 0; Index < EntryCount; Index++) {
    if (Entries[Index].HasForm) {
      HiiCount++;
    }
  }

  UnicodeSPrint (Summary, sizeof (Summary), L"%u entries (%u HII, %u device)", EntryCount, HiiCount, EntryCount - HiiCount);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 20, Summary, Theme->MutedText, Theme->Surface);

  for (Index = 0; (Index < EntryCount) && (Index < MAX_DEVICE_ROWS); Index++) {
    UnicodeSPrint (Line, sizeof (Line), L"%02u  %s", Index + 1, Entries[Index].Title);
    IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && (Index == Selected));
    RowModel.Rect      = (MODERN_UI_RECT){ RowX, Panel.Y + 54 + Index * 36, RowWidth, 30 };
    RowModel.Prompt    = Line;
    RowModel.Value     = Entries[Index].HasForm ? L"HII >" : L"Device";
    RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowNormal;
    RowModel.ValueType = Entries[Index].HasForm ? ModernUiValueAction : ModernUiValueText;
    ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
  }

  if (EntryCount == 0) {
    ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 66, L"No HII formsets found.", Theme->Warning, Theme->Surface);
  }

  ModernUiDeviceDataFreeEntries (Entries, EntryCount);
}


/**
  Draw the Security page with read-only Secure Boot state.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawSecurity (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_UI_SECURITY_SUMMARY  Summary;
  MODERN_UI_RECT  Panel;
  CONST CHAR16    *SecureBootText;
  CONST CHAR16    *SetupModeText;
  CONST CHAR16    *PkText;
  CONST CHAR16    *KekText;
  CONST CHAR16    *DbText;
  CONST CHAR16    *DbxText;
  CONST CHAR16    *Tcg2Text;
  CONST CHAR16    *TreeText;

  if (EFI_ERROR (ModernUiSecurityDataGetSummary (&Summary))) {
    ZeroMem (&Summary, sizeof (Summary));
  }

  SecureBootText = (Summary.SecureBoot == ModernUiSecurityStateEnabled) ? ModernUiGetString (ModernUiStringEnabled) :
                   ((Summary.SecureBoot == ModernUiSecurityStateDisabled) ? ModernUiGetString (ModernUiStringDisabled) : L"Unknown");
  SetupModeText = (Summary.SetupMode == ModernUiSecurityStateEnabled) ? ModernUiGetString (ModernUiStringEnabled) :
                  ((Summary.SetupMode == ModernUiSecurityStateDisabled) ? ModernUiGetString (ModernUiStringDisabled) : L"Unknown");
  PkText   = SecurityStateText (Summary.PlatformKey);
  KekText  = SecurityStateText (Summary.KeyExchangeKey);
  DbText   = SecurityStateText (Summary.SignatureDb);
  DbxText  = SecurityStateText (Summary.ForbiddenSignatureDb);
  Tcg2Text = SecurityStateText (Summary.Tcg2Protocol);
  TreeText = SecurityStateText (Summary.TreeProtocol);
  Panel = ContentRect (Ui);
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 24, ModernUiGetString (ModernUiStringSecureBoot), Theme->MutedText, Theme->Surface);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 64, SecureBootText, (Summary.SecureBoot == ModernUiSecurityStateEnabled) ? Theme->Success : Theme->Warning, Theme->Surface);
  ModernUiDrawTextFormatted (Ui, Panel.X + 20, Panel.Y + 104, Theme->MutedText, Theme->Surface, L"Setup Mode: %s", SetupModeText);
  ModernUiDrawTextFormatted (Ui, Panel.X + 20, Panel.Y + 136, Theme->MutedText, Theme->Surface, L"PK: %s    KEK: %s", PkText, KekText);
  ModernUiDrawTextFormatted (Ui, Panel.X + 20, Panel.Y + 168, Theme->MutedText, Theme->Surface, L"db: %s    dbx: %s", DbText, DbxText);
  ModernUiDrawTextFormatted (Ui, Panel.X + 20, Panel.Y + 200, Theme->MutedText, Theme->Surface, L"TCG2: %s    TrEE: %s", Tcg2Text, TreeText);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 252, ModernUiGetString (ModernUiStringSecurityReadOnly), Theme->MutedText, Theme->Surface);
}

/**
  Draw the Firmware page with read-only update and capsule state.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawFirmware (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_UI_FIRMWARE_SUMMARY  Summary;
  CONST CHAR16                *Labels[5];
  CONST CHAR16                *Values[5];

  if (EFI_ERROR (ModernUiFirmwareDataGetSummary (&Summary))) {
    ZeroMem (&Summary, sizeof (Summary));
    StrCpyS (Summary.Vendor, ARRAY_SIZE (Summary.Vendor), ModernUiGetString (ModernUiStringUnknown));
    StrCpyS (Summary.Revision, ARRAY_SIZE (Summary.Revision), ModernUiGetString (ModernUiStringUnknown));
  }

  Labels[0] = ModernUiGetString (ModernUiStringFirmwareVendor);
  Values[0] = Summary.Vendor;
  Labels[1] = ModernUiGetString (ModernUiStringFirmwareRevision);
  Values[1] = Summary.Revision;
  Labels[2] = ModernUiGetString (ModernUiStringCapsuleRuntime);
  Values[2] = CapabilityText (Summary.CapsuleRuntimeServices);
  Labels[3] = ModernUiGetString (ModernUiStringCapsuleProtocol);
  Values[3] = CapabilityText (Summary.CapsuleArchProtocol);
  Labels[4] = ModernUiGetString (ModernUiStringCapsuleReport);
  Values[4] = Summary.CapsuleReportPresent ? ModernUiGetString (ModernUiStringPresent) : ModernUiGetString (ModernUiStringNotAvailable);

  DrawProviderSummaryPage (
    Ui,
    Theme,
    Focus,
    ModernUiGetString (ModernUiStringFirmwareUpdate),
    Labels,
    Values,
    ARRAY_SIZE (Labels)
    );
}

/**
  Draw the Diagnostics page with read-only bring-up and table state.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawDiagnostics (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_UI_DIAGNOSTICS_SUMMARY  Summary;
  CHAR16                         MemoryMap[48];
  CHAR16                         Handles[48];
  CHAR16                         Tables[48];
  CONST CHAR16                   *Labels[5];
  CONST CHAR16                   *Values[5];

  if (EFI_ERROR (ModernUiDiagnosticsDataGetSummary (&Summary))) {
    ZeroMem (&Summary, sizeof (Summary));
  }

  UnicodeSPrint (MemoryMap, sizeof (MemoryMap), L"%u", Summary.MemoryDescriptorCount);
  UnicodeSPrint (Handles, sizeof (Handles), L"%u", Summary.HandleCount);
  UnicodeSPrint (Tables, sizeof (Tables), L"%u", Summary.ConfigurationTableCount);

  Labels[0] = ModernUiGetString (ModernUiStringAcpiTables);
  Values[0] = CapabilityText (Summary.AcpiPresent);
  Labels[1] = ModernUiGetString (ModernUiStringSmbiosTables);
  Values[1] = CapabilityText (Summary.SmbiosPresent);
  Labels[2] = ModernUiGetString (ModernUiStringMemoryMap);
  Values[2] = MemoryMap;
  Labels[3] = ModernUiGetString (ModernUiStringDxeHandles);
  Values[3] = Handles;
  Labels[4] = ModernUiGetString (ModernUiStringConfigurationTables);
  Values[4] = Tables;

  DrawProviderSummaryPage (
    Ui,
    Theme,
    Focus,
    ModernUiGetString (ModernUiStringDiagnosticsLogs),
    Labels,
    Values,
    ARRAY_SIZE (Labels)
    );
}

/**
  Draw the Management page with read-only BMC/IPMI/Redfish state.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawManagement (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_UI_MANAGEMENT_SUMMARY  Summary;
  CONST CHAR16                  *Labels[3];
  CONST CHAR16                  *Values[3];

  if (EFI_ERROR (ModernUiManagementDataGetSummary (&Summary))) {
    ZeroMem (&Summary, sizeof (Summary));
  }

  Labels[0] = ModernUiGetString (ModernUiStringIpmi);
  Values[0] = CapabilityText (Summary.IpmiProtocolPresent);
  Labels[1] = ModernUiGetString (ModernUiStringRedfish);
  Values[1] = CapabilityText (Summary.RedfishDiscoverPresent);
  Labels[2] = ModernUiGetString (ModernUiStringManagementInterface);
  Values[2] = CapabilityText (Summary.SmbiosManagementInterfacePresent);

  DrawProviderSummaryPage (
    Ui,
    Theme,
    Focus,
    ModernUiGetString (ModernUiStringManagement),
    Labels,
    Values,
    ARRAY_SIZE (Labels)
    );
}

/**
  Draw the Power page with read-only ACPI and thermal provider state.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawPower (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_UI_POWER_SUMMARY  Summary;
  CONST CHAR16             *Labels[5];
  CONST CHAR16             *Values[5];

  if (EFI_ERROR (ModernUiPowerDataGetSummary (&Summary))) {
    ZeroMem (&Summary, sizeof (Summary));
    StrCpyS (Summary.ChassisThermalState, ARRAY_SIZE (Summary.ChassisThermalState), ModernUiGetString (ModernUiStringUnknown));
  }

  Labels[0] = ModernUiGetString (ModernUiStringAcpiTablesProvider);
  Values[0] = CapabilityText (Summary.AcpiTablePresent);
  Labels[1] = ModernUiGetString (ModernUiStringAcpiSdtProtocol);
  Values[1] = CapabilityText (Summary.AcpiSdtProtocolPresent);
  Labels[2] = ModernUiGetString (ModernUiStringChassisThermalState);
  Values[2] = Summary.ChassisThermalState;
  Labels[3] = ModernUiGetString (ModernUiStringPowerSupply);
  Values[3] = CapabilityText (Summary.SmbiosPowerSupplyPresent);
  Labels[4] = ModernUiGetString (ModernUiStringSmbiosTables);
  Values[4] = CapabilityText (Summary.SmbiosChassisPresent);

  DrawProviderSummaryPage (
    Ui,
    Theme,
    Focus,
    ModernUiGetString (ModernUiStringPowerThermal),
    Labels,
    Values,
    ARRAY_SIZE (Labels)
    );
}

/**
  Draw the Performance page with read-only tuning provider availability.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawPerformance (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_UI_PERFORMANCE_SUMMARY  Summary;
  CONST CHAR16                   *Labels[5];
  CONST CHAR16                   *Values[5];

  if (EFI_ERROR (ModernUiPerformanceDataGetSummary (&Summary))) {
    ZeroMem (&Summary, sizeof (Summary));
  }

  Labels[0] = ModernUiGetString (ModernUiStringProcessorInventory);
  Values[0] = CapabilityText (Summary.ProcessorInventoryPresent);
  Labels[1] = ModernUiGetString (ModernUiStringMemoryInventory);
  Values[1] = CapabilityText (Summary.MemoryInventoryPresent);
  Labels[2] = ModernUiGetString (ModernUiStringCpuIo2);
  Values[2] = CapabilityText (Summary.CpuIo2ProtocolPresent);
  Labels[3] = ModernUiGetString (ModernUiStringVirtualizationPolicy);
  Values[3] = CapabilityText (Summary.VirtualizationPolicyEntryPresent);
  Labels[4] = ModernUiGetString (ModernUiStringRasPolicy);
  Values[4] = CapabilityText (Summary.RasPolicyEntryPresent);

  DrawProviderSummaryPage (
    Ui,
    Theme,
    Focus,
    ModernUiGetString (ModernUiStringPerformanceTuning),
    Labels,
    Values,
    ARRAY_SIZE (Labels)
    );
}

/**
  Draw the Exit page and selected action.

  @param[in] Ui        Initialized render context. Must not be NULL.
  @param[in] Theme     Theme token table. Must not be NULL.
  @param[in] Focus     Current focus area.
  @param[in] Selected  Selected action index. Values 0..3 are expected.
**/
STATIC
VOID
DrawExit (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     Selected
  )
{
  CONST CHAR16  *Items[4];
  CONST CHAR16  *LanguageName;
  UINTN         Index;
  UINTN         Y;
  UINTN         ValueWidth;
  BOOLEAN       IsSelected;
  MODERN_UI_RECT  Panel;
  UINTN         RowX;
  UINTN         RowWidth;
  MODERN_UI_ROW_MODEL  RowModel;
  MODERN_UI_POPUP_MODEL  PopupModel;

  LanguageName = GetLanguageOptionName (GetActiveLanguageSelection ());

  Items[0] = ModernUiGetString (ModernUiStringExitContinue);
  Items[1] = ModernUiGetString (ModernUiStringExitClassicUi);
  Items[2] = ModernUiGetString (ModernUiStringExitReset);
  Items[3] = ModernUiGetString (ModernUiStringLanguageLabel);

  Panel = ContentRect (Ui);
  RowX = Panel.X + 26;
  RowWidth = Panel.Width - 52;
  ValueWidth = 220;
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 20, ModernUiGetString (ModernUiStringExitInstruction), Theme->MutedText, Theme->Surface);

  for (Index = 0; Index < ARRAY_SIZE (Items); Index++) {
    Y = Panel.Y + 72 + Index * 54;
    IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && (Index == Selected));
    RowModel.Rect      = (MODERN_UI_RECT){ RowX, Y - 10, RowWidth, 40 };
    RowModel.Prompt    = Items[Index];
    RowModel.Value     = (Index == 3) ? LanguageName : NULL;
    RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowAction;
    RowModel.ValueType = (Index == 3) ? ModernUiValueOneOf : ModernUiValueNone;
    ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
  }

  if (mLanguageDropdownOpen) {
    UINTN  DropdownY;
    UINTN  DropdownX;
    UINTN  Option;

    DropdownX = RowX + RowWidth - ValueWidth - 12;
    DropdownY = Panel.Y + 72 + 4 * 54 - 8;
    PopupModel.Rect  = (MODERN_UI_RECT){ DropdownX, DropdownY, ValueWidth, 80 };
    PopupModel.Title = NULL;
    ModernUiEngineDrawPopup (Ui, &PopupModel, Theme);

    for (Option = 0; Option < 2; Option++) {
      IsSelected = (BOOLEAN)(Option == mLanguageDropdownSelection);
      RowModel.Rect      = (MODERN_UI_RECT){ DropdownX + 6, DropdownY + 7 + Option * 34, ValueWidth - 12, 30 };
      RowModel.Prompt    = GetLanguageOptionName (Option);
      RowModel.Value     = NULL;
      RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowNormal;
      RowModel.ValueType = ModernUiValueNone;
      ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
    }
  }
}

/**
  Apply one selected UI language and format a user-visible status message.

  @param[in]  Selection      Language selector index. Zero is Chinese, one is
                             English.
  @param[out] StatusMessage  Status buffer to update. Must not be NULL.
  @param[in]  StatusSize     Size of StatusMessage in bytes.
**/
STATIC
VOID
ApplyLanguageSelection (
  IN  UINTN   Selection,
  OUT CHAR16  *StatusMessage,
  IN  UINTN   StatusSize
  )
{
  EFI_STATUS    Status;
  CONST CHAR8   *Language;
  CONST CHAR16  *LanguageName;

  if ((StatusMessage == NULL) || (StatusSize < sizeof (CHAR16))) {
    return;
  }

  Language     = (Selection == 0) ? "zh-Hans" : "en-US";
  Status       = ModernUiSetLanguage (Language, TRUE);
  LanguageName = GetLanguageOptionName (GetActiveLanguageSelection ());

  if (EFI_ERROR (Status)) {
    UnicodeSPrint (StatusMessage, StatusSize, L"Set language variable returned: %r", Status);
  } else {
    UnicodeSPrint (StatusMessage, StatusSize, ModernUiGetString (ModernUiStringLanguageChangedFormat), LanguageName);
  }
}

/**
  Open the language drop-down or apply the highlighted language.

  @param[out] StatusMessage  Status buffer to update. Must not be NULL.
  @param[in]  StatusSize     Size of StatusMessage in bytes.
**/
STATIC
VOID
HandleLanguageSelectorEnter (
  OUT CHAR16  *StatusMessage,
  IN  UINTN   StatusSize
  )
{
  if ((StatusMessage == NULL) || (StatusSize < sizeof (CHAR16))) {
    return;
  }

  StatusMessage[0] = L'\0';
  if (!mLanguageDropdownOpen) {
    mLanguageDropdownSelection = GetActiveLanguageSelection ();
    mLanguageDropdownOpen      = TRUE;
    return;
  }

  ApplyLanguageSelection (mLanguageDropdownSelection, StatusMessage, StatusSize);
  mLanguageDropdownOpen = FALSE;
}


/**
  Load and start the classic edk2 UiApp from the same firmware volume.

  @param[in] ImageHandle  Current image handle. Must not be NULL.

  @retval EFI_SUCCESS           UiApp returned successfully.
  @retval EFI_NOT_FOUND         Current image device path could not be resolved.
  @retval EFI_OUT_OF_RESOURCES  Device path allocation failed.
  @retval others                Status returned by HandleProtocol(), LoadImage(),
                                or StartImage().
**/
STATIC
EFI_STATUS
LaunchUiAppFallback (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS                         Status;
  EFI_LOADED_IMAGE_PROTOCOL          *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL           *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL           *AppPath;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  FileNode;
  EFI_HANDLE                         ChildHandle;

  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DevicePath = DevicePathFromHandle (LoadedImage->DeviceHandle);
  if (DevicePath == NULL) {
    return EFI_NOT_FOUND;
  }

  EfiInitializeFwVolDevicepathNode (&FileNode, &mUiAppGuid);
  AppPath = AppendDevicePathNode (DevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&FileNode);
  if (AppPath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ChildHandle = NULL;
  Status      = gBS->LoadImage (FALSE, ImageHandle, AppPath, NULL, 0, &ChildHandle);
  FreePool (AppPath);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return gBS->StartImage (ChildHandle, NULL, NULL);
}

/**
  Draw the full application frame for one page.

  @param[in] Ui             Initialized render context. Must not be NULL.
  @param[in] Theme          Theme token table. Must not be NULL.
  @param[in] Page           Page to draw.
  @param[in] Focus          Current focus area.
  @param[in] DashboardSelection Selected Dashboard Quick Access entry.
  @param[in] BootSelection  Selected Boot page row.
  @param[in] DeviceSelection Selected Devices page row.
  @param[in] ExitSelection  Selected Exit page action.
  @param[in] StatusMessage  Optional status text. May be NULL.
**/
STATIC
VOID
DrawPage (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_PAGE                Page,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     DashboardSelection,
  IN UINTN                     BootSelection,
  IN UINTN                     DeviceSelection,
  IN UINTN                     ExitSelection,
  IN CONST CHAR16              *StatusMessage
  )
{
  ModernUiClear (Ui, Theme->Background);
  DrawHeader (Ui, Theme);
  DrawTabs (Ui, Theme, Page, Focus);
  DrawPageTitle (Ui, Theme, Page);

  switch (Page) {
    case PageDashboard:
      DrawDashboard (Ui, Theme, Focus, DashboardSelection);
      break;
    case PageBoot:
      DrawBoot (Ui, Theme, Focus, BootSelection);
      break;
    case PageDevices:
      DrawDevices (Ui, Theme, Focus, DeviceSelection);
      break;
    case PageSecurity:
      DrawSecurity (Ui, Theme, Focus);
      break;
    case PageFirmware:
      DrawFirmware (Ui, Theme, Focus);
      break;
    case PageDiagnostics:
      DrawDiagnostics (Ui, Theme, Focus);
      break;
    case PageManagement:
      DrawManagement (Ui, Theme, Focus);
      break;
    case PagePower:
      DrawPower (Ui, Theme, Focus);
      break;
    case PagePerformance:
      DrawPerformance (Ui, Theme, Focus);
      break;
    case PageExit:
      DrawExit (Ui, Theme, Focus, ExitSelection);
      break;
    default:
      break;
  }

  DrawFooter (Ui, Theme, Focus, StatusMessage);
}

/**
  ModernSetupApp entry point.

  @param[in] ImageHandle  UEFI image handle for this application.
  @param[in] SystemTable  UEFI system table. Must not be NULL.

  @retval EFI_SUCCESS  User selected continue boot or left setup.
  @retval others       Renderer initialization failed.
**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  MODERN_UI_RENDER_CONTEXT  Ui;
  MODERN_UI_INPUT_CONTEXT   Input;
  MODERN_UI_INPUT_EVENT     Event;
  CONST MODERN_UI_THEME     *Theme;
  SETUP_PAGE                Page;
  SETUP_FOCUS               Focus;
  UINTN                     DashboardSelection;
  UINTN                     BootSelection;
  UINTN                     DeviceSelection;
  UINTN                     ExitSelection;
  UINTN                     Selection;
  UINTN                     SelectableCount;
  CHAR16                    StatusMessage[96];
  BOOLEAN                   Redraw;

  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  mImageHandle = ImageHandle;

  Status = ModernUiRendererInit (&Ui);
  if (EFI_ERROR (Status)) {
    Print (ModernUiGetString (ModernUiStringGraphicsInitFailedFormat), Status);
    return Status;
  }

  EfiBootManagerConnectAll ();
  EfiBootManagerRefreshAllBootOption ();
  ModernUiInputInit (&Input);
  Theme         = ModernUiGetTheme ();
  Page          = PageDashboard;
  Focus         = SetupFocusNav;
  DashboardSelection = 0;
  BootSelection = 0;
  DeviceSelection = 0;
  ExitSelection = 0;
  StatusMessage[0] = L'\0';
  Redraw        = TRUE;

  for (;;) {
    if (Redraw) {
      DrawPage (&Ui, Theme, Page, Focus, DashboardSelection, BootSelection, DeviceSelection, ExitSelection, StatusMessage);
      Redraw = FALSE;
    }

    Status = ModernUiReadInput (&Input, &Event);
    if (EFI_ERROR (Status)) {
      continue;
    }

    switch (Event.Type) {
      case ModernUiInputUp:
        if ((Focus == SetupFocusContent) && (Page == PageExit) && mLanguageDropdownOpen) {
          mLanguageDropdownSelection = (mLanguageDropdownSelection == 0) ? 1 : 0;
        } else if ((Focus == SetupFocusContent) && (Page == PageDashboard)) {
          Focus = SetupFocusNav;
        } else if (Focus == SetupFocusContent) {
          SelectableCount = GetPageSelectableCount (&Ui, Page);
          if (SelectableCount > 0) {
            Selection = GetPageSelection (Page, DashboardSelection, BootSelection, DeviceSelection, ExitSelection);
            Selection = (Selection == 0) ? (SelectableCount - 1) : (Selection - 1);
            SetPageSelection (Page, Selection, &DashboardSelection, &BootSelection, &DeviceSelection, &ExitSelection);
          }
        }

        Redraw = TRUE;
        break;
      case ModernUiInputDown:
        if ((Focus == SetupFocusContent) && (Page == PageExit) && mLanguageDropdownOpen) {
          mLanguageDropdownSelection = (mLanguageDropdownSelection + 1) % 2;
        } else if (Focus == SetupFocusNav) {
          if (GetPageSelectableCount (&Ui, Page) > 0) {
            Focus = SetupFocusContent;
          }
        } else {
          SelectableCount = GetPageSelectableCount (&Ui, Page);
          if (SelectableCount > 0) {
            Selection = GetPageSelection (Page, DashboardSelection, BootSelection, DeviceSelection, ExitSelection);
            Selection = (Selection + 1) % SelectableCount;
            SetPageSelection (Page, Selection, &DashboardSelection, &BootSelection, &DeviceSelection, &ExitSelection);
          }
        }

        Redraw = TRUE;
        break;
      case ModernUiInputTab:
        mLanguageDropdownOpen = FALSE;
        Focus  = (Focus == SetupFocusNav) ? SetupFocusContent : SetupFocusNav;
        Redraw = TRUE;
        break;
      case ModernUiInputLeft:
        if ((Focus == SetupFocusContent) && (Page == PageExit) && mLanguageDropdownOpen) {
          mLanguageDropdownOpen = FALSE;
        } else if ((Focus == SetupFocusContent) && (Page == PageDashboard)) {
          DashboardSelection = (DashboardSelection == 0) ? 2 : (DashboardSelection - 1);
        } else if (Focus == SetupFocusNav) {
          Page = (Page == 0) ? (PageMax - 1) : (Page - 1);
          mLanguageDropdownOpen = FALSE;
        } else {
          Focus = SetupFocusNav;
        }

        StatusMessage[0] = L'\0';
        Redraw = TRUE;
        break;
      case ModernUiInputRight:
        if (Focus == SetupFocusNav) {
          Page = (Page + 1) % PageMax;
          mLanguageDropdownOpen = FALSE;
        } else if (Page == PageDashboard) {
          DashboardSelection = (DashboardSelection + 1) % 3;
          StatusMessage[0] = L'\0';
        } else {
          mLanguageDropdownOpen = FALSE;
          StatusMessage[0] = L'\0';
        }

        Redraw = TRUE;
        break;
      case ModernUiInputEscape:
        if ((Focus == SetupFocusContent) && (Page == PageExit) && mLanguageDropdownOpen) {
          mLanguageDropdownOpen = FALSE;
          StatusMessage[0] = L'\0';
          Redraw = TRUE;
        } else if (Focus == SetupFocusContent) {
          Focus  = SetupFocusNav;
          Redraw = TRUE;
        } else {
          return EFI_SUCCESS;
        }

        break;
      case ModernUiInputEnter:
        if (Focus == SetupFocusNav) {
          if (GetPageSelectableCount (&Ui, Page) > 0) {
            Focus = SetupFocusContent;
          }
          StatusMessage[0] = L'\0';
          Redraw = TRUE;
        } else if (Page == PageDashboard) {
          if (DashboardSelection == 0) {
            Page  = PageBoot;
            Focus = SetupFocusContent;
          } else if (DashboardSelection == 1) {
            Page  = PageDevices;
            Focus = SetupFocusContent;
          } else {
            Page  = PageSecurity;
            Focus = SetupFocusNav;
          }

          StatusMessage[0] = L'\0';
          Redraw = TRUE;
        } else if (Page == PageBoot) {
          Status = LaunchSelectedBootOption (BootSelection);
          UnicodeSPrint (StatusMessage, sizeof (StatusMessage), ModernUiGetString (ModernUiStringBootReturnedFormat), Status);
          Redraw = TRUE;
        } else if (Page == PageDevices) {
          Status = OpenSelectedDeviceEntry (DeviceSelection);
          UnicodeSPrint (StatusMessage, sizeof (StatusMessage), L"FormBrowser returned: %r", Status);
          Redraw = TRUE;
        } else if (Page == PageExit) {
          if (ExitSelection == 0) {
            return EFI_SUCCESS;
          } else if (ExitSelection == 1) {
            Status = LaunchUiAppFallback (ImageHandle);
            UnicodeSPrint (StatusMessage, sizeof (StatusMessage), ModernUiGetString (ModernUiStringClassicReturnedFormat), Status);
            Redraw = TRUE;
          } else if (ExitSelection == 2) {
            gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
          } else {
            HandleLanguageSelectorEnter (StatusMessage, sizeof (StatusMessage));
            Redraw = TRUE;
          }
        }
        break;
      default:
        break;
    }
  }
}
