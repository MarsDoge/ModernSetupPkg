/** @file
  Theme values for the ModernSetupPkg prototype.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <ModernUi/ModernUiTheme.h>
#include <Library/PcdLib.h>

#define RGB(r, g, b)  { (b), (g), (r), 0 }

STATIC CONST MODERN_UI_THEME  mOrangeTheme = {
  RGB (0x02, 0x02, 0x02),  // Background
  RGB (0x07, 0x08, 0x09),  // Surface
  RGB (0x0E, 0x0E, 0x0D),  // SurfaceRaised
  RGB (0x1D, 0x1D, 0x1B),  // Border
  RGB (0xFF, 0x6A, 0x00),  // Accent
  RGB (0x4A, 0x18, 0x05),  // AccentSoft
  RGB (0xF5, 0xF3, 0xEE),  // Text
  RGB (0x9B, 0x9A, 0x95),  // MutedText
  RGB (0xFF, 0xD2, 0x00),  // Warning
  RGB (0x68, 0xD3, 0x84),  // Success
  RGB (0x00, 0x00, 0x00),  // BackgroundBlack
  RGB (0x1E, 0x0E, 0x06),  // HeaderPattern
  RGB (0xFF, 0x6A, 0x00),  // AccentOrange
  RGB (0xFF, 0xE1, 0x00),  // AccentYellow
  RGB (0xFF, 0x8A, 0x14),  // GlowOrange
  RGB (0xB4, 0x38, 0x06),  // SelectedBand
  RGB (0xF2, 0x66, 0x00),  // PopupBorder
  RGB (0xFF, 0xD2, 0x00),  // WarningText
  RGB (0xD9, 0xD7, 0xCD)   // TelemetryText
};

STATIC CONST MODERN_UI_THEME  mRedTheme = {
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
  if (FixedPcdGet8 (PcdModernSetupTheme) == 1) {
    return &mRedTheme;
  }

  return &mOrangeTheme;
}
