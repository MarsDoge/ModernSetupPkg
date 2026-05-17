/** @file
  Theme values for the ModernSetupPkg prototype.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <ModernUi/ModernUiTheme.h>

#define RGB(r, g, b)  { (b), (g), (r), 0 }

STATIC CONST MODERN_UI_THEME  mTheme = {
  RGB (0x0A, 0x0A, 0x0A),  // Background
  RGB (0x13, 0x13, 0x13),  // Surface
  RGB (0x1C, 0x1C, 0x1C),  // SurfaceRaised
  RGB (0x55, 0x55, 0x55),  // Border
  RGB (0xC9, 0x10, 0x18),  // Accent
  RGB (0x58, 0x0B, 0x10),  // AccentSoft
  RGB (0xF2, 0xF2, 0xF0),  // Text
  RGB (0xA7, 0xA7, 0xA2),  // MutedText
  RGB (0xF4, 0xC4, 0x00),  // Warning
  RGB (0x68, 0xD3, 0x84),  // Success
  RGB (0x05, 0x05, 0x05),  // BackgroundBlack
  RGB (0x3A, 0x05, 0x08),  // HeaderPattern
  RGB (0xD6, 0x12, 0x1B),  // AccentOrange
  RGB (0xF6, 0xC8, 0x00),  // AccentYellow
  RGB (0xFF, 0x42, 0x30),  // GlowOrange
  RGB (0x70, 0x0B, 0x10),  // SelectedBand
  RGB (0xF0, 0xB8, 0x00),  // PopupBorder
  RGB (0xF6, 0xC8, 0x00),  // WarningText
  RGB (0xE1, 0xE1, 0xDC)   // TelemetryText
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
