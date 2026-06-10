/** @file
  GOP-backed renderer for ModernSetupPkg.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_RENDERER_H_
#define MODERN_UI_RENDERER_H_

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/HiiFont.h>

#include <ModernUi/ModernUiTheme.h>

//
// Minimum GOP mode the modern renderer treats as usable. The modern chrome
// (header, tab rail, right help rail, popups) cannot lay out below roughly VGA;
// when the active mode is smaller (or GOP is absent), ModernUiRendererInit
// fails so callers degrade gracefully -- the in-setup display engine falls back
// to plain text-console output and the front-page app exits to the native shell
// -- instead of painting broken or blank chrome.
//
#define MODERN_UI_MIN_RENDER_WIDTH   640
#define MODERN_UI_MIN_RENDER_HEIGHT  480

typedef struct {
  UINTN    X;
  UINTN    Y;
  UINTN    Width;
  UINTN    Height;
} MODERN_UI_RECT;

typedef struct {
  EFI_GRAPHICS_OUTPUT_PROTOCOL    *Gop;
  EFI_HII_FONT_PROTOCOL           *Font;
  UINTN                           Width;
  UINTN                           Height;
} MODERN_UI_RENDER_CONTEXT;

typedef enum {
  ModernUiTriDown,    ///< Apex points down (drop-down chevron).
  ModernUiTriUp,      ///< Apex points up.
  ModernUiTriRight    ///< Apex points right (navigate / activate arrow).
} MODERN_UI_TRI_DIR;

/**
  Blend two GOP colors by percentage weight.

  @param[in] Base    Base color used when Weight is zero.
  @param[in] Accent  Accent color used when Weight is one hundred.
  @param[in] Weight  Accent weight in percent. Values above 100 are clamped.

  @return Blended color with Reserved cleared.
**/
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
EFIAPI
ModernUiBlendColor (
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Base,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Accent,
  IN UINT8                          Weight
  );

/**
  Initialize a render context from firmware graphics services.

  @param[out] Context  Render context to initialize. Must not be NULL. On
                       success, Width and Height describe the active GOP mode.
                       The renderer may switch from a small default mode to a
                       preferred mode of at least 1024x768 when firmware offers
                       one.

  @retval EFI_SUCCESS            Context was initialized.
  @retval EFI_INVALID_PARAMETER  Context is NULL.
  @retval EFI_NOT_FOUND          GOP is unavailable or has no active mode.
**/
EFI_STATUS
EFIAPI
ModernUiRendererInit (
  OUT MODERN_UI_RENDER_CONTEXT  *Context
  );

/**
  Fill the full render target with one color.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Color    Fill color.

  @retval EFI_SUCCESS            The screen was cleared.
  @retval EFI_INVALID_PARAMETER  Context is NULL or invalid.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiClear (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Color
  );

/**
  Fill a rectangle in the active render target.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Pixel rectangle. Zero width or height is invalid.
  @param[in] Color    Fill color.

  @retval EFI_SUCCESS            Rectangle was filled or clipped outside view.
  @retval EFI_INVALID_PARAMETER  Context is NULL, GOP is unavailable, or Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiFillRect (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Color
  );

/**
  Draw a one-pixel rectangle border.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Pixel rectangle to outline. Must be at least one pixel wide
                      and high.
  @param[in] Color    Border color.

  @retval EFI_SUCCESS            Border was drawn.
  @retval EFI_INVALID_PARAMETER  Context is NULL, GOP is unavailable, or Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiStrokeRect (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Color
  );

/**
  Fill an isosceles triangle inscribed in a rectangle.

  The triangle is built from one-pixel renderer fill spans so it renders
  identically through the GOP and LVGL backends. Used to compose chevron and
  arrow control affordances.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Bounding rectangle. Zero width or height is ignored.
  @param[in] Dir      Apex direction (down / up / right).
  @param[in] Color    Fill color.

  @retval EFI_SUCCESS            Triangle was drawn (or Rect was empty).
  @retval EFI_INVALID_PARAMETER  Context is NULL.
  @retval others                 Status from the first failing fill span.
**/
EFI_STATUS
EFIAPI
ModernUiFillTriangle (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN MODERN_UI_TRI_DIR                 Dir,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Color
  );

