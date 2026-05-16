/** @file
  Theme values for the ModernSetupPkg prototype.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <ModernUi/ModernUiTheme.h>

#define RGB(r, g, b)  { (b), (g), (r), 0 }

STATIC CONST MODERN_UI_THEME  mTheme = {
  RGB (0x05, 0x08, 0x0D),  // Background
  RGB (0x0C, 0x13, 0x1C),  // Surface
  RGB (0x14, 0x20, 0x2D),  // SurfaceRaised
  RGB (0x2E, 0x42, 0x57),  // Border
  RGB (0x1E, 0xB4, 0xFF),  // Accent
  RGB (0x0B, 0x62, 0x91),  // AccentSoft
  RGB (0xF4, 0xF7, 0xFB),  // Text
  RGB (0xA8, 0xB7, 0xC6),  // MutedText
  RGB (0xFF, 0xC1, 0x47),  // Warning
  RGB (0x68, 0xD3, 0x84)   // Success
};

/**
  Return the active built-in theme token table.

  The returned pointer is owned by this library and must not be freed or
  modified by the caller.

  @return Non-NULL pointer to immutable theme tokens.
**/
CONST MODERN_UI_THEME *
EFIAPI
ModernUiGetTheme (
  VOID
  )
{
  return &mTheme;
}
