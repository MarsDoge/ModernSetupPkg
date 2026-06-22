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
#include <ModernUi/ModernUiInventoryData.h>
#include <ModernUi/ModernUiVersion.h>
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
#define MODERN_SETUP_NATIVE_BOOT_TOOLS_ROW_COUNT  1
#define MAX_DEVICE_ROWS    9
#define DASHBOARD_QUICK_CARD_COUNT  8
#define MODERN_SETUP_DASHBOARD_CONTINUE_CARD  0
//
// The server-inventory card is the last entry in the standardized quick-card
// catalog (see Docs/AppFeatureStandard.md). It is the only class-scoped card:
// it is hidden on client/unknown platforms unless a management provider reports
// live data. Because it is the trailing entry, the visible card count simply
// shrinks from the tail and the visible index still equals the catalog index --
// no mid-array remapping is needed. Any future class-scoped card MUST also be
// kept at the tail to preserve that invariant.
//
#define MODERN_SETUP_DASHBOARD_SERVER_CARD    (DASHBOARD_QUICK_CARD_COUNT - 1)
#define DASHBOARD_SECTION_TITLE_TOP 12
#define DASHBOARD_QUICK_CARD_TOP    64
#define DASHBOARD_QUICK_CARD_GAP    40
#define DASHBOARD_QUICK_GROUP_LABEL_OFFSET 24
#define DASHBOARD_QUICK_CARD_BOTTOM 10
#define DASHBOARD_QUICK_VALUE_MIN_HEIGHT  36
//
// Exit-page row layout, shared by DrawExit (drawing) and the pointer hit-test
// (input routing) so click targets always match the painted rows.
//
#define MODERN_SETUP_EXIT_ROW_TOP        72
#define MODERN_SETUP_EXIT_ROW_STRIDE     54
#define MODERN_SETUP_EXIT_ROW_HEIGHT     40
#define MODERN_SETUP_EXIT_ROW_COUNT      4
#define MODERN_SETUP_EXIT_VALUE_WIDTH    220

typedef enum {
  PageDashboard = 0,
  PageSystemInfo,
  PageBoot,
  PageDevices,
  PageSecurity,
  PageFirmware,
  PageDiagnostics,
  PageManagement,
  PagePower,
  PagePerformance,
  PageQuickSettings,
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
  MODERN_UI_INVENTORY_SUMMARY      Inventory;
  EFI_STATUS                       PlatformStatus;
  EFI_STATUS                       SecurityStatus;
  EFI_STATUS                       FirmwareStatus;
  EFI_STATUS                       DiagnosticsStatus;
  EFI_STATUS                       ManagementStatus;
  EFI_STATUS                       PowerStatus;
  EFI_STATUS                       HardwareHealthStatus;
  EFI_STATUS                       PerformanceStatus;
  EFI_STATUS                       PcieStatus;
  EFI_STATUS                       InventoryStatus;
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
  MODERN_UI_RECT  Panel;
  UINTN           RowX;
  UINTN           RowWidth;
  UINTN           RowHeight;
  UINTN           RowStride;
  UINTN           FirstRowY;
  UINTN           MaxVisibleRows;
  UINTN           HorizontalPad;
  BOOLEAN         HasPreviewPane;
  MODERN_UI_RECT  PreviewPanel;
} MODERN_SETUP_PAGE_LIST_LAYOUT;

typedef struct {
  SETUP_PAGE   Page;
  SETUP_FOCUS  Focus;
} MODERN_SETUP_DASHBOARD_ROUTE;

typedef enum {
  MODERN_SETUP_PREFERENCE_ROW_THEME = 0,
  MODERN_SETUP_PREFERENCE_ROW_DASHBOARD_DENSITY,
  MODERN_SETUP_PREFERENCE_ROW_BOOT_TIMEOUT,
  MODERN_SETUP_PREFERENCE_ROW_PROFILE_NAME,
  MODERN_SETUP_PREFERENCE_ROW_REMEMBER_LAST_PAGE,
  MODERN_SETUP_PREFERENCE_ROW_SHOW_ADVANCED_HINTS,
  MODERN_SETUP_PREFERENCE_ROW_CONFIRM_RESET,
  MODERN_SETUP_PREFERENCE_ROW_COUNT
} MODERN_SETUP_PREFERENCE_ROW;

typedef enum {
  ModernSetupPreferencePopupNone = 0,
  ModernSetupPreferencePopupChoice,
  ModernSetupPreferencePopupNumericInput,
  ModernSetupPreferencePopupStringInput
} MODERN_SETUP_PREFERENCE_POPUP_KIND;

extern EFI_HANDLE  mModernSetupImageHandle;
extern BOOLEAN     mModernSetupLanguageDropdownOpen;
extern UINTN       mModernSetupLanguageDropdownSelection;
extern BOOLEAN     mModernSetupPreferencePopupOpen;
extern UINTN       mModernSetupPreferencePopupRow;
extern UINTN       mModernSetupPreferencePopupSelection;
extern MODERN_SETUP_PREFERENCE_POPUP_KIND  mModernSetupPreferencePopupKind;
extern CHAR16      mModernSetupPreferenceInputBuffer[MODERN_UI_PREFERENCES_PROFILE_NAME_CHARS];
extern UINTN       mModernSetupPreferenceInputLength;
extern MODERN_UI_PREFERENCES  mModernSetupPreferences;

