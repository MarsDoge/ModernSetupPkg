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
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/LoadedImage.h>

#include <ModernUi/ModernUiInput.h>
#include <ModernUi/ModernUiRenderer.h>
#include <ModernUi/ModernUiTheme.h>

#define CARD_GAP       16

STATIC CONST EFI_GUID  mUiAppGuid = { 0x462CAA21, 0x7614, 0x4503, { 0x83, 0x6E, 0x8A, 0xB6, 0xF4, 0x66, 0x23, 0x31 } };

typedef enum {
  PageDashboard = 0,
  PageBoot,
  PageDevices,
  PageSecurity,
  PageExit,
  PageMax
} SETUP_PAGE;

typedef struct {
  SETUP_PAGE    Page;
  CONST CHAR16  *Title;
  CONST CHAR16  *Hint;
} PAGE_DESCRIPTOR;

STATIC CONST PAGE_DESCRIPTOR  mPages[] = {
  { PageDashboard, L"Dashboard", L"Platform overview" },
  { PageBoot,      L"Boot",      L"Boot order and entries" },
  { PageDevices,   L"Devices",   L"Firmware-visible handles" },
  { PageSecurity,  L"Security",  L"Secure Boot state" },
  { PageExit,      L"Exit",      L"Leave setup or reset" }
};

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

STATIC
UINTN
GetBootCount (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       Size;
  UINT16      *BootOrder;
  UINTN       Count;

  Status = ReadGlobalVariable (EFI_BOOT_ORDER_VARIABLE_NAME, (VOID **)&BootOrder, &Size);
  if (EFI_ERROR (Status)) {
    return 0;
  }

  Count = Size / sizeof (UINT16);
  FreePool (BootOrder);
  return Count;
}

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

STATIC
VOID
DrawHeader (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Header;

  Header = BlendAccent (Theme->Surface, Theme->AccentSoft, 35);
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 0, 0, Ui->Width, 54 }, Header);
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 0, 53, Ui->Width, 1 }, Theme->Accent);
  ModernUiDrawText (Ui, 24, 15, L"MODERN UEFI UTILITY", Theme->Text, Header);
  DrawTextF (Ui, Ui->Width - 260, 15, Theme->MutedText, Header, L"AARCH64  %ux%u", Ui->Width, Ui->Height);
}

STATIC
VOID
DrawNav (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_PAGE                Page
  )
{
  UINTN  Index;
  UINTN  Y;

  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 0, 54, 232, Ui->Height - 54 }, Theme->Surface);
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ 231, 54, 1, Ui->Height - 54 }, Theme->Border);
  ModernUiDrawText (Ui, 24, 76, L"SETUP", Theme->MutedText, Theme->Surface);

  for (Index = 0; Index < ARRAY_SIZE (mPages); Index++) {
    Y = 112 + (Index * 54);
    if (mPages[Index].Page == Page) {
      ModernUiFillRect (Ui, (MODERN_UI_RECT){ 16, Y - 10, 200, 42 }, Theme->AccentSoft);
      ModernUiFillRect (Ui, (MODERN_UI_RECT){ 16, Y - 10, 4, 42 }, Theme->Accent);
      ModernUiDrawText (Ui, 32, Y, (CHAR16 *)mPages[Index].Title, Theme->Text, Theme->AccentSoft);
    } else {
      ModernUiDrawText (Ui, 32, Y, (CHAR16 *)mPages[Index].Title, Theme->MutedText, Theme->Surface);
    }
  }

  ModernUiDrawText (Ui, 24, Ui->Height - 80, L"Up/Down: page", Theme->MutedText, Theme->Surface);
  ModernUiDrawText (Ui, 24, Ui->Height - 58, L"Enter: action", Theme->MutedText, Theme->Surface);
  ModernUiDrawText (Ui, 24, Ui->Height - 36, L"Esc: continue", Theme->MutedText, Theme->Surface);
}

STATIC
MODERN_UI_RECT
ContentRect (
  IN MODERN_UI_RENDER_CONTEXT  *Ui
  )
{
  return (MODERN_UI_RECT){ 256, 82, Ui->Width - 280, Ui->Height - 112 };
}

STATIC
VOID
DrawPageTitle (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_PAGE                Page
  )
{
  ModernUiDrawText (Ui, 256, 72, (CHAR16 *)mPages[Page].Title, Theme->Text, Theme->Background);
  ModernUiDrawText (Ui, 256, 98, (CHAR16 *)mPages[Page].Hint, Theme->MutedText, Theme->Background);
}

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

