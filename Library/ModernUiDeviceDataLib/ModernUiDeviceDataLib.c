/** @file
  Device and HII formset entry provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Protocol/FormBrowser2.h>
#include <Uefi/UefiInternalFormRepresentation.h>
#include <ModernUi/ModernUiDeviceData.h>

/**
  Copy an allocated HII string into a fixed entry field.

  @param[in] HiiHandle  HII package handle. Must not be NULL.
  @param[in] StringId   HII string token.
  @param[in] Fallback   Fallback string. Must not be NULL.
  @param[out] Buffer    Destination buffer. Must not be NULL.
  @param[in] Count      Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
CopyHiiString (
  IN  EFI_HII_HANDLE  HiiHandle,
  IN  EFI_STRING_ID   StringId,
  IN  CONST CHAR16    *Fallback,
  OUT CHAR16          *Buffer,
  IN  UINTN           Count
  )
{
  CHAR16  *Text;

  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  Text = NULL;
  if ((HiiHandle != NULL) && (StringId != 0)) {
    Text = HiiGetString (HiiHandle, StringId, NULL);
  }

  StrnCpyS (Buffer, Count, (Text == NULL) ? Fallback : Text, Count - 1);
  if (Text != NULL) {
    FreePool (Text);
  }
}

/**
  Copy a driver handle device path into a fixed entry field.

  @param[in] DriverHandle  Driver handle associated with the HII package. May be
                           NULL.
  @param[out] Buffer       Destination buffer. Must not be NULL.
  @param[in] Count         Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
CopyDriverDevicePath (
  IN  EFI_HANDLE  DriverHandle OPTIONAL,
  OUT CHAR16      *Buffer,
  IN  UINTN       Count
  )
{
  CHAR16  *Text;

  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  Buffer[0] = CHAR_NULL;
  if (DriverHandle == NULL) {
    StrCpyS (Buffer, Count, L"(no device path)");
    return;
  }

  Text = ConvertDevicePathToText (DevicePathFromHandle (DriverHandle), TRUE, TRUE);
  if (Text == NULL) {
    StrCpyS (Buffer, Count, L"(no device path)");
    return;
  }

  StrnCpyS (Buffer, Count, Text, Count - 1);
  FreePool (Text);
}

/**
  Append one parsed formset entry.

  @param[in]     HiiHandle   HII package handle. Must not be NULL.
  @param[in]     FormSet     Formset opcode to append. Must not be NULL.
  @param[in,out] Entries     Entry array storage. Must not be NULL.
  @param[in,out] EntryCount  Current output count. Must not be NULL.
  @param[in,out] Capacity    Current allocated capacity. Must not be NULL.

  @retval EFI_SUCCESS            Entry was appended.
  @retval EFI_INVALID_PARAMETER  Required pointer is NULL.
  @retval EFI_OUT_OF_RESOURCES   Entry array growth failed.
**/
STATIC
EFI_STATUS
AppendOneFormsetEntry (
  IN     EFI_HII_HANDLE           HiiHandle,
  IN     CONST EFI_IFR_FORM_SET   *FormSet,
  IN OUT MODERN_UI_DEVICE_ENTRY   **Entries,
  IN OUT UINTN                    *EntryCount,
  IN OUT UINTN                    *Capacity
  )
{
  MODERN_UI_DEVICE_ENTRY  *NewEntries;
  UINTN                   NewCapacity;
  EFI_HANDLE              DriverHandle;
  MODERN_UI_DEVICE_ENTRY  *Entry;

  if ((HiiHandle == NULL) || (FormSet == NULL) || (Entries == NULL) ||
      (EntryCount == NULL) || (Capacity == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (*EntryCount >= *Capacity) {
    NewCapacity = (*Capacity == 0) ? 8 : (*Capacity * 2);
    NewEntries = ReallocatePool (
                   sizeof (**Entries) * (*Capacity),
                   sizeof (**Entries) * NewCapacity,
                   *Entries
                   );
    if (NewEntries == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    ZeroMem (&NewEntries[*Capacity], sizeof (*NewEntries) * (NewCapacity - *Capacity));
    *Entries  = NewEntries;
    *Capacity = NewCapacity;
  }

  Entry = &(*Entries)[*EntryCount];
  Entry->HiiHandle = HiiHandle;
  CopyMem (&Entry->FormSetGuid, &FormSet->Guid, sizeof (EFI_GUID));
  Entry->HasForm = TRUE;
  CopyHiiString (
    HiiHandle,
    FormSet->FormSetTitle,
    L"UEFI setup formset",
    Entry->Title,
    ARRAY_SIZE (Entry->Title)
    );
  CopyHiiString (
    HiiHandle,
    FormSet->Help,
    L"Open this HII formset with native FormBrowser.",
    Entry->Help,
    ARRAY_SIZE (Entry->Help)
    );

  DriverHandle = NULL;
  if ((gHiiDatabase != NULL) &&
      !EFI_ERROR (gHiiDatabase->GetPackageListHandle (gHiiDatabase, HiiHandle, &DriverHandle)))
  {
    Entry->DriverHandle = DriverHandle;
  }

  CopyDriverDevicePath (
    Entry->DriverHandle,
    Entry->DevicePath,
    ARRAY_SIZE (Entry->DevicePath)
    );

  (*EntryCount)++;
  return EFI_SUCCESS;
}

/**
  Append all formset opcodes exposed by one HII handle.

  @param[in]     HiiHandle   HII package handle. Must not be NULL.
  @param[in,out] Entries     Entry array storage. Must not be NULL.
  @param[in,out] EntryCount  Current output count. Must not be NULL.
  @param[in,out] Capacity    Current allocated capacity. Must not be NULL.

  @retval EFI_SUCCESS            Formsets were appended or the HII handle had no forms.
  @retval EFI_INVALID_PARAMETER  Required pointer is NULL.
  @retval EFI_OUT_OF_RESOURCES   Allocation failed.
**/
STATIC
EFI_STATUS
AppendFormsetEntries (
  IN     EFI_HII_HANDLE          HiiHandle,
  IN OUT MODERN_UI_DEVICE_ENTRY  **Entries,
  IN OUT UINTN                   *EntryCount,
  IN OUT UINTN                   *Capacity
  )
{
  EFI_STATUS        Status;
  EFI_IFR_FORM_SET  *FormSets;
  EFI_IFR_OP_HEADER *OpHeader;
  UINTN             BufferSize;
  UINTN             Offset;

  if ((HiiHandle == NULL) || (Entries == NULL) || (EntryCount == NULL) || (Capacity == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  FormSets   = NULL;
  BufferSize = 0;
  Status = HiiGetFormSetFromHiiHandle (HiiHandle, &FormSets, &BufferSize);
  if (EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  Offset = 0;
  while (Offset < BufferSize) {
    OpHeader = (EFI_IFR_OP_HEADER *)((UINT8 *)FormSets + Offset);
    if ((OpHeader->Length == 0) || (OpHeader->Length > (BufferSize - Offset))) {
      break;
    }

    if ((OpHeader->OpCode == EFI_IFR_FORM_SET_OP) &&
        (OpHeader->Length > OFFSET_OF (EFI_IFR_FORM_SET, Flags)))
    {
      Status = AppendOneFormsetEntry (
                 HiiHandle,
                 (EFI_IFR_FORM_SET *)OpHeader,
                 Entries,
                 EntryCount,
                 Capacity
                 );
      if (EFI_ERROR (Status)) {
        break;
      }
    }

    Offset += OpHeader->Length;
  }

  FreePool (FormSets);
  return Status;
}

/**
  Append one read-only device path inventory entry.

  @param[in]     DeviceHandle  Device handle to describe. Must not be NULL.
  @param[in,out] Entries       Entry array storage. Must not be NULL.
  @param[in,out] EntryCount    Current output count. Must not be NULL.
  @param[in,out] Capacity      Current allocated capacity. Must not be NULL.

  @retval EFI_SUCCESS            Entry was appended or the handle has no device path.
  @retval EFI_INVALID_PARAMETER  Required pointer is NULL.
  @retval EFI_OUT_OF_RESOURCES   Entry array growth failed.
**/
STATIC
EFI_STATUS
AppendDevicePathEntry (
  IN     EFI_HANDLE              DeviceHandle,
  IN OUT MODERN_UI_DEVICE_ENTRY  **Entries,
  IN OUT UINTN                   *EntryCount,
  IN OUT UINTN                   *Capacity
  )
{
  MODERN_UI_DEVICE_ENTRY    *NewEntries;
  MODERN_UI_DEVICE_ENTRY    *Entry;
  UINTN                     NewCapacity;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

  if ((DeviceHandle == NULL) || (Entries == NULL) || (EntryCount == NULL) || (Capacity == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DevicePath = DevicePathFromHandle (DeviceHandle);
  if (DevicePath == NULL) {
    return EFI_SUCCESS;
  }

  if (*EntryCount >= *Capacity) {
    NewCapacity = (*Capacity == 0) ? 8 : (*Capacity * 2);
    NewEntries = ReallocatePool (
                   sizeof (**Entries) * (*Capacity),
                   sizeof (**Entries) * NewCapacity,
                   *Entries
                   );
    if (NewEntries == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    ZeroMem (&NewEntries[*Capacity], sizeof (*NewEntries) * (NewCapacity - *Capacity));
    *Entries  = NewEntries;
    *Capacity = NewCapacity;
  }

  Entry              = &(*Entries)[*EntryCount];
  Entry->DriverHandle = DeviceHandle;
  Entry->HasForm      = FALSE;
  UnicodeSPrint (Entry->Title, sizeof (Entry->Title), L"Device handle %u", *EntryCount + 1);
  StrCpyS (Entry->Help, ARRAY_SIZE (Entry->Help), L"Device path inventory entry.");
  CopyDriverDevicePath (DeviceHandle, Entry->DevicePath, ARRAY_SIZE (Entry->DevicePath));

  (*EntryCount)++;
  return EFI_SUCCESS;
}

/**
  Return TRUE when DeviceHandle is already represented by a HII formset row.

  @param[in] Entries       Entry array to inspect. May be NULL when EntryCount is zero.
  @param[in] EntryCount    Number of entries in Entries.
  @param[in] DeviceHandle  Handle to search for. May be NULL.

  @retval TRUE   DeviceHandle already appears as an entry driver handle.
  @retval FALSE  DeviceHandle is not represented yet.
**/
STATIC
BOOLEAN
IsExistingDriverHandle (
  IN CONST MODERN_UI_DEVICE_ENTRY  *Entries OPTIONAL,
  IN UINTN                         EntryCount,
  IN EFI_HANDLE                    DeviceHandle OPTIONAL
  )
{
  UINTN  Index;

  if ((Entries == NULL) || (DeviceHandle == NULL)) {
    return FALSE;
  }

  for (Index = 0; Index < EntryCount; Index++) {
    if (Entries[Index].DriverHandle == DeviceHandle) {
      return TRUE;
    }
  }

  return FALSE;
}

EFI_STATUS
EFIAPI
ModernUiDeviceDataGetEntries (
  OUT MODERN_UI_DEVICE_ENTRY  **Entries,
  OUT UINTN                   *EntryCount
  )
{
  EFI_HII_HANDLE          *HiiHandles;
  EFI_HANDLE              *DeviceHandles;
  UINTN                   DeviceHandleCount;
  UINTN                   Index;
  UINTN                   EntryCapacity;
  EFI_STATUS              Status;

  if ((Entries == NULL) || (EntryCount == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Entries    = NULL;
  *EntryCount = 0;

  EntryCapacity = 0;
  HiiHandles = HiiGetHiiHandles (NULL);
  if (HiiHandles != NULL) {
    for (Index = 0; HiiHandles[Index] != NULL; Index++) {
      Status = AppendFormsetEntries (HiiHandles[Index], Entries, EntryCount, &EntryCapacity);
      if (EFI_ERROR (Status)) {
        FreePool (HiiHandles);
        ModernUiDeviceDataFreeEntries (*Entries, *EntryCount);
        *Entries    = NULL;
        *EntryCount = 0;
        return Status;
      }
    }

    FreePool (HiiHandles);
  }
  Status = gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &DeviceHandleCount, &DeviceHandles);
  if (EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < DeviceHandleCount; Index++) {
    if (IsExistingDriverHandle (*Entries, *EntryCount, DeviceHandles[Index])) {
      continue;
    }

    Status = AppendDevicePathEntry (DeviceHandles[Index], Entries, EntryCount, &EntryCapacity);
    if (EFI_ERROR (Status)) {
      FreePool (DeviceHandles);
      ModernUiDeviceDataFreeEntries (*Entries, *EntryCount);
      *Entries    = NULL;
      *EntryCount = 0;
      return Status;
    }
  }

  FreePool (DeviceHandles);
  return EFI_SUCCESS;
}

VOID
EFIAPI
ModernUiDeviceDataFreeEntries (
  IN MODERN_UI_DEVICE_ENTRY  *Entries OPTIONAL,
  IN UINTN                   EntryCount
  )
{
  (VOID)EntryCount;
  if (Entries != NULL) {
    FreePool (Entries);
  }
}

EFI_STATUS
EFIAPI
ModernUiDeviceDataOpenEntry (
  IN CONST MODERN_UI_DEVICE_ENTRY  *Entry
  )
{
  EFI_STATUS                   Status;
  EFI_FORM_BROWSER2_PROTOCOL   *FormBrowser2;
  EFI_BROWSER_ACTION_REQUEST   ActionRequest;
  EFI_HII_HANDLE               HiiHandle;
  EFI_GUID                     FormSetGuid;

  if (Entry == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (!Entry->HasForm) {
    return EFI_UNSUPPORTED;
  }

  if (Entry->HiiHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->LocateProtocol (&gEfiFormBrowser2ProtocolGuid, NULL, (VOID **)&FormBrowser2);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  HiiHandle     = Entry->HiiHandle;
  CopyMem (&FormSetGuid, &Entry->FormSetGuid, sizeof (FormSetGuid));
  ActionRequest = EFI_BROWSER_ACTION_REQUEST_NONE;
  return FormBrowser2->SendForm (
                         FormBrowser2,
                         &HiiHandle,
                         1,
                         &FormSetGuid,
                         0,
                         NULL,
                         &ActionRequest
                         );
}
