/** @file
  Input adapter for ModernSetupPkg.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_INPUT_H_
#define MODERN_UI_INPUT_H_

#include <Uefi.h>
#include <Protocol/AbsolutePointer.h>
#include <Protocol/SimpleTextInEx.h>

typedef enum {
  ModernUiInputNone = 0,
  ModernUiInputUp,
  ModernUiInputDown,
  ModernUiInputLeft,
  ModernUiInputRight,
  ModernUiInputEnter,
  ModernUiInputEscape,
  ModernUiInputTab,
  ModernUiInputPointer,
  ModernUiInputOther
} MODERN_UI_INPUT_TYPE;

typedef struct {
  MODERN_UI_INPUT_TYPE    Type;
  CHAR16                  UnicodeChar;
  UINT16                  ScanCode;
  BOOLEAN                 PointerValid;
  UINTN                   PointerX;
  UINTN                   PointerY;
  BOOLEAN                 PointerPressed;
} MODERN_UI_INPUT_EVENT;

typedef struct {
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL    *TextInEx;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL       *TextIn;
  EFI_ABSOLUTE_POINTER_PROTOCOL        *Pointer;
} MODERN_UI_INPUT_CONTEXT;

/**
  Initialize the input context from firmware console and optional pointer services.

  @param[out] Context  Input context to initialize. Must not be NULL. Missing
                       optional protocols are recorded as NULL members.

  @retval EFI_SUCCESS            Context was initialized. This can succeed even
                                 when optional pointer input is unavailable.
  @retval EFI_INVALID_PARAMETER  Context is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiInputInit (
  OUT MODERN_UI_INPUT_CONTEXT  *Context
  );

/**
  Wait for and return the next normalized UI input event.

  @param[in,out] Context  Initialized input context. Must not be NULL.
  @param[out]    Event    Receives a normalized input event. Must not be NULL.
                          The structure is cleared before data is written.

  @retval EFI_SUCCESS            Event contains input data.
  @retval EFI_INVALID_PARAMETER  Context or Event is NULL.
  @retval EFI_NOT_READY          No usable input source is available.
**/
EFI_STATUS
EFIAPI
ModernUiReadInput (
  IN OUT MODERN_UI_INPUT_CONTEXT  *Context,
  OUT MODERN_UI_INPUT_EVENT       *Event
  );

#endif
