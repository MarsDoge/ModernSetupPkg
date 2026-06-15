/** @file
  Input adapter library for ModernSetupPkg.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <ModernUi/ModernUiInput.h>

/**
  Convert raw firmware key data into a ModernUi input type.

  @param[in] ScanCode     UEFI scan code from Simple Text Input.
  @param[in] UnicodeChar  Unicode character from Simple Text Input.

  @return Normalized UI input type. Unsupported keys return ModernUiInputOther.
**/
STATIC
MODERN_UI_INPUT_TYPE
MapKey (
  IN UINT16  ScanCode,
  IN CHAR16  UnicodeChar
  )
{
  switch (ScanCode) {
    case SCAN_UP:
      return ModernUiInputUp;
    case SCAN_DOWN:
      return ModernUiInputDown;
    case SCAN_LEFT:
      return ModernUiInputLeft;
    case SCAN_RIGHT:
      return ModernUiInputRight;
    case SCAN_ESC:
      return ModernUiInputEscape;
    default:
      break;
  }

  switch (UnicodeChar) {
    case CHAR_CARRIAGE_RETURN:
      return ModernUiInputEnter;
    case CHAR_TAB:
      return ModernUiInputTab;
    case 0:
      return ModernUiInputOther;
    default:
      return ModernUiInputOther;
  }
}

/**
  Initialize the input context from firmware console and optional pointer services.

  @param[out] Context  Input context to initialize. Must not be NULL. Missing
                       optional protocols are recorded as NULL members.

  @retval EFI_SUCCESS            Context was initialized.
  @retval EFI_INVALID_PARAMETER  Context is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiInputInit (
  OUT MODERN_UI_INPUT_CONTEXT  *Context
  )
{
  EFI_STATUS  Status;

  if (Context == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Context, sizeof (*Context));
  Context->TextIn = gST->ConIn;

  Status = gBS->HandleProtocol (
                  gST->ConsoleInHandle,
                  &gEfiSimpleTextInputExProtocolGuid,
                  (VOID **)&Context->TextInEx
                  );
  if (EFI_ERROR (Status)) {
    Context->TextInEx = NULL;
  }

  Status = gBS->LocateProtocol (
                  &gEfiAbsolutePointerProtocolGuid,
                  NULL,
                  (VOID **)&Context->Pointer
                  );
  if (EFI_ERROR (Status)) {
    Context->Pointer = NULL;
  }

  return EFI_SUCCESS;
}

/**
  Wait for and return the next normalized UI input event.

  @param[in,out] Context  Initialized input context. Must not be NULL.
  @param[out]    Event    Receives a normalized input event. Must not be NULL.

  @retval EFI_SUCCESS            Event contains input data.
  @retval EFI_INVALID_PARAMETER  Context or Event is NULL.
  @retval EFI_NOT_READY          No usable input source is available.
**/
EFI_STATUS
EFIAPI
ModernUiReadInput (
  IN OUT MODERN_UI_INPUT_CONTEXT  *Context,
  OUT MODERN_UI_INPUT_EVENT       *Event
  )
{
  EFI_STATUS                  Status;
  EFI_EVENT                   Events[2];
  UINTN                       EventCount;
  UINTN                       Index;
  EFI_KEY_DATA                KeyData;
  EFI_INPUT_KEY               Key;
  EFI_ABSOLUTE_POINTER_STATE  PointerState;

  if ((Context == NULL) || (Event == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Event, sizeof (*Event));
  Event->Type = ModernUiInputNone;

  //
  // Poll the pointer first, non-blocking. Callers that pre-wait on the same
  // WaitForInput event (e.g. alongside a periodic tick) consume its signaled
  // state before reaching this function; GetState still reports the pending
  // movement/button data, so polling here keeps that report from being lost to
  // the second blocking wait below. GetState returns EFI_NOT_READY when there
  // is nothing new, in which case we fall through to the normal wait.
  //
  if (Context->Pointer != NULL) {
    Status = Context->Pointer->GetState (Context->Pointer, &PointerState);
    if (!EFI_ERROR (Status)) {
      Event->Type           = ModernUiInputPointer;
      Event->PointerValid   = TRUE;
      Event->PointerX       = (UINTN)PointerState.CurrentX;
      Event->PointerY       = (UINTN)PointerState.CurrentY;
      Event->PointerPressed = (BOOLEAN)((PointerState.ActiveButtons & EFI_ABSP_TouchActive) != 0);
      return EFI_SUCCESS;
    }
  }

  EventCount = 0;
  if (Context->TextInEx != NULL) {
    Events[EventCount++] = Context->TextInEx->WaitForKeyEx;
  } else if (Context->TextIn != NULL) {
    Events[EventCount++] = Context->TextIn->WaitForKey;
  }

  if ((Context->Pointer != NULL) && (Context->Pointer->WaitForInput != NULL) && (EventCount < 2)) {
    Events[EventCount++] = Context->Pointer->WaitForInput;
  }

  if (EventCount == 0) {
    return EFI_NOT_READY;
  }

  Status = gBS->WaitForEvent (EventCount, Events, &Index);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((Context->Pointer != NULL) && (Index == (EventCount - 1)) && (Events[Index] == Context->Pointer->WaitForInput)) {
    Status = Context->Pointer->GetState (Context->Pointer, &PointerState);
    if (!EFI_ERROR (Status)) {
      Event->Type           = ModernUiInputPointer;
      Event->PointerValid   = TRUE;
      Event->PointerX       = (UINTN)PointerState.CurrentX;
      Event->PointerY       = (UINTN)PointerState.CurrentY;
      Event->PointerPressed = (BOOLEAN)((PointerState.ActiveButtons & EFI_ABSP_TouchActive) != 0);
      return EFI_SUCCESS;
    }
  }

  if (Context->TextInEx != NULL) {
    Status = Context->TextInEx->ReadKeyStrokeEx (Context->TextInEx, &KeyData);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Event->UnicodeChar = KeyData.Key.UnicodeChar;
    Event->ScanCode    = KeyData.Key.ScanCode;
    Event->Type        = MapKey (Event->ScanCode, Event->UnicodeChar);
    return EFI_SUCCESS;
  }

  Status = Context->TextIn->ReadKeyStroke (Context->TextIn, &Key);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Event->UnicodeChar = Key.UnicodeChar;
  Event->ScanCode    = Key.ScanCode;
  Event->Type        = MapKey (Event->ScanCode, Event->UnicodeChar);
  return EFI_SUCCESS;
}
