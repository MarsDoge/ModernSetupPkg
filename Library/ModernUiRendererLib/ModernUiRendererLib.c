/** @file
  GOP-backed renderer library for ModernSetupPkg.

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

#define MODERN_UI_ASCII_CELL_WIDTH  8
#define MODERN_UI_GRAPHIC_CELL_WIDTH  MODERN_UI_ASCII_CELL_WIDTH
#define MODERN_UI_GRAPHIC_LINE_Y       9
#define MODERN_UI_GRAPHIC_LINE_X       4
#define MODERN_UI_TEXT_SEGMENT_MAX  96
#define MODERN_UI_TARGET_WIDTH      1024
#define MODERN_UI_TARGET_HEIGHT     768

/**
  Blend two GOP colors by percentage weight.

  @param[in] Base    Base color used when Weight is zero.
  @param[in] Accent  Accent color used when Weight is one hundred.
  @param[in] Weight  Accent weight in percent. Values above 100 are clamped.

  @return Blended color with Reserved cleared.
**/
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
EFIAPI
ModernUiBlendColor (
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Base,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Accent,
  IN UINT8                          Weight
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Result;
  UINTN                          ClampedWeight;

  ClampedWeight = MIN (Weight, 100);
  Result.Red = (UINT8)(
                 ((UINTN)Base.Red * (100 - ClampedWeight) +
                  (UINTN)Accent.Red * ClampedWeight) / 100
                 );
  Result.Green = (UINT8)(
                   ((UINTN)Base.Green * (100 - ClampedWeight) +
                    (UINTN)Accent.Green * ClampedWeight) / 100
                   );
  Result.Blue = (UINT8)(
                  ((UINTN)Base.Blue * (100 - ClampedWeight) +
                   (UINTN)Accent.Blue * ClampedWeight) / 100
                  );
  Result.Reserved = 0;
  return Result;
}

/**
  Select a preferred GOP mode when the active mode is smaller than the target.

  @param[in] Gop  Graphics output protocol to inspect. Must not be NULL.

  @retval EFI_SUCCESS            Current mode is acceptable or a better mode
                                 was selected.
  @retval EFI_INVALID_PARAMETER  Gop or mode data is NULL.
**/
STATIC
EFI_STATUS
SelectPreferredGopMode (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop
  )
{
  EFI_STATUS                            Status;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;
  UINTN                                 InfoSize;
  UINT32                                Mode;
  UINT32                                BestMode;
  UINTN                                 BestArea;
  UINTN                                 Area;

  if ((Gop == NULL) || (Gop->Mode == NULL) || (Gop->Mode->Info == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Gop->Mode->Info->HorizontalResolution >= MODERN_UI_TARGET_WIDTH) &&
      (Gop->Mode->Info->VerticalResolution >= MODERN_UI_TARGET_HEIGHT))
  {
    return EFI_SUCCESS;
  }

  BestMode = Gop->Mode->MaxMode;
  BestArea = MAX_UINTN;
  for (Mode = 0; Mode < Gop->Mode->MaxMode; Mode++) {
    Info     = NULL;
    InfoSize = 0;
    Status = Gop->QueryMode (Gop, Mode, &InfoSize, &Info);
    if (EFI_ERROR (Status) || (Info == NULL)) {
      continue;
    }

    if ((Info->HorizontalResolution >= MODERN_UI_TARGET_WIDTH) &&
        (Info->VerticalResolution >= MODERN_UI_TARGET_HEIGHT))
    {
      Area = (UINTN)Info->HorizontalResolution * (UINTN)Info->VerticalResolution;
      if (Area < BestArea) {
        BestArea = Area;
        BestMode = Mode;
      }
    }

    FreePool (Info);
  }

  if ((BestMode < Gop->Mode->MaxMode) && (BestMode != Gop->Mode->Mode)) {
    Gop->SetMode (Gop, BestMode);
  }

  return EFI_SUCCESS;
}

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

  SelectPreferredGopMode (Context->Gop);
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
  Fill the full render target with one color.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Color    Fill color.

  @retval EFI_SUCCESS            The screen was cleared.
  @retval EFI_INVALID_PARAMETER  Context is NULL or invalid.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
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

