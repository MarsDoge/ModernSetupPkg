/** @file
  Shared ModernSetup visual engine implementation.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <ModernUi/ModernUiEngine.h>

#define MODERN_UI_ENGINE_RIGHT_RAIL_MIN_WIDTH  1000
#define MODERN_UI_ROW_VALUE_LANE_WIDTH         300
#define MODERN_UI_ROW_VALUE_BOX_WIDTH          280
#define MODERN_UI_ROW_VALUE_LANE_GAP           16
#define MODERN_UI_ROW_VALUE_LANE_MIN_WIDTH     (MODERN_UI_ROW_VALUE_LANE_WIDTH + MODERN_UI_ROW_VALUE_LANE_GAP + 80)

//
// Base metric tokens for the Modern UI shape vocabulary. Primitives derive their
// geometry from these named tokens instead of repeating raw pixel offsets, so a
// row/box/pill stays consistently aligned and can be polished in one place.
//
// MODERN_UI_TEXT_LINE_HEIGHT mirrors the renderer's built-in glyph cell height
// (MODERN_UI_BUILTIN_GLYPH_HEIGHT) and is the unit used to vertically centre a
// single line of text inside a box. MODERN_UI_BOX_TEXT_INSET is the symmetric
// left/right padding for text inside a row or value box.
//
#define MODERN_UI_TEXT_LINE_HEIGHT             18
#define MODERN_UI_BOX_TEXT_INSET               16

/**
  Return a display string for the current build architecture.

  @retval CHAR16*  Static architecture string for the active MDE_CPU_* target.
**/
STATIC
CONST CHAR16 *
GetArchitectureName (
  VOID
  )
{
#if defined (MDE_CPU_AARCH64)
  return L"AARCH64";
#elif defined (MDE_CPU_ARM)
  return L"ARM";
#elif defined (MDE_CPU_X64)
  return L"X64";
#elif defined (MDE_CPU_IA32)
  return L"IA32";
#elif defined (MDE_CPU_LOONGARCH64)
  return L"LOONGARCH64";
#elif defined (MDE_CPU_RISCV64)
  return L"RISCV64";
#else
  return L"UNKNOWN";
#endif
}

/**
  Return a display string for the current virtual platform family.

  @retval CHAR16*  Static platform string inferred from the active architecture.
**/
STATIC
CONST CHAR16 *
GetPlatformName (
  VOID
  )
{
#if defined (MDE_CPU_LOONGARCH64)
  return L"LoongArchVirt / QEMU";
#elif defined (MDE_CPU_AARCH64) || defined (MDE_CPU_ARM)
  return L"ArmVirt / QEMU";
#else
  return L"UEFI platform";
#endif
}

/**
  Return the top Y that vertically centres a single text line within a box.

  Centres one MODERN_UI_TEXT_LINE_HEIGHT line inside a box of the given height.
  When the box is no taller than a line the text is top-aligned (offset zero).

  @param[in] BoxY       Top pixel Y of the box.
  @param[in] BoxHeight  Box height in pixels.

  @return Pixel Y at which to draw the text line.
**/
STATIC
UINTN
ModernUiBoxTextY (
  IN UINTN  BoxY,
  IN UINTN  BoxHeight
  )
{
  return BoxY + ((BoxHeight > MODERN_UI_TEXT_LINE_HEIGHT) ? ((BoxHeight - MODERN_UI_TEXT_LINE_HEIGHT) / 2) : 0);
}

/**
  Draw a procedural header pattern band.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Pixel rectangle to fill. Width and height must be nonzero.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Pattern was drawn.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL, or Rect is empty.
  @retval others                 Status returned by renderer primitives.
**/
STATIC
EFI_STATUS
DrawPatternBand (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  TopSheen;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Baseline;
  EFI_STATUS                     Status;

  if ((Context == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Modern flat status bar: a single faint top sheen line plus a slightly
  // stronger baseline hairline give the band quiet depth and a clean shelf
  // separation, replacing the older vertical "vent" bars and horizontal
  // striations that read as dated firmware texture.
  //
  TopSheen = ModernUiBlendColor (Theme->HeaderPattern, Theme->Text, 8);
  Baseline = ModernUiBlendColor (Theme->HeaderPattern, Theme->Border, 40);

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y, Rect.Width, 1 }, TopSheen);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Rect.Height > 1) {
    Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y + Rect.Height - 1, Rect.Width, 1 }, Baseline);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Fill the header band: a solid title strip on top, fading to black below.

  The top @c SolidTop pixels stay solid @c HeaderPattern so the header text
  (drawn against a @c HeaderPattern background by the caller) sits on a matching
  fill with no glyph-cell halo. Below that strip the band eases from
  @c HeaderPattern down to @c BackgroundBlack, replacing the older hard
  top-half/bottom-half two-tone split that left a visible seam across the middle
  of the header. The fade starts at @c HeaderPattern so there is no seam where
  the solid strip meets the gradient. For very short headers the solid strip is
  capped at half the height and the fade fills whatever remains.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Pixel rectangle to fill. Width and height must be nonzero.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Header band was drawn.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL, or Rect is empty.
  @retval others                 Status returned by renderer primitives.
