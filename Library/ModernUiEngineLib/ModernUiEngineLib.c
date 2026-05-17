/** @file
  Shared ModernSetup visual engine implementation.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <ModernUi/ModernUiEngine.h>

#define MODERN_UI_ENGINE_RIGHT_RAIL_MIN_WIDTH  1000

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
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  FaintAccent;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  FaintBorder;
  UINTN                          X;
  UINTN                          LineY;
  EFI_STATUS                     Status;

  if ((Context == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  FaintAccent = ModernUiBlendColor (Theme->HeaderPattern, Theme->AccentOrange, 18);
  FaintBorder = ModernUiBlendColor (Theme->HeaderPattern, Theme->Border, 24);

  for (LineY = Rect.Y + 12; LineY < (Rect.Y + Rect.Height); LineY += 32) {
    Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, LineY, Rect.Width, 1 }, FaintBorder);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  for (X = Rect.X + 48; X < (Rect.X + Rect.Width); X += 112) {
    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){ X, Rect.Y + 6, 2, (Rect.Height > 12) ? (Rect.Height - 12) : 1 },
               FaintAccent
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if ((X + 10) < (Rect.X + Rect.Width)) {
      Status = ModernUiFillRect (
                 Context,
                 (MODERN_UI_RECT){ X + 10, Rect.Y + 2, 1, (Rect.Height > 4) ? (Rect.Height - 4) : 1 },
                 FaintBorder
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }
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

  Status = ModernUiDrawText (Context, X, Y + 48, L"AARCH64", Theme->TelemetryText, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawText (Context, X, Y + 82, L"Platform", Theme->MutedText, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiDrawText (Context, X, Y + 102, L"ArmVirt / QEMU", Theme->TelemetryText, Theme->BackgroundBlack);
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
      Status = ModernUiFillRect (
                 Context,
                 TabRect,
                 ModernUiBlendColor (Theme->BackgroundBlack, Theme->SelectedBand, 78)
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = ModernUiFillRect (
                 Context,
                 (MODERN_UI_RECT){ TabRect.X, TabRect.Y, 4, TabRect.Height },
                 Theme->AccentYellow
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = ModernUiStrokeRect (Context, TabRect, Theme->PopupBorder);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = DrawGlowStrip (
                 Context,
                 (MODERN_UI_RECT){ TabRect.X, TabY + 32, TabRect.Width, 3 },
                 Theme
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    Status = ModernUiFillRect (
               Context,
               (MODERN_UI_RECT){ X + 18, TabY + 30, (TabWidth > 36) ? (TabWidth - 36) : TabWidth, 2 },
               (TabIndex == SelectedTab) ? Theme->AccentYellow : ModernUiBlendColor (Theme->BackgroundBlack, Theme->AccentOrange, 45)
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
               (TabIndex == SelectedTab) ? ModernUiBlendColor (Theme->BackgroundBlack, Theme->SelectedBand, 78) : Theme->BackgroundBlack
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
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
  EFI_STATUS  Status;

  if ((Context == NULL) || (Theme == NULL) || (Rect.Width == 0) || (Rect.Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ModernUiFillRect (Context, Rect, Theme->SurfaceRaised);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiFillRect (Context, (MODERN_UI_RECT){ Rect.X, Rect.Y, Rect.Width, 1 }, Theme->Border);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((StatusText != NULL) && (StatusText[0] != CHAR_NULL)) {
    return ModernUiDrawText (Context, Rect.X + 24, Rect.Y + 10, StatusText, Theme->Warning, Theme->SurfaceRaised);
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
  EFI_TIME    Time;
  UINTN       ModeX;
  UINTN       TimeX;
  EFI_STATUS  Status;

  if ((Context == NULL) || (Model == NULL) || (Theme == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ModernUiClear (Context, Theme->Background);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiFillRect (Context, Model->Rect, Theme->BackgroundBlack);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ModernUiFillRect (
             Context,
             (MODERN_UI_RECT){ Model->Rect.X, Model->Rect.Y, Model->Rect.Width, Model->Rect.Height / 2 },
             Theme->HeaderPattern
             );
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

  Status = ModernUiDrawText (Context, Model->Rect.X + 26, Model->Rect.Y + 6, (Model->ProductName == NULL) ? L"MODERN SETUP" : Model->ProductName, Theme->Text, Theme->HeaderPattern);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ModeX = (Context->Width > 190) ? ((Context->Width - 190) / 2) : (Model->Rect.X + 26);
  Status = ModernUiDrawText (Context, ModeX, Model->Rect.Y + 6, (Model->ModeName == NULL) ? L"ADVANCED MODE" : Model->ModeName, Theme->AccentOrange, Theme->HeaderPattern);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!EFI_ERROR (gRT->GetTime (&Time, NULL))) {
    TimeX = (Context->Width > 210) ? (Context->Width - 210) : (Model->Rect.X + 26);
    Status = ModernUiDrawTextFormatted (
               Context,
               TimeX,
               Model->Rect.Y + 6,
               Theme->Text,
               Theme->HeaderPattern,
               L"%02d/%02d/%04d  %02d:%02d",
               Time.Month,
               Time.Day,
               Time.Year,
               Time.Hour,
               Time.Minute
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
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
  BOOLEAN                        Selected;
  BOOLEAN                        Disabled;
  BOOLEAN                        Action;
  BOOLEAN                        Subtitle;
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
      Status = ModernUiDrawTextFit (
                 Context,
                 Rows[Index].Rect.X + 16,
                 Rows[Index].Rect.Y + ((Rows[Index].Rect.Height > 18) ? ((Rows[Index].Rect.Height - 18) / 2) : 0),
                 (Rows[Index].Rect.Width > 32) ? (Rows[Index].Rect.Width - 32) : Rows[Index].Rect.Width,
                 Rows[Index].Prompt,
                 TextColor,
                 Background
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    if ((Rows[Index].Value != NULL) && (Rows[Index].Value[0] != CHAR_NULL)) {
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
  if (Rect.Width > 240) {
    Rect.X     = Rect.X + Rect.Width - 240;
    Rect.Width = 220;
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
