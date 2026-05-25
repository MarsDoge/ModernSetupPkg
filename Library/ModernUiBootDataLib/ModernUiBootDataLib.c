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
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/GlobalVariable.h>
#include <Protocol/LoadedImage.h>
#include <ModernUi/ModernUiBootData.h>

#define MODERN_UI_BOOT_POLICY_VARIABLE_ATTRS  (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS)

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
  Return whether a Boot Manager load option should be exposed by the front page.

  @param[in] CurrentImageHandle  Current app image handle. May be NULL.
  @param[in] BootOption          Boot option to inspect. Must not be NULL.

  @retval TRUE   Option is a platform boot entry.
  @retval FALSE  Option is NULL or points at the current app.
**/
STATIC
BOOLEAN
ShouldExposeBootOption (
  IN EFI_HANDLE                         CurrentImageHandle OPTIONAL,
  IN CONST EFI_BOOT_MANAGER_LOAD_OPTION *BootOption
  )
{
  if (BootOption == NULL) {
    return FALSE;
  }

  return (BOOLEAN)!IsCurrentApplicationBootOption (CurrentImageHandle, BootOption->FilePath);
}

/**
  Format the UEFI load option category into a short user-visible label.

  @param[in]  Attributes  Boot option attributes.
  @param[out] Buffer      Destination text buffer. Must not be NULL.
  @param[in]  BufferCount Number of CHAR16 elements in Buffer.
**/
STATIC
VOID
FormatBootCategory (
  IN  UINT32  Attributes,
  OUT CHAR16  *Buffer,
  IN  UINTN   BufferCount
  )
{
  UINT32  Category;

  if ((Buffer == NULL) || (BufferCount == 0)) {
    return;
  }

  Category = Attributes & LOAD_OPTION_CATEGORY;
  switch (Category) {
    case LOAD_OPTION_CATEGORY_BOOT:
      StrCpyS (Buffer, BufferCount, L"Boot");
      break;
    case LOAD_OPTION_CATEGORY_APP:
      StrCpyS (Buffer, BufferCount, L"App");
      break;
    default:
      UnicodeSPrint (Buffer, BufferCount * sizeof (CHAR16), L"Category 0x%04x", Category);
      break;
  }
}

