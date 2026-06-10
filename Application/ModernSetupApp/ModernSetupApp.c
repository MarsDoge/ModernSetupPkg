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
  Cancel and release the header-clock refresh timer, if one was armed.

  Called on every path that leaves the front-page loop so re-entering the
  application does not leak a periodic timer event. Safe to call with a NULL
  handle, which is what the loop holds when the timer could not be created.

  @param[in] TickEvent  Timer event handle, or NULL if none was armed.
**/
STATIC
VOID
ModernSetupDisarmClock (
  IN EFI_EVENT  TickEvent
  )
{
  if (TickEvent == NULL) {
    return;
  }

  gBS->SetTimer (TickEvent, TimerCancel, 0);
  gBS->CloseEvent (TickEvent);
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
  EFI_EVENT                 TickEvent;
  EFI_EVENT                 WaitSet[3];
  EFI_EVENT                 KeyEvent;
  UINTN                     WaitCount;
  UINTN                     WaitIndex;

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

  //
  // Arm a one-second periodic timer so the idle loop wakes to refresh the
  // header clock even when no key is pressed. 10,000,000 is one second in the
  // 100 ns units SetTimer expects. If the timer cannot be created the loop
  // falls back to a plain blocking input wait and the clock simply does not
  // advance until the next keystroke.
  //
  TickEvent = NULL;
  Status    = gBS->CreateEvent (EVT_TIMER, TPL_CALLBACK, NULL, NULL, &TickEvent);
  if (EFI_ERROR (Status)) {
    TickEvent = NULL;
  } else {
    gBS->SetTimer (TickEvent, TimerPeriodic, 10000000);
  }

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

    //
    // When the clock timer is armed, wait on it alongside the input sources
    // (tick last, so a pending keystroke or pointer event wins the race and is
    // serviced by ModernUiReadInput below). A timer wake only repaints the
    // header clock in place and loops again without disturbing the rest of the
    // frame.
    //
    if (TickEvent != NULL) {
      KeyEvent = (Input.TextInEx != NULL) ? Input.TextInEx->WaitForKeyEx :
                 ((Input.TextIn != NULL) ? Input.TextIn->WaitForKey : NULL);
      WaitCount = 0;
      if (KeyEvent != NULL) {
        WaitSet[WaitCount++] = KeyEvent;
      }

      if ((Input.Pointer != NULL) && (Input.Pointer->WaitForInput != NULL)) {
        WaitSet[WaitCount++] = Input.Pointer->WaitForInput;
      }

      WaitSet[WaitCount++] = TickEvent;

      Status = gBS->WaitForEvent (WaitCount, WaitSet, &WaitIndex);
      if (!EFI_ERROR (Status) && (WaitSet[WaitIndex] == TickEvent)) {
        ModernSetupRefreshHeaderClock (&Ui, Theme);
        continue;
      }
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
      if (ModernSetupBootSelectionIsNativeFallback (BootSelection, ModernSetupGetPageSelectableCount (&Ui, PageBoot))) {
        UnicodeSPrint (StatusMessage, sizeof (StatusMessage), L"Press Enter to open Native Boot Manager / Boot Maintenance");
        Redraw = TRUE;
      } else {
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

            if (Status == EFI_UNSUPPORTED) {
              UnicodeSPrint (StatusMessage, sizeof (StatusMessage), L"Use N=BootNext for App/Shell entries");
            } else {
              UnicodeSPrint (StatusMessage, sizeof (StatusMessage), L"BootOrder move up: %r", Status);
            }

            Redraw = TRUE;
            break;
          case L'-':
          case L'_':
            Status = ModernSetupMoveSelectedBootOption (BootSelection, FALSE);
            if (!EFI_ERROR (Status)) {
              BootSelection++;
            }

            if (Status == EFI_UNSUPPORTED) {
              UnicodeSPrint (StatusMessage, sizeof (StatusMessage), L"Use N=BootNext for App/Shell entries");
            } else {
              UnicodeSPrint (StatusMessage, sizeof (StatusMessage), L"BootOrder move down: %r", Status);
            }

            Redraw = TRUE;
            break;
          default:
            break;
        }
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
              ((DashboardSelection + DashboardGrid.CardsPerRow) < ModernSetupDashboardVisibleQuickCardCount ()))
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
            DashboardSelection = (DashboardSelection == 0) ? (ModernSetupDashboardVisibleQuickCardCount () - 1) : (DashboardSelection - 1);
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
            if ((((DashboardSelection % DashboardGrid.CardsPerRow) + 1) < DashboardGrid.CardsPerRow) && ((DashboardSelection + 1) < ModernSetupDashboardVisibleQuickCardCount ())) {
              DashboardSelection++;
            }
          } else {
            DashboardSelection = (DashboardSelection + 1) % ModernSetupDashboardVisibleQuickCardCount ();
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
          ModernSetupDisarmClock (TickEvent);
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
          if (ModernSetupDashboardSelectionRequestsContinue (DashboardSelection)) {
            ModernSetupDisarmClock (TickEvent);
            return EFI_SUCCESS;
          }

          if (ModernSetupGetDashboardCategoryRoute (DashboardSelection, &DashboardRoute)) {
            Page  = DashboardRoute.Page;
            Focus = DashboardRoute.Focus;
          }

          StatusMessage[0] = L'\0';
          Redraw = TRUE;
        } else if (Page == PageBoot) {
          if (ModernSetupBootSelectionIsNativeFallback (BootSelection, ModernSetupGetPageSelectableCount (&Ui, PageBoot))) {
            Status = ModernSetupLaunchUiAppFallback (ImageHandle);
          } else {
            Status = ModernSetupLaunchSelectedBootOption (BootSelection);
          }
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
            ModernSetupDisarmClock (TickEvent);
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
