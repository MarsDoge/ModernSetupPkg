/**
 * @file lv_port_indev.c
 *
 */


/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"

#include "lvgl/src/indev/lv_indev_private.h"

/*********************
 *      DEFINES
 *********************/

extern const lv_img_dsc_t mouse_cursor_icon;

typedef struct {
  EFI_SIMPLE_POINTER_PROTOCOL    *SimplePointer;
  EFI_ABSOLUTE_POINTER_PROTOCOL  *AbsPointer;
  INTN                           LastCursorX;
  INTN                           LastCursorY;
  UINT64                         LastAbsZ;
  INT32                          WheelDelta;
  UINT32                         ActiveButtons;
  BOOLEAN                        LeftButton;
  BOOLEAN                        RightButton;
} LVGL_UEFI_MOUSE;

//
// LVGL treats any non-zero pointer.diff as one full detent and scrolls by
// scroll_limit (default 10px) per call (lv_indev.c:1638). QEMU usb-mouse can
// send several wheel reports per perceived host wheel motion, so we ratchet:
// accumulate raw Z counts and only emit one ±1 detent per N counts.
// Lower this number = more sensitive. Raise = less sensitive.
//
#define LVGL_WHEEL_COUNTS_PER_DETENT  8

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void mouse_read(lv_indev_t * indev, lv_indev_data_t * data);

static void keypad_read(lv_indev_t * indev, lv_indev_data_t * data);


/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t * indev_mouse;
lv_indev_t * indev_keypad;

LVGL_UEFI_MOUSE mLvglUefiMouse;

//
// Protocol-install notification: fires when any driver installs
// EFI_ABSOLUTE_POINTER_PROTOCOL (e.g. when USB mouse binds during BDS
// ConnectAll). At constructor time USB is not yet enumerated, so the
// initial lv_uefi_mouse_create() call fails; this callback retries
// once the pointer protocol actually appears.
//
STATIC EFI_EVENT    mPointerNotifyEvent        = NULL;
STATIC VOID         *mPointerNotifyRegistration = NULL;
STATIC lv_display_t *mPointerNotifyDisplay     = NULL;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

static void keypad_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
{
  EFI_STATUS                         Status;
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *TxtInEx;
  EFI_KEY_DATA                       KeyData;
  UINT32                             KeyShift;

  data->key = 0;

  Status = gBS->HandleProtocol (gST->ConsoleInHandle, &gEfiSimpleTextInputExProtocolGuid, (VOID **)&TxtInEx);
  Status = TxtInEx->ReadKeyStrokeEx (TxtInEx, &KeyData);
  if (!EFI_ERROR (Status)) {
    switch (KeyData.Key.UnicodeChar) {
      case CHAR_CARRIAGE_RETURN:
        data->key = LV_KEY_ENTER;
        break;

      case CHAR_BACKSPACE:
        data->key = LV_KEY_BACKSPACE;
        break;

      case CHAR_TAB:
        KeyShift = KeyData.KeyState.KeyShiftState;
        data->key = (KeyShift & EFI_SHIFT_STATE_VALID) && (KeyShift & (EFI_RIGHT_SHIFT_PRESSED | EFI_LEFT_SHIFT_PRESSED)) ? LV_KEY_PREV : LV_KEY_NEXT;
        break;

      case CHAR_NULL:
        switch (KeyData.Key.ScanCode) {
        case SCAN_UP:
          data->key = LV_KEY_UP;
          break;
        
        case SCAN_DOWN:
          data->key = LV_KEY_DOWN;
          break;

        case SCAN_RIGHT:
          data->key = LV_KEY_RIGHT;
          break;

        case SCAN_LEFT:
          data->key = LV_KEY_LEFT;
          break;

        case SCAN_ESC:
          data->key = LV_KEY_ESC;
          break;

        case SCAN_DELETE:
          data->key = LV_KEY_DEL;
          break;

        case SCAN_PAGE_DOWN:
          data->key = LV_KEY_NEXT;
          break;

        case SCAN_PAGE_UP:
          data->key = LV_KEY_PREV;
          break;

        case SCAN_HOME:
          data->key = LV_KEY_HOME;
          break;

        case SCAN_END:
          data->key = LV_KEY_END;
          break;

        case SCAN_F1:  data->key = LV_KEY_F1;  break;
        case SCAN_F2:  data->key = LV_KEY_F2;  break;
        case SCAN_F3:  data->key = LV_KEY_F3;  break;
        case SCAN_F4:  data->key = LV_KEY_F4;  break;
        case SCAN_F5:  data->key = LV_KEY_F5;  break;
        case SCAN_F6:  data->key = LV_KEY_F6;  break;
        case SCAN_F7:  data->key = LV_KEY_F7;  break;
        case SCAN_F8:  data->key = LV_KEY_F8;  break;
        case SCAN_F9:  data->key = LV_KEY_F9;  break;
        case SCAN_F10: data->key = LV_KEY_F10; break;
        case SCAN_F11: data->key = LV_KEY_F11; break;
        case SCAN_F12: data->key = LV_KEY_F12; break;

        default:
          break;
        }
        break;

      case CHAR_LINEFEED:
        break;

      default:
        data->key = KeyData.Key.UnicodeChar;
        break;
    }

    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }

}


