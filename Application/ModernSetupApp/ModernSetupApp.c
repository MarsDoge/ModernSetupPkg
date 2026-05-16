/** @file
  Modern graphical setup application prototype.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/DriverSampleHii.h>
#include <Guid/GlobalVariable.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/LoadedImage.h>

#include <ModernUi/ModernUiHiiBridge.h>
#include <ModernUi/ModernUiInput.h>
#include <ModernUi/ModernUiPageAdapter.h>
#include <ModernUi/ModernUiRenderer.h>
#include <ModernUi/ModernUiString.h>
#include <ModernUi/ModernUiTheme.h>

#define CARD_GAP           16
#define TOP_BAR_HEIGHT     54
#define TAB_BAR_HEIGHT     54
#define PAGE_TITLE_HEIGHT  64
#define FOOTER_HEIGHT      36
#define SCREEN_MARGIN      24
#define MAX_BOOT_ROWS      9
#define MAX_HII_ROWS       9

STATIC CONST EFI_GUID  mUiAppGuid = { 0x462CAA21, 0x7614, 0x4503, { 0x83, 0x6E, 0x8A, 0xB6, 0xF4, 0x66, 0x23, 0x31 } };
STATIC CONST EFI_GUID  mDemoHiiFormSetGuids[] = {
  DRIVER_SAMPLE_FORMSET_GUID,
  DRIVER_SAMPLE_INVENTORY_GUID
};
STATIC EFI_HANDLE      mImageHandle;
STATIC MODERN_UI_HII_MODEL  mHiiModel;

typedef enum {
  PageDashboard = 0,
  PageBoot,
  PageDevices,
  PageSecurity,
  PageHii,
  PageExit,
  PageMax
} SETUP_PAGE;

typedef enum {
  SetupFocusNav = 0,
  SetupFocusContent
} SETUP_FOCUS;

typedef enum {
  HiiViewFormSets = 0,
  HiiViewForms,
  HiiViewItems
} HII_VIEW_LEVEL;

typedef struct {
  SETUP_PAGE           Page;
  MODERN_UI_STRING_ID  Title;
  MODERN_UI_STRING_ID  Hint;
} PAGE_DESCRIPTOR;

typedef struct {
  HII_VIEW_LEVEL  Level;
  UINTN           FormSetIndex;
  UINTN           FormIndex;
  UINTN           Selection;
  UINTN           Scroll;
} HII_VIEW_STATE;

STATIC CONST PAGE_DESCRIPTOR  mPages[] = {
  { PageDashboard, ModernUiStringPageDashboard, ModernUiStringPageDashboardHint },
  { PageBoot,      ModernUiStringPageBoot,      ModernUiStringPageBootHint      },
  { PageDevices,   ModernUiStringPageDevices,   ModernUiStringPageDevicesHint   },
  { PageSecurity,  ModernUiStringPageSecurity,  ModernUiStringPageSecurityHint  },
  { PageHii,       ModernUiStringPageHii,       ModernUiStringPageHiiHint       },
  { PageExit,      ModernUiStringPageExit,      ModernUiStringPageExitHint      }
};

STATIC HII_VIEW_STATE  mHiiView = { HiiViewFormSets, 0, 0, 0, 0 };
STATIC BOOLEAN         mLanguageDropdownOpen;
STATIC UINTN           mLanguageDropdownSelection;

STATIC
VOID
UpdateHiiScroll (
  VOID
  );

/**
  Load the HII model used by the current ArmVirt demo.

  DriverSample remains the default compatibility target, but the bridge parser
  itself is generic and accepts this GUID filter from the app shell.

  @retval EFI_SUCCESS            HII model was loaded or no matching formset exists.
  @retval EFI_INVALID_PARAMETER  Demo filter arguments are invalid.
  @retval EFI_NOT_FOUND          HII database protocol is unavailable.
  @retval EFI_OUT_OF_RESOURCES   A temporary package allocation failed.
**/
STATIC
EFI_STATUS
LoadHiiModel (
  VOID
  )
{
  return ModernUiHiiBridgeLoadFiltered (&mHiiModel, mDemoHiiFormSetGuids, ARRAY_SIZE (mDemoHiiFormSetGuids));
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
    case PageHii:
      return mHiiView.Selection;
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
    case PageHii:
      mHiiView.Selection = Selection;
      UpdateHiiScroll ();
      break;
    case PageExit:
      *ExitSelection = Selection;
      break;
    default:
      break;
  }
}

/**
  Keep the HII scroll window aligned with the selected row.
**/
STATIC
VOID
UpdateHiiScroll (
  VOID
  )
{
  if (mHiiView.Selection < mHiiView.Scroll) {
    mHiiView.Scroll = mHiiView.Selection;
  }

  if (mHiiView.Selection >= (mHiiView.Scroll + MAX_HII_ROWS)) {
    mHiiView.Scroll = mHiiView.Selection - MAX_HII_ROWS + 1;
  }
}

