/** @file
  LvglTheme.h - User-customizable theme for LvglDisplayEngineDxe.

  Edit the macros in this file and rebuild to restyle the LVGL HII form
  renderer. All colors, fonts, padding, and radius values used by
  LvglFormRenderer.c flow through these macros.

  Fonts referenced here must be enabled in LvglPkg/lv_conf.h
  (LV_FONT_MONTSERRAT_*).
**/

#ifndef LVGL_THEME_H_
#define LVGL_THEME_H_

#include <lvgl.h>

//
// Background colors (24-bit RGB; passed to lv_color_hex())
//
// NovaCore palette: near-black canvas, blue-tinted panels, accent blue.
//
#define THEME_COLOR_BG_SCREEN     0x0F141A
#define THEME_COLOR_BG_PANEL      0x182328
#define THEME_COLOR_BG_PANEL_ELEV 0x23304A
#define THEME_COLOR_BG_DIALOG     0x1B2A3A
#define THEME_COLOR_BG_SEPARATOR  0x2A3A52

//
// Text colors
//
#define THEME_COLOR_TEXT_TITLE     0xFFFFFF
#define THEME_COLOR_TEXT_PRIMARY   0xEAF2FF
#define THEME_COLOR_TEXT_SECONDARY 0x9FB0C8
#define THEME_COLOR_TEXT_DISABLED  0x6A7B92
#define THEME_COLOR_TEXT_LABEL     THEME_COLOR_TEXT_PRIMARY
#define THEME_COLOR_TEXT_POPUP     THEME_COLOR_TEXT_PRIMARY

//
// Row colors (default vs focused).
//
#define THEME_COLOR_ROW_BG            THEME_COLOR_BG_PANEL
#define THEME_COLOR_ROW_TEXT          THEME_COLOR_TEXT_PRIMARY
#define THEME_COLOR_ROW_TEXT_FOCUSED  0xFFFFFF
#define THEME_COLOR_ROW_TEXT_DISABLED THEME_COLOR_TEXT_DISABLED

//
// Accent (focus ring + active selection).
//
#define THEME_COLOR_ACCENT        0x1A6FD8
#define THEME_COLOR_ACCENT_HOVER  0x2A85F0
#define THEME_COLOR_SUCCESS       0x2BC79F
#define THEME_COLOR_WARNING       0xE9A23B
#define THEME_COLOR_DANGER        0xE25555

//
// Accent (subtitle / focus highlight) — LV_PALETTE_* enum value
//
#define THEME_ACCENT_PALETTE      LV_PALETTE_BLUE

//
// Fonts — must be enabled in lv_conf.h
//
#define THEME_FONT_TITLE          (&lv_font_montserrat_20)
#define THEME_FONT_BODY           (&lv_font_montserrat_16)
#define THEME_FONT_POPUP          (&lv_font_montserrat_16)

//
// Spacing & shape (pixels)
//
#define THEME_PAD_SCREEN          16
#define THEME_PAD_PANEL           8
#define THEME_PAD_ROW             4
#define THEME_PAD_ROW_TIGHT       2
#define THEME_PAD_DIALOG          16
#define THEME_PAD_DIALOG_ROW_GAP  10
#define THEME_PAD_DIALOG_COL_GAP  8
#define THEME_PAD_SCREEN_ROW_GAP  6
#define THEME_PAD_PANEL_ROW_GAP   4
#define THEME_PAD_LABEL_TOP       8
#define THEME_RADIUS              8
#define THEME_OVERLAY_OPA         LV_OPA_50

//
// Aptio-style chrome (header / subtitle / footer / wallpaper).
// Used by LvglDisplayEngineDxe/LvglAptioChrome.c.
//
#define THEME_COLOR_HEADER_BG_TOP     0x0B1218
#define THEME_COLOR_HEADER_BG_BOTTOM  0x16263F
#define THEME_COLOR_HEADER_TEXT       0xFFFFFF
#define THEME_COLOR_HEADER_DIM        0x9FB0C8
#define THEME_COLOR_SUBTITLE_BG       0x182328
#define THEME_COLOR_SUBTITLE_TEXT     0xEAF2FF
#define THEME_COLOR_FOOTER_BG         0x0B1218
#define THEME_COLOR_FOOTER_TEXT       0xEAF2FF
#define THEME_COLOR_FOOTER_DIM        0x6A7B92
#define THEME_COLOR_HIGHLIGHT_ROW     THEME_COLOR_ACCENT

#define THEME_HEADER_HEIGHT       44
#define THEME_SUBTITLE_HEIGHT     28
#define THEME_FOOTER_HEIGHT       32

//
// Chrome — help pane (right column).
//
#define THEME_HELPPANE_WIDTH         240
#define THEME_PAD_HELPPANE_X         14
#define THEME_PAD_HELPPANE_Y         12
#define THEME_PAD_HELPPANE_ROW_GAP   8

//
// Chrome — header / subtitle / content / footer paddings.
//
#define THEME_PAD_HEADER_X           16
#define THEME_PAD_SUBTITLE_X         16
#define THEME_PAD_CONTENT_LEFT       24
#define THEME_PAD_CONTENT_RIGHT      16
#define THEME_PAD_CONTENT_Y          12
#define THEME_PAD_CONTENT_ROW_GAP    6
#define THEME_PAD_FOOTER_X           12
#define THEME_PAD_FOOTER_Y           2
#define THEME_PAD_FOOTER_COL_GAP     6

//
// Footer "chip" widgets (one per registered hotkey + nav primitives).
//
#define THEME_PAD_CHIP_X             8
#define THEME_PAD_CHIP_Y             4
#define THEME_PAD_CHIP_COL_GAP       6
#define THEME_RADIUS_CHIP            4

//
// Form rows (StyleRow).
//
#define THEME_RADIUS_ROW             4
#define THEME_PAD_ROW_X              16
#define THEME_PAD_ROW_Y              10
#define THEME_BORDER_FOCUS           3

//
// Generic 1 px separator/border thickness used between panes.
//
#define THEME_BORDER_PANE            1

//
// Confirm/discard dialog card width as a percentage of the screen.
//
#define THEME_DIALOG_WIDTH_PCT       50

//
// Static strings shown in the chrome (no SMBIOS lookup yet).
//
#define APTIO_HEADER_TITLE        "EDK II Boot Manager"
#define APTIO_HEADER_VENDOR       "UEFI v2.90  |  EDK II"
#define APTIO_FOOTER_NAV_HINTS    LV_SYMBOL_UP LV_SYMBOL_DOWN " Select   Enter Confirm   Esc Exit   F9 Defaults   F10 Save"
#define APTIO_FOOTER_SYSINFO      "QEMU x86_64  |  OVMF"

#endif // LVGL_THEME_H_