lv_indev_t * lv_uefi_keyboard_create(void)
{
    lv_indev_t * indev = lv_indev_create();
    LV_ASSERT_MALLOC(indev);
    if(indev == NULL) {
        return NULL;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, keypad_read);

    return indev;
}


EFI_STATUS
EFIAPI
GetXYZ (
  lv_indev_t * indev_drv
  )
{
  EFI_STATUS                     Status;
  EFI_ABSOLUTE_POINTER_PROTOCOL  *AbsPointer = NULL;
  EFI_ABSOLUTE_POINTER_STATE     AbsState;
  EFI_SIMPLE_POINTER_PROTOCOL    *SimplePointer = NULL;
  EFI_SIMPLE_POINTER_STATE       SimpleState;

  lv_display_t *disp = lv_indev_get_display(indev_drv);

  int32_t hor_res = lv_display_get_horizontal_resolution(disp);
  int32_t ver_res = lv_display_get_vertical_resolution(disp);

  if (mLvglUefiMouse.AbsPointer != NULL) {
    AbsPointer = mLvglUefiMouse.AbsPointer;
    Status = AbsPointer->GetState (AbsPointer, &AbsState);
    if (!EFI_ERROR (Status)) {
      mLvglUefiMouse.LastCursorX = (AbsState.CurrentX * hor_res) / (AbsPointer->Mode->AbsoluteMaxX - AbsPointer->Mode->AbsoluteMinX);
      if (mLvglUefiMouse.LastCursorX > hor_res - 1) {
        mLvglUefiMouse.LastCursorX = hor_res - 1;
      }
      mLvglUefiMouse.LastCursorY = (AbsState.CurrentY * ver_res) / (AbsPointer->Mode->AbsoluteMaxY - AbsPointer->Mode->AbsoluteMinY);
      if (mLvglUefiMouse.LastCursorY > ver_res - 1) {
        mLvglUefiMouse.LastCursorY = ver_res - 1;
      }
      mLvglUefiMouse.LeftButton = AbsState.ActiveButtons & BIT0;

      mLvglUefiMouse.WheelDelta += (INT32)(AbsState.CurrentZ - mLvglUefiMouse.LastAbsZ);
      mLvglUefiMouse.LastAbsZ = AbsState.CurrentZ;

      return EFI_SUCCESS;
    }
  } else if (mLvglUefiMouse.SimplePointer != NULL) {
    SimplePointer = mLvglUefiMouse.SimplePointer;
    Status = SimplePointer->GetState (SimplePointer, &SimpleState);
    if (!EFI_ERROR (Status)) {
      mLvglUefiMouse.LastCursorX += (SimpleState.RelativeMovementX * hor_res) / (INT32)(50 * SimplePointer->Mode->ResolutionX);
      if (mLvglUefiMouse.LastCursorX > hor_res - 1) {
        mLvglUefiMouse.LastCursorX = hor_res - 1;
      }
      if (mLvglUefiMouse.LastCursorX < 0) {
        mLvglUefiMouse.LastCursorX = 0;
      }
      mLvglUefiMouse.LastCursorY += (SimpleState.RelativeMovementY * ver_res) / (INT32)(50 * SimplePointer->Mode->ResolutionY);
      if (mLvglUefiMouse.LastCursorY > ver_res - 1) {
        mLvglUefiMouse.LastCursorY = ver_res - 1;
      }
      if (mLvglUefiMouse.LastCursorY < 0) {
        mLvglUefiMouse.LastCursorY = 0;
      }

      mLvglUefiMouse.LeftButton = SimpleState.LeftButton;
      mLvglUefiMouse.RightButton = SimpleState.RightButton;

      mLvglUefiMouse.WheelDelta += SimpleState.RelativeMovementZ;

      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_READY;
}


EFI_STATUS
EFIAPI
EfiMouseInit (
  VOID
  )
{
  EFI_STATUS                     Status;
  EFI_ABSOLUTE_POINTER_PROTOCOL  *AbsPointer = NULL;
  EFI_SIMPLE_POINTER_PROTOCOL    *SimplePointer = NULL;
  EFI_HANDLE                     *HandleBuffer = NULL;
  UINTN                          HandleCount, Index;
  EFI_DEVICE_PATH_PROTOCOL       *DevicePath;
  BOOLEAN                        AbsPointerSupport = FALSE;
  BOOLEAN                        SimplePointerSupport = FALSE;

  HandleCount = 0;
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiAbsolutePointerProtocolGuid, NULL, &HandleCount, &HandleBuffer);
  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);
    if (!EFI_ERROR (Status)) {
      AbsPointerSupport = TRUE;
      break;
    }
  }
  if (HandleBuffer!= NULL) {
    FreePool (HandleBuffer);
    HandleBuffer = NULL;
  }

  if (AbsPointerSupport) {
    goto StartInit;
  }

  HandleCount = 0;
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimplePointerProtocolGuid, NULL, &HandleCount, &HandleBuffer);
  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);
    if (!EFI_ERROR (Status)) {
      SimplePointerSupport = TRUE;
      break;
    }
  }
  if (HandleBuffer!= NULL) {
    FreePool (HandleBuffer);
    HandleBuffer = NULL;
  }

  if (!AbsPointerSupport && !SimplePointerSupport) {
    return EFI_UNSUPPORTED;
  }