/**
  Read a UEFI global variable into an allocated buffer.

  @param[in]  Name        Variable name. Must not be NULL.
  @param[out] Buffer      Receives an allocated buffer owned by the caller. Must
                          not be NULL. Set to NULL on failure.
  @param[out] BufferSize  Receives the buffer size in bytes. Must not be NULL.

  @retval EFI_SUCCESS            Variable was read and Buffer must be freed.
  @retval EFI_INVALID_PARAMETER  Name, Buffer, or BufferSize is NULL.
  @retval EFI_OUT_OF_RESOURCES   Allocation failed.
  @retval others                 Status returned by GetVariable().
**/
STATIC
EFI_STATUS
ReadGlobalVariable (
  IN  CONST CHAR16  *Name,
  OUT VOID          **Buffer,
  OUT UINTN         *BufferSize
  )
{
  EFI_STATUS  Status;
  UINTN       Size;
  VOID        *Data;

  if ((Name == NULL) || (Buffer == NULL) || (BufferSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Buffer     = NULL;
  *BufferSize = 0;
  Size        = 0;

  Status = gRT->GetVariable ((CHAR16 *)Name, &gEfiGlobalVariableGuid, NULL, &Size, NULL);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  Data = AllocateZeroPool (Size);
  if (Data == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gRT->GetVariable ((CHAR16 *)Name, &gEfiGlobalVariableGuid, NULL, &Size, Data);
  if (EFI_ERROR (Status)) {
    FreePool (Data);
    return Status;
  }

  *Buffer     = Data;
  *BufferSize = Size;
  return EFI_SUCCESS;
}

/**
  Return whether a boot option points at this setup application image.

  @param[in] FilePath  Boot option device path. May be NULL.

  @retval TRUE   FilePath matches the loaded ModernSetupApp image path.
  @retval FALSE  FilePath is NULL, the current image path cannot be found, or it
                 points at a different image.
**/
STATIC
BOOLEAN
IsCurrentApplicationBootOption (
  IN EFI_DEVICE_PATH_PROTOCOL  *FilePath
  )
{
  EFI_STATUS                Status;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *ApplicationPath;
  UINTN                     FilePathSize;
  UINTN                     ApplicationPathSize;
  BOOLEAN                   Match;

  if ((mImageHandle == NULL) || (FilePath == NULL)) {
    return FALSE;
  }

  Status = gBS->HandleProtocol (
                  mImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (EFI_ERROR (Status) || (LoadedImage == NULL)) {
    return FALSE;
  }

  DevicePath = DevicePathFromHandle (LoadedImage->DeviceHandle);
  if ((DevicePath == NULL) || (LoadedImage->FilePath == NULL)) {
    return FALSE;
  }

  ApplicationPath = AppendDevicePathNode (DevicePath, LoadedImage->FilePath);
  if (ApplicationPath == NULL) {
    return FALSE;
  }

  FilePathSize        = GetDevicePathSize (FilePath);
  ApplicationPathSize = GetDevicePathSize (ApplicationPath);
  Match               = (BOOLEAN)(
                                  (FilePathSize == ApplicationPathSize) &&
                                  (CompareMem (FilePath, ApplicationPath, FilePathSize) == 0)
                                  );
  FreePool (ApplicationPath);
  return Match;
}

/**
  Return whether a Boot Manager load option should be shown in the Boot page.

  @param[in] BootOption  Boot option to inspect. Must not be NULL.

  @retval TRUE   The option is visible to users.
  @retval FALSE  The option is hidden or points at this setup app.
**/
STATIC
BOOLEAN
IsVisibleBootOption (
  IN CONST EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption
  )
{
  if ((BootOption == NULL) || ((BootOption->Attributes & LOAD_OPTION_HIDDEN) != 0)) {
    return FALSE;
  }

  return (BOOLEAN)!IsCurrentApplicationBootOption (BootOption->FilePath);
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
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions;
  UINTN                         BootOptionCount;
  UINTN                         Index;
  UINTN                         VisibleCount;

  BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);
  if (BootOptions == NULL) {
    return 0;
  }

  VisibleCount = 0;
  for (Index = 0; Index < BootOptionCount; Index++) {
    if (IsVisibleBootOption (&BootOptions[Index])) {
      VisibleCount++;
    }
  }

  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
  return VisibleCount;
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
  EFI_STATUS                Status;
  EFI_HANDLE                *Handles;
  UINTN                     HandleCount;
  UINTN                     Index;
  UINTN                     Count;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

  Status = gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &HandleCount, &Handles);
  if (EFI_ERROR (Status)) {
    return 0;
  }

  Count = 0;
  for (Index = 0; (Index < HandleCount) && (Count < 8); Index++) {
    DevicePath = DevicePathFromHandle (Handles[Index]);
    if (DevicePath != NULL) {
      Count++;
    }
  }

  FreePool (Handles);
  return Count;
}

/**
  Count visible items in one HII form.

  @param[in] Form  Form to inspect. May be NULL.

  @return Number of visible items.
**/
STATIC
UINTN
GetVisibleHiiItemCount (
  IN MODERN_UI_HII_FORM  *Form
  )
{
  UINTN  Index;
  UINTN  Count;

  if (Form == NULL) {
    return 0;
  }

  Count = 0;
  for (Index = 0; Index < Form->ItemCount; Index++) {
    if (Form->Items[Index].Visible) {
      Count++;
    }
  }

  return Count;
}

/**
  Return a visible HII item by visible row index.

  @param[in] Form          Form to inspect. Must not be NULL.
  @param[in] VisibleIndex  Zero-based visible item index.

  @return Matching item, or NULL when VisibleIndex is out of range.
**/
STATIC
MODERN_UI_HII_ITEM *
GetVisibleHiiItem (
  IN MODERN_UI_HII_FORM  *Form,
  IN UINTN               VisibleIndex
  )
{
  UINTN  Index;
  UINTN  Count;

  if (Form == NULL) {
    return NULL;
  }

  Count = 0;
  for (Index = 0; Index < Form->ItemCount; Index++) {
    if (!Form->Items[Index].Visible) {
      continue;
    }

    if (Count == VisibleIndex) {
      return &Form->Items[Index];
    }

    Count++;
  }

  return NULL;
}

/**
  Return the source item index for a visible HII row.

  @param[in]  Form          Form to inspect. Must not be NULL.
  @param[in]  VisibleIndex  Zero-based visible item index.
  @param[out] ItemIndex     Receives source item index. Must not be NULL.

  @retval TRUE   Matching item was found.
  @retval FALSE  VisibleIndex is out of range.
**/
STATIC
BOOLEAN
GetVisibleHiiItemIndex (
  IN  MODERN_UI_HII_FORM  *Form,
  IN  UINTN               VisibleIndex,
  OUT UINTN               *ItemIndex
  )
{
  UINTN  Index;
  UINTN  Count;

  if ((Form == NULL) || (ItemIndex == NULL)) {
    return FALSE;
  }

  Count = 0;
  for (Index = 0; Index < Form->ItemCount; Index++) {
    if (!Form->Items[Index].Visible) {
      continue;
    }

    if (Count == VisibleIndex) {
      *ItemIndex = Index;
      return TRUE;
    }

    Count++;
  }

  return FALSE;
}

/**
  Return the selectable row count for the current HII bridge view.

  @return Number of selectable HII rows available for the current view.
**/
STATIC
UINTN
GetHiiSelectableCount (
  VOID
  )
{
  if (mHiiView.Level == HiiViewFormSets) {
    return MIN (mHiiModel.FormSetCount, MAX_HII_ROWS);
  }

  if (mHiiView.FormSetIndex >= mHiiModel.FormSetCount) {
    return 0;
  }

  if (mHiiView.Level == HiiViewForms) {
    return MIN (mHiiModel.FormSets[mHiiView.FormSetIndex].FormCount, MAX_HII_ROWS);
  }

  if (mHiiView.FormIndex >= mHiiModel.FormSets[mHiiView.FormSetIndex].FormCount) {
    return 0;
  }

  return GetVisibleHiiItemCount (&mHiiModel.FormSets[mHiiView.FormSetIndex].Forms[mHiiView.FormIndex]);
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
    case PageHii:
      return GetHiiSelectableCount ();
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
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions;
  UINTN                         BootOptionCount;
  UINTN                         Index;
  UINTN                         VisibleIndex;
  EFI_STATUS                    Status;

  BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);
  if (BootOptions == NULL) {
    return EFI_NOT_FOUND;
  }

  Status       = EFI_NOT_FOUND;
  VisibleIndex = 0;
  for (Index = 0; Index < BootOptionCount; Index++) {
    if (!IsVisibleBootOption (&BootOptions[Index])) {
      continue;
    }

    if (VisibleIndex == Selection) {
      EfiBootManagerBoot (&BootOptions[Index]);
      Status = BootOptions[Index].Status;
      break;
    }

    VisibleIndex++;
  }

  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
  return Status;
}

/**
  Read the SecureBoot variable as a boolean state.

  @retval TRUE   SecureBoot exists and is non-zero.
  @retval FALSE  SecureBoot is absent, unreadable, too small, or zero.
**/
STATIC
BOOLEAN
GetSecureBootEnabled (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       Size;
  UINT8       *Value;
  BOOLEAN     Enabled;

  Status = ReadGlobalVariable (EFI_SECURE_BOOT_MODE_NAME, (VOID **)&Value, &Size);
  if (EFI_ERROR (Status) || (Size < sizeof (UINT8))) {
    return FALSE;
  }

  Enabled = (BOOLEAN)(Value[0] != 0);
  FreePool (Value);
  return Enabled;
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
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Header;
  UINTN                          ModeX;
  UINTN                          InfoX;

  Header = ModernUiBlendColor (Theme->Surface, Theme->AccentSoft, 18);
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 0, 0, Ui->Width, TOP_BAR_HEIGHT }, Header);
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 0, TOP_BAR_HEIGHT - 3, Ui->Width, 2 }, Theme->Accent);
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ SCREEN_MARGIN, 13, 3, 22 }, Theme->Accent);
  ModernUiDrawText (Ui, SCREEN_MARGIN + 16, 15, ModernUiGetString (ModernUiStringHeaderTitle), Theme->Text, Header);

  ModeX = Ui->Width / 2 - 96;
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ ModeX, 10, 192, 32 }, ModernUiBlendColor (Header, Theme->AccentSoft, 42));
  ModernUiStrokeRect (Ui, (MODERN_UI_RECT){ ModeX, 10, 192, 32 }, ModernUiBlendColor (Theme->Border, Theme->Accent, 35));
  ModernUiDrawText (Ui, ModeX + 28, 18, ModernUiGetString (ModernUiStringHeaderMode), Theme->Accent, ModernUiBlendColor (Header, Theme->AccentSoft, 42));

  InfoX = Ui->Width - 276;
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ InfoX, 10, 220, 32 }, Theme->Surface);
  ModernUiStrokeRect (Ui, (MODERN_UI_RECT){ InfoX, 10, 220, 32 }, Theme->Border);
  ModernUiDrawTextFormatted (Ui, InfoX + 18, 18, Theme->MutedText, Theme->Surface, L"AARCH64  %ux%u", Ui->Width, Ui->Height);
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
  UINTN                          TabWidth;
  UINTN                          X;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  TabColor;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  TextColor;

  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 0, TOP_BAR_HEIGHT, Ui->Width, TAB_BAR_HEIGHT }, ModernUiBlendColor (Theme->Surface, Theme->SurfaceRaised, 35));
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 0, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT - 1, Ui->Width, 1 }, Theme->Border);

  TabWidth = (Ui->Width - (SCREEN_MARGIN * 2)) / ARRAY_SIZE (mPages);
  for (Index = 0; Index < ARRAY_SIZE (mPages); Index++) {
    X        = SCREEN_MARGIN + (Index * TabWidth);
    TabColor = (mPages[Index].Page == Page) ? Theme->AccentSoft : ModernUiBlendColor (Theme->Surface, Theme->SurfaceRaised, 35);
    TextColor = (mPages[Index].Page == Page) ? Theme->Text : Theme->MutedText;

    if (mPages[Index].Page == Page) {
      ModernUiFillRect (Ui, (MODERN_UI_RECT){ X, TOP_BAR_HEIGHT + 9, TabWidth - 8, 36 }, TabColor);
      ModernUiStrokeRect (Ui, (MODERN_UI_RECT){ X, TOP_BAR_HEIGHT + 9, TabWidth - 8, 36 }, ModernUiBlendColor (Theme->Border, Theme->Accent, 25));
      ModernUiFillRect (
        Ui,
        (MODERN_UI_RECT){ X, TOP_BAR_HEIGHT + 43, TabWidth - 8, 3 },
        (Focus == SetupFocusNav) ? Theme->Accent : Theme->Border
        );
    }

    ModernUiDrawText (Ui, X + 18, TOP_BAR_HEIGHT + 20, ModernUiGetString (mPages[Index].Title), TextColor, TabColor);
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
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 0, Y, Ui->Width, FOOTER_HEIGHT }, Theme->Surface);
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 0, Y, Ui->Width, 1 }, Theme->Border);
  if ((StatusMessage != NULL) && (StatusMessage[0] != L'\0')) {
    ModernUiDrawText (Ui, SCREEN_MARGIN, Y + 10, StatusMessage, Theme->Warning, Theme->Surface);
  } else if (Focus == SetupFocusNav) {
    ModernUiDrawText (Ui, SCREEN_MARGIN, Y + 10, ModernUiGetString (ModernUiStringFooterNav), Theme->MutedText, Theme->Surface);
  } else {
    ModernUiDrawText (Ui, SCREEN_MARGIN, Y + 10, ModernUiGetString (ModernUiStringFooterContent), Theme->MutedText, Theme->Surface);
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
  UINTN   CardWidth;
  MODERN_UI_RECT  Content;
  MODERN_UI_RECT  StatusRect;

  Content = ContentRect (Ui);
  CardWidth = (Content.Width - CARD_GAP) / 2;
  StatusRect = (MODERN_UI_RECT){ Content.X, Content.Y + 216, Content.Width, 112 };
  UnicodeSPrint (Resolution, sizeof (Resolution), L"%u x %u", Ui->Width, Ui->Height);
  UnicodeSPrint (BootCount, sizeof (BootCount), ModernUiGetString (ModernUiStringBootCountFormat), GetBootCount ());

  ModernUiDrawInfoCard (Ui, (MODERN_UI_RECT){ Content.X, Content.Y, CardWidth, 92 }, ModernUiGetString (ModernUiStringFirmwareVendor), gST->FirmwareVendor, Theme);
  ModernUiDrawInfoCard (Ui, (MODERN_UI_RECT){ Content.X + CardWidth + CARD_GAP, Content.Y, CardWidth, 92 }, ModernUiGetString (ModernUiStringFirmwareRevision), L"edk2 / ArmVirt", Theme);
  ModernUiDrawInfoCard (Ui, (MODERN_UI_RECT){ Content.X, Content.Y + 108, CardWidth, 92 }, ModernUiGetString (ModernUiStringDisplay), Resolution, Theme);
  ModernUiDrawInfoCard (Ui, (MODERN_UI_RECT){ Content.X + CardWidth + CARD_GAP, Content.Y + 108, CardWidth, 92 }, ModernUiGetString (ModernUiStringBootOptions), BootCount, Theme);

  ModernUiDrawPanel (Ui, StatusRect, Theme);
  ModernUiDrawFocusFrame (Ui, StatusRect, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, StatusRect.X + 20, StatusRect.Y + 18, ModernUiGetString (ModernUiStringPrototypeStatus), Theme->MutedText, Theme->Surface);
  ModernUiDrawText (Ui, StatusRect.X + 20, StatusRect.Y + 48, ModernUiGetString (ModernUiStringPrototypeStatusValue), Theme->Text, Theme->Surface);
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
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions;
  UINTN                         BootOptionCount;
  UINTN                         Index;
  UINTN                         VisibleIndex;
  UINTN                         Y;
  CHAR16                        Line[160];
  CONST CHAR16                  *Description;
  CONST CHAR16                  *State;
  BOOLEAN                       IsSelected;
  MODERN_UI_RECT                Panel;
  UINTN                         RowX;
  UINTN                         RowWidth;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL RowBackground;

  Panel = ContentRect (Ui);
  RowX = Panel.X + 20;
  RowWidth = Panel.Width - 40;
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 20, ModernUiGetString (ModernUiStringBootInstruction), Theme->MutedText, Theme->Surface);

  BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);
  if (BootOptions == NULL) {
    ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 66, ModernUiGetString (ModernUiStringNoBootOptions), Theme->Warning, Theme->Surface);
    return;
  }

  VisibleIndex = 0;
  for (Index = 0; (Index < BootOptionCount) && (VisibleIndex < MAX_BOOT_ROWS); Index++) {
    if (!IsVisibleBootOption (&BootOptions[Index])) {
      continue;
    }

    Y           = Panel.Y + 62 + VisibleIndex * 38;
    Description = (BootOptions[Index].Description != NULL) ? BootOptions[Index].Description : ModernUiGetString (ModernUiStringNoDescription);
    State       = ((BootOptions[Index].Attributes & LOAD_OPTION_ACTIVE) != 0) ? ModernUiGetString (ModernUiStringActive) : ModernUiGetString (ModernUiStringInactive);
    IsSelected  = (BOOLEAN)((Focus == SetupFocusContent) && (VisibleIndex == Selected));
    RowBackground = ModernUiGetSelectableRowBackground (IsSelected, FALSE, FALSE, FALSE, Theme);
    UnicodeSPrint (
      Line,
      sizeof (Line),
      L"%02u  Boot%04x  %s  %s",
      VisibleIndex + 1,
      BootOptions[Index].OptionNumber,
      State,
      Description
      );
    ModernUiDrawSelectableRow (
      Ui,
      (MODERN_UI_RECT){ RowX, Y - 8, RowWidth, 32 },
      IsSelected,
      FALSE,
      FALSE,
      FALSE,
      Theme
      );

    ModernUiDrawTextFit (
      Ui,
      RowX + 16,
      Y,
      RowWidth - 32,
      Line,
      IsSelected ? Theme->Text : Theme->MutedText,
      RowBackground
      );
    VisibleIndex++;
  }

  if (VisibleIndex == 0) {
    ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 66, ModernUiGetString (ModernUiStringNoBootOptions), Theme->Warning, Theme->Surface);
  }

  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
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
  EFI_HANDLE                *Handles;
  UINTN                     HandleCount;
  UINTN                     Index;
  UINTN                     Shown;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  CHAR16                    *Text;
  CHAR16                    Line[168];
  BOOLEAN                   IsSelected;
  MODERN_UI_RECT            Panel;
  UINTN                     RowX;
  UINTN                     RowWidth;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL RowBackground;

  Panel = ContentRect (Ui);
  RowX = Panel.X + 20;
  RowWidth = Panel.Width - 40;
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);

  Status = gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &HandleCount, &Handles);
  if (EFI_ERROR (Status)) {
    ModernUiDrawText (Ui, 280, 150, ModernUiGetString (ModernUiStringUnableEnumerateHandles), Theme->Warning, Theme->Surface);
    return;
  }

  ModernUiDrawTextFormatted (Ui, Panel.X + 20, Panel.Y + 20, Theme->MutedText, Theme->Surface, ModernUiGetString (ModernUiStringHandleCountFormat), HandleCount);

  Shown = 0;
  for (Index = 0; (Index < HandleCount) && (Shown < 8); Index++) {
    DevicePath = DevicePathFromHandle (Handles[Index]);
    if (DevicePath == NULL) {
      continue;
    }

    Text = ConvertDevicePathToText (DevicePath, TRUE, TRUE);
    if (Text == NULL) {
      continue;
    }

    UnicodeSPrint (Line, sizeof (Line), L"%02u  %s", Shown + 1, Text);
    IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && (Shown == Selected));
    RowBackground = ModernUiGetSelectableRowBackground (IsSelected, FALSE, FALSE, FALSE, Theme);
    ModernUiDrawSelectableRow (
      Ui,
      (MODERN_UI_RECT){ RowX, Panel.Y + 54 + Shown * 36, RowWidth, 30 },
      IsSelected,
      FALSE,
      FALSE,
      FALSE,
      Theme
      );

    ModernUiDrawTextFit (
      Ui,
      RowX + 16,
      Panel.Y + 62 + Shown * 36,
      RowWidth - 32,
      Line,
      IsSelected ? Theme->Text : Theme->MutedText,
      RowBackground
      );
    FreePool (Text);
    Shown++;
  }

  FreePool (Handles);
}

