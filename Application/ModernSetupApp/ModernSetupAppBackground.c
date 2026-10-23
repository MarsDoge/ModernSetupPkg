/** @file App adapter to the image-local shared visual engine backdrop.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "ModernSetupAppInternal.h"
#include <ModernUi/ModernUiBackdrop.h>

VOID ModernSetupReleaseHomeBackground (VOID)
{
  ModernUiBackdropRelease ();
}

VOID ModernSetupDrawHomeSurface (IN MODERN_UI_RENDER_CONTEXT *Ui,
  IN MODERN_UI_RECT Rect, IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color, IN UINT8 Alpha)
{
  ModernUiBackdropSurface (Ui, Rect, Color, Alpha);
}

BOOLEAN ModernSetupDrawHomeBackground (IN MODERN_UI_RENDER_CONTEXT *Ui, IN MODERN_UI_RECT Region)
{
  return ModernUiBackdropPrepare (Ui, Region);
}

BOOLEAN ModernSetupDrawPageBase (IN MODERN_UI_RENDER_CONTEXT *Ui,
  IN CONST MODERN_UI_THEME *Theme, IN SETUP_PAGE Page)
{
  // One fixed full-screen crop for every App page and native form.
  if (ModernUiBackdropPrepare (Ui, (MODERN_UI_RECT){ 0, 0, Ui->Width, Ui->Height })) {
    return TRUE;
  }
  ModernUiClear (Ui, Theme->Background);
  return FALSE;
}