/**
  Format one boot option device path into a bounded single-line summary.

  @param[in]  FilePath    Device path to format. May be NULL.
  @param[out] Buffer      Destination text buffer. Must not be NULL.
  @param[in]  BufferCount Number of CHAR16 elements in Buffer.
**/
STATIC
VOID
FormatBootDevicePathSummary (
  IN  EFI_DEVICE_PATH_PROTOCOL  *FilePath OPTIONAL,
  OUT CHAR16                    *Buffer,
  IN  UINTN                     BufferCount
  )
{
  CHAR16  *Text;

  if ((Buffer == NULL) || (BufferCount == 0)) {
    return;
  }

  if (FilePath == NULL) {
    StrCpyS (Buffer, BufferCount, L"(no device path)");
    return;
  }

  Text = ConvertDevicePathToText (FilePath, TRUE, TRUE);
  if (Text == NULL) {
    StrCpyS (Buffer, BufferCount, L"(device path unavailable)");
    return;
  }

  StrnCpyS (Buffer, BufferCount, Text, BufferCount - 1);
  FreePool (Text);
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
    if (ShouldExposeBootOption (CurrentImageHandle, &BootOptions[Index])) {
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
    if (!ShouldExposeBootOption (CurrentImageHandle, &BootOptions[Index])) {
      continue;
    }

    Result[ResultIndex].OptionNumber = BootOptions[Index].OptionNumber;
    Result[ResultIndex].Attributes   = BootOptions[Index].Attributes;
    Result[ResultIndex].Active       = (BOOLEAN)((BootOptions[Index].Attributes & LOAD_OPTION_ACTIVE) != 0);
    Result[ResultIndex].Hidden       = (BOOLEAN)((BootOptions[Index].Attributes & LOAD_OPTION_HIDDEN) != 0);
    FormatBootCategory (
      BootOptions[Index].Attributes,
      Result[ResultIndex].Category,
      ARRAY_SIZE (Result[ResultIndex].Category)
      );
    FormatBootDevicePathSummary (
      BootOptions[Index].FilePath,
      Result[ResultIndex].FilePathSummary,
      ARRAY_SIZE (Result[ResultIndex].FilePathSummary)
      );
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

EFI_STATUS
EFIAPI
ModernUiBootDataGetBootNext (
  OUT UINT16   *OptionNumber,
  OUT BOOLEAN  *Present
  )
{
  EFI_STATUS  Status;
  UINTN       DataSize;
  UINT32      Attributes;
  UINT16      Value;

  if ((OptionNumber == NULL) || (Present == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *OptionNumber = 0;
  *Present      = FALSE;
  DataSize      = sizeof (Value);
  Attributes    = 0;
  Status = gRT->GetVariable (
                  L"BootNext",
                  &gEfiGlobalVariableGuid,
                  &Attributes,
                  &DataSize,
                  &Value
                  );
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (DataSize != sizeof (Value)) {
    return EFI_COMPROMISED_DATA;
  }

  *OptionNumber = Value;
  *Present      = TRUE;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ModernUiBootDataSetBootNext (
  IN UINT16  OptionNumber
  )
{
  return gRT->SetVariable (
                L"BootNext",
                &gEfiGlobalVariableGuid,
                MODERN_UI_BOOT_POLICY_VARIABLE_ATTRS,
                sizeof (OptionNumber),
                &OptionNumber
                );
}

EFI_STATUS
EFIAPI
ModernUiBootDataClearBootNext (
  VOID
  )
{
  return gRT->SetVariable (
                L"BootNext",
                &gEfiGlobalVariableGuid,
                MODERN_UI_BOOT_POLICY_VARIABLE_ATTRS,
                0,
                NULL
                );
}

EFI_STATUS
EFIAPI
ModernUiBootDataSwapBootOrderOptions (
  IN UINT16  FirstOptionNumber,
  IN UINT16  SecondOptionNumber
  )
{
  EFI_STATUS  Status;
  UINTN       DataSize;
  UINT32      Attributes;
  UINT16      *BootOrder;
  UINTN       EntryCount;
  UINTN       Index;
  UINTN       FirstIndex;
  UINTN       SecondIndex;
  UINT16      Temp;

  if (FirstOptionNumber == SecondOptionNumber) {
    return EFI_SUCCESS;
  }

  DataSize = 0;
  Status = gRT->GetVariable (
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  NULL,
                  &DataSize,
                  NULL
                  );
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  if ((DataSize == 0) || ((DataSize % sizeof (UINT16)) != 0)) {
    return EFI_COMPROMISED_DATA;
  }

  BootOrder = AllocateZeroPool (DataSize);
  if (BootOrder == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Attributes = 0;
  Status = gRT->GetVariable (
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  &Attributes,
                  &DataSize,
                  BootOrder
                  );
  if (EFI_ERROR (Status)) {
    FreePool (BootOrder);
    return Status;
  }

  EntryCount  = DataSize / sizeof (UINT16);
  FirstIndex  = EntryCount;
  SecondIndex = EntryCount;
  for (Index = 0; Index < EntryCount; Index++) {
    if (BootOrder[Index] == FirstOptionNumber) {
      FirstIndex = Index;
    }

    if (BootOrder[Index] == SecondOptionNumber) {
      SecondIndex = Index;
    }
  }

  if ((FirstIndex >= EntryCount) || (SecondIndex >= EntryCount)) {
    FreePool (BootOrder);
    return EFI_NOT_FOUND;
  }

  Temp                   = BootOrder[FirstIndex];
  BootOrder[FirstIndex]  = BootOrder[SecondIndex];
  BootOrder[SecondIndex] = Temp;

  Status = gRT->SetVariable (
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  MODERN_UI_BOOT_POLICY_VARIABLE_ATTRS,
                  DataSize,
                  BootOrder
                  );
  FreePool (BootOrder);
  return Status;
}
