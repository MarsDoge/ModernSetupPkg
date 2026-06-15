/** @file
  Performance and tuning summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <IndustryStandard/SmBios.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Protocol/CpuIo2.h>
#include <Uefi/UefiInternalFormRepresentation.h>

#include <ModernUi/ModernUiPlatformTables.h>
#include <ModernUi/ModernUiPerformanceData.h>

/**
  Return whether a protocol is currently installed.

  @param[in] ProtocolGuid  Protocol GUID to locate. Must not be NULL.

  @retval TRUE   The protocol is installed.
  @retval FALSE  The protocol is absent or ProtocolGuid is NULL.
**/
STATIC
BOOLEAN
IsProtocolPresent (
  IN CONST EFI_GUID  *ProtocolGuid
  )
{
  VOID  *Protocol;

  if (ProtocolGuid == NULL) {
    return FALSE;
  }

  Protocol = NULL;
  return (BOOLEAN)!EFI_ERROR (gBS->LocateProtocol ((EFI_GUID *)ProtocolGuid, NULL, &Protocol));
}

/**
  Return whether a string contains one of the requested keywords.

  @param[in] Text      Source text. May be NULL.
  @param[in] Keywords  Keyword array. Must not be NULL when KeywordCount is
                       nonzero.
  @param[in] KeywordCount Number of entries in Keywords.

  @retval TRUE   Text contains at least one keyword.
  @retval FALSE  No keyword matched.
**/
STATIC
BOOLEAN
ContainsAnyKeyword (
  IN CONST CHAR16       *Text OPTIONAL,
  IN CONST CHAR16 *CONST *Keywords,
  IN UINTN              KeywordCount
  )
{
  UINTN  Index;

  if ((Text == NULL) || ((KeywordCount > 0) && (Keywords == NULL))) {
    return FALSE;
  }

  for (Index = 0; Index < KeywordCount; Index++) {
    if ((Keywords[Index] != NULL) && (StrStr (Text, Keywords[Index]) != NULL)) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Return whether any HII formset title/help mentions one of the keywords.

  @param[in] Keywords      Keyword array. Must not be NULL when KeywordCount is
                           nonzero.
  @param[in] KeywordCount  Number of entries in Keywords.

  @retval TRUE   A matching HII formset exists.
  @retval FALSE  No matching HII formset was found.
**/
STATIC
BOOLEAN
HasHiiFormsetKeyword (
  IN CONST CHAR16 *CONST *Keywords,
  IN UINTN              KeywordCount
  )
{
  EFI_HII_HANDLE    *Handles;
  UINTN             HandleIndex;
  EFI_IFR_FORM_SET  *FormSets;
  EFI_IFR_OP_HEADER *OpHeader;
  UINTN             BufferSize;
  UINTN             Offset;
  CHAR16            *Title;
  CHAR16            *Help;
  BOOLEAN           Found;

  if ((KeywordCount > 0) && (Keywords == NULL)) {
    return FALSE;
  }

  Handles = HiiGetHiiHandles (NULL);
  if (Handles == NULL) {
    return FALSE;
  }

  Found = FALSE;
  for (HandleIndex = 0; !Found && (Handles[HandleIndex] != NULL); HandleIndex++) {
    FormSets   = NULL;
    BufferSize = 0;
    if (EFI_ERROR (HiiGetFormSetFromHiiHandle (Handles[HandleIndex], &FormSets, &BufferSize))) {
      continue;
    }

    Offset = 0;
    while (!Found && (Offset + sizeof (EFI_IFR_OP_HEADER) <= BufferSize)) {
      OpHeader = (EFI_IFR_OP_HEADER *)((UINT8 *)FormSets + Offset);
      if ((OpHeader->Length == 0) || (Offset + OpHeader->Length > BufferSize)) {
        break;
      }

      if (OpHeader->OpCode == EFI_IFR_FORM_SET_OP) {
        Title = HiiGetString (Handles[HandleIndex], ((EFI_IFR_FORM_SET *)OpHeader)->FormSetTitle, NULL);
        Help  = HiiGetString (Handles[HandleIndex], ((EFI_IFR_FORM_SET *)OpHeader)->Help, NULL);
        Found = (BOOLEAN)(ContainsAnyKeyword (Title, Keywords, KeywordCount) || ContainsAnyKeyword (Help, Keywords, KeywordCount));
        if (Title != NULL) {
          FreePool (Title);
        }

        if (Help != NULL) {
          FreePool (Help);
        }
      }

      Offset += OpHeader->Length;
    }

    FreePool (FormSets);
  }

  FreePool (Handles);
  return Found;
}

/**
  Collect read-only performance and tuning capability state.

  @param[out] Summary  Performance summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiPerformanceDataGetSummary (
  OUT MODERN_UI_PERFORMANCE_SUMMARY  *Summary
  )
{
  STATIC CONST CHAR16  *mVirtualizationKeywords[] = {
    L"Virtualization",
    L"VT",
    L"SVM",
    L"IOMMU"
  };
  STATIC CONST CHAR16  *mRasKeywords[] = {
    L"RAS",
    L"NUMA",
    L"Reliability",
    L"PCIe"
  };

  if (Summary == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Summary, sizeof (*Summary));
  Summary->ProcessorInventoryPresent      = ModernUiSmbiosTypePresent (SMBIOS_TYPE_PROCESSOR_INFORMATION);
  Summary->MemoryInventoryPresent         = ModernUiSmbiosTypePresent (SMBIOS_TYPE_MEMORY_DEVICE);
  Summary->CpuIo2ProtocolPresent          = IsProtocolPresent (&gEfiCpuIo2ProtocolGuid);
  Summary->VirtualizationPolicyEntryPresent = HasHiiFormsetKeyword (mVirtualizationKeywords, ARRAY_SIZE (mVirtualizationKeywords));
  Summary->RasPolicyEntryPresent          = HasHiiFormsetKeyword (mRasKeywords, ARRAY_SIZE (mRasKeywords));
  return EFI_SUCCESS;
}