STATIC
VOID
DrawDashboard (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  CHAR16  Resolution[48];
  CHAR16  BootCount[48];
  UINTN   CardWidth;

  CardWidth = (Ui->Width - 304 - CARD_GAP) / 2;
  UnicodeSPrint (Resolution, sizeof (Resolution), L"%u x %u", Ui->Width, Ui->Height);
  UnicodeSPrint (BootCount, sizeof (BootCount), L"%u entries", GetBootCount ());

  DrawInfoCard (Ui, Theme, 256, 142, CardWidth, L"Firmware Vendor", gST->FirmwareVendor);
  DrawInfoCard (Ui, Theme, 256 + CardWidth + CARD_GAP, 142, CardWidth, L"Firmware Revision", L"edk2 / ArmVirt");
  DrawInfoCard (Ui, Theme, 256, 250, CardWidth, L"Display", Resolution);
  DrawInfoCard (Ui, Theme, 256 + CardWidth + CARD_GAP, 250, CardWidth, L"Boot Options", BootCount);

  ModernUiDrawPanel (Ui, (MODERN_UI_RECT){ 256, 374, Ui->Width - 304, 132 }, Theme);
  ModernUiDrawText (Ui, 276, 396, L"Prototype Status", Theme->MutedText, Theme->Surface);
  ModernUiDrawText (Ui, 276, 430, L"GOP renderer online. Keyboard navigation is active.", Theme->Text, Theme->Surface);
  ModernUiDrawProgress (Ui, (MODERN_UI_RECT){ 276, 470, Ui->Width - 344, 12 }, 68, Theme->Border, Theme->Accent);
}

STATIC
CHAR16 *
BootDescription (
  IN UINT16  BootId
  )
{
  EFI_STATUS  Status;
  CHAR16      Name[12];
  UINTN       Size;
  UINT8       *Data;
  CHAR16      *Description;
  UINTN       Offset;
  UINTN       DescriptionSize;

  UnicodeSPrint (Name, sizeof (Name), L"Boot%04x", BootId);
  Status = ReadGlobalVariable (Name, (VOID **)&Data, &Size);
  if (EFI_ERROR (Status) || (Size < sizeof (UINT32) + sizeof (UINT16) + sizeof (CHAR16))) {
    return NULL;
  }

  Offset          = sizeof (UINT32) + sizeof (UINT16);
  DescriptionSize = StrSize ((CHAR16 *)(Data + Offset));
  if ((Offset + DescriptionSize) > Size) {
    FreePool (Data);
    return NULL;
  }

  Description = AllocateCopyPool (DescriptionSize, Data + Offset);
  FreePool (Data);
  return Description;
}

STATIC
VOID
DrawBoot (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  EFI_STATUS  Status;
  UINTN       Size;
  UINT16      *BootOrder;
  UINTN       Count;
  UINTN       Index;
  UINTN       Y;
  CHAR16      Line[160];
  CHAR16      *Description;

  ModernUiDrawPanel (Ui, ContentRect (Ui), Theme);
  ModernUiDrawText (Ui, 280, 118, L"Boot order is read-only in this prototype.", Theme->MutedText, Theme->Surface);

  Status = ReadGlobalVariable (EFI_BOOT_ORDER_VARIABLE_NAME, (VOID **)&BootOrder, &Size);
  if (EFI_ERROR (Status)) {
    ModernUiDrawText (Ui, 280, 164, L"No BootOrder variable found.", Theme->Warning, Theme->Surface);
    return;
  }

  Count = Size / sizeof (UINT16);
  for (Index = 0; (Index < Count) && (Index < 9); Index++) {
    Y           = 160 + Index * 42;
    Description = BootDescription (BootOrder[Index]);
    UnicodeSPrint (
      Line,
      sizeof (Line),
      L"%02u   Boot%04x   %s",
      Index + 1,
      BootOrder[Index],
      (Description != NULL) ? Description : L"(no description)"
      );
    ModernUiFillRect (Ui, (MODERN_UI_RECT){ 280, Y - 8, Ui->Width - 340, 34 }, (Index == 0) ? Theme->SurfaceRaised : Theme->Surface);
    ModernUiDrawText (Ui, 296, Y, Line, (Index == 0) ? Theme->Text : Theme->MutedText, (Index == 0) ? Theme->SurfaceRaised : Theme->Surface);
    if (Description != NULL) {
      FreePool (Description);
    }
  }

  FreePool (BootOrder);
}

STATIC
VOID
DrawDevices (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme
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

  ModernUiDrawPanel (Ui, ContentRect (Ui), Theme);

  Status = gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &HandleCount, &Handles);
  if (EFI_ERROR (Status)) {
    ModernUiDrawText (Ui, 280, 150, L"Unable to enumerate handles.", Theme->Warning, Theme->Surface);
    return;
  }

  DrawTextF (Ui, 280, 118, Theme->MutedText, Theme->Surface, L"%u handles visible to DXE", HandleCount);

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
    ModernUiDrawText (Ui, 280, 160 + Shown * 38, Line, Theme->Text, Theme->Surface);
    FreePool (Text);
    Shown++;
  }

  FreePool (Handles);
}

