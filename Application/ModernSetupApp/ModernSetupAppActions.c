/** @file
  Modern graphical setup application prototype.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ModernSetupAppInternal.h"

STATIC CONST EFI_GUID  mUiAppGuid = { 0x462CAA21, 0x7614, 0x4503, { 0x83, 0x6E, 0x8A, 0xB6, 0xF4, 0x66, 0x23, 0x31 } };

STATIC CONST MODERN_SETUP_DASHBOARD_ROUTE  mDashboardCategoryRoutes[DASHBOARD_QUICK_CARD_COUNT] = {
  { PageBoot,        SetupFocusContent },
  { PageDevices,     SetupFocusContent },
  { PageDiagnostics, SetupFocusNav     },
  { PageFirmware,    SetupFocusNav     },
  { PagePower,       SetupFocusNav     },
  { PagePerformance, SetupFocusNav     }
};

BOOLEAN         mModernSetupLanguageDropdownOpen;
UINTN           mModernSetupLanguageDropdownSelection;
MODERN_UI_PREFERENCES  mModernSetupPreferences;

/**
  Calculate the visible Dashboard Quick Access grid from the same layout
  contract used by drawing and keyboard navigation.

  @param[in]  Ui    Initialized render context. Must not be NULL.
  @param[out] Grid  Receives the Quick Access panel and card metrics.

  @retval TRUE   Quick Access cards are visible/selectable.
  @retval FALSE  Quick Access cards do not fit in the current content rect.
**/
BOOLEAN
ModernSetupGetDashboardQuickGrid (
  IN  MODERN_UI_RENDER_CONTEXT           *Ui,
  OUT MODERN_SETUP_DASHBOARD_QUICK_GRID  *Grid
  )
{
  MODERN_UI_RECT  Content;
  UINTN           TopHeight;
  UINTN           QuickY;
  UINTN           QuickHeight;
  UINTN           CardAreaWidth;
  UINTN           MaxRows;

  if ((Ui == NULL) || (Grid == NULL)) {
    return FALSE;
  }

  ZeroMem (Grid, sizeof (*Grid));
  Content     = ModernSetupContentRect (Ui);
  TopHeight   = (Content.Height >= 460) ? 300 : 232;
  QuickY      = Content.Y + TopHeight + 16;
  QuickHeight = (Content.Height > (TopHeight + 16)) ? (Content.Height - TopHeight - 16) : 0;
  if (QuickHeight <= 110) {
    return FALSE;
  }

  Grid->Visible    = TRUE;
  Grid->Panel      = (MODERN_UI_RECT){ Content.X, QuickY, Content.Width, QuickHeight };
  Grid->CardGap    = 14;
  Grid->CardTop    = DASHBOARD_QUICK_CARD_TOP;
  CardAreaWidth    = (Grid->Panel.Width > 40) ? (Grid->Panel.Width - 40) : Grid->Panel.Width;
  MaxRows          = (Grid->Panel.Height > (DASHBOARD_QUICK_CARD_TOP + DASHBOARD_QUICK_VALUE_MIN_HEIGHT + DASHBOARD_QUICK_CARD_BOTTOM)) ?
                     ((Grid->Panel.Height - DASHBOARD_QUICK_CARD_TOP - DASHBOARD_QUICK_CARD_BOTTOM + Grid->CardGap) / (DASHBOARD_QUICK_VALUE_MIN_HEIGHT + Grid->CardGap)) :
                     1;
  MaxRows          = MAX (1, MIN (DASHBOARD_QUICK_CARD_COUNT, MaxRows));
  Grid->CardsPerRow = (DASHBOARD_QUICK_CARD_COUNT + MaxRows - 1) / MaxRows;
  if ((Grid->Panel.Width >= 760) && (Grid->CardsPerRow < 3)) {
    Grid->CardsPerRow = 3;
  } else if ((Grid->Panel.Width >= 500) && (Grid->CardsPerRow < 2)) {
    Grid->CardsPerRow = 2;
  }

  Grid->CardsPerRow = MIN (DASHBOARD_QUICK_CARD_COUNT, Grid->CardsPerRow);
  Grid->Rows        = (DASHBOARD_QUICK_CARD_COUNT + Grid->CardsPerRow - 1) / Grid->CardsPerRow;
  Grid->CardWidth   = (CardAreaWidth > (Grid->CardGap * (Grid->CardsPerRow - 1))) ?
                      ((CardAreaWidth - (Grid->CardGap * (Grid->CardsPerRow - 1))) / Grid->CardsPerRow) :
                      MAX (1, CardAreaWidth / Grid->CardsPerRow);
  Grid->CardHeight  = (Grid->Panel.Height > (Grid->CardTop + DASHBOARD_QUICK_CARD_BOTTOM + (Grid->CardGap * (Grid->Rows - 1)))) ?
                      ((Grid->Panel.Height - Grid->CardTop - DASHBOARD_QUICK_CARD_BOTTOM - (Grid->CardGap * (Grid->Rows - 1))) / Grid->Rows) :
                      DASHBOARD_QUICK_VALUE_MIN_HEIGHT;
  return TRUE;
}

