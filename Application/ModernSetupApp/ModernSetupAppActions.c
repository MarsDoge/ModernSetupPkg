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
  { PagePerformance, SetupFocusNav     },
  { PageServerInventory, SetupFocusNav }
};

BOOLEAN         mModernSetupLanguageDropdownOpen;
UINTN           mModernSetupLanguageDropdownSelection;
BOOLEAN         mModernSetupPreferencePopupOpen;
UINTN           mModernSetupPreferencePopupRow;
UINTN           mModernSetupPreferencePopupSelection;
MODERN_SETUP_PREFERENCE_POPUP_KIND  mModernSetupPreferencePopupKind;
CHAR16          mModernSetupPreferenceInputBuffer[MODERN_UI_PREFERENCES_PROFILE_NAME_CHARS];
UINTN           mModernSetupPreferenceInputLength;
MODERN_UI_PREFERENCES  mModernSetupPreferences;

/**
  Calculate the visible Dashboard Quick Access grid from the same layout
  contract used by drawing and keyboard navigation.

  DashboardDensity is the app-owned ModernUi preference selecting the
  Dashboard layout density. Only ModernUiDashboardDensityCompact triggers
  the compact layout (tighter top summary band, smaller card gap/top, and
  reduced minimum card value height). Any other value -- including the
  default ModernUiDashboardDensityComfortable, the out-of-range sentinel
  ModernUiDashboardDensityMax, or any unrecognized UINT8 -- falls back to
  the Comfortable layout path; no validation or clamping is performed
  here. Preference sanitation/defaulting is owned by
  ModernUiPreferencesLib, which clamps unknown values back to
  ModernUiDashboardDensityComfortable before this helper is reached.

  @param[in]  Ui                Initialized render context. Must not be NULL.
  @param[in]  DashboardDensity  ModernUi dashboard density preference. Expected
                                values: ModernUiDashboardDensityComfortable (0,
                                default) or ModernUiDashboardDensityCompact.
                                Any other value is treated as Comfortable.
  @param[out] Grid              Receives the Quick Access panel and card
                                metrics. Must not be NULL. Zeroed on entry;
                                left zeroed on FALSE return.

  @retval TRUE   Quick Access cards are visible/selectable.
  @retval FALSE  Ui or Grid is NULL, or Quick Access cards do not fit in the
                 current content rect.
**/
BOOLEAN
ModernSetupGetDashboardQuickGrid (
  IN  MODERN_UI_RENDER_CONTEXT           *Ui,
  IN  UINT8                              DashboardDensity,
  OUT MODERN_SETUP_DASHBOARD_QUICK_GRID  *Grid
  )
{
  MODERN_UI_RECT  Content;
  UINTN           TopHeight;
  UINTN           QuickY;
  UINTN           QuickHeight;
  UINTN           QuickGap;
  UINTN           CardAreaWidth;
  UINTN           MaxRows;
  UINTN           ValueMinHeight;
  BOOLEAN         Compact;

  if ((Ui == NULL) || (Grid == NULL)) {
    return FALSE;
  }

  ZeroMem (Grid, sizeof (*Grid));
  Compact     = (BOOLEAN)(DashboardDensity == ModernUiDashboardDensityCompact);
  Content     = ModernSetupContentRect (Ui);
  TopHeight   = Compact ? ((Content.Height >= 460) ? 236 : 204) :
                ((Content.Height >= 460) ? 300 : 232);
  QuickGap    = Compact ? 10 : 16;
  QuickY      = Content.Y + TopHeight + QuickGap;
  QuickHeight = (Content.Height > (TopHeight + QuickGap)) ? (Content.Height - TopHeight - QuickGap) : 0;
  if (QuickHeight <= 110) {
    return FALSE;
  }

  Grid->Visible    = TRUE;
  Grid->Panel      = (MODERN_UI_RECT){ Content.X, QuickY, Content.Width, QuickHeight };
  Grid->CardGap    = Compact ? 20 : DASHBOARD_QUICK_CARD_GAP;
  Grid->CardTop    = Compact ? 42 : DASHBOARD_QUICK_CARD_TOP;
  CardAreaWidth    = (Grid->Panel.Width > 40) ? (Grid->Panel.Width - 40) : Grid->Panel.Width;
  ValueMinHeight   = Compact ? 30 : DASHBOARD_QUICK_VALUE_MIN_HEIGHT;
  MaxRows          = (Grid->Panel.Height > (Grid->CardTop + ValueMinHeight + DASHBOARD_QUICK_CARD_BOTTOM)) ?
                     ((Grid->Panel.Height - Grid->CardTop - DASHBOARD_QUICK_CARD_BOTTOM + Grid->CardGap) / (ValueMinHeight + Grid->CardGap)) :
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

        return ModernSetupGetDashboardQuickGrid (Ui, mModernSetupPreferences.DashboardDensity, &Grid) ?
               DASHBOARD_QUICK_CARD_COUNT : 0;
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
      return MODERN_SETUP_PREFERENCE_ROW_COUNT;
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

CONST CHAR16 *
ModernSetupPreferenceCheckboxValueText (
  IN UINT8  Value
  )
{
  return (Value != 0) ? L"[x] Enabled" : L"[ ] Disabled";
}

UINTN
ModernSetupGetPreferenceChoiceCount (
  IN UINTN  Row
  )
{
  switch (Row) {
    case MODERN_SETUP_PREFERENCE_ROW_THEME:
      return 4;
    case MODERN_SETUP_PREFERENCE_ROW_DASHBOARD_DENSITY:
      return 2;
    default:
      return 0;
  }
}

CONST CHAR16 *
ModernSetupGetPreferenceChoiceName (
  IN UINTN  Row,
  IN UINTN  Selection
  )
{
  if (Row == MODERN_SETUP_PREFERENCE_ROW_THEME) {
    switch (Selection) {
      case MODERN_UI_PREFERENCES_THEME_SYSTEM:
        return L"Default";
      case MODERN_UI_PREFERENCES_THEME_DARK:
        return L"Amber";
      case MODERN_UI_PREFERENCES_THEME_RED:
        return L"Accent Red";
      case MODERN_UI_PREFERENCES_THEME_GRAPHITE_GOLD:
        return L"Graphite Gold";
      default:
        return L"Default";
    }
  }

  if (Row == MODERN_SETUP_PREFERENCE_ROW_DASHBOARD_DENSITY) {
    return (Selection == ModernUiDashboardDensityCompact) ? L"Compact" : L"Comfortable";
  }

  return L"";
}

CONST CHAR16 *
ModernSetupGetPreferenceValueName (
  IN UINTN  Row
  )
{
  STATIC CHAR16  BootTimeoutText[16];

  switch (Row) {
    case MODERN_SETUP_PREFERENCE_ROW_THEME:
      return ModernSetupGetPreferenceChoiceName (Row, mModernSetupPreferences.ThemeId);
    case MODERN_SETUP_PREFERENCE_ROW_DASHBOARD_DENSITY:
      return ModernSetupGetPreferenceChoiceName (Row, mModernSetupPreferences.DashboardDensity);
    case MODERN_SETUP_PREFERENCE_ROW_BOOT_TIMEOUT:
      UnicodeSPrint (BootTimeoutText, sizeof (BootTimeoutText), L"%u sec", mModernSetupPreferences.BootTimeoutSeconds);
      return BootTimeoutText;
    case MODERN_SETUP_PREFERENCE_ROW_PROFILE_NAME:
      return mModernSetupPreferences.ProfileName;
    case MODERN_SETUP_PREFERENCE_ROW_REMEMBER_LAST_PAGE:
      return ModernSetupPreferenceCheckboxValueText (mModernSetupPreferences.RememberLastPage);
    case MODERN_SETUP_PREFERENCE_ROW_SHOW_ADVANCED_HINTS:
      return ModernSetupPreferenceCheckboxValueText (mModernSetupPreferences.ShowAdvancedHints);
    case MODERN_SETUP_PREFERENCE_ROW_CONFIRM_RESET:
      return ModernSetupPreferenceCheckboxValueText (mModernSetupPreferences.ConfirmReset);
    default:
      return L"";
  }
}

STATIC
UINTN
ModernSetupPreferenceStringLength (
  IN CONST CHAR16  *Text,
  IN UINTN         MaxChars
  )
{
  UINTN  Length;

  if (Text == NULL) {
    return 0;
  }

  for (Length = 0; (Length < MaxChars) && (Text[Length] != L'\0'); Length++) {
  }

  return Length;
}

STATIC
BOOLEAN
ModernSetupPreferenceIsPrintableAscii (
  IN CHAR16  Character
  )
{
  return (BOOLEAN)((Character >= L' ') && (Character <= L'~'));
}

STATIC
VOID
ModernSetupOpenPreferenceInputPopup (
  IN UINTN                               Row,
  IN MODERN_SETUP_PREFERENCE_POPUP_KIND  Kind
  )
{
  mModernSetupPreferencePopupRow       = Row;
  mModernSetupPreferencePopupSelection = 0;
  mModernSetupPreferencePopupKind      = Kind;
  mModernSetupPreferencePopupOpen      = TRUE;
  ZeroMem (mModernSetupPreferenceInputBuffer, sizeof (mModernSetupPreferenceInputBuffer));

  if (Kind == ModernSetupPreferencePopupNumericInput) {
    UnicodeSPrint (mModernSetupPreferenceInputBuffer, sizeof (mModernSetupPreferenceInputBuffer), L"%u", mModernSetupPreferences.BootTimeoutSeconds);
  } else if (Kind == ModernSetupPreferencePopupStringInput) {
    CopyMem (mModernSetupPreferenceInputBuffer, mModernSetupPreferences.ProfileName, sizeof (mModernSetupPreferenceInputBuffer));
    mModernSetupPreferenceInputBuffer[MODERN_UI_PREFERENCES_PROFILE_NAME_CHARS - 1] = L'\0';
  }

  mModernSetupPreferenceInputLength = ModernSetupPreferenceStringLength (
                                        mModernSetupPreferenceInputBuffer,
                                        MODERN_UI_PREFERENCES_PROFILE_NAME_CHARS
                                        );
}

STATIC
EFI_STATUS
PersistPreferencesAndStatus (
  OUT CHAR16  *StatusMessage,
  IN  UINTN   StatusSize
  )
{
  EFI_STATUS  Status;

  Status = ModernUiPreferencesSave (&mModernSetupPreferences);
  if ((StatusMessage != NULL) && (StatusSize >= sizeof (CHAR16))) {
    UnicodeSPrint (StatusMessage, StatusSize, ModernUiGetString (ModernUiStringPreferenceSavedFormat), Status);
  }

  return Status;
}

VOID
ModernSetupHandlePreferencePopupUp (
  VOID
  )
{
  UINTN  Count;

  Count = ModernSetupGetPreferenceChoiceCount (mModernSetupPreferencePopupRow);
  if (!mModernSetupPreferencePopupOpen || (mModernSetupPreferencePopupKind != ModernSetupPreferencePopupChoice) || (Count == 0)) {
    return;
  }

  mModernSetupPreferencePopupSelection = (mModernSetupPreferencePopupSelection == 0) ? (Count - 1) : (mModernSetupPreferencePopupSelection - 1);
}

VOID
ModernSetupHandlePreferencePopupDown (
  VOID
  )
{
  UINTN  Count;

  Count = ModernSetupGetPreferenceChoiceCount (mModernSetupPreferencePopupRow);
  if (!mModernSetupPreferencePopupOpen || (mModernSetupPreferencePopupKind != ModernSetupPreferencePopupChoice) || (Count == 0)) {
    return;
  }

  mModernSetupPreferencePopupSelection = (mModernSetupPreferencePopupSelection + 1) % Count;
}

VOID
ModernSetupCancelPreferencePopup (
  VOID
  )
{
  mModernSetupPreferencePopupOpen = FALSE;
  mModernSetupPreferencePopupKind = ModernSetupPreferencePopupNone;
}

VOID
ModernSetupCommitPreferencePopup (
  OUT CHAR16  *StatusMessage,
  IN  UINTN   StatusSize
  )
{
  UINTN  Index;
  UINTN  NumericValue;

  if ((StatusMessage != NULL) && (StatusSize >= sizeof (CHAR16))) {
    StatusMessage[0] = L'\0';
  }

  if (!mModernSetupPreferencePopupOpen) {
    return;
  }

  if (mModernSetupPreferencePopupKind == ModernSetupPreferencePopupNumericInput) {
    if (mModernSetupPreferenceInputLength == 0) {
      UnicodeSPrint (StatusMessage, StatusSize, L"Boot timeout must be 0..30 seconds.");
      return;
    }

    NumericValue = 0;
    for (Index = 0; Index < mModernSetupPreferenceInputLength; Index++) {
      if ((mModernSetupPreferenceInputBuffer[Index] < L'0') || (mModernSetupPreferenceInputBuffer[Index] > L'9')) {
        UnicodeSPrint (StatusMessage, StatusSize, L"Boot timeout accepts digits only.");
        return;
      }

      NumericValue = (NumericValue * 10) + (UINTN)(mModernSetupPreferenceInputBuffer[Index] - L'0');
    }

    if (NumericValue > MODERN_UI_PREFERENCES_BOOT_TIMEOUT_MAX) {
      UnicodeSPrint (StatusMessage, StatusSize, L"Boot timeout must be 0..30 seconds.");
      return;
    }

    mModernSetupPreferences.BootTimeoutSeconds = (UINT8)NumericValue;
    mModernSetupPreferencePopupOpen = FALSE;
    mModernSetupPreferencePopupKind = ModernSetupPreferencePopupNone;
    PersistPreferencesAndStatus (StatusMessage, StatusSize);
    return;
  }

  if (mModernSetupPreferencePopupKind == ModernSetupPreferencePopupStringInput) {
    if (mModernSetupPreferenceInputLength == 0) {
      UnicodeSPrint (StatusMessage, StatusSize, L"Profile name must not be empty.");
      return;
    }

    mModernSetupPreferenceInputBuffer[MODERN_UI_PREFERENCES_PROFILE_NAME_CHARS - 1] = L'\0';
    CopyMem (mModernSetupPreferences.ProfileName, mModernSetupPreferenceInputBuffer, sizeof (mModernSetupPreferences.ProfileName));
    mModernSetupPreferencePopupOpen = FALSE;
    mModernSetupPreferencePopupKind = ModernSetupPreferencePopupNone;
    PersistPreferencesAndStatus (StatusMessage, StatusSize);
    return;
  }

  switch (mModernSetupPreferencePopupRow) {
    case MODERN_SETUP_PREFERENCE_ROW_THEME:
      mModernSetupPreferences.ThemeId = (UINT8)mModernSetupPreferencePopupSelection;
      break;
    case MODERN_SETUP_PREFERENCE_ROW_DASHBOARD_DENSITY:
      mModernSetupPreferences.DashboardDensity = (UINT8)mModernSetupPreferencePopupSelection;
      break;
    default:
      mModernSetupPreferencePopupOpen = FALSE;
      mModernSetupPreferencePopupKind = ModernSetupPreferencePopupNone;
      return;
  }

  mModernSetupPreferencePopupOpen = FALSE;
  mModernSetupPreferencePopupKind = ModernSetupPreferencePopupNone;
  PersistPreferencesAndStatus (StatusMessage, StatusSize);
}

VOID
ModernSetupHandlePreferenceInputKey (
  IN  CONST MODERN_UI_INPUT_EVENT  *Event,
  OUT CHAR16                       *StatusMessage,
  IN  UINTN                        StatusSize
  )
{
  CHAR16   Character;
  BOOLEAN  NumericInput;

  if ((Event == NULL) || !mModernSetupPreferencePopupOpen) {
    return;
  }

  if ((mModernSetupPreferencePopupKind != ModernSetupPreferencePopupNumericInput) &&
      (mModernSetupPreferencePopupKind != ModernSetupPreferencePopupStringInput))
  {
    return;
  }

  if ((StatusMessage != NULL) && (StatusSize >= sizeof (CHAR16))) {
    StatusMessage[0] = L'\0';
  }

  Character = Event->UnicodeChar;
  if (Character == CHAR_BACKSPACE) {
    if (mModernSetupPreferenceInputLength > 0) {
      mModernSetupPreferenceInputLength--;
      mModernSetupPreferenceInputBuffer[mModernSetupPreferenceInputLength] = L'\0';
    }

    return;
  }

  NumericInput = (BOOLEAN)(mModernSetupPreferencePopupKind == ModernSetupPreferencePopupNumericInput);
  if (NumericInput) {
    if ((Character < L'0') || (Character > L'9')) {
      return;
    }
  } else if (!ModernSetupPreferenceIsPrintableAscii (Character)) {
    return;
  }

  if (NumericInput) {
    if (mModernSetupPreferenceInputLength >= 2) {
      return;
    }
  } else if (mModernSetupPreferenceInputLength >= (MODERN_UI_PREFERENCES_PROFILE_NAME_CHARS - 1)) {
    return;
  }

  mModernSetupPreferenceInputBuffer[mModernSetupPreferenceInputLength++] = Character;
  mModernSetupPreferenceInputBuffer[mModernSetupPreferenceInputLength] = L'\0';
}

/**
  Toggle, open, or persist one app-owned Preferences row.

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
  if ((StatusMessage == NULL) || (StatusSize < sizeof (CHAR16))) {
    return;
  }

  StatusMessage[0] = L'\0';
  switch (Selection) {
    case MODERN_SETUP_PREFERENCE_ROW_THEME:
      mModernSetupPreferencePopupRow       = Selection;
      mModernSetupPreferencePopupSelection = mModernSetupPreferences.ThemeId;
      mModernSetupPreferencePopupKind      = ModernSetupPreferencePopupChoice;
      mModernSetupPreferencePopupOpen      = TRUE;
      break;
    case MODERN_SETUP_PREFERENCE_ROW_DASHBOARD_DENSITY:
      mModernSetupPreferencePopupRow       = Selection;
      mModernSetupPreferencePopupSelection = mModernSetupPreferences.DashboardDensity;
      mModernSetupPreferencePopupKind      = ModernSetupPreferencePopupChoice;
      mModernSetupPreferencePopupOpen      = TRUE;
      break;
    case MODERN_SETUP_PREFERENCE_ROW_BOOT_TIMEOUT:
      ModernSetupOpenPreferenceInputPopup (Selection, ModernSetupPreferencePopupNumericInput);
      break;
    case MODERN_SETUP_PREFERENCE_ROW_PROFILE_NAME:
      ModernSetupOpenPreferenceInputPopup (Selection, ModernSetupPreferencePopupStringInput);
      break;
    case MODERN_SETUP_PREFERENCE_ROW_REMEMBER_LAST_PAGE:
      mModernSetupPreferences.RememberLastPage = (mModernSetupPreferences.RememberLastPage == 0) ? 1 : 0;
      PersistPreferencesAndStatus (StatusMessage, StatusSize);
      break;
    case MODERN_SETUP_PREFERENCE_ROW_SHOW_ADVANCED_HINTS:
      mModernSetupPreferences.ShowAdvancedHints = (mModernSetupPreferences.ShowAdvancedHints == 0) ? 1 : 0;
      PersistPreferencesAndStatus (StatusMessage, StatusSize);
      break;
    case MODERN_SETUP_PREFERENCE_ROW_CONFIRM_RESET:
      mModernSetupPreferences.ConfirmReset = (mModernSetupPreferences.ConfirmReset == 0) ? 1 : 0;
      PersistPreferencesAndStatus (StatusMessage, StatusSize);
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
