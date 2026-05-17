/** @file
  Theme values for the ModernSetupPkg prototype.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <ModernUi/ModernUiTheme.h>

#define RGB(r, g, b)  { (b), (g), (r), 0 }

STATIC CONST MODERN_UI_THEME  mTheme = {
  RGB (0x02, 0x02, 0x02),  // Background
  RGB (0x07, 0x08, 0x09),  // Surface
  RGB (0x10, 0x0C, 0x08),  // SurfaceRaised
  RGB (0x3C, 0x28, 0x12),  // Border
  RGB (0xFF, 0x6A, 0x00),  // Accent
  RGB (0x6D, 0x22, 0x06),  // AccentSoft
  RGB (0xF5, 0xF3, 0xEE),  // Text
  RGB (0x9B, 0x9A, 0x95),  // MutedText
  RGB (0xFF, 0xD2, 0x00),  // Warning
  RGB (0x68, 0xD3, 0x84),  // Success
  RGB (0x00, 0x00, 0x00),  // BackgroundBlack
  RGB (0x1E, 0x0E, 0x06),  // HeaderPattern
  RGB (0xFF, 0x6A, 0x00),  // AccentOrange
  RGB (0xFF, 0xE1, 0x00),  // AccentYellow
  RGB (0xFF, 0x8A, 0x14),  // GlowOrange
  RGB (0x62, 0x17, 0x04),  // SelectedBand
  RGB (0xF2, 0x66, 0x00),  // PopupBorder
  RGB (0xFF, 0xD2, 0x00),  // WarningText
  RGB (0xD9, 0xD7, 0xCD)   // TelemetryText
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
