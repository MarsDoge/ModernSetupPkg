/** @file
  EDK2-side shim that satisfies LVGL's unconditional `#include <efi.h>`.

  TEMPORARY WORKAROUND. The pinned upstream `External/lvgl`
  (commit 0b1ea312d, v9.5.0-273) inherits PR #10041 ("arch: move public headers
  to include folder"), which makes the umbrella header `<lvgl/lvgl.h>`
  unconditionally `#include "drivers/uefi/lv_uefi_gnu_efi.h"`. That header in
  turn does `#include <efi.h>` (the gnu-efi framework header) whenever
  `LV_USE_UEFI` is set -- even though this build selects the EDK2 framework via
  `LV_USE_UEFI_INCLUDE = "lv_uefi_edk2.h"` and obtains every EFI type from
  `<Uefi.h>`. EDK2 ships no `<efi.h>`, so the gnu-efi include is a hard error.

  This empty header satisfies that include harmlessly: `lv_uefi_gnu_efi.h` only
  `#define`s `LV_UEFI_GNU_EFI_HEADERS` (no code branches on it to switch types)
  and includes us; it pulls in zero gnu-efi types, so the EDK2 header path that
  `lv_uefi_edk2.h` already established is left untouched. There is no EDK2/gnu-efi
  mutual-exclusion check upstream, so both `*_HEADERS` macros being defined is
  benign.

  Remove this file once the upstream guard fix lands -- gate the gnu-efi
  `#include <efi.h>` behind the framework selection so EDK2 builds skip it -- and
  `External/lvgl` is re-pinned to an upstream commit that carries that fix.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERNUI_LVGL_EDK2_EFI_H_SHIM_
#define MODERNUI_LVGL_EDK2_EFI_H_SHIM_

//
// Intentionally empty. EFI types come from EDK2 <Uefi.h> via lv_uefi_edk2.h.
//

#endif // MODERNUI_LVGL_EDK2_EFI_H_SHIM_