/**
  Return an allocated HII string for the active ModernSetup language.

  DriverSample ships English strings, while ModernSetup defaults to zh-Hans.
  The lookup therefore falls back to en-US before returning Fallback.

  @param[in] HiiHandle  HII package-list handle. Must not be NULL.
  @param[in] StringId   String token to resolve.
  @param[in] Fallback   Fallback string when HII lookup fails. Must not be NULL.

  @return Allocated HII string owned by the caller, or Fallback when lookup
          fails. The caller must only free the return value when it differs
          from Fallback.
**/
STATIC
CONST CHAR16 *
GetHiiDisplayString (
  IN EFI_HII_HANDLE  HiiHandle,
  IN EFI_STRING_ID   StringId,
  IN CONST CHAR16    *Fallback
  )
{
  CHAR16  *Text;

  if ((HiiHandle == NULL) || (StringId == 0)) {
    return Fallback;
  }

  Text = HiiGetString (HiiHandle, StringId, ModernUiGetLanguage ());
  if (Text == NULL) {
    Text = HiiGetString (HiiHandle, StringId, "en-US");
  }

  if (Text == NULL) {
    Text = HiiGetString (HiiHandle, StringId, "en");
  }

  return (Text == NULL) ? Fallback : Text;
}

/**
  Free a HII display string when it was allocated by HiiGetString().

  @param[in] Text      String returned by GetHiiDisplayString(). May be NULL.
  @param[in] Fallback  Fallback pointer passed to GetHiiDisplayString().
**/
STATIC
VOID
FreeHiiDisplayString (
  IN CONST CHAR16  *Text,
  IN CONST CHAR16  *Fallback
  )
{
  if ((Text != NULL) && (Text != Fallback)) {
    FreePool ((VOID *)Text);
  }
}