/**
  Return the expected pixel width for a UCS-2 string.

  Built-in CJK glyphs are measured at their bitmap width. Other characters use
  the renderer's current ASCII cell width.

  @param[in] Text  Null-terminated UCS-2 string. Must not be NULL.

  @return Pixel width. NULL input returns 0.
**/
UINTN
EFIAPI
ModernUiMeasureText (
  IN CONST CHAR16  *Text
  );

/**
  Draw UCS-2 text using HII Font and built-in bitmap glyph fallback.

  @param[in] Context     Initialized render context. Must not be NULL.
  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] Text        Null-terminated UCS-2 string. Must not be NULL.
  @param[in] Color       Text foreground color.
  @param[in] Background  Background color passed to the font renderer.

  @retval EFI_SUCCESS            Text was rendered.
  @retval EFI_INVALID_PARAMETER  Context or Text is NULL.
  @retval EFI_UNSUPPORTED        HII Font protocol is unavailable for a
                                  non-built-in character run.
  @retval EFI_OUT_OF_RESOURCES   Temporary rendering allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiDrawText (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN UINTN                             X,
  IN UINTN                             Y,
  IN CONST CHAR16                      *Text,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Background
  );

/**
  Format and draw one UCS-2 text line.

  @param[in] Context     Initialized render context. Must not be NULL.
  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] Color       Text foreground color.
  @param[in] Background  Text background color.
  @param[in] Format      PrintLib format string. Must not be NULL.
  @param[in] ...         Format arguments consumed according to Format.

  @retval EFI_SUCCESS            Text was formatted and rendered.
  @retval EFI_INVALID_PARAMETER  Context or Format is NULL.
  @retval others                 Status returned by ModernUiDrawText().
**/
EFI_STATUS
EFIAPI
ModernUiDrawTextFormatted (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN UINTN                             X,
  IN UINTN                             Y,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Background,
  IN CONST CHAR16                      *Format,
  ...
  );

/**
  Draw UCS-2 text constrained to a pixel width.

  The renderer measures mixed ASCII, built-in CJK, and text-mode graphic glyphs
  and appends "..." when the string must be truncated.

  @param[in] Context     Initialized render context. Must not be NULL.
  @param[in] X           Left coordinate in pixels.
  @param[in] Y           Top coordinate in pixels.
  @param[in] MaxWidth    Maximum text width in pixels.
  @param[in] Text        Null-terminated UCS-2 string. Must not be NULL.
  @param[in] Color       Text foreground color.
  @param[in] Background  Text background color.

  @retval EFI_SUCCESS            Text was rendered or empty width was ignored.
  @retval EFI_INVALID_PARAMETER  Context or Text is NULL.
  @retval EFI_OUT_OF_RESOURCES   Temporary truncation buffer allocation failed.
  @retval others                 Status returned by ModernUiDrawText().
**/
EFI_STATUS
EFIAPI
ModernUiDrawTextFit (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN UINTN                             X,
  IN UINTN                             Y,
  IN UINTN                             MaxWidth,
  IN CONST CHAR16                      *Text,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Color,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Background
  );