EFI_STATUS
ModernSetupGetCachedProviderSnapshot (
  OUT MODERN_SETUP_PROVIDER_SNAPSHOT  *Snapshot
  );

VOID
ModernSetupInvalidateProviderSnapshotCache (
  VOID
  );

EFI_STATUS
ModernSetupGetCachedDeviceEntries (
  OUT CONST MODERN_UI_DEVICE_ENTRY  **Entries,
  OUT UINTN                         *EntryCount
  );

VOID
ModernSetupInvalidateDeviceEntriesCache (
  VOID
  );

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
ModernSetupRefreshHeaderClock (
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
ModernSetupGetBootSelectableCount (
  VOID
  );

BOOLEAN
ModernSetupBootSelectionIsNativeFallback (
  IN UINTN  Selection,
  IN UINTN  SelectableCount
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
  IN  UINT8                              DashboardDensity,
  OUT MODERN_SETUP_DASHBOARD_QUICK_GRID  *Grid
  );

BOOLEAN
ModernSetupGetPageListLayout (
  IN  MODERN_UI_RENDER_CONTEXT       *Ui,
  IN  UINT8                          DashboardDensity,
  IN  UINTN                          HardRowCap,
  IN  BOOLEAN                        AllowPreviewPane,
  OUT MODERN_SETUP_PAGE_LIST_LAYOUT  *Layout
  );

BOOLEAN
ModernSetupGetDashboardCategoryRoute (
  IN  UINTN                         Selection,
  OUT MODERN_SETUP_DASHBOARD_ROUTE  *Route
  );

BOOLEAN
ModernSetupDashboardSelectionRequestsContinue (
  IN UINTN  Selection
  );

/**
  Decide whether a standardized dashboard quick-card is applicable on the
  current platform.

  This is the single, data-driven applicability predicate required by
  Docs/AppFeatureStandard.md. All catalog cards are applicable except the
  trailing server-inventory card (MODERN_SETUP_DASHBOARD_SERVER_CARD), which is
  applicable only when the platform is server-class or a management provider
  (IPMI / Redfish / SMBIOS management interface) reports live data. The cached
  provider snapshot is consulted; no providers are re-probed here.

  @param[in] CardIndex  Catalog card index in [0, DASHBOARD_QUICK_CARD_COUNT).

  @retval TRUE   The card should be shown, navigable, and route-activatable.
  @retval FALSE  The card is hidden on this platform (CardIndex out of range
                 also returns FALSE).
**/
BOOLEAN
ModernSetupDashboardQuickCardApplicable (
  IN UINTN  CardIndex
  );

/**
  Hit-test the top tab strip for a pointer click.

  Mirrors the same visible-tab window math ModernSetupDrawTabs paints with
  (including the scrolled chevron inset), so the click targets always match the
  painted tabs.

  @param[in]  Ui    Initialized render context. Must not be NULL.
  @param[in]  Page  Currently selected page (determines the scroll window).
  @param[in]  X     Pointer X in pixels.
  @param[in]  Y     Pointer Y in pixels.
  @param[out] Hit   Receives the page of the clicked tab on success. Must not
                    be NULL.

  @retval TRUE   (X,Y) lies on a visible tab; *Hit is set.
  @retval FALSE  No tab at this position; *Hit is unchanged.
**/
BOOLEAN
ModernSetupHitTestTab (
  IN  MODERN_UI_RENDER_CONTEXT  *Ui,
  IN  SETUP_PAGE                Page,
  IN  UINTN                     X,
  IN  UINTN                     Y,
  OUT SETUP_PAGE                *Hit
  );

/**
  Move (or first-draw) the pointer cursor using save-under compositing.

  The pixels beneath the cursor are captured before the arrow is drawn and
  restored when it moves, so pointer motion repaints only a small rectangle
  instead of the whole frame (no full-screen flicker). Display-only; a no-op
  when Ui or Theme is NULL or the screen is smaller than the cursor.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] X      Cursor hotspot X in pixels (clamped on-screen).
  @param[in] Y      Cursor hotspot Y in pixels (clamped on-screen).
**/
VOID
ModernSetupMovePointerCursor (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN UINTN                     X,
  IN UINTN                     Y
  );

/**
  Forget the saved under-cursor pixels.

  Must be called after any full-frame repaint: the saved pixels describe the
  previous frame and must not be restored onto the new one.
**/
VOID
ModernSetupInvalidatePointerCursor (
  VOID
  );

/**
  Hit-test the dashboard quick-card grid for a pointer click.

  Uses the same grid contract as drawing and keyboard navigation
  (ModernSetupGetDashboardQuickGrid + the platform-visible card count), so a
  hidden card can never be clicked.

  @param[in]  Ui    Initialized render context. Must not be NULL.
  @param[in]  X     Pointer X in pixels.
  @param[in]  Y     Pointer Y in pixels.
  @param[out] Card  Receives the catalog index of the clicked card. Must not
                    be NULL.

  @retval TRUE   (X,Y) lies on a visible quick card; *Card is set.
  @retval FALSE  No card at this position; *Card is unchanged.
**/
BOOLEAN
ModernSetupHitTestDashboardCard (
  IN  MODERN_UI_RENDER_CONTEXT  *Ui,
  IN  UINTN                     X,
  IN  UINTN                     Y,
  OUT UINTN                     *Card
  );

/**
  Hit-test the Exit page rows (and the language dropdown when open) for a
  pointer click.

  Uses the shared MODERN_SETUP_EXIT_ROW_* layout constants so click targets
  always match DrawExit's painted rows.

  @param[in]  Ui              Initialized render context. Must not be NULL.
  @param[in]  X               Pointer X in pixels.
  @param[in]  Y               Pointer Y in pixels.
  @param[out] Row             Receives the clicked row index. Must not be NULL.
  @param[out] DropdownOption  Receives the clicked open-dropdown option, or
                              (UINTN)-1 when the click is on a row instead.
                              Must not be NULL.

  @retval TRUE   (X,Y) lies on an Exit row or an open dropdown option.
  @retval FALSE  Nothing clickable at this position.
**/
BOOLEAN
ModernSetupHitTestExitRow (
  IN  MODERN_UI_RENDER_CONTEXT  *Ui,
  IN  UINTN                     X,
  IN  UINTN                     Y,
  OUT UINTN                     *Row,
  OUT UINTN                     *DropdownOption
  );

/**
  Hit-test a list-page (Boot / Devices / Preferences) row for a pointer click.

  Uses the same `ModernSetupGetPageListLayout` parameters and selectable count
  the page's drawing uses, so click bands match the painted rows. The vertical
  band is the row stride starting at the first row, so a click anywhere on a
  row line selects it.

  @param[in]  Ui    Initialized render context. Must not be NULL.
  @param[in]  Page  List page under test (non-list pages return FALSE).
  @param[in]  X     Pointer X in pixels.
  @param[in]  Y     Pointer Y in pixels.
  @param[out] Row   Receives the clicked visible row index. Must not be NULL.

  @retval TRUE   (X,Y) lies on a visible list row; *Row is set.
  @retval FALSE  Not a list page, or no row at this position.
**/
BOOLEAN
ModernSetupHitTestPageListRow (
  IN  MODERN_UI_RENDER_CONTEXT  *Ui,
  IN  SETUP_PAGE                Page,
  IN  UINTN                     X,
  IN  UINTN                     Y,
  OUT UINTN                     *Row
  );

/**
  Return the number of dashboard quick-cards visible on the current platform.

  Counts the applicable catalog cards (see
  ModernSetupDashboardQuickCardApplicable). Because the only class-scoped card
  is the trailing entry, the result is a contiguous prefix length in
  [1, DASHBOARD_QUICK_CARD_COUNT]: the visible card index equals the catalog
  index, so grid layout, keyboard navigation, and route resolution can all use
  this count directly without remapping.

  @return Visible quick-card count for the current platform snapshot.
**/
UINTN
ModernSetupDashboardVisibleQuickCardCount (
  VOID
  );

EFI_STATUS
ModernSetupGetCachedBootOptions (
  OUT CONST MODERN_UI_BOOT_OPTION  **Options,
  OUT UINTN                        *OptionCount
  );

VOID
ModernSetupInvalidateBootOptionsCache (
  VOID
  );

EFI_STATUS
ModernSetupLaunchSelectedBootOption (
  IN UINTN  Selection
  );

EFI_STATUS
ModernSetupSetSelectedBootNext (
  IN UINTN  Selection
  );

EFI_STATUS
ModernSetupClearBootNext (
  VOID
  );

EFI_STATUS
ModernSetupMoveSelectedBootOption (
  IN UINTN    Selection,
  IN BOOLEAN  MoveUp
  );

BOOLEAN
ModernSetupBootOptionIsDefaultBootCandidate (
  IN CONST MODERN_UI_BOOT_OPTION  *Option
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

CONST CHAR16 *
ModernSetupGetPreferenceChoiceName (
  IN UINTN  Row,
  IN UINTN  Selection
  );

CONST CHAR16 *
ModernSetupGetPreferenceValueName (
  IN UINTN  Row
  );

CONST CHAR16 *
ModernSetupPreferenceCheckboxValueText (
  IN UINT8  Value
  );

UINTN
ModernSetupGetPreferenceChoiceCount (
  IN UINTN  Row
  );

VOID
ModernSetupHandlePreferencePopupUp (
  VOID
  );

VOID
ModernSetupHandlePreferencePopupDown (
  VOID
  );

VOID
ModernSetupCancelPreferencePopup (
  VOID
  );

VOID
ModernSetupCommitPreferencePopup (
  OUT CHAR16  *StatusMessage,
  IN  UINTN   StatusSize
  );

VOID
ModernSetupHandlePreferenceInputKey (
  IN  CONST MODERN_UI_INPUT_EVENT  *Event,
  OUT CHAR16                       *StatusMessage,
  IN  UINTN                        StatusSize
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
