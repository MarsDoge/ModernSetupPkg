/** @file
  Modern graphical setup application prototype.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/GlobalVariable.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/LoadedImage.h>

#include <ModernUi/ModernUiInput.h>
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

/**
  Blend two colors by percentage weight.

  @param[in] Base    Base color used when Weight is 0.
  @param[in] Accent  Accent color used when Weight is 100.
  @param[in] Weight  Accent weight in percent. Values above 100 are not expected.

  @return Blended color with Reserved cleared to zero.
**/
STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
BlendAccent (
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Base,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Accent,
  IN UINT8                          Weight
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Result;

  Result.Red      = (UINT8)(((UINTN)Base.Red * (100 - Weight) + (UINTN)Accent.Red * Weight) / 100);
  Result.Green    = (UINT8)(((UINTN)Base.Green * (100 - Weight) + (UINTN)Accent.Green * Weight) / 100);
  Result.Blue     = (UINT8)(((UINTN)Base.Blue * (100 - Weight) + (UINTN)Accent.Blue * Weight) / 100);
  Result.Reserved = 0;
  return Result;
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
  Draw text after truncating it to a caller-provided pixel width.

  @param[in] Ui          Initialized render context. Must not be NULL.
  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] Width       Maximum text width in pixels.
  @param[in] Text        Null-terminated UCS-2 string. Must not be NULL.
  @param[in] Color       Text foreground color.
  @param[in] Background  Text background color.
**/
STATIC
VOID
DrawTextFit (
  IN MODERN_UI_RENDER_CONTEXT       *Ui,
  IN UINTN                          X,
  IN UINTN                          Y,
  IN UINTN                          Width,
  IN CONST CHAR16                   *Text,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background
  )
{
  CHAR16  Buffer[176];
  CHAR16  Character[2];
  UINTN   Index;
  UINTN   CopyChars;
  UINTN   CurrentWidth;
  UINTN   CharacterWidth;
  UINTN   EllipsisWidth;
  UINTN   TargetWidth;

  if ((Ui == NULL) || (Text == NULL) || (Width == 0)) {
    return;
  }

  if (ModernUiMeasureText (Text) <= Width) {
    ModernUiDrawText (Ui, X, Y, Text, Color, Background);
    return;
  }

  EllipsisWidth = ModernUiMeasureText (L"...");
  TargetWidth   = (Width > EllipsisWidth) ? (Width - EllipsisWidth) : Width;
  CopyChars     = 0;
  CurrentWidth  = 0;
  Character[1]  = L'\0';
  for (Index = 0; (Text[Index] != L'\0') && (CopyChars < ARRAY_SIZE (Buffer) - 4); Index++) {
    Character[0]  = Text[Index];
    CharacterWidth = ModernUiMeasureText (Character);
    if ((CurrentWidth + CharacterWidth) > TargetWidth) {
      break;
    }

    Buffer[CopyChars++] = Text[Index];
    CurrentWidth       += CharacterWidth;
  }

  if ((Width >= EllipsisWidth) && (CopyChars < ARRAY_SIZE (Buffer) - 3)) {
    Buffer[CopyChars++] = L'.';
    Buffer[CopyChars++] = L'.';
    Buffer[CopyChars++] = L'.';
  }

  Buffer[CopyChars] = L'\0';
  ModernUiDrawText (Ui, X, Y, Buffer, Color, Background);
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
      return 3;
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
  Format and draw a single text line.

  @param[in] Ui          Initialized render context. Must not be NULL.
  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] Color       Text foreground color.
  @param[in] Background  Text background color.
  @param[in] Format      PrintLib format string. Must not be NULL.
  @param[in] ...         Format arguments.