StartInit:
  DebugPrint (DEBUG_INFO, "EfiMouseInit()\n");

  DebugPrint (DEBUG_INFO, "Abs Pointer: %d\n", AbsPointerSupport);
  DebugPrint (DEBUG_INFO, "Simple Pointer: %d\n", SimplePointerSupport);

  mLvglUefiMouse.AbsPointer    = NULL;
  mLvglUefiMouse.SimplePointer = NULL;
  mLvglUefiMouse.LastAbsZ      = 0;
  mLvglUefiMouse.WheelDelta    = 0;
  mLvglUefiMouse.ActiveButtons = 0;
  mLvglUefiMouse.LeftButton    = FALSE;
  mLvglUefiMouse.RightButton   = FALSE;

  if (AbsPointerSupport) {
    Status = gBS->HandleProtocol (gST->ConsoleInHandle, &gEfiAbsolutePointerProtocolGuid, (VOID **)&AbsPointer);
    if (!EFI_ERROR (Status) && AbsPointer != NULL) {
      mLvglUefiMouse.AbsPointer = AbsPointer;
    }
  }

  if (SimplePointerSupport && !AbsPointerSupport) {
    Status = gBS->HandleProtocol (gST->ConsoleInHandle, &gEfiSimplePointerProtocolGuid, (VOID **)&SimplePointer);
    if (!EFI_ERROR (Status) && SimplePointer != NULL) {
      mLvglUefiMouse.SimplePointer = SimplePointer;
    }
  }

  return Status;
}


