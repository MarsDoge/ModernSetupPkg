/** @file
  GOP-backed renderer primitives for ModernSetupPkg.

  Provides the three backend primitives consumed by the shared
  ModernUiRendererCommon.c: ModernUiRendererInit, ModernUiFillRect, and
  ModernUiDrawText. All higher-level geometry/text/widget functions live in the
  shared translation unit.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/HiiFont.h>

#include <ModernUi/ModernUiRenderer.h>
#include "ModernUiGlyphs.h"
#include "ModernUiRendererInternal.h"


/**
  Initialize a render context from firmware graphics services.

  @param[out] Context  Render context to initialize. Must not be NULL. On
                       success, GOP is required and Font is optional.

  @retval EFI_SUCCESS            Context was initialized.
  @retval EFI_INVALID_PARAMETER  Context is NULL.
  @retval EFI_NOT_FOUND          GOP is unavailable or has no active mode.
**/
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

  ModernUiSelectPreferredGopMode (Context->Gop);
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


/**
  Fill a rectangle in the active render target.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Pixel rectangle. Zero width or height is invalid.
  @param[in] Color    Fill color.

  @retval EFI_SUCCESS            Rectangle was filled or clipped outside view.
  @retval EFI_INVALID_PARAMETER  Context is NULL, GOP is unavailable, or Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiFillRect (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
  IN MODERN_UI_RECT                 Rect,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
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

  return Context->Gop->Blt (
                         Context->Gop,
                         &Color,
                         EfiBltVideoFill,
                         0,
                         0,
                         Rect.X,
                         Rect.Y,
                         Rect.Width,
                         Rect.Height,
                         0
                         );
}


/**
  Draw one UCS-2 text run through HII Font.

  @param[in] Context     Initialized render context. Must not be NULL.
  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] Text        Null-terminated UCS-2 string. Must not be NULL.
  @param[in] Color       Text foreground color.
  @param[in] Background  Background color passed to the font renderer.

  @retval EFI_SUCCESS            Text was rendered.
  @retval EFI_INVALID_PARAMETER  Context or Text is NULL.
  @retval EFI_UNSUPPORTED        HII Font protocol is unavailable.
  @retval EFI_OUT_OF_RESOURCES   Temporary rendering allocation failed.
**/
STATIC
EFI_STATUS
DrawHiiText (
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

  Blt->Width        = (UINT16)Context->Width;
  Blt->Height       = (UINT16)Context->Height;
  Blt->Image.Screen = Context->Gop;

  Status = Context->Font->StringToImage (
                            Context->Font,
                            EFI_HII_IGNORE_IF_NO_GLYPH | EFI_HII_OUT_FLAG_CLIP |
                            EFI_HII_OUT_FLAG_CLIP_CLEAN_X | EFI_HII_OUT_FLAG_CLIP_CLEAN_Y |
                            EFI_HII_IGNORE_LINE_BREAK | EFI_HII_DIRECT_TO_SCREEN,
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


/**
  Draw one built-in bitmap glyph.

  @param[in] Context     Initialized render context. Must not be NULL.
  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] Glyph       Built-in glyph data. Must not be NULL.
  @param[in] Color       Foreground color.
  @param[in] Background  Background color.

  @retval EFI_SUCCESS            Glyph was rendered.
  @retval EFI_INVALID_PARAMETER  Context, GOP, or Glyph is NULL.
**/
STATIC
EFI_STATUS
DrawBuiltinGlyph (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN UINTN                             X,
  IN UINTN                             Y,
  IN CONST MODERN_UI_BUILTIN_GLYPH     *Glyph,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Background
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Buffer[MODERN_UI_BUILTIN_GLYPH_HEIGHT * MODERN_UI_BUILTIN_GLYPH_HEIGHT];
  UINTN                          Row;
  UINTN                          Column;
  UINTN                          Index;
  UINTN                          Alpha;

  if ((Context == NULL) || (Context->Gop == NULL) || (Glyph == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  for (Row = 0; Row < Glyph->Height; Row++) {
    for (Column = 0; Column < Glyph->Width; Column++) {
      Index                  = Row * Glyph->Width + Column;
      Alpha                  = Glyph->Bitmap[Index];
      Buffer[Index].Red      = (UINT8)(((UINTN)Background.Red * (255 - Alpha) + (UINTN)Color.Red * Alpha) / 255);
      Buffer[Index].Green    = (UINT8)(((UINTN)Background.Green * (255 - Alpha) + (UINTN)Color.Green * Alpha) / 255);
      Buffer[Index].Blue     = (UINT8)(((UINTN)Background.Blue * (255 - Alpha) + (UINTN)Color.Blue * Alpha) / 255);
      Buffer[Index].Reserved = 0;
    }
  }

  return Context->Gop->Blt (
                         Context->Gop,
                         Buffer,
                         EfiBltBufferToVideo,
                         0,
                         0,
                         X,
                         Y,
                         Glyph->Width,
                         Glyph->Height,
                         Glyph->Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                         );
}


/**
  Draw a visible placeholder for a missing non-ASCII glyph.

  @param[in] Context     Initialized render context. Must not be NULL.
  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] Color       Placeholder stroke color.
  @param[in] Background  Placeholder fill color.

  @retval EFI_SUCCESS            Placeholder was rendered.
  @retval others                 Status from fill or stroke primitives.
**/
STATIC
EFI_STATUS
DrawMissingGlyph (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
  IN UINTN                          X,
  IN UINTN                          Y,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background
  )
{
  EFI_STATUS  Status;

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ X, Y, MODERN_UI_BUILTIN_GLYPH_WIDTH, MODERN_UI_BUILTIN_GLYPH_HEIGHT }, Background);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return ModernUiStrokeRect (Context, (MODERN_UI_RECT){ X + 2, Y + 2, MODERN_UI_BUILTIN_GLYPH_WIDTH - 4, MODERN_UI_BUILTIN_GLYPH_HEIGHT - 4 }, Color);
}


/**
  Draw UCS-2 text using HII Font and built-in bitmap glyph fallback.

  @param[in] Context     Initialized render context. Must not be NULL.
  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] Text        Null-terminated UCS-2 string. Must not be NULL.
  @param[in] Color       Text foreground color.
  @param[in] Background  Background color passed to the font renderer.

  @retval EFI_SUCCESS            Text was rendered.
  @retval EFI_INVALID_PARAMETER  Context or Text is NULL.
  @retval EFI_UNSUPPORTED        HII Font protocol is unavailable for a
                                  non-built-in character run.
  @retval EFI_OUT_OF_RESOURCES   Temporary rendering allocation failed.
**/
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
  CHAR16                         Segment[MODERN_UI_TEXT_SEGMENT_MAX + 1];
  UINTN                          Index;
  UINTN                          SegmentIndex;
  UINTN                          CurrentX;
  EFI_STATUS                     Status;
  EFI_STATUS                     ReturnStatus;
  CONST MODERN_UI_BUILTIN_GLYPH  *Glyph;

  if ((Context == NULL) || (Context->Gop == NULL) || (Text == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  CurrentX     = X;
  ReturnStatus = EFI_SUCCESS;
  for (Index = 0; Text[Index] != L'\0'; ) {
    Glyph = ModernUiFindBuiltinGlyph (Text[Index]);
    if ((Glyph != NULL) || ModernUiIsTextModeGraphicGlyph (Text[Index]) || (Text[Index] > 0x7F)) {
      if (ModernUiIsTextModeGraphicGlyph (Text[Index])) {
        Status = ModernUiDrawTextModeGraphicGlyph (Context, CurrentX, Y, Text[Index], Color, Background);
        CurrentX += MODERN_UI_GRAPHIC_CELL_WIDTH;
      } else if (Glyph != NULL) {
        Status = DrawBuiltinGlyph (Context, CurrentX, Y, Glyph, Color, Background);
        CurrentX += Glyph->Advance;
      } else {
        DEBUG ((DEBUG_VERBOSE, "%a: missing glyph U+%04x\n", __func__, Text[Index]));
        Status = DrawMissingGlyph (Context, CurrentX, Y, Color, Background);
        CurrentX += MODERN_UI_BUILTIN_GLYPH_WIDTH;
      }

      if (EFI_ERROR (Status)) {
        ReturnStatus = Status;
      }

      Index++;
      continue;
    }

    SegmentIndex = 0;
    while ((Text[Index] != L'\0') && (Text[Index] <= 0x7F) && (SegmentIndex < MODERN_UI_TEXT_SEGMENT_MAX)) {
      Segment[SegmentIndex++] = Text[Index++];
    }

    Segment[SegmentIndex] = L'\0';
    Status                = DrawHiiText (Context, CurrentX, Y, Segment, Color, Background);
    if (EFI_ERROR (Status)) {
      ReturnStatus = Status;
    }

    CurrentX += ModernUiMeasureText (Segment);
  }

  return ReturnStatus;
}

/**
  GOP one-of renderer: compose the closed drop-down from primitives.

  The GOP backend has no widget toolkit, so the best available drop-down is the
  shared themed value box, which already paints a bordered field, the selected
  option text, and a right-aligned chevron. Display-only; see the contract on
  ModernUiRenderOneOf in ModernUiRenderer.h.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rect      Control rectangle.
  @param[in] Value     Selected option text. Must not be NULL.
  @param[in] Selected  TRUE when the owning row is selected.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval others  Status from ModernUiDrawValueBox.
**/
EFI_STATUS
EFIAPI
ModernUiRenderOneOf (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  )
{
  return ModernUiDrawValueBox (Context, Rect, Value, Selected, Theme);
}