/**
  Return a display string for one HII item value.

  @param[in] FormSet  Formset that owns Item. Must not be NULL.
  @param[in] Item     Item to describe. Must not be NULL.
  @param[out] Buffer  Output text buffer. Must not be NULL.
  @param[in] Size     Size of Buffer in bytes.
**/
STATIC
VOID
FormatHiiItemValue (
  IN  MODERN_UI_HII_FORMSET  *FormSet,
  IN  MODERN_UI_HII_ITEM     *Item,
  OUT CHAR16                 *Buffer,
  IN  UINTN                  Size
  )
{
  UINTN         Index;
  CONST CHAR16  *OptionText;
  CONST CHAR16  *Fallback;

  if ((FormSet == NULL) || (Item == NULL) || (Buffer == NULL) || (Size < sizeof (CHAR16))) {
    return;
  }

  Buffer[0] = L'\0';
  if (Item->Unsupported) {
    StrCpyS (Buffer, Size / sizeof (CHAR16), ModernUiGetString (ModernUiStringHiiUnsupported));
    return;
  }

  if (Item->Disabled || Item->GrayOut) {
    if (Item->Reason == ModernUiHiiReasonUnsupportedCondition) {
      StrCpyS (Buffer, Size / sizeof (CHAR16), L"condition unsupported");
    } else if (Item->Reason == ModernUiHiiReasonGrayOut) {
      StrCpyS (Buffer, Size / sizeof (CHAR16), L"grayed");
    } else {
      StrCpyS (Buffer, Size / sizeof (CHAR16), L"disabled");
    }

    return;
  }

  switch (Item->Type) {
    case ModernUiHiiItemCheckbox:
      StrCpyS (Buffer, Size / sizeof (CHAR16), (Item->CurrentValue != 0) ? ModernUiGetString (ModernUiStringEnabled) : ModernUiGetString (ModernUiStringDisabled));
      break;
    case ModernUiHiiItemOneOf:
      for (Index = 0; Index < Item->OptionCount; Index++) {
        if (Item->Options[Index].Value == Item->CurrentValue) {
          Fallback   = L"(option)";
          OptionText = GetHiiDisplayString (FormSet->HiiHandle, Item->Options[Index].PromptId, Fallback);
          StrnCpyS (Buffer, Size / sizeof (CHAR16), OptionText, (Size / sizeof (CHAR16)) - 1);
          FreeHiiDisplayString (OptionText, Fallback);
          return;
        }
      }

      UnicodeSPrint (Buffer, Size, L"0x%lx", Item->CurrentValue);
      break;
    case ModernUiHiiItemNumeric:
      if ((Item->NumericFlags & EFI_IFR_DISPLAY) == EFI_IFR_DISPLAY_UINT_HEX) {
        UnicodeSPrint (Buffer, Size, L"0x%lx", Item->CurrentValue);
      } else {
        UnicodeSPrint (Buffer, Size, L"%lu", Item->CurrentValue);
      }
      break;
    case ModernUiHiiItemString:
    case ModernUiHiiItemPassword:
      StrCpyS (Buffer, Size / sizeof (CHAR16), ModernUiGetString (ModernUiStringHiiReadOnly));
      break;
    case ModernUiHiiItemOrderedList:
      UnicodeSPrint (Buffer, Size, L"%u options - read-only", Item->OptionCount);
      break;
    case ModernUiHiiItemDate:
      StrCpyS (Buffer, Size / sizeof (CHAR16), L"date - read-only");
      break;
    case ModernUiHiiItemTime:
      StrCpyS (Buffer, Size / sizeof (CHAR16), L"time - read-only");
      break;
    case ModernUiHiiItemAction:
      StrCpyS (Buffer, Size / sizeof (CHAR16), Item->CallbackRequired ? L"action" : ModernUiGetString (ModernUiStringHiiReadOnly));
      break;
    case ModernUiHiiItemResetButton:
      StrCpyS (Buffer, Size / sizeof (CHAR16), ModernUiGetString (ModernUiStringHiiReadOnly));
      break;
    case ModernUiHiiItemRef:
      StrCpyS (Buffer, Size / sizeof (CHAR16), L">");
      break;
    default:
      if (Item->CallbackRequired) {
        StrCpyS (Buffer, Size / sizeof (CHAR16), L"callback");
      } else if (Item->ReadOnly || (Item->Reason == ModernUiHiiReasonUnsupportedStorage)) {
        StrCpyS (Buffer, Size / sizeof (CHAR16), ModernUiGetString (ModernUiStringHiiReadOnly));
      }
      break;
  }
}

