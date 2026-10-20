/** @file
  Persistent owner of the unmodified upstream BootMaintenanceManagerUiLib.
  The constructor has registered native HII before this entry point executes.
  No unload handler: successful StartImage leaves the driver and HII resident.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>

STATIC EFI_GUID  mOwnerGuid = { 0x842d7f31, 0x7a96, 0x4ec0, { 0x98, 0xa1, 0xd5, 0xe2, 0x38, 0xb4, 0x60, 0x17 } };
STATIC UINT8     mOwnerReady;

EFI_STATUS
EFIAPI
ModernBootMaintenanceEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  (VOID)SystemTable;
  // UefiDriverEntryPoint runs the native library constructor first. If marker
  // installation fails, propagate the error: its entry wrapper runs the native
  // destructor before DXE Core frees the failed image. Do not clean up twice.
  // On success there is no unload handler, so app exit cannot destroy this HII.
  return gBS->InstallProtocolInterface (
                &ImageHandle,
                &mOwnerGuid,
                EFI_NATIVE_INTERFACE,
                &mOwnerReady
                );
}
