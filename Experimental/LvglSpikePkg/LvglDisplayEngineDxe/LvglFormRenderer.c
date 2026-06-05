/** @file
  LvglDisplayEngineDxe -- the form renderer. Maps a SetupBrowser
  FORM_DISPLAY_ENGINE_FORM onto LVGL objects and drives the input loop.

  Skeleton scope (experimental/lvgl-spike, steps 1-2): render the form title and
  one row per statement (prompt text) as LVGL labels, then hold the frame until
  ESC. Richer widgets (dropdown/checkbox/spinbox/textarea) and live editing are
  layered on later; this proves "native form -> LVGL -> GOP" end to end without
  taking any IFR/config ownership from SetupBrowser.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "LvglDisplayEngineDxe.h"

//
// One LVGL display, created lazily on the first form (GOP may not be up when the
// driver loads). NULL until then.
//
STATIC lv_display_t  *mLvglDisplay = NULL;

/**
  Convert an HII string (returned as a freshly-allocated CHAR16*) to a pool
  CHAR8* for LVGL, which consumes UTF-8/ASCII. Non-ASCII code points degrade to
  '?' -- acceptable for the skeleton. Frees the input CHAR16 buffer.

  @param[in]  Unicode  HII string to convert. May be NULL.

  @retval  Pool-allocated CHAR8 string (caller frees), or NULL. Caller owns it.
**/
STATIC
CHAR8 *
HiiStringToAscii (
  IN EFI_STRING  Unicode
  )
{
  UINTN  Len;
  CHAR8  *Ascii;
  UINTN  Index;

  if (Unicode == NULL) {
    return NULL;
  }

  Len   = StrLen (Unicode);
  Ascii = AllocatePool (Len + 1);
  if (Ascii != NULL) {
    for (Index = 0; Index < Len; Index++) {
      Ascii[Index] = (Unicode[Index] < 0x80) ? (CHAR8)Unicode[Index] : '?';
    }

    Ascii[Len] = '\0';
  }

  FreePool (Unicode);
  return Ascii;
}

/**
  Read a statement's prompt string id from its IFR opcode. Question, text, and
  subtitle opcodes all carry an EFI_IFR_STATEMENT_HEADER immediately after the
  op header; its first field is the prompt string id. Display-only read; no
  config semantics are touched.

  @param[in]  Statement  The statement. Must be non-NULL with a non-NULL OpCode.

  @retval  The prompt EFI_STRING_ID, or 0 if none.
**/
STATIC
EFI_STRING_ID
GetStatementPromptId (
  IN FORM_DISPLAY_ENGINE_STATEMENT  *Statement
  )
{
  EFI_IFR_STATEMENT_HEADER  *Header;

  if ((Statement == NULL) || (Statement->OpCode == NULL)) {
    return 0;
  }

  Header = (EFI_IFR_STATEMENT_HEADER *)(Statement->OpCode + 1);
  return Header->Prompt;
}

/**
  Build the LVGL object tree for one form: a title label plus one prompt label
  per statement, on a themed background.

  @param[in]  FormData  The form to render. Must be non-NULL.
**/
STATIC
VOID
LvglBuildForm (
  IN FORM_DISPLAY_ENGINE_FORM  *FormData
  )
{
  lv_obj_t                       *Screen;
  lv_obj_t                       *List;
  lv_obj_t                       *Title;
  LIST_ENTRY                     *Link;
  FORM_DISPLAY_ENGINE_STATEMENT  *Statement;
  CHAR8                          *Text;

  Screen = lv_screen_active ();
  lv_obj_clean (Screen);
  lv_obj_set_style_bg_color (Screen, lv_color_hex (0x0E1116), LV_PART_MAIN);
  lv_obj_set_style_bg_opa (Screen, LV_OPA_COVER, LV_PART_MAIN);

  //
  // Title from the form's HII title string id.
  //
  Title = lv_label_create (Screen);
  Text  = HiiStringToAscii (HiiGetString (FormData->HiiHandle, FormData->FormTitle, NULL));
  lv_label_set_text (Title, (Text != NULL) ? Text : "Setup");
  lv_obj_set_style_text_color (Title, lv_color_hex (0xF2C14E), LV_PART_MAIN);
  lv_obj_set_style_text_font (Title, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_align (Title, LV_ALIGN_TOP_MID, 0, 16);
  if (Text != NULL) {
    FreePool (Text);
  }

  //
  // A scrollable column for the statement rows.
  //
  List = lv_obj_create (Screen);
  lv_obj_set_size (List, lv_pct (90), lv_pct (78));
  lv_obj_align (List, LV_ALIGN_BOTTOM_MID, 0, -16);
  lv_obj_set_flex_flow (List, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa (List, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width (List, 0, LV_PART_MAIN);

  //
  // One label per statement (prompt text). Display only.
  //
  for (Link = GetFirstNode (&FormData->StatementListHead);
       !IsNull (&FormData->StatementListHead, Link);
       Link = GetNextNode (&FormData->StatementListHead, Link))
  {
    lv_obj_t  *Row;

    Statement = FORM_DISPLAY_ENGINE_STATEMENT_FROM_LINK (Link);
    Text      = HiiStringToAscii (HiiGetString (FormData->HiiHandle, GetStatementPromptId (Statement), NULL));
    if (Text == NULL) {
      continue;
    }

    Row = lv_label_create (List);
    lv_label_set_text (Row, Text);
    lv_obj_set_style_text_color (Row, lv_color_hex (0xE6EAF0), LV_PART_MAIN);
    FreePool (Text);
  }
}

/**
  See LvglDisplayEngineDxe.h.
**/
EFI_STATUS
EFIAPI
LvglFormDisplay (
  IN  FORM_DISPLAY_ENGINE_FORM  *FormData,
  OUT USER_INPUT                *UserInputData
  )
{
  EFI_INPUT_KEY  Key;
  EFI_STATUS     Status;
  VOID           *Handle;

  if ((FormData == NULL) || (UserInputData == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (UserInputData, sizeof (USER_INPUT));

  //
  // Lazily create the LVGL display now that a GOP is expected to be present.
  //
  if (mLvglDisplay == NULL) {
    Handle = lv_uefi_display_get_any ();
    if (Handle == NULL) {
      return EFI_UNSUPPORTED;
    }

    mLvglDisplay = lv_uefi_display_create (Handle);
    if (mLvglDisplay == NULL) {
      return EFI_DEVICE_ERROR;
    }
  }

  LvglBuildForm (FormData);

  //
  // Hold the frame; ESC leaves the form. SetupBrowser owns everything else --
  // we only report the exit action back to it.
  //
  for ( ; ; ) {
    lv_timer_handler ();
    gBS->Stall (10 * 1000);
    lv_tick_inc (10);

    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if (!EFI_ERROR (Status) && (Key.ScanCode == SCAN_ESC)) {
      UserInputData->SelectedStatement = NULL;
      UserInputData->Action            = BROWSER_ACTION_FORM_EXIT;
      break;
    }
  }

  return EFI_SUCCESS;
}

/**
  See LvglDisplayEngineDxe.h.
**/
VOID
EFIAPI
LvglExitDisplay (
  VOID
  )
{
  if (mLvglDisplay != NULL) {
    lv_obj_clean (lv_screen_active ());
    lv_refr_now (mLvglDisplay);
  }

  gST->ConOut->ClearScreen (gST->ConOut);
  gST->ConOut->SetCursorPosition (gST->ConOut, 0, 0);
  gST->ConOut->EnableCursor (gST->ConOut, TRUE);
}