/**
  Draw a themed panel surface and border.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Panel rectangle.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Panel was drawn.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiDrawPanel (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Draw a focus frame only when requested.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rect      Rectangle to outline when HasFocus is TRUE.
  @param[in] HasFocus  TRUE to draw the focus frame.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Focus frame was drawn or skipped.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL, or Rect is empty
                                  when HasFocus is TRUE.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiDrawFocusFrame (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN BOOLEAN                           HasFocus,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Draw a compact title/value information card.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Card rectangle. Must be large enough for two text rows.
  @param[in] Title    Card title text. Must not be NULL.
  @param[in] Value    Card value text. Must not be NULL.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Card was drawn.
  @retval EFI_INVALID_PARAMETER  Context, Title, Value, or Theme is NULL, or
                                  Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
  @retval others                 Status returned by text rendering.
**/
EFI_STATUS
EFIAPI
ModernUiDrawInfoCard (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Title,
  IN CONST CHAR16                      *Value,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Return the themed background color used by a selectable row.

  @param[in] Selected  TRUE when the row is selected.
  @param[in] Disabled  TRUE when the row is visible but disabled or grayed.
  @param[in] Action    TRUE when the row represents an action-like command.
  @param[in] Subtitle  TRUE when the row is a section subtitle.
  @param[in] Theme     Theme token table. Must not be NULL.

  @return Row background color. NULL Theme returns zero.
**/
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
EFIAPI
ModernUiGetSelectableRowBackground (
  IN BOOLEAN                           Selected,
  IN BOOLEAN                           Disabled,
  IN BOOLEAN                           Action,
  IN BOOLEAN                           Subtitle,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Draw a themed selectable row surface.

  This is a shared visual primitive for DisplayEngine statement rows and
  experimental app list rows. It does not own FormBrowser or HII semantics.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rect      Pixel rectangle for the row surface.
  @param[in] Selected  TRUE when the row is selected.
  @param[in] Disabled  TRUE when the row is visible but disabled or grayed.
  @param[in] Action    TRUE when the row represents an action-like command.
  @param[in] Subtitle  TRUE when the row is a section subtitle.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Row was drawn.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL, or Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiDrawSelectableRow (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN BOOLEAN                           Selected,
  IN BOOLEAN                           Disabled,
  IN BOOLEAN                           Action,
  IN BOOLEAN                           Subtitle,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Draw the standard border used around an action/selectable row.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rect      Row rectangle.
  @param[in] Selected  TRUE when the row is selected.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Row border was drawn.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL, or Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiDrawSelectableRowBorder (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Draw a value selector box with a trailing drop-down marker.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rect      Selector rectangle.
  @param[in] Value     Selected value text. Must not be NULL.
  @param[in] Selected  TRUE when the owning row is selected.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Selector was drawn.
  @retval EFI_INVALID_PARAMETER  Context, Value, or Theme is NULL, or Rect is
                                  empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
  @retval others                 Status returned by text rendering.
**/
EFI_STATUS
EFIAPI
ModernUiDrawValueBox (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Render a one-of (choice) control as the backend's best available drop-down.

  Display-only: this paints the closed drop-down showing the currently selected
  option text; it never opens a list or owns selection/input. edk2 FormBrowser
  still owns the actual choice (its own selection popup, ConfigAccess, callbacks).
  Backends diverge by capability:

  - GOP backend composes the drop-down from primitives (value box + an in-box
    chevron), matching the rest of the GOP chrome.
  - LVGL backend renders a real `lv_dropdown` widget (its own box, border, and
    `LV_SYMBOL_DOWN` arrow) snapshotted into the shadow canvas, so a one-of reads
    as a genuine toolkit control rather than a hand-composed box.

  The caller must not also draw the separate one-of affordance cue for this value;
  the rendered control carries its own arrow.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rect      Control rectangle (the value lane).
  @param[in] Value     Selected option text. Must not be NULL.
  @param[in] Selected  TRUE when the owning row is selected/focused.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Control was drawn (or clipped outside view).
  @retval EFI_INVALID_PARAMETER  Context, Value, or Theme is NULL, or Rect empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary widget/snapshot allocation failed.
  @retval others                 Status returned by the underlying renderer.
**/
EFI_STATUS
EFIAPI
ModernUiRenderOneOf (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Render a checkbox/boolean control as the backend's best widget (display-only).

  LVGL renders a real `lv_checkbox` (checked state inferred from the "[X]"/"[ ]"
  value text); GOP draws a bordered field with the value text. edk2 owns the
  toggle. Same NULL/Rect contract as ModernUiRenderOneOf.
**/
EFI_STATUS
EFIAPI
ModernUiRenderCheckbox (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Render a string control as the backend's best widget (display-only).

  LVGL renders a real one-line `lv_textarea` showing the current text; GOP draws
  a bordered field. edk2 owns editing. Same NULL/Rect contract as
  ModernUiRenderOneOf.
**/
EFI_STATUS
EFIAPI
ModernUiRenderString (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Render a password control as the backend's best widget (display-only).

  LVGL renders an `lv_textarea` in password mode (dots); GOP draws a bordered
  field with the value text (already masked by FormBrowser). edk2 owns editing.
**/
EFI_STATUS
EFIAPI
ModernUiRenderPassword (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Render a numeric control as the backend's best widget (display-only).

  LVGL renders a real `lv_spinbox`-styled field showing the current number; GOP
  draws a bordered field with the value text. edk2 owns the adjustment.
**/
EFI_STATUS
EFIAPI
ModernUiRenderNumeric (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Render an ordered-list control as the backend's best widget (display-only).

  LVGL renders a real list-style field (an `LV_SYMBOL_LIST`-prefixed `lv_obj`
  field) showing the current option order; GOP draws a bordered field. edk2 owns
  the reorder popup. Same NULL/Rect contract as ModernUiRenderOneOf.
**/
EFI_STATUS
EFIAPI
ModernUiRenderOrderedList (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Render a date/time control as the backend's best widget (display-only).

  LVGL renders a segmented field (the value laid out with spaced `/ : -`
  delimiters so the month/day/year or hour/minute/second segments read as
  discrete cells); GOP draws a bordered field. edk2 owns segment editing.

  Note: in the in-setup DisplayEngine, date/time keeps its native per-segment
  rendering and the type cue, because FormBrowser highlights the active segment
  in place and a full-lane widget overlay would mask that feedback. This entry
  point completes the engine value vocabulary for the app-facing draw path.
  Same NULL/Rect contract as ModernUiRenderOneOf.
**/
EFI_STATUS
EFIAPI
ModernUiRenderDateTime (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Draw a bordered value field (no drop-down arrow) with inset text.

  The arrow-less companion to ModernUiDrawValueBox, used by the GOP backend to
  present non-drop-down controls (checkbox/string/password/numeric) as boxed
  fields consistent with the drop-down value box.

  @param[in] Context   Initialized render context. Must not be NULL.
  @param[in] Rect      Field rectangle.
  @param[in] Value     Field text. Must not be NULL.
  @param[in] Selected  TRUE when the owning row is selected.
  @param[in] Theme     Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Field was drawn.
  @retval EFI_INVALID_PARAMETER  Context, Value, or Theme is NULL, or Rect empty.
**/
EFI_STATUS
EFIAPI
ModernUiDrawFieldBox (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST CHAR16                      *Value,
  IN BOOLEAN                           Selected,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Draw a drop-down list frame.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Drop-down rectangle.
  @param[in] Theme    Theme token table. Must not be NULL.

  @retval EFI_SUCCESS            Drop-down frame was drawn.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL, or Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiDrawDropdownFrame (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN CONST MODERN_UI_THEME             *Theme
  );

/**
  Draw a horizontal progress bar.

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Rect     Progress track rectangle.
  @param[in] Percent  Completion percentage. Values above 100 are clamped.
  @param[in] Track    Track color.
  @param[in] Fill     Fill color.

  @retval EFI_SUCCESS            Progress bar was drawn.
  @retval EFI_INVALID_PARAMETER  Context is NULL, GOP is unavailable, or Rect is empty.
  @retval EFI_OUT_OF_RESOURCES   Temporary BLT allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiDrawProgress (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Rect,
  IN UINTN                             Percent,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Track,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     Fill
  );

/**
  Composite the OEM branding watermark into a region's whitespace (display-only).

  This is the renderer half of the OEM branding slot: an original, theme-tinted
  watermark drawn low-opacity and centered toward the bottom of Region, so it
  fills the empty content area without fighting the setup text. The asset ships
  no IBV/commercial art -- it is an A8 coverage map generated from
  Assets/Branding/oem-watermark.svg by Scripts/gen-oem-watermark.py.

  - LVGL backend alpha-blends the A8 coverage map (tinted with the theme's muted
    text color) directly into the shadow canvas and BLTs the affected region.
  - GOP backend is currently a no-op (returns EFI_SUCCESS); a primitive composite
    is a follow-up.

  Because the native FormBrowser repaints the content area after the chrome, the
  DisplayEngine caller invokes this *after* the content rows are painted and
  confines Region to the statement column (the help/right-rail columns are
  repainted by later form states and would clip the mark).

  @param[in] Context  Initialized render context. Must not be NULL.
  @param[in] Region   Pixel region to anchor the watermark within (e.g. the
                      statement content panel). A region too small for the mark
                      is skipped.
  @param[in] Theme    Theme token table (supplies the tint). Must not be NULL.

  @retval EFI_SUCCESS            Watermark composited, or skipped as a no-op.
  @retval EFI_INVALID_PARAMETER  Context or Theme is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiDrawOemWatermark (
  IN MODERN_UI_RENDER_CONTEXT          *Context,
  IN MODERN_UI_RECT                    Region,
  IN CONST MODERN_UI_THEME             *Theme
  );

#endif
