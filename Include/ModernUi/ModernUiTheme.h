/** @file
  Modern setup UI theme definitions.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_THEME_H_
#define MODERN_UI_THEME_H_

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>

typedef struct {
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    Background;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    Surface;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    SurfaceRaised;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    Border;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    Accent;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    AccentSoft;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    Text;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    MutedText;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    Warning;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    Success;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    BackgroundBlack;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    HeaderPattern;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    AccentOrange;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    AccentYellow;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    GlowOrange;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    SelectedBand;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    PopupBorder;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    WarningText;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    TelemetryText;
} MODERN_UI_THEME;

/**
  Return the active built-in theme token table.

  The returned pointer is owned by ModernUiThemeLib and must not be freed or
  modified by the caller.

  @return Non-NULL pointer to immutable theme tokens.
**/
CONST MODERN_UI_THEME *
EFIAPI
ModernUiGetTheme (
  VOID
  );

/**
  Return a built-in theme token table for an app-owned runtime preference.

  The returned pointer is owned by ModernUiThemeLib and must not be freed or
  modified by the caller. Invalid preference identifiers fall back to the
  active build-time default theme without modifying any caller-owned state.

  @param[in] ThemeId  App-owned theme preference identifier.

  @return Non-NULL pointer to immutable theme tokens.
**/
CONST MODERN_UI_THEME *
EFIAPI
ModernUiGetThemeForPreference (
  IN UINT8  ThemeId
  );

#endif
