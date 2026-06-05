/** @file
  LvglSpikeProbe -- experimental LVGL-on-LoongArch render demo.

  Brings up LVGL's upstream UEFI backend on a GOP display and draws a small demo
  screen (themed background + title + subtitle + rounded button), then runs the
  LVGL handler loop holding the frame until ESC is pressed. This both forces the
  full LVGL closure (core + software renderer + UEFI port, with the
  LoongArch64/RISC-V64 arch-gate patch) to compile/link AND gives a visible
  on-hardware result.

  experimental/lvgl-spike only; never ship, never default overlay.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>

#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/uefi/lv_uefi.h"
#include "lvgl/src/drivers/uefi/lv_uefi_context.h"
#include "lvgl/src/drivers/uefi/lv_uefi_display.h"

/**
  Build the demo UI on the active screen: dark background, a large title, a
  subtitle line, and a rounded accent button with a centered caption.

  @param[in] Screen  The LVGL screen object to populate. Must be non-NULL.
**/
STATIC
VOID
LvglSpikeBuildUi (
  IN lv_obj_t  *Screen
  )
{
  lv_obj_t  *Title;
  lv_obj_t  *Subtitle;
  lv_obj_t  *Button;
  lv_obj_t  *ButtonLabel;

  lv_obj_set_style_bg_color (Screen, lv_color_hex (0x0E1116), LV_PART_MAIN);
  lv_obj_set_style_bg_opa (Screen, LV_OPA_COVER, LV_PART_MAIN);

  Title = lv_label_create (Screen);
  lv_label_set_text (Title, "ModernSetupPkg  x  LVGL");
  lv_obj_set_style_text_color (Title, lv_color_hex (0xF2C14E), LV_PART_MAIN);
  lv_obj_set_style_text_font (Title, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_align (Title, LV_ALIGN_CENTER, 0, -60);

  Subtitle = lv_label_create (Screen);
  lv_label_set_text (Subtitle, "LVGL v9.5.0 software renderer on LoongArch64 UEFI");
  lv_obj_set_style_text_color (Subtitle, lv_color_hex (0xB8C0CC), LV_PART_MAIN);
  lv_obj_align (Subtitle, LV_ALIGN_CENTER, 0, -20);

  Button = lv_button_create (Screen);
  lv_obj_set_size (Button, 220, 56);
  lv_obj_align (Button, LV_ALIGN_CENTER, 0, 50);
  lv_obj_set_style_radius (Button, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color (Button, lv_color_hex (0xF2C14E), LV_PART_MAIN);

  ButtonLabel = lv_label_create (Button);
  lv_label_set_text (ButtonLabel, "Press ESC to exit");
  lv_obj_set_style_text_color (ButtonLabel, lv_color_hex (0x10141A), LV_PART_MAIN);
  lv_obj_center (ButtonLabel);
}

/**
  Entry point. Initializes the LVGL UEFI backend + core, creates one display on
  a GOP-backed handle, draws the demo UI, then pumps lv_timer_handler() in a
  loop (advancing the tick manually) until the user presses ESC. Falls back to a
  text message on ConOut when no GOP display can be found.

  @param[in] ImageHandle  Firmware-allocated image handle.
  @param[in] SystemTable  Pointer to the EFI System Table.

  @retval EFI_SUCCESS     Always (build/render probe, not a product).
**/
EFI_STATUS
EFIAPI
LvglSpikeProbeMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  void           *DisplayHandle;
  lv_display_t   *Display;
  EFI_INPUT_KEY  Key;
  EFI_STATUS     Status;

  Print (L"LvglSpikeProbe: start\n");
  lv_uefi_init (ImageHandle, SystemTable);
  Print (L"LvglSpikeProbe: lv_uefi_init done\n");
  lv_init ();
  Print (L"LvglSpikeProbe: lv_init done\n");

  DisplayHandle = lv_uefi_display_get_any ();
  Print (L"LvglSpikeProbe: display handle = %p\n", DisplayHandle);
  if (DisplayHandle == NULL) {
    Print (L"LvglSpikeProbe: no EFI_GRAPHICS_OUTPUT_PROTOCOL display found.\n");
    return EFI_UNSUPPORTED;
  }

  Display = lv_uefi_display_create (DisplayHandle);
  Print (L"LvglSpikeProbe: display created = %p\n", Display);
  if (Display == NULL) {
    Print (L"LvglSpikeProbe: lv_uefi_display_create failed.\n");
    return EFI_DEVICE_ERROR;
  }

  LvglSpikeBuildUi (lv_screen_active ());
  Print (L"LvglSpikeProbe: UI built; rendering -- press ESC to exit\n");

  //
  // Hold the rendered frame; ESC exits. Tick is advanced manually so LVGL
  // timers/refresh run even if no timestamp tick source is registered.
  //
  for ( ; ; ) {
    lv_timer_handler ();
    gBS->Stall (10 * 1000);
    lv_tick_inc (10);

    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if (!EFI_ERROR (Status) && (Key.ScanCode == SCAN_ESC)) {
      break;
    }
  }

  gST->ConOut->ClearScreen (gST->ConOut);
  return EFI_SUCCESS;
}