//
// Scroll amount in pixels per wheel detent. LVGL's built-in
// indev_proc_pointer_diff path uses indev->scroll_limit (default 10px) but
// only fires when pointer.last_pressed is set — i.e. after the user has
// clicked or dragged once. We bypass that by directly scrolling the
// nearest scrollable ancestor of the obj under the cursor.
//
#define LVGL_WHEEL_SCROLL_PIXELS  40

static lv_obj_t * find_scrollable_at_point(lv_display_t * disp, lv_point_t * p)
{
  lv_obj_t *screen = lv_display_get_screen_active(disp);
  if (screen == NULL) return NULL;

  lv_obj_t *hit = lv_indev_search_obj(screen, p);
  if (hit == NULL) return NULL;

  for (lv_obj_t *o = hit; o != NULL; o = lv_obj_get_parent(o)) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_SCROLLABLE) &&
        lv_obj_get_scroll_top(o) + lv_obj_get_scroll_bottom(o) > 0)
    {
      return o;
    }
  }
  return NULL;
}

static void mouse_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
{
  int wheel_step = 0;

  GetXYZ(indev_drv);
  data->point.x = mLvglUefiMouse.LastCursorX;
  data->point.y = mLvglUefiMouse.LastCursorY;
  if (mLvglUefiMouse.LeftButton) {
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }

  if (mLvglUefiMouse.WheelDelta >= LVGL_WHEEL_COUNTS_PER_DETENT) {
    wheel_step = 1;
    mLvglUefiMouse.WheelDelta -= LVGL_WHEEL_COUNTS_PER_DETENT;
  } else if (mLvglUefiMouse.WheelDelta <= -LVGL_WHEEL_COUNTS_PER_DETENT) {
    wheel_step = -1;
    mLvglUefiMouse.WheelDelta += LVGL_WHEEL_COUNTS_PER_DETENT;
  }

  data->enc_diff = 0;

  if (wheel_step != 0) {
    lv_display_t *disp = lv_indev_get_display(indev_drv);
    lv_point_t p = { (int32_t)data->point.x, (int32_t)data->point.y };
    lv_obj_t *target = find_scrollable_at_point(disp, &p);
    if (target != NULL) {
      lv_obj_scroll_by_bounded(
        target,
        0,
        wheel_step * LVGL_WHEEL_SCROLL_PIXELS,
        LV_ANIM_OFF
        );
    }
  }
}


void lv_indev_set_cusor_start(lv_indev_t * indev)
{
    if(indev == NULL) return;

    lv_display_t *disp = lv_indev_get_display(indev);

    int32_t hor_res = lv_display_get_horizontal_resolution(disp);
    int32_t ver_res = lv_display_get_vertical_resolution(disp);

    indev->pointer.act_point.x = hor_res / 2;
    indev->pointer.act_point.y = ver_res / 2;
    mLvglUefiMouse.LastCursorX = hor_res / 2;
    mLvglUefiMouse.LastCursorY = ver_res / 2;
    mLvglUefiMouse.WheelDelta = 0;
    mLvglUefiMouse.ActiveButtons = 0;
    mLvglUefiMouse.LeftButton = FALSE;
    mLvglUefiMouse.RightButton = FALSE;
}

