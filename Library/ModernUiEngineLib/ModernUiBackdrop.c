/** @file
  Optional shared artwork; each linked image owns its own pristine cache.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>
#include <ModernUi/ModernUiBackdrop.h>

#ifndef MODERN_SETUP_HOME_BACKGROUND
#define MODERN_SETUP_HOME_BACKGROUND  0
#endif

#if MODERN_SETUP_HOME_BACKGROUND
#include <ModernSetupHomeBackground.generated.h>

// Per linked image, never borrowed from another image or captured from canvas.
STATIC EFI_GRAPHICS_OUTPUT_BLT_PIXEL *mHomePristine;
STATIC MODERN_UI_RECT                mHomeRegion;
STATIC MODERN_UI_RENDER_CONTEXT      *mHomeOwner;
STATIC UINTN mHomeWidth, mHomeHeight;

STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
HomePixel (
  IN UINTN X,
  IN UINTN Y
  )
{
  UINTN Value;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel;

  Value = (Y * HOME_BACKGROUND_WIDTH + X) * 2;
  Value = mHomeRgb565[Value] | ((UINTN)mHomeRgb565[Value + 1] << 8);
  Pixel.Red      = (UINT8)(((Value >> 11) * 255 + 15) / 31);
  Pixel.Green    = (UINT8)((((Value >> 5) & 63) * 255 + 31) / 63);
  Pixel.Blue     = (UINT8)(((Value & 31) * 255 + 15) / 31);
  Pixel.Reserved = 0;
  return Pixel;
}

STATIC
UINT8
HomeInterpolate (
  IN UINT8 A, IN UINT8 B, IN UINT8 C, IN UINT8 D,
  IN UINTN Fx, IN UINTN Fy
  )
{
  return (UINT8)((((UINTN)A * (256 - Fx) + B * Fx) * (256 - Fy) +
                  ((UINTN)C * (256 - Fx) + D * Fx) * Fy + 32768) >> 16);
}
#endif

/** Release only this linked image's cache. Safe after failure or repeated calls. */
VOID
ModernUiBackdropRelease (
  VOID
  )
{
#if MODERN_SETUP_HOME_BACKGROUND
  if (mHomePristine != NULL) {
    FreePool (mHomePristine);
    mHomePristine = NULL;
  }
  mHomeOwner = NULL;
#endif
}

/** Explicit surface: always blend from pristine artwork, not the canvas.
    Alpha is opacity in 0..255. Invalid/cache/failure paths use the old fill. */
VOID
ModernUiBackdropSurface (
  IN MODERN_UI_RENDER_CONTEXT       *Ui,
  IN MODERN_UI_RECT                 Rect,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN UINT8                         Alpha
  )
{
#if MODERN_SETUP_HOME_BACKGROUND
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Pixels;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Art;
  UINTN X, Y;
  EFI_STATUS Status;

  if ((Ui != NULL) && (Ui == mHomeOwner) && (mHomePristine != NULL) &&
      (Ui->Width == mHomeWidth) && (Ui->Height == mHomeHeight) &&
      (Rect.Width > 0) && (Rect.Height > 0) &&
      (Rect.X >= mHomeRegion.X) && (Rect.Y >= mHomeRegion.Y) &&
      (Rect.X - mHomeRegion.X <= mHomeRegion.Width) &&
      (Rect.Y - mHomeRegion.Y <= mHomeRegion.Height) &&
      (Rect.Width <= mHomeRegion.Width - (Rect.X - mHomeRegion.X)) &&
      (Rect.Height <= mHomeRegion.Height - (Rect.Y - mHomeRegion.Y)))
  {
    Pixels = AllocatePool (Rect.Width * Rect.Height * sizeof (*Pixels));
    if (Pixels != NULL) {
      for (Y = 0; Y < Rect.Height; Y++) {
        for (X = 0; X < Rect.Width; X++) {
          Art = mHomePristine[(Rect.Y - mHomeRegion.Y + Y) * mHomeRegion.Width +
                             Rect.X - mHomeRegion.X + X];
          Pixels[Y * Rect.Width + X].Red = (UINT8)(((UINTN)Color.Red * Alpha + Art.Red * (255 - Alpha) + 127) / 255);
          Pixels[Y * Rect.Width + X].Green = (UINT8)(((UINTN)Color.Green * Alpha + Art.Green * (255 - Alpha) + 127) / 255);
          Pixels[Y * Rect.Width + X].Blue = (UINT8)(((UINTN)Color.Blue * Alpha + Art.Blue * (255 - Alpha) + 127) / 255);
          Pixels[Y * Rect.Width + X].Reserved = 0;
        }
      }
      Status = ModernUiRestoreRect (Ui, Rect, Pixels);
      FreePool (Pixels);
      if (!EFI_ERROR (Status)) {
        return;
      }
    }
  }
#endif
  ModernUiFillRect (Ui, Rect, Color);
}