/**
  Draw one selectable HII bridge row.

  @param[in] Ui          Initialized render context. Must not be NULL.
  @param[in] Theme       Theme token table. Must not be NULL.
  @param[in] Panel       Content panel rectangle.
  @param[in] Row         Zero-based visible row index.
  @param[in] Selected    TRUE when the row is selected.
  @param[in] Disabled    TRUE when the row is visible but not actionable.
  @param[in] Action      TRUE when the row represents a callback action.
  @param[in] Primary     Primary row text. Must not be NULL.
  @param[in] Secondary   Optional secondary text. May be NULL.
**/
STATIC
VOID
DrawHiiRow (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN MODERN_UI_RECT            Panel,
  IN UINTN                     Row,
  IN BOOLEAN                   Selected,
  IN BOOLEAN                   Disabled,
  IN BOOLEAN                   Action,
  IN CONST CHAR16              *Primary,
  IN CONST CHAR16              *Secondary
  )
{
  UINTN       RowX;
  UINTN       RowWidth;
  UINTN       Y;
  CHAR16      Line[192];
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  TextColor;

  RowX     = Panel.X + 20;
  RowWidth = Panel.Width - 40;
  Y        = Panel.Y + 62 + Row * 36;

  if ((Secondary != NULL) && (Secondary[0] != L'\0')) {
    UnicodeSPrint (Line, sizeof (Line), L"%s  -  %s", Primary, Secondary);
  } else {
    StrnCpyS (Line, ARRAY_SIZE (Line), Primary, ARRAY_SIZE (Line) - 1);
  }

  Background = ModernUiGetSelectableRowBackground (Selected, Disabled, Action, FALSE, Theme);
  TextColor  = Disabled ? Theme->Border : (Selected ? Theme->Text : (Action ? Theme->Text : Theme->MutedText));
  ModernUiDrawSelectableRow (
    Ui,
    (MODERN_UI_RECT){ RowX, Y - 8, RowWidth, 30 },
    Selected,
    Disabled,
    Action,
    FALSE,
    Theme
    );

  ModernUiDrawTextFit (Ui, RowX + 16, Y, RowWidth - 32, Line, TextColor, Background);
}