lv_indev_t * lv_uefi_mouse_create(lv_display_t * disp)
{
    //
    // Idempotent: if a pointer indev already exists, return it.
    // This lets callers invoke lv_uefi_mouse_create() lazily — e.g. after BDS
    // has connected USB — without worrying about whether the constructor-time
    // attempt succeeded.
    //
    lv_indev_t * existing = NULL;
    while ((existing = lv_indev_get_next(existing)) != NULL) {
        if (lv_indev_get_type(existing) == LV_INDEV_TYPE_POINTER) {
            return existing;
        }
    }

    if (EfiMouseInit() != EFI_SUCCESS) {
        return NULL;
    }

    lv_indev_t * indev = lv_indev_create();
    LV_ASSERT_MALLOC(indev);
    if(indev == NULL) {
      return NULL;
    }

    DebugPrint (DEBUG_INFO, "Create Mouse\n");

    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, mouse_read);
    lv_indev_set_display (indev, disp);

    LV_IMG_DECLARE(mouse_cursor_icon);
    lv_obj_t * mouse_cursor = lv_image_create(lv_screen_active());
    lv_image_set_src(mouse_cursor, &mouse_cursor_icon);
    lv_indev_set_cusor_start(indev);
    lv_indev_set_cursor(indev, mouse_cursor);

    return indev;
}

STATIC
VOID
EFIAPI
OnPointerProtocolInstalled (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
    if (mPointerNotifyDisplay != NULL) {
        lv_uefi_mouse_create (mPointerNotifyDisplay);
    }
}

void lv_port_indev_init(lv_display_t * disp)
{
    lv_uefi_mouse_create(disp);

    lv_uefi_keyboard_create();

    //
    // If the mouse couldn't be created yet (USB not connected at this point
    // in boot), register for notification when the pointer protocol shows up.
    //
    if (mPointerNotifyEvent == NULL) {
        EFI_STATUS Status;

        mPointerNotifyDisplay = disp;

        Status = gBS->CreateEvent (
                       EVT_NOTIFY_SIGNAL,
                       TPL_CALLBACK,
                       OnPointerProtocolInstalled,
                       NULL,
                       &mPointerNotifyEvent
                       );
        if (!EFI_ERROR (Status)) {
            gBS->RegisterProtocolNotify (
                   &gEfiAbsolutePointerProtocolGuid,
                   mPointerNotifyEvent,
                   &mPointerNotifyRegistration
                   );
        }
    }
}

void lv_uefi_keypad_drain(void)
{
  EFI_STATUS                         Status;
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *TxtInEx;
  EFI_KEY_DATA                       KeyData;
  lv_indev_t                         *Indev;

  //
  // Drain all pending keystrokes from the EFI console.
  //
  Status = gBS->HandleProtocol (gST->ConsoleInHandle, &gEfiSimpleTextInputExProtocolGuid, (VOID **)&TxtInEx);
  if (!EFI_ERROR (Status)) {
    while (!EFI_ERROR (TxtInEx->ReadKeyStrokeEx (TxtInEx, &KeyData))) {
    }
  }

  //
  // Force every keypad indev to RELEASED so LVGL doesn't interpret the
  // pending ENTER release as a click on the next focused widget.
  //
  Indev = NULL;
  while ((Indev = lv_indev_get_next (Indev)) != NULL) {
    if (lv_indev_get_type (Indev) == LV_INDEV_TYPE_KEYPAD) {
      Indev->keypad.last_state = LV_INDEV_STATE_RELEASED;
      Indev->keypad.last_key   = 0;
    }
  }
}

void lv_port_indev_close()
{
  if (mPointerNotifyEvent != NULL) {
    gBS->CloseEvent (mPointerNotifyEvent);
    mPointerNotifyEvent         = NULL;
    mPointerNotifyRegistration  = NULL;
    mPointerNotifyDisplay       = NULL;
  }

  mLvglUefiMouse.AbsPointer = NULL;
  mLvglUefiMouse.SimplePointer = NULL;
  mLvglUefiMouse.LastCursorX = 0;
  mLvglUefiMouse.LastCursorY = 0;
  mLvglUefiMouse.LastAbsZ = 0;
  mLvglUefiMouse.WheelDelta = 0;
  mLvglUefiMouse.ActiveButtons = 0;
  mLvglUefiMouse.LeftButton = FALSE;
  mLvglUefiMouse.RightButton = FALSE;
}