**/
STATIC
EFI_STATUS
DrawHeaderGradient (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  UINTN       SolidTop;
  UINTN       FadeHeight;
  UINTN       BandCount;
  UINTN       BandHeight;
  UINTN       Index;
  UINTN       BandY;
  UINTN       Ratio;
  EFI_STATUS  Status;

  if ((Context == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // 26px clears the 18px header glyph row plus its top inset, so the title,
  // mode, and clock all sit on solid HeaderPattern. Short headers fall back to
  // a half-height strip.
  //
  SolidTop = (Rect.Height >= 36) ? 26 : (Rect.Height / 2);
  Status   = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y, Rect.Width, SolidTop }, Theme->HeaderPattern);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FadeHeight = Rect.Height - SolidTop;
  if (FadeHeight == 0) {
    return EFI_SUCCESS;
  }

  //
  // Render the fade as a few horizontal bands. Each descends a little further
  // toward BackgroundBlack; the first band stays at HeaderPattern (ratio 0) so
  // it meets the solid strip seamlessly, and the last band absorbs any rounding
  // remainder so the wash reaches the exact baseline without spilling past it.
  //
  BandCount  = (FadeHeight >= 6) ? 6 : FadeHeight;
  BandHeight = FadeHeight / BandCount;
  for (Index = 0; Index < BandCount; Index++) {
    BandY  = Rect.Y + SolidTop + (Index * BandHeight);
    Ratio  = (BandCount > 1) ? ((Index * 82) / (BandCount - 1)) : 82;
    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){
                 Rect.X,
                 BandY,
                 Rect.Width,
                 (Index == (BandCount - 1)) ? ((Rect.Y + Rect.Height) - BandY) : BandHeight
               },
               ModernUiBlendColor (Theme->HeaderPattern, Theme->BackgroundBlack, Ratio)
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Draw a three-pixel accent glow strip.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Pixel rectangle for the glow. Width must be nonzero and
                      height must be at least three pixels.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Glow strip was drawn.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL, or Rect is too small.
  @retval others                 Status returned by renderer primitives.
**/
STATIC
EFI_STATUS
DrawGlowStrip (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  EFI_STATUS  Status;

  if ((Context == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height < 3)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y, Rect.Width, 1 }, Theme->GlowOrange);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y + 1, Rect.Width, 1 }, Theme->AccentOrange);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y + 2, Rect.Width, 1 }, Theme->AccentYellow);
}

