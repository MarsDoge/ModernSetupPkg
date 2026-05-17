/** @file
  Boot option data provider for ModernSetupApp.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_BOOT_DATA_H_
#define MODERN_UI_BOOT_DATA_H_

#include <Uefi.h>

#define MODERN_UI_BOOT_DESCRIPTION_MAX  160

typedef struct {
  UINT16     OptionNumber;
  UINT32     Attributes;
  BOOLEAN    Active;
  CHAR16     Description[MODERN_UI_BOOT_DESCRIPTION_MAX];
} MODERN_UI_BOOT_OPTION;

/**
  Enumerate visible Boot#### options.

  @param[in]  CurrentImageHandle  Optional current app image handle used to
                                  hide the app's own boot option. May be NULL.
  @param[out] Options             Receives an allocated option array. Must not
                                  be NULL. Free with ModernUiBootDataFreeOptions().
  @param[out] OptionCount         Receives the number of entries. Must not be
                                  NULL.

  @retval EFI_SUCCESS            Options was returned. It may be NULL when
                                 OptionCount is zero.
  @retval EFI_INVALID_PARAMETER  Options or OptionCount is NULL.
  @retval EFI_OUT_OF_RESOURCES   Allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiBootDataGetOptions (
  IN  EFI_HANDLE             CurrentImageHandle OPTIONAL,
  OUT MODERN_UI_BOOT_OPTION  **Options,
  OUT UINTN                  *OptionCount
  );

/**
  Free a boot option array returned by ModernUiBootDataGetOptions().

  @param[in] Options      Option array. May be NULL.
  @param[in] OptionCount  Number of entries in Options.
**/
VOID
EFIAPI
ModernUiBootDataFreeOptions (
  IN MODERN_UI_BOOT_OPTION  *Options OPTIONAL,
  IN UINTN                  OptionCount
  );

/**
  Launch one Boot#### option by option number.

  @param[in] OptionNumber  Boot#### number to launch.

  @retval EFI_SUCCESS    Boot option launched and returned successfully.
  @retval EFI_NOT_FOUND  OptionNumber was not found.
  @retval others         Status recorded by UefiBootManagerLib.
**/
EFI_STATUS
EFIAPI
ModernUiBootDataBootOption (
  IN UINT16  OptionNumber
  );

#endif