/**
  Draw DriverSample HII bridge content.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawHiiBridge (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_UI_RECT          Panel;
  UINTN                   Index;
  BOOLEAN                 IsSelected;
  MODERN_UI_HII_FORMSET   *FormSet;
  MODERN_UI_HII_FORM      *Form;
  MODERN_UI_HII_ITEM      *Item;
  CONST CHAR16            *Title;
  CONST CHAR16            *Fallback;
  CHAR16                  Value[96];
  CHAR16                  Breadcrumb[192];
  UINTN                   Row;
  CONST MODERN_UI_PAGE_ADAPTER  *Adapter;
  MODERN_UI_PAGE_CONTEXT        AdapterContext;

  Panel = ContentRect (Ui);
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 20, ModernUiGetString (ModernUiStringHiiEnterForm), Theme->MutedText, Theme->Surface);

  if (mHiiModel.FormSetCount == 0) {
    ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 66, ModernUiGetString (ModernUiStringHiiNoFormsets), Theme->Warning, Theme->Surface);
    return;
  }

  if (mHiiView.Level == HiiViewFormSets) {
    for (Index = 0; (Index < mHiiModel.FormSetCount) && (Index < MAX_HII_ROWS); Index++) {
      FormSet    = &mHiiModel.FormSets[Index];
      IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && (Index == mHiiView.Selection));
      Fallback   = ModernUiGetString (ModernUiStringHiiFormsets);
      Title      = GetHiiDisplayString (FormSet->HiiHandle, FormSet->TitleId, Fallback);
      UnicodeSPrint (Value, sizeof (Value), L"%u forms", FormSet->FormCount);
      DrawHiiRow (Ui, Theme, Panel, Index, IsSelected, FALSE, FALSE, Title, Value);
      FreeHiiDisplayString (Title, Fallback);
    }

    return;
  }

  if (mHiiView.FormSetIndex >= mHiiModel.FormSetCount) {
    return;
  }

  FormSet = &mHiiModel.FormSets[mHiiView.FormSetIndex];
  if (mHiiView.Level == HiiViewForms) {
    for (Index = 0; (Index < FormSet->FormCount) && (Index < MAX_HII_ROWS); Index++) {
      Form       = &FormSet->Forms[Index];
      IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && (Index == mHiiView.Selection));
      Fallback   = ModernUiGetString (ModernUiStringHiiForms);
      Title      = GetHiiDisplayString (FormSet->HiiHandle, Form->TitleId, Fallback);
      UnicodeSPrint (Value, sizeof (Value), L"%u visible / %u items", GetVisibleHiiItemCount (Form), Form->ItemCount);
      DrawHiiRow (Ui, Theme, Panel, Index, IsSelected, FALSE, FALSE, Title, Value);
      FreeHiiDisplayString (Title, Fallback);
    }

    return;
  }

  if (mHiiView.FormIndex >= FormSet->FormCount) {
    return;
  }

  Form = &FormSet->Forms[mHiiView.FormIndex];
  Adapter = ModernUiFindPageAdapterByGuid (&FormSet->Guid, FormSet);
  if ((Adapter != NULL) && (Adapter->Draw != NULL)) {
    ZeroMem (&AdapterContext, sizeof (AdapterContext));
    AdapterContext.Version           = MODERN_UI_PAGE_ADAPTER_VERSION;
    AdapterContext.Ui                = Ui;
    AdapterContext.Theme             = Theme;
    AdapterContext.HiiModel          = &mHiiModel;
    AdapterContext.FormSet           = FormSet;
    AdapterContext.Form              = Form;
    AdapterContext.FormId            = Form->FormId;
    AdapterContext.Selection         = mHiiView.Selection;
    AdapterContext.Scroll            = mHiiView.Scroll;
    AdapterContext.HasFocus          = (BOOLEAN)(Focus == SetupFocusContent);
    if (!EFI_ERROR (Adapter->Draw (Adapter, &AdapterContext))) {
      return;
    }
  }

  Fallback = ModernUiGetString (ModernUiStringHiiForms);
  Title    = GetHiiDisplayString (FormSet->HiiHandle, Form->TitleId, Fallback);
  UnicodeSPrint (Breadcrumb, sizeof (Breadcrumb), L"%s / %s", ModernUiGetString (ModernUiStringHiiForms), Title);
  ModernUiDrawTextFit (Ui, Panel.X + 20, Panel.Y + 42, Panel.Width - 40, Breadcrumb, Theme->MutedText, Theme->Surface);
  FreeHiiDisplayString (Title, Fallback);

  for (Row = 0, Index = mHiiView.Scroll; Row < MAX_HII_ROWS; Index++) {
    Item = GetVisibleHiiItem (Form, Index);
    if (Item == NULL) {
      break;
    }

    IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && (Index == mHiiView.Selection));
    Fallback   = ModernUiGetString (ModernUiStringHiiItems);
    Title      = GetHiiDisplayString (FormSet->HiiHandle, Item->PromptId, Fallback);
    FormatHiiItemValue (FormSet, Item, Value, sizeof (Value));
    DrawHiiRow (
      Ui,
      Theme,
      Panel,
      Row,
      IsSelected,
      (BOOLEAN)(Item->Disabled || Item->GrayOut || Item->ReadOnly || Item->Unsupported),
      (BOOLEAN)(Item->Type == ModernUiHiiItemAction),
      Title,
      Value
      );
    FreeHiiDisplayString (Title, Fallback);
    Row++;
  }
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
  BOOLEAN  SecureBoot;
  MODERN_UI_RECT  Panel;

  SecureBoot = GetSecureBootEnabled ();
  Panel = ContentRect (Ui);
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 24, ModernUiGetString (ModernUiStringSecureBoot), Theme->MutedText, Theme->Surface);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 64, SecureBoot ? ModernUiGetString (ModernUiStringEnabled) : ModernUiGetString (ModernUiStringDisabled), SecureBoot ? Theme->Success : Theme->Warning, Theme->Surface);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 116, ModernUiGetString (ModernUiStringSecurityReadOnly), Theme->MutedText, Theme->Surface);
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
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  RowBackground;

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
    RowBackground = ModernUiGetSelectableRowBackground (IsSelected, FALSE, TRUE, FALSE, Theme);
    ModernUiDrawSelectableRow (
      Ui,
      (MODERN_UI_RECT){ RowX, Y - 10, RowWidth, 40 },
      IsSelected,
      FALSE,
      TRUE,
      FALSE,
      Theme
      );
    ModernUiDrawSelectableRowBorder (Ui, (MODERN_UI_RECT){ RowX, Y - 10, RowWidth, 40 }, IsSelected, Theme);

    if (Index == 3) {
      ModernUiDrawText (Ui, RowX + 20, Y, Items[Index], IsSelected ? Theme->Text : Theme->MutedText, RowBackground);
      ModernUiDrawValueBox (
        Ui,
        (MODERN_UI_RECT){ RowX + RowWidth - ValueWidth - 12, Y - 4, ValueWidth, 28 },
        LanguageName,
        IsSelected,
        Theme
        );
    } else {
      ModernUiDrawText (Ui, RowX + 20, Y, (CHAR16 *)Items[Index], IsSelected ? Theme->Text : Theme->MutedText, RowBackground);
    }
  }

  if (mLanguageDropdownOpen) {
    UINTN  DropdownY;
    UINTN  DropdownX;
    UINTN  Option;

    DropdownX = RowX + RowWidth - ValueWidth - 12;
    DropdownY = Panel.Y + 72 + 4 * 54 - 8;
    ModernUiDrawDropdownFrame (Ui, (MODERN_UI_RECT){ DropdownX, DropdownY, ValueWidth, 80 }, Theme);

    for (Option = 0; Option < 2; Option++) {
      IsSelected = (BOOLEAN)(Option == mLanguageDropdownSelection);
      RowBackground = ModernUiGetSelectableRowBackground (IsSelected, FALSE, FALSE, FALSE, Theme);
      ModernUiDrawSelectableRow (
        Ui,
        (MODERN_UI_RECT){ DropdownX + 6, DropdownY + 7 + Option * 34, ValueWidth - 12, 30 },
        IsSelected,
        FALSE,
        FALSE,
        FALSE,
        Theme
        );
      ModernUiDrawText (
        Ui,
        DropdownX + 20,
        DropdownY + 14 + Option * 34,
        GetLanguageOptionName (Option),
        IsSelected ? Theme->Text : Theme->MutedText,
        RowBackground
        );
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
  Navigate one level back in the HII bridge view.

  @retval TRUE   The HII view consumed the back action.
  @retval FALSE  The HII view was already at the top level.
**/
STATIC
BOOLEAN
HiiViewBack (
  VOID
  )
{
  if (mHiiView.Level == HiiViewItems) {
    ModernUiHiiBridgeNotifyForm (&mHiiModel, mHiiView.FormSetIndex, mHiiView.FormIndex, FALSE, NULL);
    mHiiView.Level     = HiiViewForms;
    mHiiView.Selection = mHiiView.FormIndex;
    mHiiView.Scroll    = 0;
    return TRUE;
  }

  if (mHiiView.Level == HiiViewForms) {
    mHiiView.Level     = HiiViewFormSets;
    mHiiView.Selection = mHiiView.FormSetIndex;
    mHiiView.Scroll    = 0;
    return TRUE;
  }

  return FALSE;
}