**/
STATIC
VOID
DrawTextF (
  IN MODERN_UI_RENDER_CONTEXT       *Ui,
  IN UINTN                          X,
  IN UINTN                          Y,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background,
  IN CONST CHAR16                   *Format,
  ...
  )
{
  VA_LIST  Marker;
  CHAR16   Buffer[192];

  VA_START (Marker, Format);
  UnicodeVSPrint (Buffer, sizeof (Buffer), Format, Marker);
  VA_END (Marker);

  ModernUiDrawText (Ui, X, Y, Buffer, Color, Background);
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

  Header = BlendAccent (Theme->Surface, Theme->AccentSoft, 35);
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 0, 0, Ui->Width, TOP_BAR_HEIGHT }, Header);
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 0, TOP_BAR_HEIGHT - 1, Ui->Width, 1 }, Theme->Accent);
  ModernUiDrawText (Ui, SCREEN_MARGIN, 15, ModernUiGetString (ModernUiStringHeaderTitle), Theme->Text, Header);
  ModernUiDrawText (Ui, Ui->Width / 2 - 72, 15, ModernUiGetString (ModernUiStringHeaderMode), Theme->Accent, Header);
  DrawTextF (Ui, Ui->Width - 244, 15, Theme->MutedText, Header, L"AARCH64  %ux%u", Ui->Width, Ui->Height);
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

  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 0, TOP_BAR_HEIGHT, Ui->Width, TAB_BAR_HEIGHT }, Theme->Surface);
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 0, TOP_BAR_HEIGHT + TAB_BAR_HEIGHT - 1, Ui->Width, 1 }, Theme->Border);

  TabWidth = (Ui->Width - (SCREEN_MARGIN * 2)) / ARRAY_SIZE (mPages);
  for (Index = 0; Index < ARRAY_SIZE (mPages); Index++) {
    X        = SCREEN_MARGIN + (Index * TabWidth);
    TabColor = (mPages[Index].Page == Page) ? Theme->AccentSoft : Theme->Surface;
    TextColor = (mPages[Index].Page == Page) ? Theme->Text : Theme->MutedText;

    if (mPages[Index].Page == Page) {
      ModernUiFillRect (Ui, (MODERN_UI_RECT){ X, TOP_BAR_HEIGHT + 9, TabWidth - 8, 36 }, TabColor);
      ModernUiFillRect (
        Ui,
        (MODERN_UI_RECT){ X, TOP_BAR_HEIGHT + 43, TabWidth - 8, 3 },
        (Focus == SetupFocusNav) ? Theme->Accent : Theme->Border
        );
    }

    ModernUiDrawText (Ui, X + 12, TOP_BAR_HEIGHT + 20, ModernUiGetString (mPages[Index].Title), TextColor, TabColor);
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
  Draw a focus border around a content rectangle when content has focus.

  @param[in] Ui        Initialized render context. Must not be NULL.
  @param[in] Theme     Theme token table. Must not be NULL.
  @param[in] Rect      Content rectangle.
  @param[in] HasFocus  TRUE when content focus should be visible.
**/
STATIC
VOID
DrawContentFocus (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN MODERN_UI_RECT            Rect,
  IN BOOLEAN                   HasFocus
  )
{
  if (HasFocus) {
    ModernUiStrokeRect (Ui, Rect, Theme->Accent);
  }
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
  Draw a compact information card.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] X      Left coordinate in pixels.
  @param[in] Y      Top coordinate in pixels.
  @param[in] W      Card width in pixels.
  @param[in] Title  Card title. Must not be NULL.
  @param[in] Value  Card value. Must not be NULL.
**/
STATIC
VOID
DrawInfoCard (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN UINTN                     X,
  IN UINTN                     Y,
  IN UINTN                     W,
  IN CONST CHAR16              *Title,
  IN CONST CHAR16              *Value
  )
{
  ModernUiDrawPanel (Ui, (MODERN_UI_RECT){ X, Y, W, 92 }, Theme);
  ModernUiDrawText (Ui, X + 18, Y + 16, (CHAR16 *)Title, Theme->MutedText, Theme->Surface);
  ModernUiDrawText (Ui, X + 18, Y + 48, (CHAR16 *)Value, Theme->Text, Theme->Surface);
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

  DrawInfoCard (Ui, Theme, Content.X, Content.Y, CardWidth, ModernUiGetString (ModernUiStringFirmwareVendor), gST->FirmwareVendor);
  DrawInfoCard (Ui, Theme, Content.X + CardWidth + CARD_GAP, Content.Y, CardWidth, ModernUiGetString (ModernUiStringFirmwareRevision), L"edk2 / ArmVirt");
  DrawInfoCard (Ui, Theme, Content.X, Content.Y + 108, CardWidth, ModernUiGetString (ModernUiStringDisplay), Resolution);
  DrawInfoCard (Ui, Theme, Content.X + CardWidth + CARD_GAP, Content.Y + 108, CardWidth, ModernUiGetString (ModernUiStringBootOptions), BootCount);

  ModernUiDrawPanel (Ui, StatusRect, Theme);
  DrawContentFocus (Ui, Theme, StatusRect, (BOOLEAN)(Focus == SetupFocusContent));
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

  Panel = ContentRect (Ui);
  RowX = Panel.X + 20;
  RowWidth = Panel.Width - 40;
  ModernUiDrawPanel (Ui, Panel, Theme);
  DrawContentFocus (Ui, Theme, Panel, (BOOLEAN)(Focus == SetupFocusContent));
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
    UnicodeSPrint (
      Line,
      sizeof (Line),
      L"%02u  Boot%04x  %s  %s",
      VisibleIndex + 1,
      BootOptions[Index].OptionNumber,
      State,
      Description
      );
    ModernUiFillRect (Ui, (MODERN_UI_RECT){ RowX, Y - 8, RowWidth, 32 }, IsSelected ? Theme->AccentSoft : Theme->Surface);
    if (IsSelected) {
      ModernUiFillRect (Ui, (MODERN_UI_RECT){ RowX, Y - 8, 4, 32 }, Theme->Accent);
    }

    DrawTextFit (Ui, RowX + 16, Y, RowWidth - 32, Line, IsSelected ? Theme->Text : Theme->MutedText, IsSelected ? Theme->AccentSoft : Theme->Surface);
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

  Panel = ContentRect (Ui);
  RowX = Panel.X + 20;
  RowWidth = Panel.Width - 40;
  ModernUiDrawPanel (Ui, Panel, Theme);
  DrawContentFocus (Ui, Theme, Panel, (BOOLEAN)(Focus == SetupFocusContent));

  Status = gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &HandleCount, &Handles);
  if (EFI_ERROR (Status)) {
    ModernUiDrawText (Ui, 280, 150, ModernUiGetString (ModernUiStringUnableEnumerateHandles), Theme->Warning, Theme->Surface);
    return;
  }

  DrawTextF (Ui, Panel.X + 20, Panel.Y + 20, Theme->MutedText, Theme->Surface, ModernUiGetString (ModernUiStringHandleCountFormat), HandleCount);

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
    ModernUiFillRect (
      Ui,
      (MODERN_UI_RECT){ RowX, Panel.Y + 54 + Shown * 36, RowWidth, 30 },
      IsSelected ? Theme->AccentSoft : Theme->Surface
      );
    if (IsSelected) {
      ModernUiFillRect (Ui, (MODERN_UI_RECT){ RowX, Panel.Y + 54 + Shown * 36, 4, 30 }, Theme->Accent);
    }

    DrawTextFit (Ui, RowX + 16, Panel.Y + 62 + Shown * 36, RowWidth - 32, Line, IsSelected ? Theme->Text : Theme->MutedText, IsSelected ? Theme->AccentSoft : Theme->Surface);
    FreePool (Text);
    Shown++;
  }

  FreePool (Handles);
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
  DrawContentFocus (Ui, Theme, Panel, (BOOLEAN)(Focus == SetupFocusContent));
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 24, ModernUiGetString (ModernUiStringSecureBoot), Theme->MutedText, Theme->Surface);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 64, SecureBoot ? ModernUiGetString (ModernUiStringEnabled) : ModernUiGetString (ModernUiStringDisabled), SecureBoot ? Theme->Success : Theme->Warning, Theme->Surface);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 116, ModernUiGetString (ModernUiStringSecurityReadOnly), Theme->MutedText, Theme->Surface);
}

