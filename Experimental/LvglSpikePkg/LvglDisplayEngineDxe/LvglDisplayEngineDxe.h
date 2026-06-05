/** @file
  LvglDisplayEngineDxe -- experimental EDKII_FORM_DISPLAY_ENGINE_PROTOCOL backend
  that renders native SetupBrowser forms with LVGL instead of edk2's text-grid
  DisplayEngineDxe.

  BOUNDARY: like every DisplayEngine, this only RENDERS and reports user input.
  SetupBrowser keeps ownership of IFR parsing, ConfigAccess, callbacks, and
  varstore writes. This driver never writes HII variables.

  experimental/lvgl-spike only; never a default overlay.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef LVGL_DISPLAY_ENGINE_DXE_H_
#define LVGL_DISPLAY_ENGINE_DXE_H_

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Protocol/DisplayProtocol.h>

#include <Library/LvglCoreLib.h>

/**
  Render one form with LVGL and block until the user asks to leave it, then
  report the result to SetupBrowser.

  @param[in]  FormData       The form (statements, title, HII handle) to show.
                             Must be non-NULL.
  @param[out] UserInputData  Filled with the user's action/selection. Must be
                             non-NULL. For this skeleton: ESC -> Action =
                             BROWSER_ACTION_FORM_EXIT, SelectedStatement = NULL.

  @retval EFI_SUCCESS        The form was shown and user input was collected.
**/
EFI_STATUS
EFIAPI
LvglFormDisplay (
  IN  FORM_DISPLAY_ENGINE_FORM  *FormData,
  OUT USER_INPUT                *UserInputData
  );

/**
  Tear the LVGL display back down and restore the text console after a form
  session ends.
**/
VOID
EFIAPI
LvglExitDisplay (
  VOID
  );

#endif // LVGL_DISPLAY_ENGINE_DXE_H_
