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

EFI_STATUS
EFIAPI
ModernUiInputInit (
  OUT MODERN_UI_INPUT_CONTEXT  *Context
  );

EFI_STATUS
EFIAPI
ModernUiReadInput (
  IN OUT MODERN_UI_INPUT_CONTEXT  *Context,
  OUT MODERN_UI_INPUT_EVENT       *Event
  );

#endif