/**
  Draw the Exit page and selected action.

  @param[in] Ui        Initialized render context. Must not be NULL.
  @param[in] Theme     Theme token table. Must not be NULL.
  @param[in] Focus     Current focus area.
  @param[in] Selected  Selected action index. Values 0..2 are expected.
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
  CONST CHAR16  *Items[] = {
    ModernUiGetString (ModernUiStringExitContinue),
    ModernUiGetString (ModernUiStringExitClassicUi),
    ModernUiGetString (ModernUiStringExitReset)
  };
  UINTN         Index;
  UINTN         Y;
  BOOLEAN       IsSelected;
  MODERN_UI_RECT  Panel;
  UINTN         RowX;
  UINTN         RowWidth;

  Panel = ContentRect (Ui);
  RowX = Panel.X + 20;
  RowWidth = Panel.Width - 40;
  ModernUiDrawPanel (Ui, Panel, Theme);
  DrawContentFocus (Ui, Theme, Panel, (BOOLEAN)(Focus == SetupFocusContent));
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 20, ModernUiGetString (ModernUiStringExitInstruction), Theme->MutedText, Theme->Surface);

  for (Index = 0; Index < ARRAY_SIZE (Items); Index++) {
    Y = Panel.Y + 76 + Index * 56;
    IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && (Index == Selected));
    ModernUiFillRect (
      Ui,
      (MODERN_UI_RECT){ RowX, Y - 12, RowWidth, 42 },
      IsSelected ? Theme->AccentSoft : Theme->Surface
      );
    if (IsSelected) {
      ModernUiFillRect (Ui, (MODERN_UI_RECT){ RowX, Y - 12, 4, 42 }, Theme->Accent);
    }

    ModernUiDrawText (Ui, RowX + 20, Y, (CHAR16 *)Items[Index], IsSelected ? Theme->Text : Theme->MutedText, IsSelected ? Theme->AccentSoft : Theme->Surface);
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
        if (Focus == SetupFocusContent) {
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
        if (Focus == SetupFocusNav) {
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
        Focus  = (Focus == SetupFocusNav) ? SetupFocusContent : SetupFocusNav;
        Redraw = TRUE;
        break;
      case ModernUiInputLeft:
        if (Focus == SetupFocusNav) {
          Page = (Page == 0) ? (PageMax - 1) : (Page - 1);
        } else {
          Focus = SetupFocusNav;
        }

        StatusMessage[0] = L'\0';
        Redraw = TRUE;
        break;
      case ModernUiInputRight:
        if (Focus == SetupFocusNav) {
          Page = (Page + 1) % PageMax;
        } else {
          StatusMessage[0] = L'\0';
        }

        Redraw = TRUE;
        break;
      case ModernUiInputEscape:
        if (Focus == SetupFocusContent) {
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
        } else if (Page == PageExit) {
          if (ExitSelection == 0) {
            return EFI_SUCCESS;
          } else if (ExitSelection == 1) {
            Status = LaunchUiAppFallback (ImageHandle);
            UnicodeSPrint (StatusMessage, sizeof (StatusMessage), ModernUiGetString (ModernUiStringClassicReturnedFormat), Status);
            Redraw = TRUE;
          } else {
            gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
          }
        }
        break;
      default:
        break;
    }
  }
}
