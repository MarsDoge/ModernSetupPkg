/** @file
  Modern graphical setup application prototype.

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
#include <ModernUi/ModernUiInput.h>
#include <ModernUi/ModernUiEngine.h>
#include <ModernUi/ModernUiPlatformData.h>
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
  { PageExit,      ModernUiStringPageExit,      ModernUiStringPageExitHint      }
};

STATIC BOOLEAN         mLanguageDropdownOpen;
STATIC UINTN           mLanguageDropdownSelection;

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
  @param[in] BootSelection    Current Boot page selection.
  @param[in] DeviceSelection  Current Devices page selection.
  @param[in] ExitSelection    Current Exit page selection.

  @return Selected item index for Page. Pages without selectable rows return 0.
**/
STATIC
UINTN
GetPageSelection (
  IN SETUP_PAGE  Page,
  IN UINTN       BootSelection,
  IN UINTN       DeviceSelection,
  IN UINTN       ExitSelection
  )
{
  switch (Page) {
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
  @param[in,out] BootSelection    Boot page selection storage. Must not be NULL.
  @param[in,out] DeviceSelection  Devices page selection storage. Must not be NULL.
  @param[in,out] ExitSelection    Exit page selection storage. Must not be NULL.
**/
STATIC
VOID
SetPageSelection (
  IN     SETUP_PAGE  Page,
  IN     UINTN       Selection,
  IN OUT UINTN       *BootSelection,
  IN OUT UINTN       *DeviceSelection,
  IN OUT UINTN       *ExitSelection
  )
{
  switch (Page) {
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

  @param[in] Page  Page whose selectable count is requested.

  @return Number of selectable rows or actions available on Page.
**/
STATIC
UINTN
GetPageSelectableCount (
  IN SETUP_PAGE  Page
  )
{
  switch (Page) {
    case PageBoot:
      return MIN (GetBootCount (), MAX_BOOT_ROWS);
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
  Draw the Dashboard page.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawDashboard (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  CHAR16  Resolution[48];
  CHAR16  BootCount[48];
  CHAR16  MemoryText[48];
  CHAR16  SecurityText[48];
  UINTN   CardWidth;
  MODERN_UI_RECT  Content;
  MODERN_UI_RECT  StatusRect;
  MODERN_UI_PLATFORM_SUMMARY  Platform;
  MODERN_UI_SECURITY_SUMMARY  Security;

  Content = ContentRect (Ui);
  CardWidth = (Content.Width - CARD_GAP) / 2;
  StatusRect = (MODERN_UI_RECT){ Content.X, Content.Y + 216, Content.Width, 112 };
  UnicodeSPrint (Resolution, sizeof (Resolution), L"%u x %u", Ui->Width, Ui->Height);
  UnicodeSPrint (BootCount, sizeof (BootCount), ModernUiGetString (ModernUiStringBootCountFormat), GetBootCount ());
  if (EFI_ERROR (ModernUiPlatformDataGetSummary (&Platform))) {
    ZeroMem (&Platform, sizeof (Platform));
    StrCpyS (Platform.FirmwareVendor, ARRAY_SIZE (Platform.FirmwareVendor), L"Unknown");
    StrCpyS (Platform.FirmwareRevision, ARRAY_SIZE (Platform.FirmwareRevision), L"Unknown");
    StrCpyS (Platform.Architecture, ARRAY_SIZE (Platform.Architecture), L"Unknown");
  }

  ModernUiSecurityDataGetSummary (&Security);
  UnicodeSPrint (MemoryText, sizeof (MemoryText), L"%lu MB", Platform.MemorySizeMb);
  UnicodeSPrint (
    SecurityText,
    sizeof (SecurityText),
    L"%s / %s",
    Platform.Architecture,
    (Security.SecureBoot == ModernUiSecurityStateEnabled) ? ModernUiGetString (ModernUiStringEnabled) : ModernUiGetString (ModernUiStringDisabled)
    );

  ModernUiDrawInfoCard (Ui, (MODERN_UI_RECT){ Content.X, Content.Y, CardWidth, 92 }, ModernUiGetString (ModernUiStringFirmwareVendor), Platform.FirmwareVendor, Theme);
  ModernUiDrawInfoCard (Ui, (MODERN_UI_RECT){ Content.X + CardWidth + CARD_GAP, Content.Y, CardWidth, 92 }, ModernUiGetString (ModernUiStringFirmwareRevision), Platform.FirmwareRevision, Theme);
  ModernUiDrawInfoCard (Ui, (MODERN_UI_RECT){ Content.X, Content.Y + 108, CardWidth, 92 }, ModernUiGetString (ModernUiStringDisplay), Resolution, Theme);
  ModernUiDrawInfoCard (Ui, (MODERN_UI_RECT){ Content.X + CardWidth + CARD_GAP, Content.Y + 108, CardWidth, 92 }, ModernUiGetString (ModernUiStringBootOptions), BootCount, Theme);

  ModernUiDrawPanel (Ui, StatusRect, Theme);
  ModernUiDrawFocusFrame (Ui, StatusRect, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, StatusRect.X + 20, StatusRect.Y + 18, ModernUiGetString (ModernUiStringPrototypeStatus), Theme->MutedText, Theme->Surface);
  ModernUiDrawText (Ui, StatusRect.X + 20, StatusRect.Y + 48, SecurityText, Theme->Text, Theme->Surface);
  ModernUiDrawText (Ui, StatusRect.X + 20, StatusRect.Y + 72, MemoryText, Theme->MutedText, Theme->Surface);
  ModernUiDrawProgress (Ui, (MODERN_UI_RECT){ StatusRect.X + 20, StatusRect.Y + 82, StatusRect.Width - 40, 12 }, 68, Theme->Border, Theme->Accent);
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
  CONST CHAR16                  *State;
  BOOLEAN                       IsSelected;
  MODERN_UI_RECT                Panel;
  UINTN                         RowX;
  UINTN                         RowWidth;
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

  for (Index = 0; (Index < BootOptionCount) && (Index < MAX_BOOT_ROWS); Index++) {
    Y           = Panel.Y + 62 + Index * 38;
    State       = BootOptions[Index].Active ? ModernUiGetString (ModernUiStringActive) : ModernUiGetString (ModernUiStringInactive);
    IsSelected  = (BOOLEAN)((Focus == SetupFocusContent) && (Index == Selected));
    UnicodeSPrint (
      Line,
      sizeof (Line),
      L"%02u  Boot%04x  %s  %s",
      Index + 1,
      BootOptions[Index].OptionNumber,
      State,
      BootOptions[Index].Description
      );
    RowModel.Rect      = (MODERN_UI_RECT){ RowX, Y - 8, RowWidth, 32 };
    RowModel.Prompt    = Line;
    RowModel.Value     = NULL;
    RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowNormal;
    RowModel.ValueType = ModernUiValueNone;
    ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
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
  CHAR16                    Line[168];
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

  ModernUiDrawTextFormatted (Ui, Panel.X + 20, Panel.Y + 20, Theme->MutedText, Theme->Surface, ModernUiGetString (ModernUiStringHandleCountFormat), EntryCount);

  for (Index = 0; (Index < EntryCount) && (Index < MAX_DEVICE_ROWS); Index++) {
    UnicodeSPrint (Line, sizeof (Line), L"%02u  %s", Index + 1, Entries[Index].Title);
    IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && (Index == Selected));
    RowModel.Rect      = (MODERN_UI_RECT){ RowX, Panel.Y + 54 + Index * 36, RowWidth, 30 };
    RowModel.Prompt    = Line;
    RowModel.Value     = Entries[Index].HasForm ? L">" : L"-";
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

  if (EFI_ERROR (ModernUiSecurityDataGetSummary (&Summary))) {
    ZeroMem (&Summary, sizeof (Summary));
  }

  SecureBootText = (Summary.SecureBoot == ModernUiSecurityStateEnabled) ? ModernUiGetString (ModernUiStringEnabled) :
                   ((Summary.SecureBoot == ModernUiSecurityStateDisabled) ? ModernUiGetString (ModernUiStringDisabled) : L"Unknown");
  SetupModeText = (Summary.SetupMode == ModernUiSecurityStateEnabled) ? ModernUiGetString (ModernUiStringEnabled) :
                  ((Summary.SetupMode == ModernUiSecurityStateDisabled) ? ModernUiGetString (ModernUiStringDisabled) : L"Unknown");
  PkText  = (Summary.PlatformKey == ModernUiSecurityStatePresent) ? L"Present" : ((Summary.PlatformKey == ModernUiSecurityStateAbsent) ? L"Absent" : L"Unknown");
  KekText = (Summary.KeyExchangeKey == ModernUiSecurityStatePresent) ? L"Present" : ((Summary.KeyExchangeKey == ModernUiSecurityStateAbsent) ? L"Absent" : L"Unknown");
  DbText  = (Summary.SignatureDb == ModernUiSecurityStatePresent) ? L"Present" : ((Summary.SignatureDb == ModernUiSecurityStateAbsent) ? L"Absent" : L"Unknown");
  DbxText = (Summary.ForbiddenSignatureDb == ModernUiSecurityStatePresent) ? L"Present" : ((Summary.ForbiddenSignatureDb == ModernUiSecurityStateAbsent) ? L"Absent" : L"Unknown");
  Panel = ContentRect (Ui);
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 24, ModernUiGetString (ModernUiStringSecureBoot), Theme->MutedText, Theme->Surface);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 64, SecureBootText, (Summary.SecureBoot == ModernUiSecurityStateEnabled) ? Theme->Success : Theme->Warning, Theme->Surface);
  ModernUiDrawTextFormatted (Ui, Panel.X + 20, Panel.Y + 104, Theme->MutedText, Theme->Surface, L"Setup Mode: %s", SetupModeText);
  ModernUiDrawTextFormatted (Ui, Panel.X + 20, Panel.Y + 136, Theme->MutedText, Theme->Surface, L"PK: %s    KEK: %s", PkText, KekText);
  ModernUiDrawTextFormatted (Ui, Panel.X + 20, Panel.Y + 168, Theme->MutedText, Theme->Surface, L"db: %s    dbx: %s", DbText, DbxText);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 220, ModernUiGetString (ModernUiStringSecurityReadOnly), Theme->MutedText, Theme->Surface);
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
      DrawDashboard (Ui, Theme, Focus);
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
  BootSelection = 0;
  DeviceSelection = 0;
  ExitSelection = 0;
  StatusMessage[0] = L'\0';
  Redraw        = TRUE;

  for (;;) {
    if (Redraw) {
      DrawPage (&Ui, Theme, Page, Focus, BootSelection, DeviceSelection, ExitSelection, StatusMessage);
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
        } else if (Focus == SetupFocusContent) {
          SelectableCount = GetPageSelectableCount (Page);
          if (SelectableCount > 0) {
            Selection = GetPageSelection (Page, BootSelection, DeviceSelection, ExitSelection);
            Selection = (Selection == 0) ? (SelectableCount - 1) : (Selection - 1);
            SetPageSelection (Page, Selection, &BootSelection, &DeviceSelection, &ExitSelection);
          }
        }

        Redraw = TRUE;
        break;
      case ModernUiInputDown:
        if ((Focus == SetupFocusContent) && (Page == PageExit) && mLanguageDropdownOpen) {
          mLanguageDropdownSelection = (mLanguageDropdownSelection + 1) % 2;
        } else if (Focus == SetupFocusNav) {
          Focus = SetupFocusContent;
        } else {
          SelectableCount = GetPageSelectableCount (Page);
          if (SelectableCount > 0) {
            Selection = GetPageSelection (Page, BootSelection, DeviceSelection, ExitSelection);
            Selection = (Selection + 1) % SelectableCount;
            SetPageSelection (Page, Selection, &BootSelection, &DeviceSelection, &ExitSelection);
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
          Focus  = SetupFocusContent;
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
