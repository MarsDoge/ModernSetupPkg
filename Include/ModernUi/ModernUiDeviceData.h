/** @file
  Device and HII formset entry provider for ModernSetupApp.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_DEVICE_DATA_H_
#define MODERN_UI_DEVICE_DATA_H_

#include <Uefi.h>
#include <Protocol/HiiDatabase.h>

#define MODERN_UI_DEVICE_TEXT_MAX  160

typedef struct {
  EFI_HII_HANDLE    HiiHandle;
  EFI_HANDLE        DriverHandle;
  EFI_GUID          FormSetGuid;
  BOOLEAN           HasForm;
  CHAR16            Title[MODERN_UI_DEVICE_TEXT_MAX];
  CHAR16            Help[MODERN_UI_DEVICE_TEXT_MAX];
  CHAR16            DevicePath[MODERN_UI_DEVICE_TEXT_MAX];
} MODERN_UI_DEVICE_ENTRY;

/**
  Enumerate HII formset entries and read-only device path entries.

  Entries with HasForm set to TRUE can be opened through FormBrowser2. Entries
  with HasForm set to FALSE are inventory rows and are not opened by this
  library.

  @param[out] Entries     Receives an allocated entry array. Must not be NULL.
                          Free with ModernUiDeviceDataFreeEntries().
  @param[out] EntryCount  Receives the number of entries. Must not be NULL.

  @retval EFI_SUCCESS            Entries were enumerated. Entries may be NULL
                                 when EntryCount is zero.
  @retval EFI_INVALID_PARAMETER  Entries or EntryCount is NULL.
  @retval EFI_OUT_OF_RESOURCES   Allocation failed.
**/
EFI_STATUS
EFIAPI
ModernUiDeviceDataGetEntries (
  OUT MODERN_UI_DEVICE_ENTRY  **Entries,
  OUT UINTN                   *EntryCount
  );

/**
  Free a device entry array returned by ModernUiDeviceDataGetEntries().

  @param[in] Entries     Entry array. May be NULL.
  @param[in] EntryCount  Number of entries in Entries.
**/
VOID
EFIAPI
ModernUiDeviceDataFreeEntries (
  IN MODERN_UI_DEVICE_ENTRY  *Entries OPTIONAL,
  IN UINTN                   EntryCount
  );

/**
  Open one HII formset entry through native FormBrowser2.

  @param[in] Entry  Entry returned by ModernUiDeviceDataGetEntries(). Must not
                    be NULL.

  @retval EFI_SUCCESS            FormBrowser returned successfully.
  @retval EFI_INVALID_PARAMETER  Entry is NULL or has no HII handle.
  @retval EFI_UNSUPPORTED        Entry is a read-only device inventory row.
  @retval EFI_NOT_FOUND          FormBrowser2 is unavailable.
  @retval others                 Status returned by SendForm().
**/
EFI_STATUS
EFIAPI
ModernUiDeviceDataOpenEntry (
  IN CONST MODERN_UI_DEVICE_ENTRY  *Entry
  );

#endif
