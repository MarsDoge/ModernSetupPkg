/** @file
  Static GUID page adapter registry for ModernSetupPkg.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>

#include <ModernUi/ModernUiPageAdapter.h>

/**
  Return the compiled-in page adapter registry.

  v0.2.0 intentionally ships the registry mechanism before adding OEM-specific
  adapters. Platform packages can add entries here or replace this library class
  with their own implementation.

  @param[out] Count  Receives the number of adapters. Must not be NULL.

  @return Pointer to an array of adapter pointers. NULL is valid when Count is 0.
**/
CONST MODERN_UI_PAGE_ADAPTER *CONST *
EFIAPI
ModernUiGetStaticPageAdapters (
  OUT UINTN  *Count
  )
{
  if (Count == NULL) {
    return NULL;
  }

  *Count = 0;
  return NULL;
}

/**
  Find the best adapter for a HII formset GUID.

  @param[in] FormSetGuid  Formset GUID to match. Must not be NULL.
  @param[in] FormSet      Optional formset metadata used by Supported().

  @return Highest-priority matching adapter, or NULL when no adapter exists.
**/
CONST MODERN_UI_PAGE_ADAPTER *
EFIAPI
ModernUiFindPageAdapterByGuid (
  IN CONST EFI_GUID                *FormSetGuid,
  IN CONST MODERN_UI_HII_FORMSET   *FormSet OPTIONAL
  )
{
  CONST MODERN_UI_PAGE_ADAPTER *CONST  *Adapters;
  CONST MODERN_UI_PAGE_ADAPTER        *Best;
  UINTN                               Count;
  UINTN                               Index;

  if (FormSetGuid == NULL) {
    return NULL;
  }

  Adapters = ModernUiGetStaticPageAdapters (&Count);
  Best     = NULL;
  for (Index = 0; Index < Count; Index++) {
    if ((Adapters == NULL) || (Adapters[Index] == NULL)) {
      continue;
    }

    if (!CompareGuid (FormSetGuid, &Adapters[Index]->TargetFormSetGuid)) {
      continue;
    }

    if ((Adapters[Index]->Supported != NULL) && !Adapters[Index]->Supported (Adapters[Index], FormSet)) {
      continue;
    }

    if ((Best == NULL) || (Adapters[Index]->Priority > Best->Priority)) {
      Best = Adapters[Index];
    }
  }

  return Best;
}
