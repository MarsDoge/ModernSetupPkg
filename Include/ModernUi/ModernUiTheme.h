/** @file
  Modern setup UI theme definitions.

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
} MODERN_UI_THEME;

CONST MODERN_UI_THEME *
EFIAPI
ModernUiGetTheme (
  VOID
  );

#endif