/**
  Draw the optional right-side platform telemetry rail.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Pixel rectangle to fill. Width and height must be nonzero.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Rail was drawn.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL, or Rect is empty.
  @retval others                 Status returned by renderer primitives.
**/
STATIC
EFI_STATUS
DrawRightRail (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  UINTN       X;
  UINTN       Y;
  UINTN       SeparatorWidth;
  EFI_STATUS  Status;

  if ((Context == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ModernUiFillRect (Context, Rect, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y, 1, Rect.Height }, Theme->Border);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  X = Rect.X + 16;
  Y = Rect.Y + 18;
  SeparatorWidth = (Rect.Width > 24) ? (Rect.Width - 24) : Rect.Width;

  Status = ModernUiDrawText (Context, X, Y, L"CPU", Theme->AccentYellow, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawText (Context, X, Y + 28, L"Architecture", Theme->MutedText, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawText (Context, X, Y + 48, GetArchitectureName (), Theme->TelemetryText, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawText (Context, X, Y + 82, L"Platform", Theme->MutedText, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawText (Context, X, Y + 102, GetPlatformName (), Theme->TelemetryText, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X + 12, Y + 136, SeparatorWidth, 1 }, Theme->Border);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawText (Context, X, Y + 158, L"Memory", Theme->AccentYellow, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawText (Context, X, Y + 188, L"Provided by", Theme->MutedText, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawText (Context, X, Y + 208, L"UEFI memory map", Theme->TelemetryText, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X + 12, Y + 242, SeparatorWidth, 1 }, Theme->Border);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawText (Context, X, Y + 264, L"Voltage", Theme->AccentYellow, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawText (Context, X, Y + 294, L"Sensor provider", Theme->MutedText, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawText (Context, X, Y + 314, L"N/A", Theme->TelemetryText, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ModernUiEngineInit (
  OUT MODERN_UI_RENDER_CONTEXT  *Context
  )
{
  return ModernUiRendererInit (Context);
}

EFI_STATUS
EFIAPI
ModernUiEngineComputeLayout (
  IN  CONST MODERN_UI_RENDER_CONTEXT  *Context,
  IN  UINTN                           HeaderHeight,
  IN  UINTN                           FooterHeight,
  IN  UINTN                           Margin,
  IN  UINTN                           RightRailWidth,
  OUT MODERN_UI_LAYOUT                *Layout
  )
{
  if ((Context == NULL) || (Layout == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Layout, sizeof (*Layout));
  Layout->Header = (MODERN_UI_RECT){ 0, 0, Context->Width, HeaderHeight };
  Layout->Footer = (MODERN_UI_RECT){ 0, (Context->Height > FooterHeight) ? (Context->Height - FooterHeight) : 0, Context->Width, FooterHeight };
  Layout->Content = (MODERN_UI_RECT){
                      Margin,
                      HeaderHeight,
                      (Context->Width > (Margin * 2)) ? (Context->Width - (Margin * 2)) : Context->Width,
                      (Context->Height > (HeaderHeight + FooterHeight)) ? (Context->Height - HeaderHeight - FooterHeight) : 0
                    };
  Layout->TabBar = Layout->Header;

  Layout->RightRailVisible = (BOOLEAN)((Context->Width >= MODERN_UI_ENGINE_RIGHT_RAIL_MIN_WIDTH) &&
                                       (RightRailWidth > 0) &&
                                       (Layout->Content.Width > (RightRailWidth + 360)));
  if (Layout->RightRailVisible) {
    Layout->RightRail = (MODERN_UI_RECT){
                         Layout->Content.X + Layout->Content.Width - RightRailWidth,
                         Layout->Content.Y,
                         RightRailWidth,
                         Layout->Content.Height
                       };
    Layout->Content.Width -= RightRailWidth + Margin;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ModernUiEngineDrawTabs (
  IN MODERN_UI_RENDER_CONTEXT     *Context,
  IN MODERN_UI_RECT               Rect,
  IN CONST MODERN_UI_TAB_MODEL    *Tabs,
  IN UINTN                        TabCount,
  IN UINTN                        SelectedTab,
  IN CONST MODERN_UI_THEME        *Theme
  )
{
  UINTN       TabWidth;
  UINTN       TabIndex;
  UINTN       X;
  UINTN       TextWidth;
  UINTN       TabY;
  MODERN_UI_RECT TabRect;
  EFI_STATUS  Status;

  if ((Context == NULL) || (Theme == NULL) || ((TabCount > 0) && (Tabs == NULL)) || (Rect.Width == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if (TabCount == 0) {
    return EFI_SUCCESS;
  }

  TabY     = Rect.Y + ((Rect.Height > 52) ? (Rect.Height - 52) : 0);
  TabWidth = Rect.Width / TabCount;
  for (TabIndex = 0; TabIndex < TabCount; TabIndex++) {
    X         = Rect.X + (TabIndex * TabWidth);
    TextWidth = ModernUiMeasureText (Tabs[TabIndex].Text);
    TabRect   = (MODERN_UI_RECT){ X + 14, TabY + 2, (TabWidth > 28) ? (TabWidth - 28) : TabWidth, 28 };
    if (TabIndex == SelectedTab) {
      //
      // Modern flat tab: a soft background tint marks the active tab without a
      // hard outline or left bar; the bright underline indicator drawn below is
      // the primary affordance.
      //
      Status = ModernUiFillRect (
                 Context,
                 TabRect,
                 ModernUiBlendColor (Theme->BackgroundBlack, Theme->SelectedBand, 34)
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){ X + 18, TabY + 30, (TabWidth > 36) ? (TabWidth - 36) : TabWidth, (TabIndex == SelectedTab) ? 3 : 1 },
               (TabIndex == SelectedTab) ? Theme->AccentYellow : ModernUiBlendColor (Theme->BackgroundBlack, Theme->Border, 60)
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = ModernUiDrawTextFit (
               Context,
               X + ((TabWidth > TextWidth) ? ((TabWidth - TextWidth) / 2) : 8),
               TabY + 8,
               (TabWidth > 24) ? (TabWidth - 24) : TabWidth,
               Tabs[TabIndex].Text,
               (TabIndex == SelectedTab) ? Theme->AccentYellow : ModernUiBlendColor (Theme->Text, Theme->AccentOrange, 35),
               (TabIndex == SelectedTab) ? ModernUiBlendColor (Theme->BackgroundBlack, Theme->SelectedBand, 34) : Theme->BackgroundBlack
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
ModernUiEngineStatusColor (
  IN CONST CHAR16           *StatusText,
  IN CONST MODERN_UI_THEME  *Theme
  )
{
  if ((StatusText == NULL) || (Theme == NULL)) {
    return (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){ 0, 0, 0, 0 };
  }

  if ((StrCmp (StatusText, L"UNSAVED CHANGES") == 0) || (StrCmp (StatusText, L"REBOOT REQUIRED") == 0)) {
    return Theme->Warning;
  }

  if (StrCmp (StatusText, L"LIVE REFRESH") == 0) {
    return Theme->Success;
  }

  if (StrCmp (StatusText, L"MODAL VIEW") == 0) {
    return Theme->AccentYellow;
  }

  return Theme->AccentOrange;
}

/**
  Draw a status pill: the base badge shape of the Modern UI vocabulary.

  Fills the pill body with a faint accent-tinted surface, paints a solid accent
  bar down the left edge plus a one-pixel top sheen, then draws the label
  vertically centred (via ModernUiBoxTextY) and inset past the accent bar. The
  caller owns the pill's position and size through Rect; the interior geometry
  derives from the shared text tokens so the label never crowds the edges (the
  earlier inline chip pinned an 18px label into a 20px box, which looked clipped).

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Pill bounding box. Width and height must be nonzero.
  @param[in] Text     Label text. Must not be NULL.
  @param[in] Accent   Accent color for the left bar and the label.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Pill was drawn.
  @retval EFI_INVALID_PARAMETER  A required pointer is NULL or Rect is empty.
  @retval others                 Status returned by renderer primitives.
**/
STATIC
EFI_STATUS
ModernUiEngineDrawStatusPill (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
  IN MODERN_UI_RECT                 Rect,
  IN CONST CHAR16                   *Text,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Accent,
  IN CONST MODERN_UI_THEME          *Theme
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Fill;
  UINTN                          TextWidth;
  EFI_STATUS                     Status;

  if ((Context == NULL) || (Text == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Fill = ModernUiBlendColor (Theme->BackgroundBlack, Accent, 16);

  Status = ModernUiFillRect (Context, Rect, Fill);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y, 4, Rect.Height }, Accent);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Rect.Width > 4) {
    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){ Rect.X + 4, Rect.Y, Rect.Width - 4, 1 },
               ModernUiBlendColor (Accent, Theme->BackgroundBlack, 55)
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  TextWidth = (Rect.Width > (MODERN_UI_BOX_TEXT_INSET * 2)) ? (Rect.Width - (MODERN_UI_BOX_TEXT_INSET * 2)) : Rect.Width;
  return ModernUiDrawTextFit (
           Context,
           Rect.X + MODERN_UI_BOX_TEXT_INSET,
           ModernUiBoxTextY (Rect.Y, Rect.Height),
           TextWidth,
           Text,
           Accent,
           Fill
           );
}

EFI_STATUS
EFIAPI
ModernUiEngineDrawFooter (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN MODERN_UI_RECT            Rect,
  IN CONST CHAR16              *StatusText,
  IN CONST MODERN_UI_THEME     *Theme
  )
{
  EFI_STATUS                     Status;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  StatusColor;
  UINTN                          ChipWidth;
  UINTN                          TextWidth;
  UINTN                          MaxChipWidth;

  if ((Context == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ModernUiFillRect (Context, Rect, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y, Rect.Width, 1 }, Theme->Border);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Rect.Height > 2) {
    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){ Rect.X, Rect.Y + 1, Rect.Width, 1 },
               ModernUiBlendColor (Theme->BackgroundBlack, Theme->AccentOrange, 25)
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  //
  // Status pill, drawn through the shared pill primitive. The pill sits just
  // under the footer's top border and is sized to comfortably contain one text
  // line (height = line + 6) so the label stays vertically centred instead of
  // crammed. The height guard keeps the pill from overflowing a short footer.
  //
  if ((StatusText != NULL) && (StatusText[0] != CHAR_NULL) && (Rect.Width > 80) &&
      (Rect.Height >= (MODERN_UI_TEXT_LINE_HEIGHT + 12)))
  {
    StatusColor  = ModernUiEngineStatusColor (StatusText, Theme);
    TextWidth    = ModernUiMeasureText (StatusText);
    MaxChipWidth = (Rect.Width > 96) ? (Rect.Width - 48) : Rect.Width;
    ChipWidth    = MIN (MaxChipWidth, TextWidth + 42);

    return ModernUiEngineDrawStatusPill (
             Context,
             (MODERN_UI_RECT){ Rect.X + 18, Rect.Y + 6, ChipWidth, MODERN_UI_TEXT_LINE_HEIGHT + 6 },
             StatusText,
             StatusColor,
             Theme
             );
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ModernUiEngineDrawPage (
  IN MODERN_UI_RENDER_CONTEXT  *Context,
  IN CONST MODERN_UI_PAGE_MODEL *Model,
  IN CONST MODERN_UI_THEME      *Theme
  )
{
  EFI_TIME       Time;
  CONST CHAR16   *ProductName;
  CONST CHAR16   *ModeName;
  CHAR16         TimeText[40];
  UINTN          TextY;
  UINTN          LeftEdge;
  UINTN          RightEdge;
  UINTN          ProductEnd;
  UINTN          TimeStart;
  UINTN          ModeX;
  UINTN          ModeWidth;
  UINTN          TimeWidth;
  EFI_STATUS     Status;

  if ((Context == NULL) || (Model == NULL) || (Theme == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ModernUiClear (Context, Theme->Background);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DrawHeaderGradient (Context, Model->Rect, Theme);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DrawPatternBand (Context, Model->Rect, Theme);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Model->Rect.X, Model->Rect.Y + Model->Rect.Height - 2, Model->Rect.Width, 2 }, Theme->AccentOrange);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Lay the header out from both edges inward: the product name anchors the
  // left, the clock is right-aligned by its measured width, and the mode label
  // is centred in whatever gap remains. Measuring instead of using fixed column
  // offsets keeps the three labels balanced and collision-free from the 1024px
  // resolution floor up through wide captures.
  //
  TextY       = Model->Rect.Y + 6;
  LeftEdge    = Model->Rect.X + 26;
  RightEdge   = (Model->Rect.Width > 52) ? (Model->Rect.X + Model->Rect.Width - 26) : (Model->Rect.X + Model->Rect.Width);
  ProductName = (Model->ProductName == NULL) ? L"MODERN SETUP" : Model->ProductName;
  ModeName    = (Model->ModeName == NULL) ? L"ADVANCED MODE" : Model->ModeName;

  Status = ModernUiDrawText (Context, LeftEdge, TextY, ProductName, Theme->Text, Theme->HeaderPattern);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ProductEnd = LeftEdge + ModernUiMeasureText (ProductName);
  TimeStart  = RightEdge;
  if (!EFI_ERROR (gRT->GetTime (&Time, NULL))) {
    UnicodeSPrint (
      TimeText,
      sizeof (TimeText),
      L"%02d/%02d/%04d  %02d:%02d:%02d",
      Time.Month,
      Time.Day,
      Time.Year,
      Time.Hour,
      Time.Minute,
      Time.Second
      );
    TimeWidth = ModernUiMeasureText (TimeText);
    TimeStart = (RightEdge > (ProductEnd + 16 + TimeWidth)) ? (RightEdge - TimeWidth) : (ProductEnd + 16);
    Status    = ModernUiDrawText (Context, TimeStart, TextY, TimeText, Theme->Text, Theme->HeaderPattern);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  //
  // Centre the mode label in the gap between the product name and the clock,
  // then clamp it so it never overruns either neighbour on a narrow header.
  //
  ModeWidth = ModernUiMeasureText (ModeName);
  ModeX     = LeftEdge;
  if ((TimeStart > (ProductEnd + 24)) && ((TimeStart - ProductEnd) > ModeWidth)) {
    ModeX = ProductEnd + (((TimeStart - ProductEnd) - ModeWidth) / 2);
  } else if (ProductEnd + 16 + ModeWidth <= RightEdge) {
    ModeX = ProductEnd + 16;
  }

  Status = ModernUiDrawText (Context, ModeX, TextY, ModeName, Theme->AccentOrange, Theme->HeaderPattern);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiEngineDrawTabs (Context, Model->Rect, Model->Tabs, Model->TabCount, Model->SelectedTab, Theme);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((Model->Layout.Footer.Width != 0) && (Model->Layout.Footer.Height != 0)) {
    Status = ModernUiEngineDrawFooter (Context, Model->Layout.Footer, Model->StatusText, Theme);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if ((Model->Layout.Content.Width != 0) && (Model->Layout.Content.Height != 0)) {
    Status = ModernUiFillRect (Context, Model->Layout.Content, Theme->Surface);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if ((Model->Layout.Content.Width > 4) && (Model->Layout.Content.Height > 4)) {
      Status = ModernUiStrokeRect (
                 Context,
                 Model->Layout.Content,
                 ModernUiBlendColor (Theme->Border, Theme->AccentOrange, 18)
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = ModernUiFillRect (
                 Context,
                 (MODERN_UI_RECT){ Model->Layout.Content.X, Model->Layout.Content.Y, Model->Layout.Content.Width, 1 },
                 ModernUiBlendColor (Theme->AccentOrange, Theme->BackgroundBlack, 20)
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }
  }

  if (Model->DrawRightRail && Model->Layout.RightRailVisible) {
    Status = DrawRightRail (Context, Model->Layout.RightRail, Theme);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Draw one statement row: the base row shape of the Modern UI vocabulary.

  Paints the role-derived selection surface, then the prompt text, then the
  value (if any) through the value-lane primitive. All geometry comes from the
  shared metric tokens (text inset, line height, value lane) so every row stays
  aligned and the row shape can be polished in one place. The prompt lane shrinks
  to clear the value lane only when the row has a value and is wide enough to
  host one; otherwise the prompt uses the full inset-to-inset width.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Row      Row model to draw. Must not be NULL.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Row was drawn.
  @retval EFI_INVALID_PARAMETER  A required pointer is NULL.
  @retval others                 Status returned by renderer primitives.
**/
STATIC
EFI_STATUS
ModernUiEngineDrawStatementRow (
  IN MODERN_UI_RENDER_CONTEXT   *Context,
  IN CONST MODERN_UI_ROW_MODEL  *Row,
  IN CONST MODERN_UI_THEME      *Theme
  )
{
  BOOLEAN                        Selected;
  BOOLEAN                        Disabled;
  BOOLEAN                        Action;
  BOOLEAN                        Subtitle;
  BOOLEAN                        HasValue;
  UINTN                          PromptWidth;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  TextColor;
  EFI_STATUS                     Status;

  if ((Context == NULL) || (Row == NULL) || (Theme == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Selected   = (BOOLEAN)(Row->Role == ModernUiRowSelected);
  Disabled   = (BOOLEAN)((Row->Role == ModernUiRowDisabled) || (Row->Role == ModernUiRowReadOnly));
  Action     = (BOOLEAN)(Row->Role == ModernUiRowAction);
  Subtitle   = (BOOLEAN)(Row->Role == ModernUiRowSubtitle);
  HasValue   = (BOOLEAN)((Row->Value != NULL) && (Row->Value[0] != CHAR_NULL));
  Background = ModernUiGetSelectableRowBackground (Selected, Disabled, Action, Subtitle, Theme);
  TextColor  = Disabled ? Theme->MutedText : (Selected ? Theme->Text : Theme->MutedText);
  if (Row->Role == ModernUiRowWarning) {
    TextColor = Theme->WarningText;
  }

  Status = ModernUiDrawSelectableRow (Context, Row->Rect, Selected, Disabled, Action, Subtitle, Theme);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((Row->Prompt != NULL) && (Row->Prompt[0] != CHAR_NULL)) {
    PromptWidth = (Row->Rect.Width > (MODERN_UI_BOX_TEXT_INSET * 2)) ? (Row->Rect.Width - (MODERN_UI_BOX_TEXT_INSET * 2)) : Row->Rect.Width;
    if (HasValue && (Row->Rect.Width > MODERN_UI_ROW_VALUE_LANE_MIN_WIDTH)) {
      PromptWidth = Row->Rect.Width - MODERN_UI_ROW_VALUE_LANE_WIDTH - MODERN_UI_ROW_VALUE_LANE_GAP - MODERN_UI_BOX_TEXT_INSET;
    }

    Status = ModernUiDrawTextFit (
               Context,
               Row->Rect.X + MODERN_UI_BOX_TEXT_INSET,
               ModernUiBoxTextY (Row->Rect.Y, Row->Rect.Height),
               PromptWidth,
               Row->Prompt,
               TextColor,
               Background
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (HasValue) {
    Status = ModernUiEngineDrawValue (
               Context,
               &(MODERN_UI_VALUE_MODEL){
                 Row->Rect,
                 Row->Value,
                 Row->ValueType,
                 Selected
               },
               Theme
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ModernUiEngineDrawRows (
  IN MODERN_UI_RENDER_CONTEXT   *Context,
  IN CONST MODERN_UI_ROW_MODEL  *Rows,
  IN UINTN                      RowCount,
  IN CONST MODERN_UI_THEME      *Theme
  )
{
  UINTN       Index;
  EFI_STATUS  Status;

  if ((Context == NULL) || (Theme == NULL) || ((RowCount > 0) && (Rows == NULL))) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < RowCount; Index++) {
    Status = ModernUiEngineDrawStatementRow (Context, &Rows[Index], Theme);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ModernUiEngineDrawValue (
  IN MODERN_UI_RENDER_CONTEXT    *Context,
  IN CONST MODERN_UI_VALUE_MODEL *Value,
  IN CONST MODERN_UI_THEME       *Theme
  )
{
  MODERN_UI_RECT  Rect;
  UINTN           CueSide;

  if ((Context == NULL) || (Value == NULL) || (Theme == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Value->Type == ModernUiValueNone) || (Value->Text == NULL)) {
    return EFI_SUCCESS;
  }

  Rect = Value->Rect;
  if (Rect.Width > MODERN_UI_ROW_VALUE_LANE_MIN_WIDTH) {
    Rect.X     = Rect.X + Rect.Width - MODERN_UI_ROW_VALUE_LANE_WIDTH;
    Rect.Width = MODERN_UI_ROW_VALUE_BOX_WIDTH;
  }

  //
  // Paint the shared per-type affordance just left of the value lane so a
  // checkbox, drop-down, numeric, date/time, password, string, ordered-list,
  // or action control reads the same here as in the in-setup DisplayEngine.
  // Plain text values carry no affordance.
  //
  // Widget-mapped controls render as real backend widgets that carry their own
  // affordance, so the external cue is drawn only for the remaining cued type
  // (action). One-of/checkbox/numeric/string/password/ordered-list/date-time all
  // map to widgets on the app-facing draw path.
  if ((Value->Type != ModernUiValueText) &&
      (Value->Type != ModernUiValueOneOf) &&
      (Value->Type != ModernUiValueCheckbox) &&
      (Value->Type != ModernUiValueNumeric) &&
      (Value->Type != ModernUiValueString) &&
      (Value->Type != ModernUiValuePassword) &&
      (Value->Type != ModernUiValueOrderedList) &&
      (Value->Type != ModernUiValueDateTime) &&
      (Rect.X > Value->Rect.X))
  {
    CueSide = MIN ((Rect.Height > 6) ? (Rect.Height - 6) : 0, 14);
    if (CueSide >= 6) {
      ModernUiEngineDrawControlCue (
        Context,
        (MODERN_UI_RECT){ Rect.X - CueSide - 8, Rect.Y + (Rect.Height - CueSide) / 2, CueSide, CueSide },
        Value->Type,
        Theme->AccentYellow,
        ModernUiBlendColor (Theme->AccentOrange, Theme->BackgroundBlack, 70)
        );
    }
  }

  //
  // Route each widget-mapped control to the backend's best rendering: real LVGL
  // widgets (lv_dropdown / lv_checkbox / lv_textarea) on the LVGL backend, themed
  // value/field boxes on GOP.
  //
  switch (Value->Type) {
    case ModernUiValueOneOf:
      return ModernUiRenderOneOf (Context, Rect, Value->Text, Value->Selected, Theme);
    case ModernUiValueCheckbox:
      return ModernUiRenderCheckbox (Context, Rect, Value->Text, Value->Selected, Theme);
    case ModernUiValueNumeric:
      return ModernUiRenderNumeric (Context, Rect, Value->Text, Value->Selected, Theme);
    case ModernUiValueString:
      return ModernUiRenderString (Context, Rect, Value->Text, Value->Selected, Theme);
    case ModernUiValuePassword:
      return ModernUiRenderPassword (Context, Rect, Value->Text, Value->Selected, Theme);
    case ModernUiValueOrderedList:
      return ModernUiRenderOrderedList (Context, Rect, Value->Text, Value->Selected, Theme);
    case ModernUiValueDateTime:
      return ModernUiRenderDateTime (Context, Rect, Value->Text, Value->Selected, Theme);
    case ModernUiValueText:
      return ModernUiDrawValueBox (Context, Rect, Value->Text, Value->Selected, Theme);
    default:
      break;
  }

  return ModernUiDrawTextFit (
           Context,
           Rect.X + MODERN_UI_BOX_TEXT_INSET,
           ModernUiBoxTextY (Rect.Y, Rect.Height),
           (Rect.Width > (MODERN_UI_BOX_TEXT_INSET * 2)) ? (Rect.Width - (MODERN_UI_BOX_TEXT_INSET * 2)) : Rect.Width,
           Value->Text,
           Value->Selected ? Theme->Text : Theme->MutedText,
           ModernUiGetSelectableRowBackground (Value->Selected, FALSE, FALSE, FALSE, Theme)
           );
}

EFI_STATUS
EFIAPI
ModernUiEngineDrawControlCue (
  IN MODERN_UI_RENDER_CONTEXT       *Context,
  IN MODERN_UI_RECT                 CueRect,
  IN MODERN_UI_VALUE_TYPE           Type,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  CueColor,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  FillColor
  )
{
  UINTN       W;
  UINTN       H;
  UINTN       Index;
  UINTN       DotSide;
  EFI_STATUS  Status;

  if (Context == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((CueRect.Width < 6) || (CueRect.Height < 6)) {
    return EFI_SUCCESS;
  }

  W      = CueRect.Width;
  H      = CueRect.Height;
  Status = EFI_SUCCESS;

  switch (Type) {
    case ModernUiValueCheckbox:
      //
      // Toggle box: the stored on/off is still shown as value text; this only
      // marks the row as a checkbox.
      //
      Status = ModernUiStrokeRect (Context, CueRect, CueColor);
      if (!EFI_ERROR (Status)) {
        Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ CueRect.X + 3, CueRect.Y + 3, W - 6, H - 6 }, FillColor);
      }

      break;

    case ModernUiValueOneOf:
      //
      // Drop-down chevron.
      //
      Status = ModernUiFillTriangle (Context, CueRect, ModernUiTriDown, CueColor);
      break;

    case ModernUiValueOrderedList:
      //
      // Up / down reorder chevrons.
      //
      Status = ModernUiFillTriangle (Context, (MODERN_UI_RECT){ CueRect.X, CueRect.Y, W, H / 2 - 1 }, ModernUiTriUp, CueColor);
      if (!EFI_ERROR (Status)) {
        Status = ModernUiFillTriangle (Context, (MODERN_UI_RECT){ CueRect.X, CueRect.Y + H / 2 + 1, W, H / 2 - 1 }, ModernUiTriDown, CueColor);
      }

      break;

    case ModernUiValueNumeric:
      //
      // Adjust indicator: a plus mark.
      //
      Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ CueRect.X + 2, CueRect.Y + H / 2 - 1, W - 4, 2 }, CueColor);
      if (!EFI_ERROR (Status)) {
        Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ CueRect.X + W / 2 - 1, CueRect.Y + 2, 2, H - 4 }, CueColor);
      }

      break;

    case ModernUiValueDateTime:
      //
      // Two segment ticks (three-field value).
      //
      Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ CueRect.X + W / 3, CueRect.Y + 1, 2, H - 2 }, CueColor);
      if (!EFI_ERROR (Status)) {
        Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ CueRect.X + (W * 2) / 3, CueRect.Y + 1, 2, H - 2 }, CueColor);
      }

      break;

    case ModernUiValuePassword:
      //
      // A short row of masked dots.
      //
      DotSide = MAX (2, W / 5);
      for (Index = 0; Index < 3; Index++) {
        Status = ModernUiFillRect (
                   Context,
                   (MODERN_UI_RECT){ CueRect.X + 2 + Index * (DotSide + 1), CueRect.Y + H / 2 - DotSide / 2, DotSide, DotSide },
                   CueColor
                   );
        if (EFI_ERROR (Status)) {
          break;
        }
      }

      break;

    case ModernUiValueString:
      //
      // Text caret.
      //
      Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ CueRect.X + W / 2, CueRect.Y + 1, 2, H - 2 }, CueColor);
      break;

    case ModernUiValueAction:
      //
      // Navigate / activate arrow.
      //
      Status = ModernUiFillTriangle (Context, CueRect, ModernUiTriRight, CueColor);
      break;

    default:
      break;
  }

  return Status;
}

EFI_STATUS
EFIAPI
ModernUiEngineDrawPopup (
  IN MODERN_UI_RENDER_CONTEXT    *Context,
  IN CONST MODERN_UI_POPUP_MODEL *Popup,
  IN CONST MODERN_UI_THEME       *Theme
  )
{
  EFI_STATUS  Status;

  if ((Context == NULL) || (Popup == NULL) || (Theme == NULL) || (Popup->Rect.Width == 0) || (Popup->Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Popup->Rect.Width > 12) && (Popup->Rect.Height > 12)) {
    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){ Popup->Rect.X + 8, Popup->Rect.Y + 8, Popup->Rect.Width, Popup->Rect.Height },
               ModernUiBlendColor (Theme->BackgroundBlack, Theme->AccentOrange, 12)
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  Status = ModernUiFillRect (Context, Popup->Rect, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiStrokeRect (Context, Popup->Rect, Theme->PopupBorder);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Popup->Rect.Height > 4) {
    Status = DrawGlowStrip (Context, (MODERN_UI_RECT){ Popup->Rect.X + 1, Popup->Rect.Y + 1, Popup->Rect.Width - 2, 3 }, Theme);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if ((Popup->Title != NULL) && (Popup->Title[0] != CHAR_NULL)) {
    return ModernUiDrawText (Context, Popup->Rect.X + 16, Popup->Rect.Y + 12, Popup->Title, Theme->Text, Theme->BackgroundBlack);
  }

  return EFI_SUCCESS;
}