/**
  Convert a callback action request into ModernSetup navigation feedback.

  @param[in]     Request        Browser action request returned by callback.
  @param[out]    StatusMessage  Status text buffer. Must not be NULL.
  @param[in]     StatusSize     Size of StatusMessage in bytes.

  @retval TRUE   Request changed HII navigation state.
  @retval FALSE  Request did not require navigation.
**/
STATIC
BOOLEAN
HandleHiiActionRequest (
  IN  EFI_BROWSER_ACTION_REQUEST  Request,
  OUT CHAR16                      *StatusMessage,
  IN  UINTN                       StatusSize
  )
{
  if ((StatusMessage == NULL) || (StatusSize < sizeof (CHAR16))) {
    return FALSE;
  }

  switch (Request) {
    case EFI_BROWSER_ACTION_REQUEST_EXIT:
      UnicodeSPrint (StatusMessage, StatusSize, L"Callback requested exit");
      return HiiViewBack ();
    case EFI_BROWSER_ACTION_REQUEST_FORM_SUBMIT_EXIT:
      UnicodeSPrint (StatusMessage, StatusSize, L"Callback requested submit and exit");
      return HiiViewBack ();
    case EFI_BROWSER_ACTION_REQUEST_FORM_DISCARD_EXIT:
      UnicodeSPrint (StatusMessage, StatusSize, L"Callback requested discard and exit");
      return HiiViewBack ();
    case EFI_BROWSER_ACTION_REQUEST_SUBMIT:
      UnicodeSPrint (StatusMessage, StatusSize, L"Callback requested submit");
      break;
    case EFI_BROWSER_ACTION_REQUEST_FORM_APPLY:
      UnicodeSPrint (StatusMessage, StatusSize, L"Callback requested form apply");
      break;
    case EFI_BROWSER_ACTION_REQUEST_FORM_DISCARD:
      UnicodeSPrint (StatusMessage, StatusSize, L"Callback requested form discard");
      break;
    case EFI_BROWSER_ACTION_REQUEST_QUESTION_APPLY:
      UnicodeSPrint (StatusMessage, StatusSize, L"Callback requested question apply");
      break;
    case EFI_BROWSER_ACTION_REQUEST_RESET:
      UnicodeSPrint (StatusMessage, StatusSize, L"Callback requested reset");
      break;
    case EFI_BROWSER_ACTION_REQUEST_RECONNECT:
      UnicodeSPrint (StatusMessage, StatusSize, L"Callback requested reconnect");
      break;
    default:
      break;
  }

  return FALSE;
}

