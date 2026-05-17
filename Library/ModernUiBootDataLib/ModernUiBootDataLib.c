/** @file
  Boot option data provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>
#include <ModernUi/ModernUiBootData.h>

/**
  Return whether a boot option points at the current setup application.

  @param[in] CurrentImageHandle  Current app image handle. May be NULL.
  @param[in] FilePath            Boot option file path. May be NULL.

  @retval TRUE   FilePath matches CurrentImageHandle.
  @retval FALSE  FilePath is different or cannot be compared.
**/
STATIC
BOOLEAN
IsCurrentApplicationBootOption (
  IN EFI_HANDLE                CurrentImageHandle OPTIONAL,
  IN EFI_DEVICE_PATH_PROTOCOL  *FilePath OPTIONAL
  )
{
  EFI_STATUS                Status;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *ApplicationPath;
  UINTN                     FilePathSize;
  UINTN                     ApplicationPathSize;
  BOOLEAN                   Match;

  if ((CurrentImageHandle == NULL) || (FilePath == NULL)) {
    return FALSE;
  }

  Status = gBS->HandleProtocol (
                  CurrentImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (EFI_ERROR (Status) || (LoadedImage == NULL)) {
    return FALSE;
  }

  DevicePath = DevicePathFromHandle (LoadedImage->DeviceHandle);
  if ((DevicePath == NULL) || (LoadedImage->FilePath == NULL)) {
    return FALSE;
  }

  ApplicationPath = AppendDevicePathNode (DevicePath, LoadedImage->FilePath);
  if (ApplicationPath == NULL) {
    return FALSE;
  }

  FilePathSize        = GetDevicePathSize (FilePath);
  ApplicationPathSize = GetDevicePathSize (ApplicationPath);
  Match               = (BOOLEAN)(
                                  (FilePathSize == ApplicationPathSize) &&
                                  (CompareMem (FilePath, ApplicationPath, FilePathSize) == 0)
                                  );
  FreePool (ApplicationPath);
  return Match;
}

/**
  Return whether a Boot Manager load option should be shown.

  @param[in] CurrentImageHandle  Current app image handle. May be NULL.
  @param[in] BootOption          Boot option to inspect. Must not be NULL.

  @retval TRUE   Option is visible.
  @retval FALSE  Option is hidden, NULL, or points at the current app.
**/
STATIC
BOOLEAN
IsVisibleBootOption (
  IN EFI_HANDLE                         CurrentImageHandle OPTIONAL,
  IN CONST EFI_BOOT_MANAGER_LOAD_OPTION *BootOption
  )
{
  if ((BootOption == NULL) || ((BootOption->Attributes & LOAD_OPTION_HIDDEN) != 0)) {
    return FALSE;
  }

  return (BOOLEAN)!IsCurrentApplicationBootOption (CurrentImageHandle, BootOption->FilePath);
}

EFI_STATUS
EFIAPI
ModernUiBootDataGetOptions (
  IN  EFI_HANDLE             CurrentImageHandle OPTIONAL,
  OUT MODERN_UI_BOOT_OPTION  **Options,
  OUT UINTN                  *OptionCount
  )
{
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions;
  MODERN_UI_BOOT_OPTION         *Result;
  UINTN                         BootOptionCount;
  UINTN                         Index;
  UINTN                         VisibleCount;
  UINTN                         ResultIndex;

  if ((Options == NULL) || (OptionCount == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Options     = NULL;
  *OptionCount = 0;

  BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);
  if (BootOptions == NULL) {
    return EFI_SUCCESS;
  }

  VisibleCount = 0;
  for (Index = 0; Index < BootOptionCount; Index++) {
    if (IsVisibleBootOption (CurrentImageHandle, &BootOptions[Index])) {
      VisibleCount++;
    }
  }

  if (VisibleCount == 0) {
    EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
    return EFI_SUCCESS;
  }

  Result = AllocateZeroPool (sizeof (*Result) * VisibleCount);
  if (Result == NULL) {
    EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
    return EFI_OUT_OF_RESOURCES;
  }

  ResultIndex = 0;
  for (Index = 0; Index < BootOptionCount; Index++) {
    if (!IsVisibleBootOption (CurrentImageHandle, &BootOptions[Index])) {
      continue;
    }

    Result[ResultIndex].OptionNumber = BootOptions[Index].OptionNumber;
    Result[ResultIndex].Attributes   = BootOptions[Index].Attributes;
    Result[ResultIndex].Active       = (BOOLEAN)((BootOptions[Index].Attributes & LOAD_OPTION_ACTIVE) != 0);
    if (BootOptions[Index].Description != NULL) {
      StrnCpyS (
        Result[ResultIndex].Description,
        ARRAY_SIZE (Result[ResultIndex].Description),
        BootOptions[Index].Description,
        ARRAY_SIZE (Result[ResultIndex].Description) - 1
        );
    } else {
      StrCpyS (Result[ResultIndex].Description, ARRAY_SIZE (Result[ResultIndex].Description), L"(no description)");
    }

    ResultIndex++;
  }

  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
  *Options     = Result;
  *OptionCount = VisibleCount;
  return EFI_SUCCESS;
}

VOID
EFIAPI
ModernUiBootDataFreeOptions (
  IN MODERN_UI_BOOT_OPTION  *Options OPTIONAL,
  IN UINTN                  OptionCount
  )
{
  (VOID)OptionCount;
  if (Options != NULL) {
    FreePool (Options);
  }
}

EFI_STATUS
EFIAPI
ModernUiBootDataBootOption (
  IN UINT16  OptionNumber
  )
{
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions;
  UINTN                         BootOptionCount;
  UINTN                         Index;
  EFI_STATUS                    Status;

  BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);
  if (BootOptions == NULL) {
    return EFI_NOT_FOUND;
  }

  Status = EFI_NOT_FOUND;
  for (Index = 0; Index < BootOptionCount; Index++) {
    if (BootOptions[Index].OptionNumber == OptionNumber) {
      EfiBootManagerBoot (&BootOptions[Index]);
      Status = BootOptions[Index].Status;
      break;
    }
  }

  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
  return Status;
}
