/** @file
  Private shared declarations for the ModernSetupPkg renderer libraries.

  ModernUiRendererCommon.c implements the backend-agnostic renderer surface
  (geometry compositions, text measurement, themed widgets) on top of three
  backend primitives that each renderer provides: ModernUiRendererInit,
  ModernUiFillRect, and ModernUiDrawText. The GOP renderer
  (Library/ModernUiRendererLib) and the LVGL renderer
  (Library/ModernUiLvglRendererLib) share ModernUiRendererCommon.c and
  ModernUiGlyphs.c verbatim and supply only those three primitives.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_RENDERER_INTERNAL_H_
#define MODERN_UI_RENDERER_INTERNAL_H_

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <ModernUi/ModernUiRenderer.h>

//
// Fixed text-grid metrics shared by both renderers. The renderer measures and
// lays out on an 8 px ASCII cell / fixed-width CJK cell model so that callers
// computing positions from ModernUiMeasureText() are backend-stable.
//
#define MODERN_UI_ASCII_CELL_WIDTH    8
#define MODERN_UI_GRAPHIC_CELL_WIDTH  MODERN_UI_ASCII_CELL_WIDTH
#define MODERN_UI_GRAPHIC_LINE_Y      9
#define MODERN_UI_GRAPHIC_LINE_X      4
#define MODERN_UI_TEXT_SEGMENT_MAX    96
#define MODERN_UI_TARGET_WIDTH        1024
#define MODERN_UI_TARGET_HEIGHT       768

/**
  Select a preferred GOP mode when the active mode is smaller than the target.

  @param[in] Gop  Graphics output protocol to inspect. Must not be NULL.

  @retval EFI_SUCCESS            Current mode is acceptable or a better mode
                                 was selected.
  @retval EFI_INVALID_PARAMETER  Gop or mode data is NULL.
**/
EFI_STATUS
ModernUiSelectPreferredGopMode (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop
  );

/**
  Return whether a UCS-2 character is a UEFI text-mode graphics character that
  should be rendered as a narrow fixed-width shape.

  @param[in] CodePoint  UCS-2 code point to classify.

  @retval TRUE   CodePoint is a box, arrow, triangle, or checkbox glyph used by
                 edk2 text-mode setup UI.
  @retval FALSE  CodePoint should use normal text or built-in glyph rendering.
**/
BOOLEAN
ModernUiIsTextModeGraphicGlyph (
  IN CHAR16  CodePoint
  );

/**
  Draw one UEFI text-mode graphics character as narrow renderer primitives.

  The glyph is composed entirely from ModernUiFillRect/ModernUiStrokeRect, so it
  renders identically through either backend.

  @param[in] Context    Initialized render context. Must not be NULL.
  @param[in] X          Left coordinate in pixels.
  @param[in] Y          Top coordinate in pixels.
  @param[in] CodePoint  Box, arrow, triangle, or checkbox glyph to render.
  @param[in] Color      Foreground color.
  @param[in] Background  Background fill color.

  @retval EFI_SUCCESS            Glyph was rendered.
  @retval EFI_INVALID_PARAMETER  Context is NULL or GOP is unavailable.
  @retval others                 Status from fill or stroke primitives.
**/
EFI_STATUS
ModernUiDrawTextModeGraphicGlyph (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
  IN UINTN                          X,
  IN UINTN                          Y,
  IN CHAR16                         CodePoint,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background
  );

#endif // MODERN_UI_RENDERER_INTERNAL_H_
