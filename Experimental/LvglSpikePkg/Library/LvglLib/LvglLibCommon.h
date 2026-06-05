

#ifndef __LVGL_LIB_COMMON_H__
#define __LVGL_LIB_COMMON_H__

#include "lvgl/lvgl.h"
#include "lv_port_indev.h"

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/TimerLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextInEx.h>
#include <Protocol/SimplePointer.h>
#include <Protocol/AbsolutePointer.h>

#include "LvglUefiPort.h"


#define  EXIT_BTN_NONE  0x0
#define  EXIT_BTN_YES   0x1
#define  EXIT_BTN_NO    0x2


lv_display_t * lv_uefi_disp_create(int32_t hor_res, int32_t ver_res);

VOID
EFIAPI
LvglUefiEscExitRegister (
  VOID
  );

VOID
EFIAPI
LvglUefiEscExitUnregister (
  VOID
  );

#endif
