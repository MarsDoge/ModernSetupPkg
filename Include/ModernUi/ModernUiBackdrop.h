/** @file
  Shared presentation only; statically linked and independently owned per image.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef MODERN_UI_BACKDROP_H_
#define MODERN_UI_BACKDROP_H_
#include <ModernUi/ModernUiRenderer.h>

/**
  Replace this image's cache and paint opaque artwork; never capture the canvas.
  @param[in] Ui      Initialized context, or NULL to fail safely.
  @param[in] Region  Nonempty in-bounds region, at most 4096 by 2160 pixels.
  @retval TRUE      Artwork restored and cache retained by this linked image.
  @retval FALSE     Disabled, invalid, allocation or restore failure; old cache
                    released. Caller must draw its solid fallback.
**/
BOOLEAN ModernUiBackdropPrepare (IN MODERN_UI_RENDER_CONTEXT *Ui, IN MODERN_UI_RECT Region);

/**
  Restore pristine artwork, resampling only if owner, dimensions or crop change.
  @param[in] Ui      Initialized context, or NULL to fail safely.
  @param[in] Region  Same bounds contract as ModernUiBackdropPrepare.
  @retval TRUE      Entire region restored, including on native/App reentry.
  @retval FALSE     Caller must draw solid fallback; no cross-image ownership.
**/
BOOLEAN ModernUiBackdropRedraw (IN MODERN_UI_RENDER_CONTEXT *Ui, IN MODERN_UI_RECT Region);

/**
  Draw an explicitly opted-in panel against immutable artwork, not drawn pixels.
  @param[in] Ui     Initialized non-NULL context; no ownership transfer.
  @param[in] Rect   Valid renderer rectangle, within the cached region to blend.
  @param[in] Color  Solid panel colour.
  @param[in] Alpha  Opacity, 0 (artwork) through 255 (solid).
  Missing cache, owner mismatch or composition failure uses the original solid
  FillRect. Renderer errors remain best-effort, as for existing panel drawing.
**/
VOID ModernUiBackdropSurface (IN MODERN_UI_RENDER_CONTEXT *Ui, IN MODERN_UI_RECT Rect,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color, IN UINT8 Alpha);

/** Release this linked image's cache. Safe repeatedly or with no active cache. */
VOID ModernUiBackdropRelease (VOID);
#endif
