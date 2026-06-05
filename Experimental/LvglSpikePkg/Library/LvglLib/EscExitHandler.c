#include "LvglLibCommon.h"
#include <Library/LvglLib.h>


UINT8 mExitBtnYes = EXIT_BTN_NONE;
STATIC BOOLEAN mPopUpActive = FALSE;
VOID *mEscNotifyHandle = NULL;

static lv_group_t *g_default_group = NULL;

void exit_confirm_cb(lv_event_t * e)
{

    lv_obj_t * btn = lv_event_get_target_obj(e);
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    LV_UNUSED(label);

    const char * txt = lv_label_get_text(label);

    if (strcmp(txt, "Yes") == 0) {
      mExitBtnYes = EXIT_BTN_YES;
    } else if (strcmp(txt, "No") == 0) {
      mExitBtnYes = EXIT_BTN_NO;
    } else {
      mExitBtnYes = EXIT_BTN_NONE;
    }

    lv_obj_t * msgbox = lv_event_get_user_data(e);

    lv_group_t *msgbox_group = (lv_group_t *)lv_obj_get_user_data(msgbox);

    lv_indev_t *indev = NULL;
    for(;;) {
      indev = lv_indev_get_next(indev);
      if(!indev) {
        break;
      }
      
      if(lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD) {
        lv_indev_set_group(indev, g_default_group);
      }
    }

    lv_group_del(msgbox_group);

    lv_msgbox_close(msgbox);

    mPopUpActive = false;
}


EFI_STATUS
EFIAPI
EscKeyNotifyCallBack (
  IN EFI_KEY_DATA  *KeyData
  )
{
  if (mPopUpActive) {
    return EFI_ALREADY_STARTED;
  }

  mPopUpActive = true;

  g_default_group = lv_group_get_default();

  lv_group_t *msgbox_group = lv_group_create();

  lv_obj_t * mbox = lv_msgbox_create(NULL);

  lv_msgbox_add_title(mbox, "Exit?");
  lv_msgbox_add_text(mbox, "Exit this APP?");

  lv_obj_t * btn_yes = lv_msgbox_add_footer_button(mbox, "Yes");
  lv_obj_add_event_cb(btn_yes, exit_confirm_cb, LV_EVENT_CLICKED, mbox);
  lv_group_add_obj(msgbox_group, btn_yes);

  lv_obj_t * btn_no = lv_msgbox_add_footer_button(mbox, "No");
  lv_obj_add_event_cb(btn_no, exit_confirm_cb, LV_EVENT_CLICKED, mbox);
  lv_group_add_obj(msgbox_group, btn_no);

  lv_group_focus_obj(btn_yes);

  lv_indev_t *indev = NULL;
  for(;;) {
    indev = lv_indev_get_next(indev);
    if(!indev) {
      break;
    }
    
    if(lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD) {
      lv_indev_set_group(indev, msgbox_group);
    }
  }

  lv_obj_set_user_data(mbox, msgbox_group);

  lv_obj_align(mbox, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t * bg = lv_obj_get_parent(mbox);
  lv_obj_set_style_bg_opa(bg, LV_OPA_70, 0);
  lv_obj_set_style_bg_color(bg, lv_palette_main(LV_PALETTE_GREY), 0);

  return EFI_SUCCESS;

}

VOID
EFIAPI
LvglUefiEscExitRegister (
  VOID
  )
{
  EFI_STATUS                         Status;
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *TextInEx;
  EFI_KEY_DATA                       KeyData;

  Status = gBS->HandleProtocol(gST->ConsoleInHandle, &gEfiSimpleTextInputExProtocolGuid, (VOID**)&TextInEx);
  if (EFI_ERROR(Status)) {
    return;
  }

  ZeroMem(&KeyData, sizeof(KeyData));
  KeyData.Key.ScanCode = SCAN_ESC;
  Status = TextInEx->RegisterKeyNotify (
                       TextInEx,
                       &KeyData, 
                       EscKeyNotifyCallBack,
                       &mEscNotifyHandle
                       );

}

VOID
EFIAPI
LvglUefiEscExitUnregister (
  VOID
  )
{
  EFI_STATUS                         Status;
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *TextInEx;

  if (mEscNotifyHandle == NULL) {
    return;
  }

  Status = gBS->HandleProtocol(gST->ConsoleInHandle, &gEfiSimpleTextInputExProtocolGuid, (VOID**)&TextInEx);
  if (EFI_ERROR(Status)) {
    return;
  }

  Status = TextInEx->UnregisterKeyNotify (TextInEx, mEscNotifyHandle);
  if (!EFI_ERROR(Status)) {
    mEscNotifyHandle = NULL;
  }
}
