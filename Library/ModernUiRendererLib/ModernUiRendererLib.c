/** @file
  GOP-backed renderer library for ModernSetupPkg.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/HiiFont.h>

#include <ModernUi/ModernUiRenderer.h>

EFI_STATUS
EFIAPI
ModernUiRendererInit (
  OUT MODERN_UI_RENDER_CONTEXT  *Context
  )
{
  EFI_STATUS  Status;

  if (Context == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Context, sizeof (*Context));

  Status = gBS->HandleProtocol (
                  gST->ConsoleOutHandle,
                  &gEfiGraphicsOutputProtocolGuid,
                  (VOID **)&Context->Gop
                  );
  if (EFI_ERROR (Status)) {
    Status = gBS->LocateProtocol (
                    &gEfiGraphicsOutputProtocolGuid,
                    NULL,
                    (VOID **)&Context->Gop
                    );
  }

  if (EFI_ERROR (Status) || (Context->Gop == NULL) || (Context->Gop->Mode == NULL) || (Context->Gop->Mode->Info == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: GOP unavailable: %r\n", __func__, Status));
    return EFI_NOT_FOUND;
  }

  Context->Width  = Context->Gop->Mode->Info->HorizontalResolution;
  Context->Height = Context->Gop->Mode->Info->VerticalResolution;

  Status = gBS->LocateProtocol (
                  &gEfiHiiFontProtocolGuid,
                  NULL,
                  (VOID **)&Context->Font
                  );
  if (EFI_ERROR (Status)) {
    Context->Font = NULL;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ModernUiFillRect (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
  IN MODERN_UI_RECT                 Rect,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Line;
  UINTN                          Index;
  EFI_STATUS                     Status;

  if ((Context == NULL) || (Context->Gop == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Rect.X >= Context->Width) || (Rect.Y >= Context->Height)) {
    return EFI_SUCCESS;
  }

  if ((Rect.X + Rect.Width) > Context->Width) {
    Rect.Width = Context->Width - Rect.X;
  }

  if ((Rect.Y + Rect.Height) > Context->Height) {
    Rect.Height = Context->Height - Rect.Y;
  }

  Line = AllocatePool (Rect.Width * sizeof (*Line));
  if (Line == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (Index = 0; Index < Rect.Width; Index++) {
    Line[Index] = Color;
  }

  Status = Context->Gop->Blt (
                           Context->Gop,
                           Line,
                           EfiBltBufferToVideo,
                           0,
                           0,
                           Rect.X,
                           Rect.Y,
                           Rect.Width,
                           Rect.Height,
                           0
                           );
  FreePool (Line);
  return Status;
}

EFI_STATUS
EFIAPI
ModernUiClear (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  if (Context == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return ModernUiFillRect (Context, (MODERN_UI_RECT){ 0, 0, Context->Width, Context->Height }, Color);
}

EFI_STATUS
EFIAPI
ModernUiStrokeRect (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
  IN MODERN_UI_RECT                 Rect,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  EFI_STATUS  Status;

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y, Rect.Width, 1 }, Color);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y + Rect.Height - 1, Rect.Width, 1 }, Color);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y, 1, Rect.Height }, Color);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X + Rect.Width - 1, Rect.Y, 1, Rect.Height }, Color);
}

EFI_STATUS
EFIAPI
ModernUiDrawText (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
  IN UINTN                          X,
  IN UINTN                          Y,
  IN CONST CHAR16                   *Text,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background
  )
{
  EFI_IMAGE_OUTPUT        *Blt;
  EFI_FONT_DISPLAY_INFO   FontInfo;
  EFI_STATUS              Status;

  if ((Context == NULL) || (Context->Gop == NULL) || (Text == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Context->Font == NULL) {
    return EFI_UNSUPPORTED;
  }

  Blt = AllocateZeroPool (sizeof (*Blt));
  if (Blt == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ZeroMem (&FontInfo, sizeof (FontInfo));
  FontInfo.ForegroundColor = Color;
  FontInfo.BackgroundColor = Background;

  Blt->Image.Screen = Context->Gop;

  Status = Context->Font->StringToImage (
                            Context->Font,
                            EFI_HII_DIRECT_TO_SCREEN | EFI_HII_IGNORE_IF_NO_GLYPH | EFI_HII_IGNORE_LINE_BREAK,
                            (EFI_STRING)Text,
                            &FontInfo,
                            &Blt,
                            X,
                            Y,
                            NULL,
                            NULL,
                            NULL
                            );
  FreePool (Blt);
  return Status;
}

EFI_STATUS
EFIAPI
ModernUiDrawPanel (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  EFI_STATUS  Status;

  if ((Context == NULL) || (Theme == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ModernUiFillRect (Context, Rect, Theme->Surface);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return ModernUiStrokeRect (Context, Rect, Theme->Border);
}

EFI_STATUS
EFIAPI
ModernUiDrawProgress (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
  IN MODERN_UI_RECT                 Rect,
  IN UINTN                          Percent,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Track,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Fill
  )
{
  EFI_STATUS  Status;
  UINTN       FillWidth;

  if (Percent > 100) {
    Percent = 100;
  }

  Status = ModernUiFillRect (Context, Rect, Track);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FillWidth = (Rect.Width * Percent) / 100;
  if (FillWidth == 0) {
    return EFI_SUCCESS;
  }

  return ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y, FillWidth, Rect.Height }, Fill);
}
