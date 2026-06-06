/** @file
  LVGL-backed renderer library for ModernSetupPkg (experimental/lvgl-spike).

  This is a drop-in replacement for the hand-rolled GOP rasterizer
  (Library/ModernUiRendererLib). It implements the identical
  ModernUiRenderer.h API, but instead of issuing GOP fills and bitmap
  blits directly, every primitive is composited by LVGL's software draw
  pipeline:

    - All geometry (fills, borders, panels, rows, cards, progress, value
      boxes, drop-downs) is drawn with lv_draw_rect.
    - ASCII text is drawn with lv_draw_label using LVGL's Montserrat font.
    - Non-ASCII (CJK) runs fall back to the firmware HII font composited
      into the same buffer, because LVGL ships no bundled CJK coverage.

  The bridge is a persistent full-screen XRGB8888 canvas that acts as a
  shadow framebuffer. Each primitive renders into the canvas through an
  LVGL draw layer and then BLTs only its bounding region to the live GOP
  surface. This keeps the imperative (immediate-mode) renderer contract
  that ModernDisplayEngineDxe and ModernSetupApp depend on, while routing
  all actual pixel generation through LVGL.

  SetupBrowser / FormBrowser / HII ownership is unchanged; this library
  only paints.

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

#include <lvgl/lvgl.h>

#include <ModernUi/ModernUiRenderer.h>
#include "../ModernUiRendererLib/ModernUiGlyphs.h"
#include "../ModernUiRendererLib/ModernUiRendererInternal.h"

#define MODERN_UI_TEXT_CELL_HEIGHT    MODERN_UI_BUILTIN_GLYPH_HEIGHT

//
// LVGL bridge state. The library is statically linked into a single
// consuming module (the display engine or the app), so one shadow canvas
// per module instance is sufficient. mLvInitDone guards the one-time
// lv_uefi_init()/lv_init() handshake against repeated ModernUiRendererInit().
//
STATIC BOOLEAN                        mLvInitDone = FALSE;
STATIC BOOLEAN                        mLvglReady  = FALSE;
STATIC lv_display_t                   *mDisplay   = NULL;
STATIC lv_obj_t                       *mCanvas    = NULL;
STATIC EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *mCanvasBuf = NULL;
STATIC VOID                           *mDispBuf   = NULL;
STATIC UINTN                          mCanvasW    = 0;
STATIC UINTN                          mCanvasH    = 0;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL   *mGop       = NULL;
STATIC EFI_HII_FONT_PROTOCOL          *mFont      = NULL;


/**
  Convert a GOP BLT pixel to an LVGL color.

  @param[in] Pixel  Source GOP pixel. Reserved is ignored.

  @return Equivalent lv_color_t.
**/
STATIC
lv_color_t
ToLvColor (
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Pixel
  )
{
  return lv_color_make (Pixel.Red, Pixel.Green, Pixel.Blue);
}


/**
  No-op LVGL display flush callback.

  The shadow canvas is composited and BLT'd by this library directly; the
  bridge display is never refreshed through LVGL's timer pipeline, so this
  callback only acknowledges completion to satisfy the display contract.

  @param[in] Display  LVGL display being flushed.
  @param[in] Area     Flushed area. Unused.
  @param[in] PxMap    Pixel data. Unused.
**/
STATIC
VOID
LvglBridgeFlush (
  IN lv_display_t   *Display,
  IN const lv_area_t *Area,
  IN UINT8          *PxMap
  )
{
  (VOID)Area;
  (VOID)PxMap;
  lv_display_flush_ready (Display);
}


/**
  Push a rectangular region of the shadow canvas to the live GOP surface.

  The region is clipped to the canvas extent. NULL/empty regions and a
  not-yet-initialized bridge are ignored.

  @param[in] X       Left coordinate in canvas/screen pixels.
  @param[in] Y       Top coordinate in canvas/screen pixels.
  @param[in] Width   Region width in pixels.
  @param[in] Height  Region height in pixels.
**/
STATIC
VOID
BltCanvasRegion (
  IN UINTN  X,
  IN UINTN  Y,
  IN UINTN  Width,
  IN UINTN  Height
  )
{
  if (!mLvglReady || (mGop == NULL) || (mCanvasBuf == NULL)) {
    return;
  }

  if ((X >= mCanvasW) || (Y >= mCanvasH) || (Width == 0) || (Height == 0)) {
    return;
  }

  if ((X + Width) > mCanvasW) {
    Width = mCanvasW - X;
  }

  if ((Y + Height) > mCanvasH) {
    Height = mCanvasH - Y;
  }

  mGop->Blt (
          mGop,
          mCanvasBuf,
          EfiBltBufferToVideo,
          X,
          Y,
          X,
          Y,
          Width,
          Height,
          mCanvasW * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
          );
}


