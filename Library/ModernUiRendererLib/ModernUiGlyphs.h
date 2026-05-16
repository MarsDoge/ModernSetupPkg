/** @file
  Built-in bitmap glyph declarations for ModernUiRendererLib.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_GLYPHS_H_
#define MODERN_UI_GLYPHS_H_

#include <Uefi.h>

#define MODERN_UI_BUILTIN_GLYPH_WIDTH   18
#define MODERN_UI_BUILTIN_GLYPH_HEIGHT  18

typedef struct {
  CHAR16    CodePoint;
  UINT8     Width;
  UINT8     Height;
  UINT8     Advance;
  UINT8     Bitmap[MODERN_UI_BUILTIN_GLYPH_WIDTH * MODERN_UI_BUILTIN_GLYPH_HEIGHT];
} MODERN_UI_BUILTIN_GLYPH;

/**
  Find one built-in bitmap glyph.

  @param[in] CodePoint  UCS-2 code point to look up.

  @return Pointer to immutable glyph data, or NULL when the glyph is absent.
**/
CONST MODERN_UI_BUILTIN_GLYPH *
ModernUiFindBuiltinGlyph (
  IN CHAR16  CodePoint
  );

#endif
