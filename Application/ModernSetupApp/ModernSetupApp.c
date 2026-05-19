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
  UINTN                     ExitSelection;
  UINTN                     Selection;
  UINTN                     SelectableCount;
  MODERN_SETUP_DASHBOARD_QUICK_GRID  DashboardGrid;
  CHAR16                    StatusMessage[96];
  BOOLEAN                   Redraw;

  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  mModernSetupImageHandle = ImageHandle;

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
      ModernSetupDrawCurrentPage (&Ui, Theme, Page, Focus, DashboardSelection, BootSelection, DeviceSelection, ExitSelection, StatusMessage);
      Redraw = FALSE;
    }

    Status = ModernUiReadInput (&Input, &Event);
    if (EFI_ERROR (Status)) {
      continue;
    }

    switch (Event.Type) {
      case ModernUiInputUp:
        if ((Focus == SetupFocusContent) && (Page == PageExit) && mModernSetupLanguageDropdownOpen) {
          mModernSetupLanguageDropdownSelection = (mModernSetupLanguageDropdownSelection == 0) ? 1 : 0;
        } else if ((Focus == SetupFocusContent) && (Page == PageDashboard)) {
          if (ModernSetupGetDashboardQuickGrid (&Ui, &DashboardGrid) && (DashboardSelection >= DashboardGrid.CardsPerRow)) {
            DashboardSelection -= DashboardGrid.CardsPerRow;
          } else {
            Focus = SetupFocusNav;
          }
        } else if (Focus == SetupFocusContent) {
          SelectableCount = ModernSetupGetPageSelectableCount (&Ui, Page);
          if (SelectableCount > 0) {
            Selection = ModernSetupGetPageSelection (Page, DashboardSelection, BootSelection, DeviceSelection, ExitSelection);
            Selection = (Selection == 0) ? (SelectableCount - 1) : (Selection - 1);
            ModernSetupSetPageSelection (Page, Selection, &DashboardSelection, &BootSelection, &DeviceSelection, &ExitSelection);
          }
        }

        Redraw = TRUE;
        break;
      case ModernUiInputDown:
        if ((Focus == SetupFocusContent) && (Page == PageExit) && mModernSetupLanguageDropdownOpen) {
          mModernSetupLanguageDropdownSelection = (mModernSetupLanguageDropdownSelection + 1) % 2;
        } else if (Focus == SetupFocusNav) {
          if (ModernSetupGetPageSelectableCount (&Ui, Page) > 0) {
            Focus = SetupFocusContent;
          }
        } else if (Page == PageDashboard) {
          if (ModernSetupGetDashboardQuickGrid (&Ui, &DashboardGrid) && ((DashboardSelection + DashboardGrid.CardsPerRow) < DASHBOARD_QUICK_CARD_COUNT)) {
            DashboardSelection += DashboardGrid.CardsPerRow;
          }
        } else {
          SelectableCount = ModernSetupGetPageSelectableCount (&Ui, Page);
          if (SelectableCount > 0) {
            Selection = ModernSetupGetPageSelection (Page, DashboardSelection, BootSelection, DeviceSelection, ExitSelection);
            Selection = (Selection + 1) % SelectableCount;
            ModernSetupSetPageSelection (Page, Selection, &DashboardSelection, &BootSelection, &DeviceSelection, &ExitSelection);
          }
        }

        Redraw = TRUE;
        break;
      case ModernUiInputTab:
        mModernSetupLanguageDropdownOpen = FALSE;
        Focus  = (Focus == SetupFocusNav) ? SetupFocusContent : SetupFocusNav;
        Redraw = TRUE;
        break;
      case ModernUiInputLeft:
        if ((Focus == SetupFocusContent) && (Page == PageExit) && mModernSetupLanguageDropdownOpen) {
          mModernSetupLanguageDropdownOpen = FALSE;
        } else if ((Focus == SetupFocusContent) && (Page == PageDashboard)) {
          if (ModernSetupGetDashboardQuickGrid (&Ui, &DashboardGrid) && ((DashboardSelection % DashboardGrid.CardsPerRow) > 0)) {
            DashboardSelection--;
          } else if (!DashboardGrid.Visible) {
            DashboardSelection = (DashboardSelection == 0) ? (DASHBOARD_QUICK_CARD_COUNT - 1) : (DashboardSelection - 1);
          }
        } else if (Focus == SetupFocusNav) {
          Page = (Page == 0) ? (PageMax - 1) : (Page - 1);
          mModernSetupLanguageDropdownOpen = FALSE;
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
        } else if (Page == PageDashboard) {
          if (ModernSetupGetDashboardQuickGrid (&Ui, &DashboardGrid)) {
            if ((((DashboardSelection % DashboardGrid.CardsPerRow) + 1) < DashboardGrid.CardsPerRow) && ((DashboardSelection + 1) < DASHBOARD_QUICK_CARD_COUNT)) {
              DashboardSelection++;
            }
          } else {
            DashboardSelection = (DashboardSelection + 1) % DASHBOARD_QUICK_CARD_COUNT;
          }
          StatusMessage[0] = L'\0';
        } else {
          mModernSetupLanguageDropdownOpen = FALSE;
          StatusMessage[0] = L'\0';
        }

        Redraw = TRUE;
        break;
      case ModernUiInputEscape:
        if ((Focus == SetupFocusContent) && (Page == PageExit) && mModernSetupLanguageDropdownOpen) {
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
          if (DashboardSelection == 0) {
            Page  = PageBoot;
            Focus = SetupFocusContent;
          } else if (DashboardSelection == 1) {
            Page  = PageDevices;
            Focus = SetupFocusContent;
          } else if (DashboardSelection == 2) {
            Page  = PageDiagnostics;
            Focus = SetupFocusNav;
          } else if (DashboardSelection == 3) {
            Page  = PageFirmware;
            Focus = SetupFocusNav;
          } else if (DashboardSelection == 4) {
            Page  = PagePower;
            Focus = SetupFocusNav;
          } else {
            Page  = PagePerformance;
            Focus = SetupFocusNav;
          }

          StatusMessage[0] = L'\0';
          Redraw = TRUE;
        } else if (Page == PageBoot) {
          Status = ModernSetupLaunchSelectedBootOption (BootSelection);
          UnicodeSPrint (StatusMessage, sizeof (StatusMessage), ModernUiGetString (ModernUiStringBootReturnedFormat), Status);
          Redraw = TRUE;
        } else if (Page == PageDevices) {
          Status = ModernSetupOpenSelectedDeviceEntry (DeviceSelection);
          UnicodeSPrint (StatusMessage, sizeof (StatusMessage), L"FormBrowser returned: %r", Status);
          Redraw = TRUE;
        } else if (Page == PageExit) {
          if (ExitSelection == 0) {
            return EFI_SUCCESS;
          } else if (ExitSelection == 1) {
            Status = ModernSetupLaunchUiAppFallback (ImageHandle);
            UnicodeSPrint (StatusMessage, sizeof (StatusMessage), ModernUiGetString (ModernUiStringClassicReturnedFormat), Status);
            Redraw = TRUE;
          } else if (ExitSelection == 2) {
            gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
          } else {
            ModernSetupHandleLanguageSelectorEnter (StatusMessage, sizeof (StatusMessage));
            Redraw = TRUE;
          }
        }
        break;
      default:
        break;
    }
  }
}