/**
  Initialize a render context and the LVGL shadow-canvas bridge.

  On the first call this performs the one-time LVGL UEFI handshake
  (lv_uefi_init() must precede lv_init()), then creates a bridge display
  and a full-screen XRGB8888 canvas sized to the active GOP mode. Later
  calls reuse the existing bridge and only refresh the cached context.

  @param[out] Context  Render context to initialize. Must not be NULL. On
                       success, Width and Height describe the active GOP mode.

  @retval EFI_SUCCESS            Context and bridge were initialized.
  @retval EFI_INVALID_PARAMETER  Context is NULL.
  @retval EFI_NOT_FOUND          GOP is unavailable or has no active mode.
  @retval EFI_OUT_OF_RESOURCES   Canvas allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiRendererInit (
  OUT MODERN_UI_RENDER_CONTEXT  *Context
  )
{
  EFI_STATUS  Status;
  UINTN       CanvasBytes;
  UINTN       DispBytes;

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

  //
  // One-time LVGL handshake. lv_uefi_init() caches the image handle and
  // system table for the LVGL UEFI backend and must run before lv_init().
  //
  if (!mLvInitDone) {
    lv_uefi_init (gImageHandle, gST);
    lv_init ();
    mLvInitDone = TRUE;
  }

  //
  // (Re)build the shadow canvas if it is missing or the mode changed.
  //
  if (!mLvglReady || (mCanvasW != Context->Width) || (mCanvasH != Context->Height)) {
    if (mCanvasBuf != NULL) {
      FreePool (mCanvasBuf);
      mCanvasBuf = NULL;
    }

    if (mDispBuf != NULL) {
      FreePool (mDispBuf);
      mDispBuf = NULL;
    }

    CanvasBytes = Context->Width * Context->Height * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
    mCanvasBuf  = AllocateZeroPool (CanvasBytes);
    if (mCanvasBuf == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    //
    // The bridge display is never refreshed through LVGL's timer pipeline,
    // but a small draw buffer keeps the display contract well-formed.
    //
    DispBytes = Context->Width * 16 * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
    mDispBuf  = AllocateZeroPool (DispBytes);
    if (mDispBuf == NULL) {
      FreePool (mCanvasBuf);
      mCanvasBuf = NULL;
      return EFI_OUT_OF_RESOURCES;
    }

    if (mDisplay == NULL) {
      mDisplay = lv_display_create ((int32_t)Context->Width, (int32_t)Context->Height);
      if (mDisplay == NULL) {
        FreePool (mCanvasBuf);
        FreePool (mDispBuf);
        mCanvasBuf = NULL;
        mDispBuf   = NULL;
        return EFI_OUT_OF_RESOURCES;
      }

      lv_display_set_flush_cb (mDisplay, LvglBridgeFlush);
    }

    lv_display_set_buffers (
      mDisplay,
      mDispBuf,
      NULL,
      (uint32_t)DispBytes,
      LV_DISPLAY_RENDER_MODE_PARTIAL
      );

    mCanvas = lv_canvas_create (lv_display_get_screen_active (mDisplay));
    if (mCanvas == NULL) {
      FreePool (mCanvasBuf);
      FreePool (mDispBuf);
      mCanvasBuf = NULL;
      mDispBuf   = NULL;
      return EFI_OUT_OF_RESOURCES;
    }

    lv_canvas_set_buffer (
      mCanvas,
      mCanvasBuf,
      (int32_t)Context->Width,
      (int32_t)Context->Height,
      LV_COLOR_FORMAT_XRGB8888
      );
  }

  mGop     = Context->Gop;
  mFont    = Context->Font;
  mCanvasW = Context->Width;
  mCanvasH = Context->Height;
  mLvglReady = TRUE;

  return EFI_SUCCESS;
}


/**
  Fill a rectangle by compositing an opaque lv_draw_rect into the shadow
  canvas, then BLT the affected region to the GOP surface.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Pixel rectangle. Zero width or height is invalid.
  @param[in] Color    Fill color.

  @retval EFI_SUCCESS            Rectangle was filled or clipped outside view.
  @retval EFI_INVALID_PARAMETER  Context is NULL, GOP is unavailable, or Rect is empty.
**/
EFI_STATUS
EFIAPI
ModernUiFillRect (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
  IN MODERN_UI_RECT                 Rect,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  lv_layer_t          Layer;
  lv_draw_rect_dsc_t  Dsc;
  lv_area_t           Coords;

  if ((Context == NULL) || (Context->Gop == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!mLvglReady || (mCanvas == NULL)) {
    return EFI_NOT_READY;
  }

  if ((Rect.X >= mCanvasW) || (Rect.Y >= mCanvasH)) {
    return EFI_SUCCESS;
  }

  if ((Rect.X + Rect.Width) > mCanvasW) {
    Rect.Width = mCanvasW - Rect.X;
  }

  if ((Rect.Y + Rect.Height) > mCanvasH) {
    Rect.Height = mCanvasH - Rect.Y;
  }

  lv_canvas_init_layer (mCanvas, &Layer);

  lv_draw_rect_dsc_init (&Dsc);
  Dsc.bg_color = ToLvColor (Color);
  Dsc.bg_opa   = LV_OPA_COVER;

  Coords.x1 = (int32_t)Rect.X;
  Coords.y1 = (int32_t)Rect.Y;
  Coords.x2 = (int32_t)(Rect.X + Rect.Width - 1);
  Coords.y2 = (int32_t)(Rect.Y + Rect.Height - 1);
  lv_draw_rect (&Layer, &Dsc, &Coords);

  lv_canvas_finish_layer (mCanvas, &Layer);

  BltCanvasRegion (Rect.X, Rect.Y, Rect.Width, Rect.Height);
  return EFI_SUCCESS;
}


/**
  Composite a UCS-2 run through the firmware HII font into the shadow canvas.

  Used for non-ASCII (CJK) runs, which LVGL's bundled Latin fonts do not
  cover. HII renders into the canvas bitmap (not direct-to-screen) so the
  run composites with the surrounding LVGL output and is BLT'd in one pass.

  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] Text        Null-terminated UCS-2 run. Must not be NULL.
  @param[in] PixelWidth  Pre-measured run width used for the BLT region.
  @param[in] Color       Text foreground color.
  @param[in] Background  Text background color.

  @retval EFI_SUCCESS            Run was rendered.
  @retval EFI_UNSUPPORTED        HII Font protocol is unavailable.
  @retval EFI_OUT_OF_RESOURCES   Temporary allocation failed.
  @retval others                 Status from StringToImage().
**/
STATIC
EFI_STATUS
DrawHiiCanvasRun (
  IN UINTN                          X,
  IN UINTN                          Y,
  IN CONST CHAR16                   *Text,
  IN UINTN                          PixelWidth,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background
  )
{
  EFI_IMAGE_OUTPUT       *Blt;
  EFI_FONT_DISPLAY_INFO  FontInfo;
  EFI_STATUS             Status;

  if (mFont == NULL) {
    return EFI_UNSUPPORTED;
  }

  Blt = AllocateZeroPool (sizeof (*Blt));
  if (Blt == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ZeroMem (&FontInfo, sizeof (FontInfo));
  FontInfo.ForegroundColor = Color;
  FontInfo.BackgroundColor = Background;

  //
  // Target the shadow canvas bitmap (Image.Bitmap), NOT the live screen, so
  // the run blends with the LVGL-drawn pixels already in the canvas.
  //
  Blt->Width        = (UINT16)mCanvasW;
  Blt->Height       = (UINT16)mCanvasH;
  Blt->Image.Bitmap = mCanvasBuf;

  Status = mFont->StringToImage (
                    mFont,
                    EFI_HII_IGNORE_IF_NO_GLYPH | EFI_HII_OUT_FLAG_CLIP |
                    EFI_HII_OUT_FLAG_CLIP_CLEAN_X | EFI_HII_OUT_FLAG_CLIP_CLEAN_Y |
                    EFI_HII_IGNORE_LINE_BREAK,
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

  BltCanvasRegion (X, Y, PixelWidth, MODERN_UI_TEXT_CELL_HEIGHT);
  return Status;
}


/**
  Composite one embedded bitmap glyph into the shadow canvas.

  ModernUiGlyphs.c carries the package's own CJK/icon glyph bitmaps (the
  alpha coverage the app uses for its localized labels). These are rendered
  here by alpha-blending each pixel against the canvas-supplied background,
  then BLT'd. This keeps the original glyph fidelity while everything else
  goes through LVGL; LVGL ships no CJK font, so this is the primary non-ASCII
  path and HII is only a fallback for code points without an embedded glyph.

  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] Glyph       Embedded glyph data. Must not be NULL.
  @param[in] Color       Foreground color.
  @param[in] Background  Background color blended under the glyph coverage.

  @retval EFI_SUCCESS            Glyph was composited.
  @retval EFI_INVALID_PARAMETER  Glyph is NULL or the bridge is not ready.
**/
STATIC
EFI_STATUS
DrawBuiltinGlyphCanvas (
  IN UINTN                          X,
  IN UINTN                          Y,
  IN CONST MODERN_UI_BUILTIN_GLYPH  *Glyph,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background
  )
{
  UINTN  Row;
  UINTN  Column;
  UINTN  Alpha;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Pixel;

  if ((Glyph == NULL) || !mLvglReady || (mCanvasBuf == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  for (Row = 0; (Row < Glyph->Height) && ((Y + Row) < mCanvasH); Row++) {
    for (Column = 0; (Column < Glyph->Width) && ((X + Column) < mCanvasW); Column++) {
      Alpha          = Glyph->Bitmap[Row * Glyph->Width + Column];
      Pixel          = &mCanvasBuf[(Y + Row) * mCanvasW + (X + Column)];
      Pixel->Red     = (UINT8)(((UINTN)Background.Red * (255 - Alpha) + (UINTN)Color.Red * Alpha) / 255);
      Pixel->Green   = (UINT8)(((UINTN)Background.Green * (255 - Alpha) + (UINTN)Color.Green * Alpha) / 255);
      Pixel->Blue    = (UINT8)(((UINTN)Background.Blue * (255 - Alpha) + (UINTN)Color.Blue * Alpha) / 255);
      Pixel->Reserved = 0;
    }
  }

  BltCanvasRegion (X, Y, Glyph->Width, Glyph->Height);
  return EFI_SUCCESS;
}


/**
  Draw an ASCII run with lv_draw_label, on a freshly painted background.

  The run background is filled and the label is drawn in a single LVGL
  draw layer so both composite into the shadow canvas before one BLT.

  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] Ascii       Null-terminated ASCII (UTF-8 compatible) run.
  @param[in] PixelWidth  Pre-measured run width used for background and BLT.
  @param[in] Color       Text foreground color.
  @param[in] Background  Background color painted behind the run.

  @retval EFI_SUCCESS  Run was rendered (or empty width ignored).
**/
STATIC
EFI_STATUS
DrawLvglAsciiRun (
  IN UINTN                          X,
  IN UINTN                          Y,
  IN CONST CHAR8                    *Ascii,
  IN UINTN                          PixelWidth,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background
  )
{
  lv_layer_t           Layer;
  lv_draw_rect_dsc_t   RectDsc;
  lv_draw_label_dsc_t  LabelDsc;
  lv_area_t            BgCoords;
  lv_area_t            TextCoords;
  int32_t              BottomY;
  UINTN                BltWidth;

  if ((PixelWidth == 0) || (X >= mCanvasW) || (Y >= mCanvasH)) {
    return EFI_SUCCESS;
  }

  if ((X + PixelWidth) > mCanvasW) {
    PixelWidth = mCanvasW - X;
  }

  BottomY = (int32_t)(Y + MODERN_UI_TEXT_CELL_HEIGHT - 1);
  if (BottomY >= (int32_t)mCanvasH) {
    BottomY = (int32_t)mCanvasH - 1;
  }

  //
  // The background is painted at the layout-reserved run width (measured on
  // the 8 px cell model the callers expect), but the label is clipped to the
  // canvas right edge so LVGL's variable-width glyphs are not truncated. Any
  // overshoot into a neighbouring column is repainted by the caller's next
  // column draw. The BLT covers the wider of the two so glyph overshoot
  // reaches the screen.
  //
  BgCoords.x1 = (int32_t)X;
  BgCoords.y1 = (int32_t)Y;
  BgCoords.x2 = (int32_t)(X + PixelWidth - 1);
  BgCoords.y2 = BottomY;

  TextCoords.x1 = (int32_t)X;
  TextCoords.y1 = (int32_t)Y;
  TextCoords.x2 = (int32_t)mCanvasW - 1;
  TextCoords.y2 = BottomY;

  lv_canvas_init_layer (mCanvas, &Layer);

  lv_draw_rect_dsc_init (&RectDsc);
  RectDsc.bg_color = ToLvColor (Background);
  RectDsc.bg_opa   = LV_OPA_COVER;
  lv_draw_rect (&Layer, &RectDsc, &BgCoords);

  lv_draw_label_dsc_init (&LabelDsc);
  LabelDsc.text  = (const char *)Ascii;
  LabelDsc.font  = LV_FONT_DEFAULT;
  LabelDsc.color = ToLvColor (Color);
  LabelDsc.opa   = LV_OPA_COVER;
  LabelDsc.align = LV_TEXT_ALIGN_LEFT;
  lv_draw_label (&Layer, &LabelDsc, &TextCoords);

  lv_canvas_finish_layer (mCanvas, &Layer);

  //
  // BLT enough width to carry glyph overshoot past the reserved cell width,
  // bounded by the canvas. ~60% slack covers Montserrat's wider caps.
  //
  BltWidth = PixelWidth + (PixelWidth * 3) / 5 + MODERN_UI_ASCII_CELL_WIDTH;
  if ((X + BltWidth) > mCanvasW) {
    BltWidth = mCanvasW - X;
  }

  BltCanvasRegion (X, Y, BltWidth, MODERN_UI_TEXT_CELL_HEIGHT);
  return EFI_SUCCESS;
}


/**
  Draw UCS-2 text by routing each homogeneous run to the best backend:
  text-mode graphic glyphs to LVGL rectangles, ASCII to lv_draw_label, and
  non-ASCII (CJK) to the firmware HII font composited into the canvas.

  @param[in] Context     Initialized render context. Must not be NULL.
  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] Text        Null-terminated UCS-2 string. Must not be NULL.
  @param[in] Color       Text foreground color.
  @param[in] Background  Background color.

  @retval EFI_SUCCESS            Text was rendered.
  @retval EFI_INVALID_PARAMETER  Context or Text is NULL.
  @retval others                 First error from a run backend.
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
  CHAR16                         Run16[MODERN_UI_TEXT_SEGMENT_MAX + 1];
  CHAR8                          Ascii[MODERN_UI_TEXT_SEGMENT_MAX + 1];
  UINTN                          Index;
  UINTN                          RunLen;
  UINTN                          CurrentX;
  UINTN                          RunWidth;
  EFI_STATUS                     Status;
  EFI_STATUS                     ReturnStatus;
  CONST MODERN_UI_BUILTIN_GLYPH  *Glyph;

  if ((Context == NULL) || (Context->Gop == NULL) || (Text == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!mLvglReady || (mCanvas == NULL)) {
    return EFI_NOT_READY;
  }

  CurrentX     = X;
  ReturnStatus = EFI_SUCCESS;
  Index        = 0;
  while (Text[Index] != L'\0') {
    //
    // UEFI text-mode graphic glyph: emit as LVGL rectangles, one cell each.
    //
    if (ModernUiIsTextModeGraphicGlyph (Text[Index])) {
      Status = ModernUiDrawTextModeGraphicGlyph (Context, CurrentX, Y, Text[Index], Color, Background);
      if (EFI_ERROR (Status)) {
        ReturnStatus = Status;
      }

      CurrentX += MODERN_UI_GRAPHIC_CELL_WIDTH;
      Index++;
      continue;
    }

    //
    // Non-ASCII (CJK / icon): prefer the package's embedded bitmap glyph
    // (LVGL ships no CJK font), accumulating code points without an embedded
    // glyph into an HII sub-run that is flushed at each glyph boundary.
    //
    if (Text[Index] > 0x7F) {
      RunLen = 0;
      while ((Text[Index] != L'\0') && (Text[Index] > 0x7F) && !ModernUiIsTextModeGraphicGlyph (Text[Index])) {
        Glyph = ModernUiFindBuiltinGlyph (Text[Index]);
        if (Glyph != NULL) {
          //
          // Flush any pending HII sub-run before the embedded glyph.
          //
          if (RunLen > 0) {
            Run16[RunLen] = L'\0';
            RunWidth      = ModernUiMeasureText (Run16);
            Status        = DrawHiiCanvasRun (CurrentX, Y, Run16, RunWidth, Color, Background);
            if (EFI_ERROR (Status)) {
              ReturnStatus = Status;
            }

            CurrentX += RunWidth;
            RunLen    = 0;
          }

          Status = DrawBuiltinGlyphCanvas (CurrentX, Y, Glyph, Color, Background);
          if (EFI_ERROR (Status)) {
            ReturnStatus = Status;
          }

          CurrentX += Glyph->Advance;
          Index++;
          continue;
        }

        if (RunLen >= MODERN_UI_TEXT_SEGMENT_MAX) {
          Run16[RunLen] = L'\0';
          RunWidth      = ModernUiMeasureText (Run16);
          Status        = DrawHiiCanvasRun (CurrentX, Y, Run16, RunWidth, Color, Background);
          if (EFI_ERROR (Status)) {
            ReturnStatus = Status;
          }

          CurrentX += RunWidth;
          RunLen    = 0;
        }

        Run16[RunLen++] = Text[Index++];
      }

      //
      // Flush the trailing HII sub-run, if any.
      //
      if (RunLen > 0) {
        Run16[RunLen] = L'\0';
        RunWidth      = ModernUiMeasureText (Run16);
        Status        = DrawHiiCanvasRun (CurrentX, Y, Run16, RunWidth, Color, Background);
        if (EFI_ERROR (Status)) {
          ReturnStatus = Status;
        }

        CurrentX += RunWidth;
      }

      continue;
    }

    //
    // ASCII run: LVGL lv_draw_label.
    //
    RunLen = 0;
    while ((Text[Index] != L'\0') && (Text[Index] <= 0x7F) &&
           !ModernUiIsTextModeGraphicGlyph (Text[Index]) && (RunLen < MODERN_UI_TEXT_SEGMENT_MAX))
    {
      Run16[RunLen] = Text[Index];
      Ascii[RunLen] = (CHAR8)Text[Index];
      RunLen++;
      Index++;
    }

    Run16[RunLen] = L'\0';
    Ascii[RunLen] = '\0';
    RunWidth      = ModernUiMeasureText (Run16);
    Status        = DrawLvglAsciiRun (CurrentX, Y, Ascii, RunWidth, Color, Background);
    if (EFI_ERROR (Status)) {
      ReturnStatus = Status;
    }

    CurrentX += RunWidth;
  }

  return ReturnStatus;
}


/**
  Convert a UCS-2 run to an ASCII label for LVGL's Latin fonts.

  Glyph-width markers and non-ASCII code points are dropped/replaced; CJK in a
  widget label is a known limitation of the widget path (handled elsewhere for
  primitive text).

  @param[out] Out  Destination ASCII buffer.
  @param[in]  Cap  Capacity of Out in bytes (>= 1).
  @param[in]  Src  Null-terminated UCS-2 source. May be NULL (empty result).
**/
STATIC
VOID
LvglAsciiLabel (
  OUT CHAR8         *Out,
  IN  UINTN         Cap,
  IN  CONST CHAR16  *Src
  )
{
  UINTN  SrcIdx;
  UINTN  DstIdx;

  DstIdx = 0;
  if (Src != NULL) {
    for (SrcIdx = 0; (Src[SrcIdx] != CHAR_NULL) && (DstIdx < (Cap - 1)); SrcIdx++) {
      if (Src[SrcIdx] >= 0xFFF0) {
        continue;
      }

      Out[DstIdx++] = ((Src[SrcIdx] >= 0x20) && (Src[SrcIdx] < 0x7F)) ? (CHAR8)Src[SrcIdx] : '?';
    }
  }

  Out[DstIdx] = '\0';
}

/**
  Apply the shared display-only control styling to an LVGL widget.

  Sizes the widget to the value lane and themes its surface/border to match the
  surrounding chrome. Per-widget text/symbol colors are set by the caller.

  @param[in] Obj       Widget object. Must not be NULL.
  @param[in] Rect      Control rectangle.
  @param[in] Selected  TRUE when the owning row is selected.
  @param[in] Theme     Theme token table. Must not be NULL.
**/
STATIC
VOID
LvglStyleControl (
  IN lv_obj_t               *Obj,
  IN MODERN_UI_RECT         Rect,
  IN BOOLEAN                Selected,
  IN CONST MODERN_UI_THEME  *Theme
  )
{
  lv_obj_set_size (Obj, (int32_t)Rect.Width, (int32_t)Rect.Height);
  lv_obj_set_pos (Obj, 0, 0);
  lv_obj_set_style_bg_color (Obj, ToLvColor (Selected ? Theme->SelectedBand : Theme->Surface), 0);
  lv_obj_set_style_bg_opa (Obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color (Obj, ToLvColor (Selected ? Theme->PopupBorder : Theme->Border), 0);
}

/**
  Snapshot a display-only widget and alpha-composite it into the shadow canvas.

  Lays out Obj, renders it to an ARGB8888 draw buffer via lv_snapshot_take, and
  alpha-blends the result over the row background already in the canvas (so the
  widget's rounded corners blend cleanly), then BLTs the region and deletes Obj.
  Falls back to the themed value box if layout/snapshot fails.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Obj       Widget object (deleted by this call). Must not be NULL.
  @param[in] Rect      Control rectangle.
  @param[in] Value     Original value text (for the fallback path).
  @param[in] Selected  TRUE when the owning row is selected.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval EFI_SUCCESS  Widget composited (or fell back successfully).
**/
STATIC
EFI_STATUS
LvglComposeSnapshot (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN lv_obj_t                  *Obj,
  IN MODERN_UI_RECT            Rect,
  IN CONST CHAR16              *Value,
  IN BOOLEAN                   Selected,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  lv_draw_buf_t                  *Snap;
  UINTN                          SnapW;
  UINTN                          SnapH;
  UINTN                          Stride;
  UINTN                          RowIdx;
  UINTN                          ColIdx;
  UINT8                          *SrcRow;
  UINT8                          *SrcPix;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Dst;
  UINT32                         Alpha;

  lv_obj_update_layout (Obj);

  Snap = lv_snapshot_take (Obj, LV_COLOR_FORMAT_ARGB8888);
  if (Snap == NULL) {
    lv_obj_delete (Obj);
    return ModernUiDrawValueBox (Context, Rect, Value, Selected, Theme);
  }

  SnapW  = Snap->header.w;
  SnapH  = Snap->header.h;
  Stride = Snap->header.stride;

  //
  // Alpha-composite ARGB8888 (B,G,R,A byte order, matching EFI BLT pixels) over
  // the row background already present in the canvas.
  //
  for (RowIdx = 0; (RowIdx < SnapH) && ((Rect.Y + RowIdx) < mCanvasH); RowIdx++) {
    Dst    = mCanvasBuf + (Rect.Y + RowIdx) * mCanvasW + Rect.X;
    SrcRow = Snap->data + RowIdx * Stride;
    for (ColIdx = 0; (ColIdx < SnapW) && ((Rect.X + ColIdx) < mCanvasW); ColIdx++, Dst++) {
      SrcPix = SrcRow + ColIdx * 4;
      Alpha  = SrcPix[3];
      if (Alpha == 0) {
        continue;
      }

      if (Alpha >= 255) {
        Dst->Blue  = SrcPix[0];
        Dst->Green = SrcPix[1];
        Dst->Red   = SrcPix[2];
      } else {
        Dst->Blue  = (UINT8)((SrcPix[0] * Alpha + Dst->Blue  * (255 - Alpha)) / 255);
        Dst->Green = (UINT8)((SrcPix[1] * Alpha + Dst->Green * (255 - Alpha)) / 255);
        Dst->Red   = (UINT8)((SrcPix[2] * Alpha + Dst->Red   * (255 - Alpha)) / 255);
      }
    }
  }

  lv_draw_buf_destroy (Snap);
  lv_obj_delete (Obj);

  BltCanvasRegion (
    Rect.X,
    Rect.Y,
    MIN (SnapW, mCanvasW - Rect.X),
    MIN (SnapH, mCanvasH - Rect.Y)
    );

  return EFI_SUCCESS;
}


/**
  LVGL one-of renderer: render a real `lv_dropdown` widget into the shadow canvas.

  This is the first IFR-opcode->LVGL-widget mapping (one-of -> lv_dropdown). The
  widget is display-only: a transient closed drop-down is created, labelled with
  the selected option, themed, and rendered to an off-screen buffer via
  `lv_snapshot_take`, then alpha-composited over the row background already in the
  canvas (so the widget's rounded corners blend cleanly). edk2 FormBrowser still
  owns the actual selection; we never route input into the widget. Falls back to
  the composed value box when LVGL is not ready, the rect is off-screen, or
  widget/snapshot allocation fails.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rect      Control rectangle (value lane).
  @param[in] Value     Selected option text. Must not be NULL.
  @param[in] Selected  TRUE when the owning row is selected.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Widget composited (or fell back successfully).
  @retval EFI_INVALID_PARAMETER  Context, Value, or Theme is NULL, or Rect empty.
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
  lv_obj_t  *Dropdown;
  CHAR8     Label[128];

  if ((Context == NULL) || (Value == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!mLvglReady || (mCanvas == NULL) || (Rect.X >= mCanvasW) || (Rect.Y >= mCanvasH)) {
    return ModernUiDrawValueBox (Context, Rect, Value, Selected, Theme);
  }

  LvglAsciiLabel (Label, sizeof (Label), Value);

  Dropdown = lv_dropdown_create (lv_display_get_screen_active (mDisplay));
  if (Dropdown == NULL) {
    return ModernUiDrawValueBox (Context, Rect, Value, Selected, Theme);
  }

  lv_dropdown_set_text (Dropdown, Label);
  LvglStyleControl (Dropdown, Rect, Selected, Theme);
  lv_obj_set_style_text_color (Dropdown, ToLvColor (Theme->Text), 0);

  return LvglComposeSnapshot (Context, Dropdown, Rect, Value, Selected, Theme);
}

/**
  Render an LVGL checkbox (display-only) for a checkbox/boolean control.

  The checkbox's checked state is inferred from an 'X'/'x' in the value text
  (the FormBrowser "[X]"/"[ ]" convention); the value text becomes the label with
  a leading "[.]" marker stripped. edk2 still owns the toggle. Falls back to the
  value box when LVGL is unavailable.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rect      Control rectangle.
  @param[in] Value     Value text (e.g. "[X] Enabled"). Must not be NULL.
  @param[in] Selected  TRUE when the owning row is selected.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval others  Status from the snapshot composite or value-box fallback.
**/
EFI_STATUS
EFIAPI
ModernUiRenderCheckbox (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  )
{
  lv_obj_t       *Checkbox;
  BOOLEAN        Checked;

  if ((Context == NULL) || (Value == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!mLvglReady || (mCanvas == NULL) || (Rect.X >= mCanvasW) || (Rect.Y >= mCanvasH)) {
    return ModernUiDrawValueBox (Context, Rect, Value, Selected, Theme);
  }

  //
  // Infer checked state from the "[X]"/"[ ]" marker. The box indicator alone
  // conveys the state (the prompt is already in the row's left label), so the
  // widget carries no text -- an empty label avoids leaking the raw "[X]"
  // bracket characters next to the box.
  //
  Checked = (BOOLEAN)((StrStr (Value, L"X") != NULL) || (StrStr (Value, L"x") != NULL));

  Checkbox = lv_checkbox_create (lv_display_get_screen_active (mDisplay));
  if (Checkbox == NULL) {
    return ModernUiDrawValueBox (Context, Rect, Value, Selected, Theme);
  }

  lv_checkbox_set_text (Checkbox, "");
  if (Checked) {
    lv_obj_add_state (Checkbox, LV_STATE_CHECKED);
  }

  LvglStyleControl (Checkbox, Rect, Selected, Theme);
  lv_obj_set_style_bg_opa (Checkbox, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width (Checkbox, 0, 0);
  lv_obj_set_style_text_color (Checkbox, ToLvColor (Selected ? Theme->Text : Theme->MutedText), 0);

  return LvglComposeSnapshot (Context, Checkbox, Rect, Value, Selected, Theme);
}

/**
  Render an LVGL text field (display-only) for string/numeric/password controls.

  Builds a one-line `lv_textarea` showing the current value (password mode masks
  it). edk2 still owns editing. Shared by the string, numeric, and password
  renderers. Falls back to the field box when LVGL is unavailable.

  @param[in] Context     Initialized render context. Must not be NULL.
  @param[in] Rect        Control rectangle.
  @param[in] Value       Value text. Must not be NULL.
  @param[in] Selected    TRUE when the owning row is selected.
  @param[in] Theme       Theme token table. Must not be NULL.
  @param[in] PasswordMode TRUE to mask the text as a password field.

  @retval others  Status from the snapshot composite or field-box fallback.
**/
STATIC
EFI_STATUS
LvglRenderTextField (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN CONST CHAR16              *Value,
  IN BOOLEAN                   Selected,
  IN CONST MODERN_UI_THEME     *Theme,
  IN BOOLEAN                   PasswordMode
  )
{
  lv_obj_t  *Field;
  lv_obj_t  *Label;
  CHAR8     Text[128];
  UINTN     Index;

  if ((Context == NULL) || (Value == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!mLvglReady || (mCanvas == NULL) || (Rect.X >= mCanvasW) || (Rect.Y >= mCanvasH)) {
    return ModernUiDrawFieldBox (Context, Rect, Value, Selected, Theme);
  }

  LvglAsciiLabel (Text, sizeof (Text), Value);
  if (PasswordMode) {
    for (Index = 0; Text[Index] != '\0'; Index++) {
      if (Text[Index] != ' ') {
        Text[Index] = '*';
      }
    }
  }

  //
  // A styled lv_obj container plus an lv_label is the reliable display-only
  // "field" rendering: a real LVGL widget surface, without lv_textarea's editing
  // cursor/scroll behavior (which obscures short text at row height).
  //
  Field = lv_obj_create (lv_display_get_screen_active (mDisplay));
  if (Field == NULL) {
    return ModernUiDrawFieldBox (Context, Rect, Value, Selected, Theme);
  }

  LvglStyleControl (Field, Rect, Selected, Theme);
  lv_obj_set_style_radius (Field, 4, 0);
  lv_obj_set_style_pad_all (Field, 0, 0);
  lv_obj_remove_flag (Field, LV_OBJ_FLAG_SCROLLABLE);

  Label = lv_label_create (Field);
  if (Label != NULL) {
    lv_label_set_text (Label, Text);
    lv_obj_set_style_text_color (Label, ToLvColor (Selected ? Theme->Text : Theme->MutedText), 0);
    lv_obj_align (Label, LV_ALIGN_LEFT_MID, 8, 0);
  }

  return LvglComposeSnapshot (Context, Field, Rect, Value, Selected, Theme);
}

/**
  LVGL string renderer: a real one-line lv_textarea. See ModernUiRenderString.
**/
EFI_STATUS
EFIAPI
ModernUiRenderString (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  )
{
  return LvglRenderTextField (Context, Rect, Value, Selected, Theme, FALSE);
}

/**
  LVGL password renderer: a masked lv_textarea. See ModernUiRenderPassword.
**/
EFI_STATUS
EFIAPI
ModernUiRenderPassword (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  )
{
  return LvglRenderTextField (Context, Rect, Value, Selected, Theme, TRUE);
}

/**
  LVGL numeric renderer: a real lv_textarea field showing the number.

  See ModernUiRenderNumeric. A spinbox-style numeric widget is a future
  refinement; the field already presents the value as a genuine LVGL control.
**/
EFI_STATUS
EFIAPI
ModernUiRenderNumeric (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  )
{
  return LvglRenderTextField (Context, Rect, Value, Selected, Theme, FALSE);
}

/**
  LVGL ordered-list renderer: a real list-style field showing the option order.

  Builds a styled `lv_obj` field with an `LV_SYMBOL_LIST` glyph prefixing the
  current option order, so an ordered list reads as a genuine list control rather
  than a cue glyph over native text. edk2 FormBrowser still owns the reorder popup
  and the actual ordering. Falls back to the field box when LVGL is unavailable.
  See ModernUiRenderOrderedList in ModernUiRenderer.h.
**/
EFI_STATUS
EFIAPI
ModernUiRenderOrderedList (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  )
{
  lv_obj_t  *Field;
  lv_obj_t  *Label;
  CHAR16    Norm[160];
  CHAR8     Text[128];
  CHAR8     Decorated[160];

  if ((Context == NULL) || (Value == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Join the CR-separated option order onto one line so it reads as a sequence
  // instead of showing the raw separators (see ModernUiNormalizeOrderedListText).
  //
  ModernUiNormalizeOrderedListText (Norm, ARRAY_SIZE (Norm), Value);

  if (!mLvglReady || (mCanvas == NULL) || (Rect.X >= mCanvasW) || (Rect.Y >= mCanvasH)) {
    return ModernUiDrawFieldBox (Context, Rect, Norm, Selected, Theme);
  }

  LvglAsciiLabel (Text, sizeof (Text), Norm);
  //
  // LV_SYMBOL_LIST is a UTF-8 glyph from the bundled Montserrat symbol set; it is
  // copied verbatim ahead of the ASCII order text to mark the field as a list.
  //
  AsciiSPrint (Decorated, sizeof (Decorated), "%a  %a", LV_SYMBOL_LIST, Text);

  Field = lv_obj_create (lv_display_get_screen_active (mDisplay));
  if (Field == NULL) {
    return ModernUiDrawFieldBox (Context, Rect, Norm, Selected, Theme);
  }

  LvglStyleControl (Field, Rect, Selected, Theme);
  lv_obj_set_style_radius (Field, 4, 0);
  lv_obj_set_style_pad_all (Field, 0, 0);
  lv_obj_remove_flag (Field, LV_OBJ_FLAG_SCROLLABLE);

  Label = lv_label_create (Field);
  if (Label != NULL) {
    lv_label_set_text (Label, Decorated);
    lv_obj_set_style_text_color (Label, ToLvColor (Selected ? Theme->Text : Theme->MutedText), 0);
    lv_obj_align (Label, LV_ALIGN_LEFT_MID, 8, 0);
  }

  return LvglComposeSnapshot (Context, Field, Rect, Norm, Selected, Theme);
}

/**
  LVGL date/time renderer: a segmented field showing month/day/year or H:M:S.

  Spaces the value around its `/ : -` delimiters so the segments read as discrete
  cells, then renders them centered in a styled `lv_obj` field. edk2 owns segment
  editing. In the in-setup DisplayEngine date/time keeps native per-segment
  rendering (see ModernUiRenderDateTime in ModernUiRenderer.h); this entry point
  serves the app-facing draw path. Falls back to the field box when LVGL is
  unavailable.
**/
EFI_STATUS
EFIAPI
ModernUiRenderDateTime (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  )
{
  lv_obj_t  *Field;
  lv_obj_t  *Label;
  CHAR8     Raw[64];
  CHAR8     Spaced[128];
  UINTN     Src;
  UINTN     Dst;
  CHAR8     Ch;

  if ((Context == NULL) || (Value == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!mLvglReady || (mCanvas == NULL) || (Rect.X >= mCanvasW) || (Rect.Y >= mCanvasH)) {
    return ModernUiDrawFieldBox (Context, Rect, Value, Selected, Theme);
  }

  LvglAsciiLabel (Raw, sizeof (Raw), Value);
  //
  // Pad each date/time delimiter with surrounding spaces so the segments read as
  // discrete cells (e.g. "06 / 05 / 2026"), without parsing the field layout.
  //
  for (Src = 0, Dst = 0; (Raw[Src] != '\0') && (Dst < (sizeof (Spaced) - 4)); Src++) {
    Ch = Raw[Src];
    if ((Ch == '/') || (Ch == ':') || (Ch == '-')) {
      if ((Dst > 0) && (Spaced[Dst - 1] != ' ')) {
        Spaced[Dst++] = ' ';
      }

      Spaced[Dst++] = Ch;
      Spaced[Dst++] = ' ';
    } else {
      Spaced[Dst++] = Ch;
    }
  }

  Spaced[Dst] = '\0';

  Field = lv_obj_create (lv_display_get_screen_active (mDisplay));
  if (Field == NULL) {
    return ModernUiDrawFieldBox (Context, Rect, Value, Selected, Theme);
  }

  LvglStyleControl (Field, Rect, Selected, Theme);
  lv_obj_set_style_radius (Field, 4, 0);
  lv_obj_set_style_pad_all (Field, 0, 0);
  lv_obj_remove_flag (Field, LV_OBJ_FLAG_SCROLLABLE);

  Label = lv_label_create (Field);
  if (Label != NULL) {
    lv_label_set_text (Label, Spaced);
    lv_obj_set_style_text_color (Label, ToLvColor (Selected ? Theme->Text : Theme->MutedText), 0);
    lv_obj_align (Label, LV_ALIGN_CENTER, 0, 0);
  }

  return LvglComposeSnapshot (Context, Field, Rect, Value, Selected, Theme);
}