/**
  Resolve a Dashboard setup category card to its destination page and focus.

  The first two categories keep content focus so Enter can immediately act on
  boot/device rows. Provider-summary destinations keep navigation focus because
  their current pages are read-only overview panels.

  @param[in]  Selection  Zero-based Dashboard category card index.
  @param[out] Route      Receives the page and focus destination. Must not be NULL.

  @retval TRUE   Selection maps to a supported category route.
  @retval FALSE  Selection is out of range or Route is NULL.
**/
BOOLEAN
ModernSetupGetDashboardCategoryRoute (
  IN  UINTN                         Selection,
  OUT MODERN_SETUP_DASHBOARD_ROUTE  *Route
  )
{
  if ((Route == NULL) || (Selection >= ARRAY_SIZE (mDashboardCategoryRoutes))) {
    return FALSE;
  }

  *Route = mDashboardCategoryRoutes[Selection];
  return TRUE;
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
CONST CHAR16 *
ModernSetupGetLanguageOptionName (
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
UINTN
ModernSetupGetActiveLanguageSelection (
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
  @param[in] PreferencesSelection Current Preferences page selection.
  @param[in] ExitSelection    Current Exit page selection.

  @return Selected item index for Page. Pages without selectable rows return 0.
**/
UINTN
ModernSetupGetPageSelection (
  IN SETUP_PAGE  Page,
  IN UINTN       DashboardSelection,
  IN UINTN       BootSelection,
  IN UINTN       DeviceSelection,
  IN UINTN       PreferencesSelection,
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
    case PagePreferences:
      return PreferencesSelection;
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
  @param[in,out] PreferencesSelection Preferences page selection storage. Must not be NULL.
  @param[in,out] ExitSelection    Exit page selection storage. Must not be NULL.
**/
VOID
ModernSetupSetPageSelection (
  IN     SETUP_PAGE  Page,
  IN     UINTN       Selection,
  IN OUT UINTN       *DashboardSelection,
  IN OUT UINTN       *BootSelection,
  IN OUT UINTN       *DeviceSelection,
  IN OUT UINTN       *PreferencesSelection,
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
    case PagePreferences:
      *PreferencesSelection = Selection;
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
UINTN
ModernSetupGetBootCount (
  VOID
  )
{
  EFI_STATUS             Status;
  MODERN_UI_BOOT_OPTION  *Options;
  UINTN                  OptionCount;

  Options = NULL;
  Status = ModernUiBootDataGetOptions (mModernSetupImageHandle, &Options, &OptionCount);
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
UINTN
ModernSetupGetVisibleDeviceCount (
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
UINTN
ModernSetupGetPageSelectableCount (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN SETUP_PAGE  Page
  )
{
  switch (Page) {
    case PageDashboard:
      {
        MODERN_SETUP_DASHBOARD_QUICK_GRID  Grid;

        return ModernSetupGetDashboardQuickGrid (Ui, &Grid) ? DASHBOARD_QUICK_CARD_COUNT : 0;
      }
    case PageBoot:
      {
        MODERN_UI_RECT  Panel;
        UINTN           MaxRows;

        Panel   = ModernSetupContentRect (Ui);
        MaxRows = (Panel.Height > 96) ? ((Panel.Height - 92) / 58) : 0;
        return MIN (ModernSetupGetBootCount (), MIN (MaxRows, MAX_BOOT_ROWS));
      }
    case PageDevices:
      return ModernSetupGetVisibleDeviceCount ();
    case PagePreferences:
      return 3;
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
EFI_STATUS
ModernSetupLaunchSelectedBootOption (
  IN UINTN  Selection
  )
{
  EFI_STATUS             Status;
  MODERN_UI_BOOT_OPTION  *Options;
  UINTN                  OptionCount;
  UINT16                 OptionNumber;

  Options = NULL;
  Status = ModernUiBootDataGetOptions (mModernSetupImageHandle, &Options, &OptionCount);
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
EFI_STATUS
ModernSetupOpenSelectedDeviceEntry (
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
  LanguageName = ModernSetupGetLanguageOptionName (ModernSetupGetActiveLanguageSelection ());

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
VOID
ModernSetupHandleLanguageSelectorEnter (
  OUT CHAR16  *StatusMessage,
  IN  UINTN   StatusSize
  )
{
  if ((StatusMessage == NULL) || (StatusSize < sizeof (CHAR16))) {
    return;
  }

  StatusMessage[0] = L'\0';
  if (!mModernSetupLanguageDropdownOpen) {
    mModernSetupLanguageDropdownSelection = ModernSetupGetActiveLanguageSelection ();
    mModernSetupLanguageDropdownOpen      = TRUE;
    return;
  }

  ApplyLanguageSelection (mModernSetupLanguageDropdownSelection, StatusMessage, StatusSize);
  mModernSetupLanguageDropdownOpen = FALSE;
}

/**
  Toggle or persist one app-owned Preferences row.

  @param[in]  Selection      Preferences row index.
  @param[out] StatusMessage  Status buffer to update. Must not be NULL.
  @param[in]  StatusSize     Size of StatusMessage in bytes.
**/
VOID
ModernSetupHandlePreferencesEnter (
  IN  UINTN   Selection,
  OUT CHAR16  *StatusMessage,
  IN  UINTN   StatusSize
  )
{
  EFI_STATUS  Status;

  if ((StatusMessage == NULL) || (StatusSize < sizeof (CHAR16))) {
    return;
  }

  StatusMessage[0] = L'\0';
  switch (Selection) {
    case 0:
      mModernSetupPreferences.ConfirmReset = (mModernSetupPreferences.ConfirmReset == 0) ? 1 : 0;
      break;
    case 1:
      Status = ModernUiPreferencesSave (&mModernSetupPreferences);
      UnicodeSPrint (StatusMessage, StatusSize, ModernUiGetString (ModernUiStringPreferenceSavedFormat), Status);
      break;
    case 2:
      ModernUiPreferencesResetToDefaults (&mModernSetupPreferences);
      UnicodeSPrint (StatusMessage, StatusSize, ModernUiGetString (ModernUiStringPreferenceDefaultsLoaded));
      break;
    default:
      break;
  }
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
EFI_STATUS
ModernSetupLaunchUiAppFallback (
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