STATIC
VOID
DrawSecurity (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  BOOLEAN  SecureBoot;

  SecureBoot = GetSecureBootEnabled ();
  ModernUiDrawPanel (Ui, ContentRect (Ui), Theme);
  ModernUiDrawText (Ui, 280, 126, L"Secure Boot", Theme->MutedText, Theme->Surface);
  ModernUiDrawText (Ui, 280, 166, SecureBoot ? L"Enabled" : L"Disabled", SecureBoot ? Theme->Success : Theme->Warning, Theme->Surface);
  ModernUiDrawText (Ui, 280, 218, L"Key management is intentionally read-only in v1.", Theme->MutedText, Theme->Surface);
}

STATIC
VOID
DrawExit (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN UINTN                     Selected
  )
{
  CONST CHAR16  *Items[] = {
    L"Continue boot",
    L"Launch classic UiApp fallback",
    L"Reset system"
  };
  UINTN         Index;
  UINTN         Y;

  ModernUiDrawPanel (Ui, ContentRect (Ui), Theme);
  ModernUiDrawText (Ui, 280, 118, L"Use Left/Right to select an action, Enter to run it.", Theme->MutedText, Theme->Surface);

  for (Index = 0; Index < ARRAY_SIZE (Items); Index++) {
    Y = 172 + Index * 58;
    ModernUiFillRect (
      Ui,
      (MODERN_UI_RECT){ 280, Y - 12, Ui->Width - 340, 42 },
      (Index == Selected) ? Theme->AccentSoft : Theme->Surface
      );
    ModernUiDrawText (Ui, 300, Y, (CHAR16 *)Items[Index], (Index == Selected) ? Theme->Text : Theme->MutedText, (Index == Selected) ? Theme->AccentSoft : Theme->Surface);
  }
}

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

STATIC
VOID
DrawPage (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_PAGE                Page,
  IN UINTN                     ExitSelection
  )
{
  ModernUiClear (Ui, Theme->Background);
  DrawHeader (Ui, Theme);
  DrawNav (Ui, Theme, Page);
  DrawPageTitle (Ui, Theme, Page);

  switch (Page) {
    case PageDashboard:
      DrawDashboard (Ui, Theme);
      break;
    case PageBoot:
      DrawBoot (Ui, Theme);
      break;
    case PageDevices:
      DrawDevices (Ui, Theme);
      break;
    case PageSecurity:
      DrawSecurity (Ui, Theme);
      break;
    case PageExit:
      DrawExit (Ui, Theme, ExitSelection);
      break;
    default:
      break;
  }
}

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
  UINTN                     ExitSelection;
  BOOLEAN                   Redraw;

  gBS->SetWatchdogTimer (0, 0, 0, NULL);

  Status = ModernUiRendererInit (&Ui);
  if (EFI_ERROR (Status)) {
    Print (L"ModernSetupApp: graphics initialization failed: %r\n", Status);
    return Status;
  }

  ModernUiInputInit (&Input);
  Theme         = ModernUiGetTheme ();
  Page          = PageDashboard;
  ExitSelection = 0;
  Redraw        = TRUE;

  for (;;) {
    if (Redraw) {
      DrawPage (&Ui, Theme, Page, ExitSelection);
      Redraw = FALSE;
    }

    Status = ModernUiReadInput (&Input, &Event);
    if (EFI_ERROR (Status)) {
      continue;
    }

    switch (Event.Type) {
      case ModernUiInputUp:
        Page   = (Page == 0) ? (PageMax - 1) : (Page - 1);
        Redraw = TRUE;
        break;
      case ModernUiInputDown:
      case ModernUiInputTab:
        Page   = (Page + 1) % PageMax;
        Redraw = TRUE;
        break;
      case ModernUiInputLeft:
        if (Page == PageExit) {
          ExitSelection = (ExitSelection == 0) ? 2 : (ExitSelection - 1);
          Redraw        = TRUE;
        }
        break;
      case ModernUiInputRight:
        if (Page == PageExit) {
          ExitSelection = (ExitSelection + 1) % 3;
          Redraw        = TRUE;
        }
        break;
      case ModernUiInputEscape:
        return EFI_SUCCESS;
      case ModernUiInputEnter:
        if (Page == PageExit) {
          if (ExitSelection == 0) {
            return EFI_SUCCESS;
          } else if (ExitSelection == 1) {
            LaunchUiAppFallback (ImageHandle);
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
