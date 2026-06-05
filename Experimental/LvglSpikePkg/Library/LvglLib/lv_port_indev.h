
/**
 * @file lv_port_indev.h
 *
 */

#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "LvglLibCommon.h"

/*********************
 *      DEFINES
 *********************/

//
// Custom LVGL key codes for EFI function keys F1-F12.
// Mirror of the defines in <Library/LvglLib.h>.
//
#define LV_KEY_F1   0x0101U
#define LV_KEY_F2   0x0102U
#define LV_KEY_F3   0x0103U
#define LV_KEY_F4   0x0104U
#define LV_KEY_F5   0x0105U
#define LV_KEY_F6   0x0106U
#define LV_KEY_F7   0x0107U
#define LV_KEY_F8   0x0108U
#define LV_KEY_F9   0x0109U
#define LV_KEY_F10  0x010AU
#define LV_KEY_F11  0x010BU
#define LV_KEY_F12  0x010CU

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void lv_port_indev_init(lv_display_t * disp);

void lv_port_indev_close();

/**
  Drain the EFI keyboard buffer and reset the LVGL keypad indev state so that
  no pending key-press leaks into the next event loop.
**/
void lv_uefi_keypad_drain(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_PORT_INDEV_TEMPL_H*/