/**
  Draw a one-pixel rectangle border.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Pixel rectangle to outline.
  @param[in] Color    Border color.

  @retval EFI_SUCCESS            Border was drawn.
  @retval EFI_INVALID_PARAMETER  Context is NULL, GOP is unavailable, or Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
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
  Return whether a UCS-2 character is a UEFI text-mode graphics character that
  should be rendered as a narrow fixed-width shape.

  @param[in] CodePoint  UCS-2 code point to classify.

  @retval TRUE   CodePoint is a box, arrow, triangle, or checkbox glyph used by
                 edk2 text-mode setup UI.
  @retval FALSE  CodePoint should use normal text or built-in glyph rendering.
**/
STATIC
BOOLEAN
IsTextModeGraphicGlyph (
  IN CHAR16  CodePoint
  )
{
  return (((CodePoint >= BOXDRAW_HORIZONTAL) && (CodePoint <= BOXDRAW_DOUBLE_VERTICAL_HORIZONTAL)) ||
          ((CodePoint >= ARROW_LEFT) && (CodePoint <= ARROW_DOWN)) ||
          (CodePoint == 0x25A0) ||
          (CodePoint == 0x25A1) ||
          (CodePoint == 0x25B2) ||
          (CodePoint == 0x25B6) ||
          (CodePoint == 0x25BA) ||
          (CodePoint == 0x25BC) ||
          (CodePoint == 0x25C0) ||
          (CodePoint == 0x25C4));
}

