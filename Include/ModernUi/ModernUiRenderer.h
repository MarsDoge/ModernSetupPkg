/** @file
  GOP-backed renderer for ModernSetupPkg.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_RENDERER_H_
#define MODERN_UI_RENDERER_H_

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/HiiFont.h>

#include <ModernUi/ModernUiTheme.h>

typedef struct {
  UINTN    X;
  UINTN    Y;
  UINTN    Width;
  UINTN    Height;
} MODERN_UI_RECT;

typedef struct {
  EFI_GRAPHICS_OUTPUT_PROTOCOL    *Gop;
  EFI_HII_FONT_PROTOCOL           *Font;
  UINTN                           Width;
  UINTN                           Height;
} MODERN_UI_RENDER_CONTEXT;

EFI_STATUS
EFIAPI
ModernUiRendererInit (
  OUT MODERN_UI_RENDER_CONTEXT  *Context
  );

EFI_STATUS
EFIAPI
ModernUiClear (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Color
  );

EFI_STATUS
EFIAPI
ModernUiFillRect (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Color
  );

EFI_STATUS
EFIAPI
ModernUiStrokeRect (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Color
  );

EFI_STATUS
EFIAPI
ModernUiDrawText (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN UINTN                             X,
  IN UINTN                             Y,
  IN CONST CHAR16                      *Text,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Background
  );

EFI_STATUS
EFIAPI
ModernUiDrawPanel (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST MODERN_UI_THEME             *Theme
  );

EFI_STATUS
EFIAPI
ModernUiDrawProgress (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN UINTN                             Percent,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Track,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Fill
  );

#endif
