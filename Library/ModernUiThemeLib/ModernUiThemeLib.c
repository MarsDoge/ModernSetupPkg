/** @file
  Theme values for the ModernSetupPkg prototype.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <ModernUi/ModernUiTheme.h>

#define RGB(r, g, b)  { (b), (g), (r), 0 }

STATIC CONST MODERN_UI_THEME  mTheme = {
  RGB (0x08, 0x0D, 0x14),  // Background
  RGB (0x10, 0x18, 0x24),  // Surface
  RGB (0x18, 0x24, 0x34),  // SurfaceRaised
  RGB (0x2B, 0x3A, 0x4D),  // Border
  RGB (0x16, 0xA3, 0xFF),  // Accent
  RGB (0x0C, 0x4D, 0x78),  // AccentSoft
  RGB (0xF2, 0xF6, 0xFB),  // Text
  RGB (0x9A, 0xAA, 0xBD),  // MutedText
  RGB (0xFF, 0xB0, 0x20),  // Warning
  RGB (0x52, 0xD2, 0x73)   // Success
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
