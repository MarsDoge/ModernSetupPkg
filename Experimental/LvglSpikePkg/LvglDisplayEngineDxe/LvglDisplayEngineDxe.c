/** @file
  LvglDisplayEngineDxe -- driver glue: initializes the LVGL UEFI backend and
  installs EDKII_FORM_DISPLAY_ENGINE_PROTOCOL so SetupBrowser drives LVGL instead
  of the native text-grid DisplayEngineDxe.

  SetupBrowser keeps all IFR/config/callback ownership; this driver only renders.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "LvglDisplayEngineDxe.h"
#include <Protocol/FormBrowserEx.h>

typedef struct {
  EFI_HANDLE                            Handle;
  EDKII_FORM_DISPLAY_ENGINE_PROTOCOL    FormDisplayProt;
} LVGL_DISPLAY_ENGINE_PRIVATE;

/**
  Confirm how to handle changed data when a form session ends. This skeleton
  never edits varstore data, so it asks SetupBrowser to discard. SetupBrowser
  still owns the actual decision and any writes.

  @retval BROWSER_ACTION_DISCARD  Discard pending changes.
**/
STATIC
UINTN
EFIAPI
LvglConfirmDataChange (
  VOID
  )
{
  return BROWSER_ACTION_DISCARD;
}

STATIC LVGL_DISPLAY_ENGINE_PRIVATE  mPrivate = {
  NULL,
  {
    LvglFormDisplay,
    LvglExitDisplay,
    LvglConfirmDataChange
  }
};

/**
  Driver entry point. Caches the image handle / system table for the LVGL UEFI
  backend, initializes LVGL, and installs the form-display protocol. The LVGL
  display itself is created lazily on the first form (GOP may not be up yet).

  @param[in] ImageHandle  This driver's image handle.
  @param[in] SystemTable  The EFI System Table.

  @retval EFI_SUCCESS  The protocol was installed.
  @retval Others       Protocol installation failed.
**/
EFI_STATUS
EFIAPI
InitializeLvglDisplayEngine (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  //
  // Must precede lv_init() per the upstream LVGL UEFI backend contract.
  //
  lv_uefi_init (ImageHandle, SystemTable);
  lv_init ();

  return gBS->InstallProtocolInterface (
                &mPrivate.Handle,
                &gEdkiiFormDisplayEngineProtocolGuid,
                EFI_NATIVE_INTERFACE,
                &mPrivate.FormDisplayProt
                );
}
