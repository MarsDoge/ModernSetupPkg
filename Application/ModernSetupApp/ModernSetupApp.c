/** @file
  Modern graphical setup application prototype.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ModernSetupAppInternal.h"

EFI_HANDLE      mModernSetupImageHandle;

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
  UINTN                     PreferencesSelection;
  UINTN                     ExitSelection;
  UINTN                     Selection;
  UINTN                     SelectableCount;
  MODERN_SETUP_DASHBOARD_QUICK_GRID  DashboardGrid;
  MODERN_SETUP_DASHBOARD_ROUTE       DashboardRoute;
  CHAR16                    StatusMessage[96];
  BOOLEAN                   Redraw;
  BOOLEAN                   ResetConfirmationPending;
  SETUP_PAGE                OldPage;
  SETUP_FOCUS               OldFocus;
  UINTN                     OldDashboardSelection;
  UINTN                     OldBootSelection;
  UINTN                     OldDeviceSelection;
  UINTN                     OldPreferencesSelection;
  UINTN                     OldExitSelection;
  BOOLEAN                   OldLanguageDropdownOpen;
  UINTN                     OldLanguageDropdownSelection;
  BOOLEAN                   OldPreferencePopupOpen;
  UINTN                     OldPreferencePopupRow;
  UINTN                     OldPreferencePopupSelection;
  MODERN_SETUP_PREFERENCE_POPUP_KIND  OldPreferencePopupKind;
  UINTN                     OldPreferenceInputLength;
  BOOLEAN                   OldResetConfirmationPending;
  MODERN_UI_PREFERENCES     OldPreferences;
  CHAR16                    OldStatusMessage[96];

  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  mModernSetupImageHandle = ImageHandle;

  Status = ModernUiRendererInit (&Ui);
  if (EFI_ERROR (Status)) {
    Print (ModernUiGetString (ModernUiStringGraphicsInitFailedFormat), Status);
    return Status;
  }

  EfiBootManagerConnectAll ();
  EfiBootManagerRefreshAllBootOption ();
  Status = ModernUiPreferencesLoad (&mModernSetupPreferences);
  if (EFI_ERROR (Status)) {
    ModernUiPreferencesResetToDefaults (&mModernSetupPreferences);
  }
  ModernUiInputInit (&Input);
  Theme         = ModernUiGetThemeForPreference (mModernSetupPreferences.ThemeId);
  Page          = PageDashboard;
  Focus         = SetupFocusNav;
  DashboardSelection = 0;
  BootSelection = 0;
  DeviceSelection = 0;
  PreferencesSelection = 0;
  ExitSelection = 0;
  StatusMessage[0] = L'\0';
  Redraw        = TRUE;
  ResetConfirmationPending = FALSE;

  for (;;) {
    if (Redraw) {
      Theme = ModernUiGetThemeForPreference (mModernSetupPreferences.ThemeId);
      ModernSetupDrawCurrentPage (&Ui, Theme, Page, Focus, DashboardSelection, BootSelection, DeviceSelection, PreferencesSelection, ExitSelection, StatusMessage);
      Redraw = FALSE;
    }

    Status = ModernUiReadInput (&Input, &Event);
    if (EFI_ERROR (Status)) {
      continue;
    }

    OldPage                      = Page;
    OldFocus                     = Focus;
    OldDashboardSelection        = DashboardSelection;
    OldBootSelection             = BootSelection;
    OldDeviceSelection           = DeviceSelection;
    OldPreferencesSelection      = PreferencesSelection;
    OldExitSelection             = ExitSelection;
    OldLanguageDropdownOpen      = mModernSetupLanguageDropdownOpen;
    OldLanguageDropdownSelection = mModernSetupLanguageDropdownSelection;
    OldPreferencePopupOpen       = mModernSetupPreferencePopupOpen;
    OldPreferencePopupRow        = mModernSetupPreferencePopupRow;
    OldPreferencePopupSelection  = mModernSetupPreferencePopupSelection;
    OldPreferencePopupKind       = mModernSetupPreferencePopupKind;
    OldPreferenceInputLength     = mModernSetupPreferenceInputLength;
    OldResetConfirmationPending  = ResetConfirmationPending;
    CopyMem (&OldPreferences, &mModernSetupPreferences, sizeof (OldPreferences));
    CopyMem (OldStatusMessage, StatusMessage, sizeof (OldStatusMessage));

    if ((Focus == SetupFocusContent) && (Page == PagePreferences) && mModernSetupPreferencePopupOpen && (Event.Type == ModernUiInputOther)) {
      ModernSetupHandlePreferenceInputKey (&Event, StatusMessage, sizeof (StatusMessage));
      ResetConfirmationPending = FALSE;
      Redraw = TRUE;
      if ((OldPreferenceInputLength == mModernSetupPreferenceInputLength) &&
          (StrCmp (OldStatusMessage, StatusMessage) == 0) &&
          (CompareMem (&OldPreferences, &mModernSetupPreferences, sizeof (OldPreferences)) == 0))
      {
        Redraw = FALSE;
      }

      continue;
    }

    if (Event.Type != ModernUiInputEnter) {
      ResetConfirmationPending = FALSE;
    }

    if ((Focus == SetupFocusContent) && (Page == PageBoot) && (Event.Type == ModernUiInputOther)) {
      switch (Event.UnicodeChar) {
        case L'n':
        case L'N':
          Status = ModernSetupSetSelectedBootNext (BootSelection);
          UnicodeSPrint (StatusMessage, sizeof (StatusMessage), L"BootNext set: %r", Status);
          Redraw = TRUE;
          break;
        case L'c':
        case L'C':
          Status = ModernSetupClearBootNext ();
          UnicodeSPrint (StatusMessage, sizeof (StatusMessage), L"BootNext cleared: %r", Status);
          Redraw = TRUE;
          break;
        case L'+':
        case L'=':
          Status = ModernSetupMoveSelectedBootOption (BootSelection, TRUE);
          if (!EFI_ERROR (Status) && (BootSelection > 0)) {
            BootSelection--;
          }

          UnicodeSPrint (StatusMessage, sizeof (StatusMessage), L"BootOrder move up: %r", Status);
          Redraw = TRUE;
          break;
        case L'-':
        case L'_':
          Status = ModernSetupMoveSelectedBootOption (BootSelection, FALSE);
          if (!EFI_ERROR (Status)) {
            BootSelection++;
          }

          UnicodeSPrint (StatusMessage, sizeof (StatusMessage), L"BootOrder move down: %r", Status);
          Redraw = TRUE;
          break;
        default:
          break;
      }

      if (Redraw) {
        continue;
      }
    }

    switch (Event.Type) {
      case ModernUiInputUp:
        if ((Focus == SetupFocusContent) && (Page == PagePreferences) && mModernSetupPreferencePopupOpen) {
          ModernSetupHandlePreferencePopupUp ();
        } else if ((Focus == SetupFocusContent) && (Page == PageExit) && mModernSetupLanguageDropdownOpen) {
          mModernSetupLanguageDropdownSelection = (mModernSetupLanguageDropdownSelection == 0) ? 1 : 0;
        } else if ((Focus == SetupFocusContent) && (Page == PageDashboard)) {
          if (ModernSetupGetDashboardQuickGrid (&Ui, mModernSetupPreferences.DashboardDensity, &DashboardGrid) &&
              (DashboardSelection >= DashboardGrid.CardsPerRow))
          {
            DashboardSelection -= DashboardGrid.CardsPerRow;
          } else {
            Focus = SetupFocusNav;
          }
        } else if (Focus == SetupFocusContent) {
          SelectableCount = ModernSetupGetPageSelectableCount (&Ui, Page);
          if (SelectableCount > 0) {
            Selection = ModernSetupGetPageSelection (Page, DashboardSelection, BootSelection, DeviceSelection, PreferencesSelection, ExitSelection);
            Selection = (Selection == 0) ? (SelectableCount - 1) : (Selection - 1);
            ModernSetupSetPageSelection (Page, Selection, &DashboardSelection, &BootSelection, &DeviceSelection, &PreferencesSelection, &ExitSelection);
          }
        }

        Redraw = TRUE;
        break;
      case ModernUiInputDown:
        if ((Focus == SetupFocusContent) && (Page == PagePreferences) && mModernSetupPreferencePopupOpen) {
          ModernSetupHandlePreferencePopupDown ();
        } else if ((Focus == SetupFocusContent) && (Page == PageExit) && mModernSetupLanguageDropdownOpen) {
          mModernSetupLanguageDropdownSelection = (mModernSetupLanguageDropdownSelection + 1) % 2;
        } else if (Focus == SetupFocusNav) {
          if (ModernSetupGetPageSelectableCount (&Ui, Page) > 0) {
            Focus = SetupFocusContent;
          }
        } else if (Page == PageDashboard) {
          if (ModernSetupGetDashboardQuickGrid (&Ui, mModernSetupPreferences.DashboardDensity, &DashboardGrid) &&
              ((DashboardSelection + DashboardGrid.CardsPerRow) < DASHBOARD_QUICK_CARD_COUNT))
          {
            DashboardSelection += DashboardGrid.CardsPerRow;
          }
        } else {
          SelectableCount = ModernSetupGetPageSelectableCount (&Ui, Page);
          if (SelectableCount > 0) {
            Selection = ModernSetupGetPageSelection (Page, DashboardSelection, BootSelection, DeviceSelection, PreferencesSelection, ExitSelection);
            Selection = (Selection + 1) % SelectableCount;
            ModernSetupSetPageSelection (Page, Selection, &DashboardSelection, &BootSelection, &DeviceSelection, &PreferencesSelection, &ExitSelection);
          }
        }

        Redraw = TRUE;
        break;
      case ModernUiInputTab:
        mModernSetupLanguageDropdownOpen = FALSE;
        ModernSetupCancelPreferencePopup ();
        Focus  = (Focus == SetupFocusNav) ? SetupFocusContent : SetupFocusNav;
        Redraw = TRUE;
        break;
      case ModernUiInputLeft:
        if ((Focus == SetupFocusContent) && (Page == PagePreferences) && mModernSetupPreferencePopupOpen) {
          ModernSetupCancelPreferencePopup ();
        } else if ((Focus == SetupFocusContent) && (Page == PageExit) && mModernSetupLanguageDropdownOpen) {
          mModernSetupLanguageDropdownOpen = FALSE;
        } else if ((Focus == SetupFocusContent) && (Page == PageDashboard)) {
          if (ModernSetupGetDashboardQuickGrid (&Ui, mModernSetupPreferences.DashboardDensity, &DashboardGrid) &&
              ((DashboardSelection % DashboardGrid.CardsPerRow) > 0))
          {
            DashboardSelection--;
          } else if (!DashboardGrid.Visible) {
            DashboardSelection = (DashboardSelection == 0) ? (DASHBOARD_QUICK_CARD_COUNT - 1) : (DashboardSelection - 1);
          }
        } else if (Focus == SetupFocusNav) {
          Page = (Page == 0) ? (PageMax - 1) : (Page - 1);
          mModernSetupLanguageDropdownOpen = FALSE;
          ModernSetupCancelPreferencePopup ();
        } else {
          Focus = SetupFocusNav;
        }

        StatusMessage[0] = L'\0';
        Redraw = TRUE;
        break;
      case ModernUiInputRight:
        if (Focus == SetupFocusNav) {
          Page = (Page + 1) % PageMax;
          mModernSetupLanguageDropdownOpen = FALSE;
          ModernSetupCancelPreferencePopup ();
        } else if (Page == PageDashboard) {
          if (ModernSetupGetDashboardQuickGrid (&Ui, mModernSetupPreferences.DashboardDensity, &DashboardGrid)) {
            if ((((DashboardSelection % DashboardGrid.CardsPerRow) + 1) < DashboardGrid.CardsPerRow) && ((DashboardSelection + 1) < DASHBOARD_QUICK_CARD_COUNT)) {
              DashboardSelection++;
            }
          } else {
            DashboardSelection = (DashboardSelection + 1) % DASHBOARD_QUICK_CARD_COUNT;
          }
          StatusMessage[0] = L'\0';
        } else {
          mModernSetupLanguageDropdownOpen = FALSE;
          ModernSetupCancelPreferencePopup ();
          StatusMessage[0] = L'\0';
        }

        Redraw = TRUE;
        break;
      case ModernUiInputEscape:
        if ((Focus == SetupFocusContent) && (Page == PagePreferences) && mModernSetupPreferencePopupOpen) {
          ModernSetupCancelPreferencePopup ();
          StatusMessage[0] = L'\0';
          Redraw = TRUE;
        } else if ((Focus == SetupFocusContent) && (Page == PageExit) && mModernSetupLanguageDropdownOpen) {
          mModernSetupLanguageDropdownOpen = FALSE;
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
          if (ModernSetupGetPageSelectableCount (&Ui, Page) > 0) {
            Focus = SetupFocusContent;
          }
          StatusMessage[0] = L'\0';
          Redraw = TRUE;
        } else if (Page == PageDashboard) {
          if (ModernSetupGetDashboardCategoryRoute (DashboardSelection, &DashboardRoute)) {
            Page  = DashboardRoute.Page;
            Focus = DashboardRoute.Focus;
          }

          StatusMessage[0] = L'\0';
          Redraw = TRUE;
        } else if (Page == PageBoot) {
          Status = ModernSetupLaunchSelectedBootOption (BootSelection);
          UnicodeSPrint (StatusMessage, sizeof (StatusMessage), ModernUiGetString (ModernUiStringClassicReturnedFormat), Status);
          Redraw = TRUE;
        } else if (Page == PageDevices) {
          Status = ModernSetupOpenSelectedDeviceEntry (DeviceSelection);
          UnicodeSPrint (StatusMessage, sizeof (StatusMessage), L"FormBrowser returned: %r", Status);
          Redraw = TRUE;
        } else if (Page == PagePreferences) {
          if (mModernSetupPreferencePopupOpen) {
            ModernSetupCommitPreferencePopup (StatusMessage, sizeof (StatusMessage));
          } else {
            ModernSetupHandlePreferencesEnter (PreferencesSelection, StatusMessage, sizeof (StatusMessage));
          }
          Redraw = TRUE;
        } else if (Page == PageExit) {
          if (ExitSelection == 0) {
            ResetConfirmationPending = FALSE;
            return EFI_SUCCESS;
          } else if (ExitSelection == 1) {
            ResetConfirmationPending = FALSE;
            Status = ModernSetupLaunchUiAppFallback (ImageHandle);
            UnicodeSPrint (StatusMessage, sizeof (StatusMessage), ModernUiGetString (ModernUiStringClassicReturnedFormat), Status);
            Redraw = TRUE;
          } else if (ExitSelection == 2) {
            if ((mModernSetupPreferences.ConfirmReset != 0) && !ResetConfirmationPending) {
              UnicodeSPrint (StatusMessage, sizeof (StatusMessage), L"Press Enter again to reset system.");
              ResetConfirmationPending = TRUE;
              Redraw = TRUE;
              break;
            }

            gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
          } else {
            ResetConfirmationPending = FALSE;
            ModernSetupHandleLanguageSelectorEnter (StatusMessage, sizeof (StatusMessage));
            Redraw = TRUE;
          }
        }
        break;
      default:
        break;
    }

    if (((OldPage == PageBoot) && (Page != PageBoot)) ||
        ((Event.Type == ModernUiInputEnter) &&
         ((OldPage == PageBoot) ||
          (OldPage == PageDevices) ||
          ((OldPage == PageExit) && (OldExitSelection == 1)))))
    {
      ModernSetupInvalidateBootOptionsCache ();
      if ((OldPage == PageDevices) || ((OldPage == PageExit) && (OldExitSelection == 1))) {
        ModernSetupInvalidateDeviceEntriesCache ();
        ModernSetupInvalidateProviderSnapshotCache ();
      }
    }

    if (Redraw && (Event.Type != ModernUiInputEnter) &&
        (OldPage == Page) &&
        (OldFocus == Focus) &&
        (OldDashboardSelection == DashboardSelection) &&
        (OldBootSelection == BootSelection) &&
        (OldDeviceSelection == DeviceSelection) &&
        (OldPreferencesSelection == PreferencesSelection) &&
        (OldExitSelection == ExitSelection) &&
        (OldLanguageDropdownOpen == mModernSetupLanguageDropdownOpen) &&
        (OldLanguageDropdownSelection == mModernSetupLanguageDropdownSelection) &&
        (OldPreferencePopupOpen == mModernSetupPreferencePopupOpen) &&
        (OldPreferencePopupRow == mModernSetupPreferencePopupRow) &&
        (OldPreferencePopupSelection == mModernSetupPreferencePopupSelection) &&
        (OldPreferencePopupKind == mModernSetupPreferencePopupKind) &&
        (OldPreferenceInputLength == mModernSetupPreferenceInputLength) &&
        (OldResetConfirmationPending == ResetConfirmationPending) &&
        (StrCmp (OldStatusMessage, StatusMessage) == 0) &&
        (CompareMem (&OldPreferences, &mModernSetupPreferences, sizeof (OldPreferences)) == 0))
    {
      Redraw = FALSE;
    }
  }
}