/**
  Draw one UEFI text-mode graphics character as a narrow GOP primitive.

  @param[in] Context    Initialized render context. Must not be NULL.
  @param[in] X          Left coordinate in pixels.
  @param[in] Y          Top coordinate in pixels.
  @param[in] CodePoint  Box, arrow, triangle, or checkbox glyph to render.
  @param[in] Color      Foreground color.
  @param[in] Background Background fill color.

  @retval EFI_SUCCESS            Glyph was rendered.
  @retval EFI_INVALID_PARAMETER  Context is NULL or GOP is unavailable.
  @retval others                 Status from fill or stroke primitives.
**/
STATIC
EFI_STATUS
DrawTextModeGraphicGlyph (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
  IN UINTN                          X,
  IN UINTN                          Y,
  IN CHAR16                         CodePoint,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background
  )
{
  EFI_STATUS  Status;
  BOOLEAN     Horizontal;
  BOOLEAN     Vertical;
  BOOLEAN     Left;
  BOOLEAN     Right;
  BOOLEAN     Up;
  BOOLEAN     Down;
  UINTN       Index;

  if ((Context == NULL) || (Context->Gop == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ X, Y, MODERN_UI_GRAPHIC_CELL_WIDTH, MODERN_UI_BUILTIN_GLYPH_HEIGHT }, Background);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Horizontal = FALSE;
  Vertical   = FALSE;
  Left       = FALSE;
  Right      = FALSE;
  Up         = FALSE;
  Down       = FALSE;

  switch (CodePoint) {
    case BOXDRAW_HORIZONTAL:
    case BOXDRAW_DOUBLE_HORIZONTAL:
      Left = Right = TRUE;
      break;
    case BOXDRAW_VERTICAL:
    case BOXDRAW_DOUBLE_VERTICAL:
      Up = Down = TRUE;
      break;
    case BOXDRAW_DOWN_RIGHT:
    case BOXDRAW_DOWN_RIGHT_DOUBLE:
    case BOXDRAW_DOWN_DOUBLE_RIGHT:
    case BOXDRAW_DOUBLE_DOWN_RIGHT:
      Right = Down = TRUE;
      break;
    case BOXDRAW_DOWN_LEFT:
    case BOXDRAW_DOWN_LEFT_DOUBLE:
    case BOXDRAW_DOWN_DOUBLE_LEFT:
    case BOXDRAW_DOUBLE_DOWN_LEFT:
      Left = Down = TRUE;
      break;
    case BOXDRAW_UP_RIGHT:
    case BOXDRAW_UP_RIGHT_DOUBLE:
    case BOXDRAW_UP_DOUBLE_RIGHT:
    case BOXDRAW_DOUBLE_UP_RIGHT:
      Right = Up = TRUE;
      break;
    case BOXDRAW_UP_LEFT:
    case BOXDRAW_UP_LEFT_DOUBLE:
    case BOXDRAW_UP_DOUBLE_LEFT:
    case BOXDRAW_DOUBLE_UP_LEFT:
      Left = Up = TRUE;
      break;
    case BOXDRAW_VERTICAL_RIGHT:
    case BOXDRAW_VERTICAL_RIGHT_DOUBLE:
    case BOXDRAW_VERTICAL_DOUBLE_RIGHT:
    case BOXDRAW_DOUBLE_VERTICAL_RIGHT:
      Up = Down = Right = TRUE;
      break;
    case BOXDRAW_VERTICAL_LEFT:
    case BOXDRAW_VERTICAL_LEFT_DOUBLE:
    case BOXDRAW_VERTICAL_DOUBLE_LEFT:
    case BOXDRAW_DOUBLE_VERTICAL_LEFT:
      Up = Down = Left = TRUE;
      break;
    case BOXDRAW_DOWN_HORIZONTAL:
    case BOXDRAW_DOWN_HORIZONTAL_DOUBLE:
    case BOXDRAW_DOWN_DOUBLE_HORIZONTAL:
    case BOXDRAW_DOUBLE_DOWN_HORIZONTAL:
      Left = Right = Down = TRUE;
      break;
    case BOXDRAW_UP_HORIZONTAL:
    case BOXDRAW_UP_HORIZONTAL_DOUBLE:
    case BOXDRAW_UP_DOUBLE_HORIZONTAL:
    case BOXDRAW_DOUBLE_UP_HORIZONTAL:
      Left = Right = Up = TRUE;
      break;
    case BOXDRAW_VERTICAL_HORIZONTAL:
    case BOXDRAW_VERTICAL_HORIZONTAL_DOUBLE:
    case BOXDRAW_VERTICAL_DOUBLE_HORIZONTAL:
    case BOXDRAW_DOUBLE_VERTICAL_HORIZONTAL:
      Left = Right = Up = Down = TRUE;
      break;
    case ARROW_RIGHT:
    case 0x25B6:
    case 0x25BA:
      for (Index = 0; Index < 6; Index++) {
        Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ X + 2 + Index, Y + 5 + Index, 1, 7 - (Index * 2 > 6 ? 6 : Index * 2) }, Color);
        if (EFI_ERROR (Status)) {
          return Status;
        }
      }

      return EFI_SUCCESS;
    case ARROW_LEFT:
    case 0x25C0:
    case 0x25C4:
      for (Index = 0; Index < 6; Index++) {
        Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ X + 6 - Index, Y + 5 + Index, 1, 7 - (Index * 2 > 6 ? 6 : Index * 2) }, Color);
        if (EFI_ERROR (Status)) {
          return Status;
        }
      }

      return EFI_SUCCESS;
    case ARROW_UP:
    case 0x25B2:
      Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ X + 3, Y + 5, 2, 8 }, Color);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      return ModernUiFillRect (Context, (MODERN_UI_RECT){ X + 2, Y + 6, 4, 2 }, Color);
    case ARROW_DOWN:
    case 0x25BC:
      Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ X + 3, Y + 5, 2, 8 }, Color);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      return ModernUiFillRect (Context, (MODERN_UI_RECT){ X + 2, Y + 10, 4, 2 }, Color);
    case 0x25A0:
      return ModernUiFillRect (Context, (MODERN_UI_RECT){ X + 1, Y + 6, 6, 6 }, Color);
    case 0x25A1:
      return ModernUiStrokeRect (Context, (MODERN_UI_RECT){ X + 1, Y + 6, 6, 6 }, Color);
    default:
      break;
  }

  Horizontal = Left || Right;
  Vertical   = Up || Down;
  if (Horizontal) {
    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){
                 X + (Left ? 0 : MODERN_UI_GRAPHIC_LINE_X),
                 Y + MODERN_UI_GRAPHIC_LINE_Y,
                 (Left && Right) ? MODERN_UI_GRAPHIC_CELL_WIDTH : (MODERN_UI_GRAPHIC_CELL_WIDTH - MODERN_UI_GRAPHIC_LINE_X),
                 1
               },
               Color
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (Vertical) {
    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){
                 X + MODERN_UI_GRAPHIC_LINE_X,
                 Y + (Up ? 0 : MODERN_UI_GRAPHIC_LINE_Y),
                 1,
                 (Up && Down) ? MODERN_UI_BUILTIN_GLYPH_HEIGHT : (MODERN_UI_BUILTIN_GLYPH_HEIGHT - MODERN_UI_GRAPHIC_LINE_Y)
               },
               Color
               );
  }

  return Status;
}