/** Paint an opaque centered cover crop through the shadow-canvas API.
    Bilinear sampling avoids enlarged nearest-neighbour blocks. */
BOOLEAN
ModernUiBackdropPrepare (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN MODERN_UI_RECT            Region
  )
{
#if MODERN_SETUP_HOME_BACKGROUND
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Pixels;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL A, B, C, D;
  EFI_STATUS Status;
  UINTN CropWidth, CropHeight, Left, Top;
  UINTN SourceX, SourceY, NextX, NextY, Fx, Fy, X, Y;

  ModernUiBackdropRelease ();
  if ((Ui == NULL) || (Region.Width == 0) || (Region.Height == 0) ||
      (Region.Width > 4096) || (Region.Height > 2160) ||
      (Region.X > Ui->Width) || (Region.Y > Ui->Height) ||
      (Region.Width > Ui->Width - Region.X) ||
      (Region.Height > Ui->Height - Region.Y))
  {
    return FALSE;
  }

  CropWidth  = HOME_BACKGROUND_WIDTH;
  CropHeight = HOME_BACKGROUND_HEIGHT;
  if (Region.Width * CropHeight > Region.Height * CropWidth) {
    CropHeight = MAX (1, CropWidth * Region.Height / Region.Width);
  } else {
    CropWidth = MAX (1, CropHeight * Region.Width / Region.Height);
  }
  Left = (HOME_BACKGROUND_WIDTH - CropWidth) / 2;
  Top  = (HOME_BACKGROUND_HEIGHT - CropHeight) / 2;
  Pixels = AllocatePool (Region.Width * Region.Height * sizeof (*Pixels));
  if (Pixels == NULL) {
    return FALSE;
  }

  for (Y = 0; Y < Region.Height; Y++) {
    SourceY = (Region.Height > 1) ? Y * (CropHeight - 1) * 256 / (Region.Height - 1) : 0;
    Fy = SourceY & 255;
    SourceY = Top + (SourceY >> 8);
    NextY = MIN (SourceY + 1, Top + CropHeight - 1);
    for (X = 0; X < Region.Width; X++) {
      SourceX = (Region.Width > 1) ? X * (CropWidth - 1) * 256 / (Region.Width - 1) : 0;
      Fx = SourceX & 255;
      SourceX = Left + (SourceX >> 8);
      NextX = MIN (SourceX + 1, Left + CropWidth - 1);
      A = HomePixel (SourceX, SourceY);
      B = HomePixel (NextX, SourceY);
      C = HomePixel (SourceX, NextY);
      D = HomePixel (NextX, NextY);
      Pixels[Y * Region.Width + X].Red = HomeInterpolate (A.Red, B.Red, C.Red, D.Red, Fx, Fy);
      Pixels[Y * Region.Width + X].Green = HomeInterpolate (A.Green, B.Green, C.Green, D.Green, Fx, Fy);
      Pixels[Y * Region.Width + X].Blue = HomeInterpolate (A.Blue, B.Blue, C.Blue, D.Blue, Fx, Fy);
      Pixels[Y * Region.Width + X].Reserved = 0;
    }
  }
  Status = ModernUiRestoreRect (Ui, Region, Pixels);
  if (EFI_ERROR (Status)) {
    FreePool (Pixels);
  } else {
    mHomePristine = Pixels;
    mHomeRegion   = Region;
    mHomeOwner    = Ui;
    mHomeWidth    = Ui->Width;
    mHomeHeight   = Ui->Height;
  }
  return (BOOLEAN)!EFI_ERROR (Status);
#else
  return FALSE;
#endif
}



/** Restore the immutable image on same-sized reentry without resampling.
    Always writes the entire region: another image may have owned GOP since
    our last frame. A size/owner/crop change prepares a fresh bounded cache. */
BOOLEAN
ModernUiBackdropRedraw (
  IN MODERN_UI_RENDER_CONTEXT *Ui,
  IN MODERN_UI_RECT Region
  )
{
#if MODERN_SETUP_HOME_BACKGROUND
  if ((Ui != NULL) && (Ui == mHomeOwner) && (mHomePristine != NULL) &&
      (Ui->Width == mHomeWidth) && (Ui->Height == mHomeHeight) &&
      (Region.X == mHomeRegion.X) && (Region.Y == mHomeRegion.Y) &&
      (Region.Width == mHomeRegion.Width) && (Region.Height == mHomeRegion.Height)) {
    if (!EFI_ERROR (ModernUiRestoreRect (Ui, Region, mHomePristine))) {
      return TRUE;
    }
    ModernUiBackdropRelease ();
    return FALSE;
  }
#endif
  return ModernUiBackdropPrepare (Ui, Region);
}

RETURN_STATUS
EFIAPI
ModernUiBackdropDestructor (VOID)
{
  ModernUiBackdropRelease ();
  return EFI_SUCCESS;
}
