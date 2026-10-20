/** @file
  Start the native owner once, before replacement-app form discovery.
  This library is linked only by replace-UiApp overlays. BDS has connected the
  console before starting Setup; do not move this operation to DXE dispatch.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/HiiDatabase.h>
#include <Protocol/HiiString.h>
#include <Protocol/HiiConfigRouting.h>
#include <Protocol/FormBrowser2.h>
#include <Protocol/FormBrowserEx2.h>
#include <Protocol/DevicePathToText.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>

STATIC EFI_GUID  mOwnerGuid = { 0x842d7f31, 0x7a96, 0x4ec0, { 0x98, 0xa1, 0xd5, 0xe2, 0x38, 0xb4, 0x60, 0x17 } };

STATIC
EFI_STATUS
ModernBootMaintenanceEnsureOwner (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                         Status;
  VOID                               *Interface;
  EFI_LOADED_IMAGE_PROTOCOL          *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL           *DevicePath;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  FileNode;
  EFI_HANDLE                         OwnerImage;
  UINTN                              Index;
  EFI_GUID                           *Required[] = {
    &gEfiHiiDatabaseProtocolGuid,
    &gEfiHiiStringProtocolGuid,
    &gEfiHiiConfigRoutingProtocolGuid,
    &gEfiFormBrowser2ProtocolGuid,
    &gEdkiiFormBrowserEx2ProtocolGuid,
    &gEfiDevicePathToTextProtocolGuid
  };

  // Global protocol state, not an app-image-local flag: nested Setup reuses it.
  Status = gBS->LocateProtocol (&mOwnerGuid, NULL, &Interface);
  if (!EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  // Upstream GetConsoleOutMode dereferences ConOut->Mode in its constructor.
  if ((SystemTable->ConOut == NULL) || (SystemTable->ConOut->Mode == NULL)) {
    return EFI_NOT_READY;
  }

  for (Index = 0; Index < ARRAY_SIZE (Required); Index++) {
    Status = gBS->LocateProtocol (Required[Index], NULL, &Interface);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Overlay places the owner in the same FV as the replacement Setup image.
  DevicePath = DevicePathFromHandle (LoadedImage->DeviceHandle);
  if (DevicePath == NULL) {
    return EFI_NOT_FOUND;
  }

  EfiInitializeFwVolDevicepathNode (&FileNode, &mOwnerGuid);
  DevicePath = AppendDevicePathNode (DevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&FileNode);
  if (DevicePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  OwnerImage = NULL;
  Status = gBS->LoadImage (FALSE, ImageHandle, DevicePath, NULL, 0, &OwnerImage);
  FreePool (DevicePath);
  if (EFI_ERROR (Status)) {
    // Security violations may still return an allocated image handle.
    if (OwnerImage != NULL) {
      gBS->UnloadImage (OwnerImage);
    }
    return Status;
  }

  Status = gBS->StartImage (OwnerImage, NULL, NULL);
  DEBUG ((DEBUG_INFO, "Modern Boot Maintenance owner: %r\n", Status));
  if (EFI_ERROR (Status)) {
    // Pre-entry failures can leave the loaded image alive. If entry ran and
    // failed, DXE Core already freed it (and the entry wrapper ran destructors).
    // Probe the handle before unloading; never invoke native destructors here.
    if (!EFI_ERROR (gBS->HandleProtocol (
                          OwnerImage,
                          &gEfiLoadedImageProtocolGuid,
                          (VOID **)&LoadedImage
                          )))
    {
      gBS->UnloadImage (OwnerImage);
    }

    return Status;
  }

  // Never unload the successful owner when this app exits.
  return gBS->LocateProtocol (&mOwnerGuid, NULL, &Interface);
}

EFI_STATUS
EFIAPI
ModernBootMaintenanceLoaderConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = ModernBootMaintenanceEnsureOwner (ImageHandle, SystemTable);
  DEBUG ((DEBUG_INFO, "Modern Boot Maintenance availability: %r\n", Status));
  // AutoGen asserts library constructor errors; an unavailable optional native
  // owner must not prevent Setup from starting. Discovery exposes availability.
  (VOID)Status;
  return EFI_SUCCESS;
}