/**
  Return the expected pixel width for a UCS-2 string.

  Built-in CJK glyphs are measured at their bitmap width. Other characters use
  the renderer's current ASCII cell width.

  @param[in] Text  Null-terminated UCS-2 string. Must not be NULL.

  @return Pixel width. NULL input returns 0.
**/
UINTN
EFIAPI
ModernUiMeasureText (
  IN CONST CHAR16  *Text
  )
{
  UINTN                         Index;
  UINTN                         Width;
  CONST MODERN_UI_BUILTIN_GLYPH *Glyph;

  if (Text == NULL) {
    return 0;
  }

  Width = 0;
  for (Index = 0; Text[Index] != L'\0'; Index++) {
    Glyph = ModernUiFindBuiltinGlyph (Text[Index]);
    if (IsTextModeGraphicGlyph (Text[Index])) {
      Width += MODERN_UI_GRAPHIC_CELL_WIDTH;
    } else if (Glyph != NULL) {
      Width += Glyph->Advance;
    } else if (Text[Index] > 0x7F) {
      Width += MODERN_UI_BUILTIN_GLYPH_WIDTH;
    } else {
      Width += MODERN_UI_ASCII_CELL_WIDTH;
    }
  }

  return Width;
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
    if ((Glyph != NULL) || IsTextModeGraphicGlyph (Text[Index]) || (Text[Index] > 0x7F)) {
      if (IsTextModeGraphicGlyph (Text[Index])) {
        Status = DrawTextModeGraphicGlyph (Context, CurrentX, Y, Text[Index], Color, Background);
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
  Format and draw one UCS-2 text line.

  @param[in] Context     Initialized render context. Must not be NULL.
  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] Color       Text foreground color.
  @param[in] Background  Text background color.
  @param[in] Format      PrintLib format string. Must not be NULL.
  @param[in] ...         Format arguments consumed according to Format.

  @retval EFI_SUCCESS            Text was formatted and rendered.
  @retval EFI_INVALID_PARAMETER  Context or Format is NULL.
  @retval others                 Status returned by ModernUiDrawText().
**/
EFI_STATUS
EFIAPI
ModernUiDrawTextFormatted (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
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

  if ((Context == NULL) || (Format == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  VA_START (Marker, Format);
  UnicodeVSPrint (Buffer, sizeof (Buffer), Format, Marker);
  VA_END (Marker);

  return ModernUiDrawText (Context, X, Y, Buffer, Color, Background);
}

/**
  Draw UCS-2 text constrained to a pixel width.

  The renderer measures mixed ASCII, built-in CJK, and text-mode graphic glyphs
  and appends "..." when the string must be truncated.

  @param[in] Context     Initialized render context. Must not be NULL.
  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] MaxWidth    Maximum text width in pixels.
  @param[in] Text        Null-terminated UCS-2 string. Must not be NULL.
  @param[in] Color       Text foreground color.
  @param[in] Background  Text background color.

  @retval EFI_SUCCESS            Text was rendered or empty width was ignored.
  @retval EFI_INVALID_PARAMETER  Context or Text is NULL.
  @retval EFI_OUT_OF_RESOURCES   Temporary truncation buffer allocation failed.
  @retval others                 Status returned by ModernUiDrawText().
**/
EFI_STATUS
EFIAPI
ModernUiDrawTextFit (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
  IN UINTN                          X,
  IN UINTN                          Y,
  IN UINTN                          MaxWidth,
  IN CONST CHAR16                   *Text,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background
  )
{
  CHAR16      *Buffer;
  CHAR16      Character[2];
  EFI_STATUS  Status;
  UINTN       Index;
  UINTN       CopyChars;
  UINTN       CurrentWidth;
  UINTN       CharacterWidth;
  UINTN       EllipsisWidth;
  UINTN       TargetWidth;
  UINTN       TextLength;

  if ((Context == NULL) || (Text == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (MaxWidth == 0) {
    return EFI_SUCCESS;
  }

  if (ModernUiMeasureText (Text) <= MaxWidth) {
    return ModernUiDrawText (Context, X, Y, Text, Color, Background);
  }

  TextLength = StrLen (Text);
  Buffer     = AllocateZeroPool ((TextLength + 4) * sizeof (CHAR16));
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  EllipsisWidth = ModernUiMeasureText (L"...");
  TargetWidth   = (MaxWidth > EllipsisWidth) ? (MaxWidth - EllipsisWidth) : MaxWidth;
  CopyChars     = 0;
  CurrentWidth  = 0;
  Character[1]  = L'\0';

  for (Index = 0; Text[Index] != L'\0'; Index++) {
    Character[0]   = Text[Index];
    CharacterWidth = ModernUiMeasureText (Character);
    if ((CurrentWidth + CharacterWidth) > TargetWidth) {
      break;
    }

    Buffer[CopyChars++] = Text[Index];
    CurrentWidth       += CharacterWidth;
  }

  if ((MaxWidth >= EllipsisWidth) && ((CopyChars + 3) < (TextLength + 4))) {
    Buffer[CopyChars++] = L'.';
    Buffer[CopyChars++] = L'.';
    Buffer[CopyChars++] = L'.';
  }

  Buffer[CopyChars] = L'\0';
  Status            = ModernUiDrawText (Context, X, Y, Buffer, Color, Background);
  FreePool (Buffer);
  return Status;
}

/**
  Draw a themed panel surface and border.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Panel rectangle.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Panel was drawn.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
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

  if ((Rect.Width > 2) && (Rect.Height > 2)) {
    Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X + 1, Rect.Y + 1, Rect.Width - 2, 1 }, Theme->SurfaceRaised);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return ModernUiStrokeRect (Context, Rect, Theme->Border);
}

/**
  Draw a focus frame only when requested.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rect      Rectangle to outline when HasFocus is TRUE.
  @param[in] HasFocus  TRUE to draw the focus frame.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Focus frame was drawn or skipped.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL, or Rect is empty
                                  when HasFocus is TRUE.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiDrawFocusFrame (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN BOOLEAN                   HasFocus,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  if ((Context == NULL) || (Theme == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!HasFocus) {
    return EFI_SUCCESS;
  }

  return ModernUiStrokeRect (Context, Rect, Theme->Accent);
}

/**
  Draw a compact title/value information card.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Card rectangle. Must be large enough for two text rows.
  @param[in] Title    Card title text. Must not be NULL.
  @param[in] Value    Card value text. Must not be NULL.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Card was drawn.
  @retval EFI_INVALID_PARAMETER  Context, Title, Value, or Theme is NULL, or
                                  Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
  @retval others                 Status returned by text rendering.
**/
EFI_STATUS
EFIAPI
ModernUiDrawInfoCard (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN CONST CHAR16              *Title,
  IN CONST CHAR16              *Value,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  EFI_STATUS  Status;

  if ((Context == NULL) || (Title == NULL) || (Value == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ModernUiDrawPanel (Context, Rect, Theme);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawTextFit (Context, Rect.X + 18, Rect.Y + 16, Rect.Width - 36, Title, Theme->MutedText, Theme->Surface);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return ModernUiDrawTextFit (Context, Rect.X + 18, Rect.Y + 48, Rect.Width - 36, Value, Theme->Text, Theme->Surface);
}

/**
  Return the themed background color used by a selectable row.

  @param[in] Selected  TRUE when the row is selected.
  @param[in] Disabled  TRUE when the row is visible but disabled or grayed.
  @param[in] Action    TRUE when the row represents an action-like command.
  @param[in] Subtitle  TRUE when the row is a section subtitle.
  @param[in] Theme     Theme token table. Must not be NULL.

  @return Row background color. NULL Theme returns zero.
**/
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
EFIAPI
ModernUiGetSelectableRowBackground (
  IN BOOLEAN                Selected,
  IN BOOLEAN                Disabled,
  IN BOOLEAN                Action,
  IN BOOLEAN                Subtitle,
  IN CONST MODERN_UI_THEME  *Theme
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  RowColor;

  ZeroMem (&RowColor, sizeof (RowColor));
  if (Theme == NULL) {
    return RowColor;
  }

  if (Selected) {
    RowColor = ModernUiBlendColor (Theme->BackgroundBlack, Theme->SelectedBand, 74);
  } else if (Action || Subtitle) {
    RowColor = Theme->SurfaceRaised;
  } else {
    RowColor = Theme->Surface;
  }

  if (Disabled && !Selected) {
    RowColor = ModernUiBlendColor (Theme->Surface, Theme->Background, 35);
  }

  return RowColor;
}

/**
  Draw a themed selectable row surface.

  This is a shared visual primitive for DisplayEngine statement rows and
  experimental app list rows. It does not own FormBrowser or HII semantics.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rect      Pixel rectangle for the row surface.
  @param[in] Selected  TRUE when the row is selected.
  @param[in] Disabled  TRUE when the row is visible but disabled or grayed.
  @param[in] Action    TRUE when the row represents an action-like command.
  @param[in] Subtitle  TRUE when the row is a section subtitle.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Row was drawn.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL, or Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiDrawSelectableRow (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN BOOLEAN                   Selected,
  IN BOOLEAN                   Disabled,
  IN BOOLEAN                   Action,
  IN BOOLEAN                   Subtitle,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  RowColor;
  EFI_STATUS                     Status;

  if ((Context == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  RowColor = ModernUiGetSelectableRowBackground (Selected, Disabled, Action, Subtitle, Theme);
  Status = ModernUiFillRect (Context, Rect, RowColor);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Selected && (Rect.Width > 6) && (Rect.Height > 4)) {
    if ((Rect.Width > 10) && (Rect.Height > 8)) {
      Status = ModernUiFillRect (
                 Context,
                 (MODERN_UI_RECT){ Rect.X + 6, Rect.Y + 3, Rect.Width - 6, Rect.Height - 6 },
                 ModernUiBlendColor (RowColor, Theme->AccentOrange, 18)
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){ Rect.X, Rect.Y, Rect.Width, 2 },
               ModernUiBlendColor (Theme->AccentYellow, Theme->AccentOrange, 45)
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){ Rect.X, Rect.Y + Rect.Height - 3, Rect.Width, 2 },
               ModernUiBlendColor (Theme->AccentOrange, Theme->SelectedBand, 35)
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){ Rect.X, Rect.Y + 2, 7, Rect.Height - 4 },
               Theme->AccentYellow
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (Rect.Width > 20) {
      Status = ModernUiStrokeRect (Context, Rect, Theme->PopupBorder);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      return ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){ Rect.X + 8, Rect.Y + 2, Rect.Width - 8, 1 },
               ModernUiBlendColor (Theme->AccentYellow, Theme->AccentOrange, 45)
               );
    }

    return EFI_SUCCESS;
  }

  if (Subtitle && (Rect.Width > 6)) {
    return ModernUiFillRect (
             Context,
             (MODERN_UI_RECT){ Rect.X, Rect.Y + Rect.Height - 1, Rect.Width, 1 },
             Theme->Border
             );
  }

  return EFI_SUCCESS;
}

/**
  Draw the standard border used around an action/selectable row.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rect      Row rectangle.
  @param[in] Selected  TRUE when the row is selected.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Row border was drawn.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL, or Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiDrawSelectableRowBorder (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN BOOLEAN                   Selected,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  if ((Context == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  return ModernUiStrokeRect (
           Context,
           Rect,
           Selected ? ModernUiBlendColor (Theme->AccentOrange, Theme->Border, 30) : Theme->Border
           );
}

/**
  Draw a value selector box with a trailing drop-down marker.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rect      Selector rectangle.
  @param[in] Value     Selected value text. Must not be NULL.
  @param[in] Selected  TRUE when the owning row is selected.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Selector was drawn.
  @retval EFI_INVALID_PARAMETER  Context, Value, or Theme is NULL, or Rect is
                                  empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
  @retval others                 Status returned by text rendering.
**/
EFI_STATUS
EFIAPI
ModernUiDrawValueBox (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN CONST CHAR16              *Value,
  IN BOOLEAN                   Selected,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  EFI_STATUS  Status;

  if ((Context == NULL) || (Value == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ModernUiFillRect (Context, Rect, Selected ? Theme->SelectedBand : Theme->Surface);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiStrokeRect (Context, Rect, Selected ? Theme->PopupBorder : Theme->Border);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawTextFit (
             Context,
             Rect.X + 16,
             Rect.Y + ((Rect.Height > 18) ? ((Rect.Height - 18) / 2) : 0),
             (Rect.Width > 48) ? (Rect.Width - 48) : Rect.Width,
             Value,
             Theme->Text,
             Selected ? Theme->SelectedBand : Theme->Surface
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return ModernUiDrawText (
           Context,
           Rect.X + Rect.Width - 26,
           Rect.Y + ((Rect.Height > 18) ? ((Rect.Height - 18) / 2) : 0),
           L"v",
           Theme->AccentYellow,
           Selected ? Theme->SelectedBand : Theme->Surface
           );
}

/**
  Draw a drop-down list frame.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Drop-down rectangle.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Drop-down frame was drawn.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL, or Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiDrawDropdownFrame (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  EFI_STATUS  Status;

  if ((Context == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ModernUiFillRect (Context, Rect, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiStrokeRect (Context, Rect, Theme->PopupBorder);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((Rect.Width <= 2) || (Rect.Height <= 2)) {
    return EFI_SUCCESS;
  }

  return ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X + 1, Rect.Y + 1, Rect.Width - 2, 1 }, Theme->GlowOrange);
}

/**
  Draw a horizontal progress bar.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Progress track rectangle.
  @param[in] Percent  Completion percentage. Values above 100 are clamped.
  @param[in] Track    Track color.
  @param[in] Fill     Fill color.

  @retval EFI_SUCCESS            Progress bar was drawn.
  @retval EFI_INVALID_PARAMETER  Context is NULL, GOP is unavailable, or Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
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
