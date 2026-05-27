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

  if ((StatusText != NULL) && (StatusText[0] != CHAR_NULL) && (Rect.Width > 80) && (Rect.Height > 22)) {
    StatusColor  = ModernUiEngineStatusColor (StatusText, Theme);
    TextWidth    = ModernUiMeasureText (StatusText);
    MaxChipWidth = (Rect.Width > 96) ? (Rect.Width - 48) : Rect.Width;
    ChipWidth    = MIN (MaxChipWidth, TextWidth + 42);

    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){ Rect.X + 18, Rect.Y + 7, ChipWidth, 20 },
               ModernUiBlendColor (Theme->BackgroundBlack, StatusColor, 16)
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){ Rect.X + 18, Rect.Y + 7, 4, 20 },
               StatusColor
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){ Rect.X + 22, Rect.Y + 7, ChipWidth - 4, 1 },
               ModernUiBlendColor (StatusColor, Theme->BackgroundBlack, 55)
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    return ModernUiDrawTextFit (
             Context,
             Rect.X + 34,
             Rect.Y + 11,
             (ChipWidth > 30) ? (ChipWidth - 30) : ChipWidth,
             StatusText,
             StatusColor,
             ModernUiBlendColor (Theme->BackgroundBlack, StatusColor, 16)
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

EFI_STATUS
EFIAPI
ModernUiEngineDrawRows (
  IN MODERN_UI_RENDER_CONTEXT   *Context,
  IN CONST MODERN_UI_ROW_MODEL  *Rows,
  IN UINTN                      RowCount,
  IN CONST MODERN_UI_THEME      *Theme
  )
{
  UINTN                          Index;
  UINTN                          PromptWidth;
  BOOLEAN                        Selected;
  BOOLEAN                        Disabled;
  BOOLEAN                        Action;
  BOOLEAN                        Subtitle;
  BOOLEAN                        HasValue;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  TextColor;
  EFI_STATUS                     Status;

  if ((Context == NULL) || (Theme == NULL) || ((RowCount > 0) && (Rows == NULL))) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < RowCount; Index++) {
    Selected = (BOOLEAN)(Rows[Index].Role == ModernUiRowSelected);
    Disabled = (BOOLEAN)((Rows[Index].Role == ModernUiRowDisabled) || (Rows[Index].Role == ModernUiRowReadOnly));
    Action   = (BOOLEAN)(Rows[Index].Role == ModernUiRowAction);
    Subtitle = (BOOLEAN)(Rows[Index].Role == ModernUiRowSubtitle);
    HasValue = (BOOLEAN)((Rows[Index].Value != NULL) && (Rows[Index].Value[0] != CHAR_NULL));
    Background = ModernUiGetSelectableRowBackground (Selected, Disabled, Action, Subtitle, Theme);
    TextColor  = Disabled ? Theme->MutedText : (Selected ? Theme->Text : Theme->MutedText);
    if (Rows[Index].Role == ModernUiRowWarning) {
      TextColor = Theme->WarningText;
    }

    Status = ModernUiDrawSelectableRow (Context, Rows[Index].Rect, Selected, Disabled, Action, Subtitle, Theme);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if ((Rows[Index].Prompt != NULL) && (Rows[Index].Prompt[0] != CHAR_NULL)) {
      PromptWidth = (Rows[Index].Rect.Width > 32) ? (Rows[Index].Rect.Width - 32) : Rows[Index].Rect.Width;
      if (HasValue && (Rows[Index].Rect.Width > MODERN_UI_ROW_VALUE_LANE_MIN_WIDTH)) {
        PromptWidth = Rows[Index].Rect.Width - MODERN_UI_ROW_VALUE_LANE_WIDTH - MODERN_UI_ROW_VALUE_LANE_GAP - 16;
      }

      Status = ModernUiDrawTextFit (
                 Context,
                 Rows[Index].Rect.X + 16,
                 Rows[Index].Rect.Y + ((Rows[Index].Rect.Height > 18) ? ((Rows[Index].Rect.Height - 18) / 2) : 0),
                 PromptWidth,
                 Rows[Index].Prompt,
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
                   Rows[Index].Rect,
                   Rows[Index].Value,
                   Rows[Index].ValueType,
                   Selected
                 },
                 Theme
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }
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

  if ((Value->Type == ModernUiValueOneOf) || (Value->Type == ModernUiValueText)) {
    return ModernUiDrawValueBox (Context, Rect, Value->Text, Value->Selected, Theme);
  }

  return ModernUiDrawTextFit (
           Context,
           Rect.X + 16,
           Rect.Y + ((Rect.Height > 18) ? ((Rect.Height - 18) / 2) : 0),
           (Rect.Width > 32) ? (Rect.Width - 32) : Rect.Width,
           Value->Text,
           Value->Selected ? Theme->Text : Theme->MutedText,
           ModernUiGetSelectableRowBackground (Value->Selected, FALSE, FALSE, FALSE, Theme)
           );
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
