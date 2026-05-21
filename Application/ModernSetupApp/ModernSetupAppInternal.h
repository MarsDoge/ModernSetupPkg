/** @file
  Modern setup application internal declarations.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_SETUP_APP_INTERNAL_H_
#define MODERN_SETUP_APP_INTERNAL_H_

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
#include <ModernUi/ModernUiHardwareHealthData.h>
#include <ModernUi/ModernUiInput.h>
#include <ModernUi/ModernUiEngine.h>
#include <ModernUi/ModernUiManagementData.h>
#include <ModernUi/ModernUiPerformanceData.h>
#include <ModernUi/ModernUiPreferences.h>
#include <ModernUi/ModernUiPcieData.h>
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
#define DASHBOARD_QUICK_CARD_COUNT  7
#define DASHBOARD_SECTION_TITLE_TOP 12
#define DASHBOARD_QUICK_CARD_TOP    64
#define DASHBOARD_QUICK_CARD_GAP    40
#define DASHBOARD_QUICK_GROUP_LABEL_OFFSET 24
#define DASHBOARD_QUICK_CARD_BOTTOM 10
#define DASHBOARD_QUICK_VALUE_MIN_HEIGHT  36

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
  PageServerInventory,
  PagePreferences,
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

typedef struct {
  MODERN_UI_PLATFORM_SUMMARY       Platform;
  MODERN_UI_SECURITY_SUMMARY       Security;
  MODERN_UI_FIRMWARE_SUMMARY       Firmware;
  MODERN_UI_DIAGNOSTICS_SUMMARY    Diagnostics;
  MODERN_UI_MANAGEMENT_SUMMARY     Management;
  MODERN_UI_POWER_SUMMARY          Power;
  MODERN_UI_HARDWARE_HEALTH_SUMMARY HardwareHealth;
  MODERN_UI_PERFORMANCE_SUMMARY    Performance;
  MODERN_UI_PCIE_SUMMARY           Pcie;
  EFI_STATUS                       PlatformStatus;
  EFI_STATUS                       SecurityStatus;
  EFI_STATUS                       FirmwareStatus;
  EFI_STATUS                       DiagnosticsStatus;
  EFI_STATUS                       ManagementStatus;
  EFI_STATUS                       PowerStatus;
  EFI_STATUS                       HardwareHealthStatus;
  EFI_STATUS                       PerformanceStatus;
  EFI_STATUS                       PcieStatus;
} MODERN_SETUP_PROVIDER_SNAPSHOT;

typedef enum {
  ModernSetupProviderHealthReady = 0,
  ModernSetupProviderHealthDegraded,
  ModernSetupProviderHealthNotReady
} MODERN_SETUP_PROVIDER_HEALTH_STATE;

typedef struct {
  MODERN_SETUP_PROVIDER_HEALTH_STATE  State;
  UINTN                               TotalProviders;
  UINTN                               ReadyProviders;
  UINTN                               UnavailableProviders;
  EFI_STATUS                          FirstIssueStatus;
  CONST CHAR16                        *FirstIssueName;
} MODERN_SETUP_PROVIDER_HEALTH_SUMMARY;

typedef struct {
  BOOLEAN         Visible;
  MODERN_UI_RECT  Panel;
  UINTN           CardsPerRow;
  UINTN           Rows;
  UINTN           CardGap;
  UINTN           CardTop;
  UINTN           CardHeight;
  UINTN           CardWidth;
} MODERN_SETUP_DASHBOARD_QUICK_GRID;

typedef struct {
  SETUP_PAGE   Page;
  SETUP_FOCUS  Focus;
} MODERN_SETUP_DASHBOARD_ROUTE;

extern EFI_HANDLE  mModernSetupImageHandle;
extern BOOLEAN     mModernSetupLanguageDropdownOpen;
extern UINTN       mModernSetupLanguageDropdownSelection;
extern MODERN_UI_PREFERENCES  mModernSetupPreferences;

MODERN_UI_RECT
ModernSetupContentRect (
  IN MODERN_UI_RENDER_CONTEXT  *Ui
  );

VOID
ModernSetupDrawHeader (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme
  );

VOID
ModernSetupDrawTabs (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_PAGE                Page,
  IN SETUP_FOCUS               Focus
  );

VOID
ModernSetupDrawFooter (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN CONST CHAR16              *StatusMessage
  );

VOID
ModernSetupDrawPageTitle (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_PAGE                Page
  );

VOID
ModernSetupDrawDashboard (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     Selection
  );

EFI_STATUS
ModernSetupGetProviderSnapshot (
  OUT MODERN_SETUP_PROVIDER_SNAPSHOT  *Snapshot
  );

EFI_STATUS
ModernSetupGetProviderHealthSummary (
  IN  CONST MODERN_SETUP_PROVIDER_SNAPSHOT  *Snapshot,
  OUT MODERN_SETUP_PROVIDER_HEALTH_SUMMARY  *Health
  );

CONST CHAR16 *
ModernSetupGetProviderHealthStateText (
  IN MODERN_SETUP_PROVIDER_HEALTH_STATE  State
  );

VOID
ModernSetupDrawCurrentPage (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_PAGE                Page,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     DashboardSelection,
  IN UINTN                     BootSelection,
  IN UINTN                     DeviceSelection,
  IN UINTN                     PreferencesSelection,
  IN UINTN                     ExitSelection,
  IN CONST CHAR16              *StatusMessage
  );

UINTN
ModernSetupGetPageSelection (
  IN SETUP_PAGE  Page,
  IN UINTN       DashboardSelection,
  IN UINTN       BootSelection,
  IN UINTN       DeviceSelection,
  IN UINTN       PreferencesSelection,
  IN UINTN       ExitSelection
  );

VOID
ModernSetupSetPageSelection (
  IN     SETUP_PAGE  Page,
  IN     UINTN       Selection,
  IN OUT UINTN       *DashboardSelection,
  IN OUT UINTN       *BootSelection,
  IN OUT UINTN       *DeviceSelection,
  IN OUT UINTN       *PreferencesSelection,
  IN OUT UINTN       *ExitSelection
  );

UINTN
ModernSetupGetBootCount (
  VOID
  );

UINTN
ModernSetupGetVisibleDeviceCount (
  VOID
  );

UINTN
ModernSetupGetPageSelectableCount (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN SETUP_PAGE                Page
  );

BOOLEAN
ModernSetupGetDashboardQuickGrid (
  IN  MODERN_UI_RENDER_CONTEXT           *Ui,
  OUT MODERN_SETUP_DASHBOARD_QUICK_GRID  *Grid
  );

BOOLEAN
ModernSetupGetDashboardCategoryRoute (
  IN  UINTN                         Selection,
  OUT MODERN_SETUP_DASHBOARD_ROUTE  *Route
  );

EFI_STATUS
ModernSetupLaunchSelectedBootOption (
  IN UINTN  Selection
  );

EFI_STATUS
ModernSetupOpenSelectedDeviceEntry (
  IN UINTN  Selection
  );

VOID
ModernSetupHandleLanguageSelectorEnter (
  OUT CHAR16  *StatusMessage,
  IN  UINTN   StatusSize
  );

VOID
ModernSetupHandlePreferencesEnter (
  IN  UINTN   Selection,
  OUT CHAR16  *StatusMessage,
  IN  UINTN   StatusSize
  );

EFI_STATUS
ModernSetupLaunchUiAppFallback (
  IN EFI_HANDLE  ImageHandle
  );

CONST CHAR16 *
ModernSetupGetLanguageOptionName (
  IN UINTN  Selection
  );

UINTN
ModernSetupGetActiveLanguageSelection (
  VOID
  );

#endif