/**
  Find a form by form ID in the current HII formset.

  @param[in]  FormSet       Formset to search. Must not be NULL.
  @param[in]  FormId        Form ID to locate.
  @param[out] FormIndex     Receives the form index. Must not be NULL.

  @retval TRUE   Matching form was found.
  @retval FALSE  No matching form exists.
**/
STATIC
BOOLEAN
FindHiiFormIndex (
  IN  MODERN_UI_HII_FORMSET  *FormSet,
  IN  EFI_FORM_ID            FormId,
  OUT UINTN                  *FormIndex
  )
{
  UINTN  Index;

  if ((FormSet == NULL) || (FormIndex == NULL)) {
    return FALSE;
  }

  for (Index = 0; Index < FormSet->FormCount; Index++) {
    if (FormSet->Forms[Index].FormId == FormId) {
      *FormIndex = Index;
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Execute Enter on the current HII bridge selection.

  @param[out] StatusMessage  Status buffer for user-visible feedback. Must not
                             be NULL.
  @param[in]  StatusSize     Size of StatusMessage in bytes.
**/
STATIC
VOID
HiiViewEnter (
  OUT CHAR16  *StatusMessage,
  IN  UINTN   StatusSize
  )
{
  MODERN_UI_HII_FORMSET  *FormSet;
  MODERN_UI_HII_FORM     *Form;
  MODERN_UI_HII_ITEM     *Item;
  EFI_STATUS             Status;
  EFI_BROWSER_ACTION_REQUEST  Request;
  EFI_IFR_TYPE_VALUE      CallbackValue;
  UINTN                  TargetFormIndex;
  UINTN                  ItemIndex;

  if ((StatusMessage == NULL) || (StatusSize < sizeof (CHAR16))) {
    return;
  }

  StatusMessage[0] = L'\0';
  if (mHiiView.Level == HiiViewFormSets) {
    if (mHiiView.Selection < mHiiModel.FormSetCount) {
      mHiiView.FormSetIndex = mHiiView.Selection;
      mHiiView.Level        = HiiViewForms;
      mHiiView.Selection    = 0;
      mHiiView.Scroll       = 0;
    }

    return;
  }

  if (mHiiView.FormSetIndex >= mHiiModel.FormSetCount) {
    return;
  }

  FormSet = &mHiiModel.FormSets[mHiiView.FormSetIndex];
  if (mHiiView.Level == HiiViewForms) {
    if (mHiiView.Selection < FormSet->FormCount) {
      mHiiView.FormIndex = mHiiView.Selection;
      mHiiView.Level     = HiiViewItems;
      mHiiView.Selection = 0;
      mHiiView.Scroll    = 0;
      Status = ModernUiHiiBridgeNotifyForm (&mHiiModel, mHiiView.FormSetIndex, mHiiView.FormIndex, TRUE, &Request);
      if (EFI_ERROR (Status)) {
        UnicodeSPrint (StatusMessage, StatusSize, L"FORM_OPEN callback returned: %r", Status);
      } else if (Request != EFI_BROWSER_ACTION_REQUEST_NONE) {
        HandleHiiActionRequest (Request, StatusMessage, StatusSize);
      }

      LoadHiiModel ();
    }

    return;
  }

  if (mHiiView.FormIndex >= FormSet->FormCount) {
    return;
  }

  Form = &FormSet->Forms[mHiiView.FormIndex];
  if (!GetVisibleHiiItemIndex (Form, mHiiView.Selection, &ItemIndex)) {
    return;
  }

  Item = &Form->Items[ItemIndex];
  if (Item->CallbackRequired) {
    Request = EFI_BROWSER_ACTION_REQUEST_NONE;
    ZeroMem (&CallbackValue, sizeof (CallbackValue));
    Status = ModernUiHiiBridgeRunCallback (
               &mHiiModel,
               mHiiView.FormSetIndex,
               mHiiView.FormIndex,
               ItemIndex,
               &CallbackValue,
               &Request
               );
    if (EFI_ERROR (Status)) {
      UnicodeSPrint (StatusMessage, StatusSize, L"Callback returned: %r", Status);
      return;
    }

    if (HandleHiiActionRequest (Request, StatusMessage, StatusSize)) {
      LoadHiiModel ();
      return;
    }

    LoadHiiModel ();
    FormSet = &mHiiModel.FormSets[mHiiView.FormSetIndex];
    if (mHiiView.FormIndex >= FormSet->FormCount) {
      return;
    }

    Form = &FormSet->Forms[mHiiView.FormIndex];
    if (!GetVisibleHiiItemIndex (Form, mHiiView.Selection, &ItemIndex)) {
      return;
    }

    Item = &Form->Items[ItemIndex];
    if ((CallbackValue.ref.FormId != 0) && (Item->Type == ModernUiHiiItemRef)) {
      Item->TargetFormId = CallbackValue.ref.FormId;
    }
  }

  if ((Item->Type == ModernUiHiiItemRef) && FindHiiFormIndex (FormSet, Item->TargetFormId, &TargetFormIndex)) {
    ModernUiHiiBridgeNotifyForm (&mHiiModel, mHiiView.FormSetIndex, mHiiView.FormIndex, FALSE, NULL);
    mHiiView.FormIndex = TargetFormIndex;
    mHiiView.Selection = 0;
    mHiiView.Scroll    = 0;
    Status = ModernUiHiiBridgeNotifyForm (&mHiiModel, mHiiView.FormSetIndex, mHiiView.FormIndex, TRUE, &Request);
    if (EFI_ERROR (Status)) {
      UnicodeSPrint (StatusMessage, StatusSize, L"FORM_OPEN callback returned: %r", Status);
    } else if (Request != EFI_BROWSER_ACTION_REQUEST_NONE) {
      HandleHiiActionRequest (Request, StatusMessage, StatusSize);
    }

    LoadHiiModel ();
    return;
  }

  if (Item->CallbackRequired || (Item->Type == ModernUiHiiItemAction) || (Item->Type == ModernUiHiiItemResetButton)) {
    return;
  }

  Status = ModernUiHiiBridgeApplyNextValue (&mHiiModel, mHiiView.FormSetIndex, mHiiView.FormIndex, ItemIndex);
  if (EFI_ERROR (Status)) {
    UnicodeSPrint (StatusMessage, StatusSize, ModernUiGetString (ModernUiStringHiiRouteReturnedFormat), Status);
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
    case PageHii:
      DrawHiiBridge (Ui, Theme, Focus);
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
  LoadHiiModel ();
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
        } else if ((Focus == SetupFocusContent) && (Page == PageHii) && HiiViewBack ()) {
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
        } else if (Page == PageHii) {
          HiiViewEnter (StatusMessage, sizeof (StatusMessage));
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
