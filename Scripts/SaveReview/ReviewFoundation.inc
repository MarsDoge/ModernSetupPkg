/** @file
  Browser-private, deliberately unwired review primitives.
  Included after Setup.h and the native StorageToConfigResp declaration.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

/** Compare the exact request serialized by native submission.

  BaselineKnown must come from successful load/synchronization provenance, NOT
  BROWSER_STORAGE.Initialized (set BEFORE ExtractConfig on this baseline).
  Responses remain browser-private and are erased before release. Even changed
  non-secret data is opaque until a complete question/alias sensitivity map exists.
  No baseline updates, driver reads, validation or question evaluation occur here.
**/
EFI_STATUS
ModernReviewRequestChanged (
  IN BROWSER_STORAGE  *Storage,
  IN CHAR16           *Request,
  IN BOOLEAN          BaselineKnown,
  OUT BOOLEAN         *Changed
  )
{
  EFI_STATUS       Status;
  CHAR16           *Before;
  CHAR16           *After;
  LIST_ENTRY       *Link;
  NAME_VALUE_NODE  *Node;

  if (Changed == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  *Changed = FALSE;
  if ((Storage == NULL) || (Request == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  if (!BaselineKnown) {
    return EFI_NOT_READY;
  }
  if ((Storage->Type == EFI_HII_VARSTORE_BUFFER) ||
      (Storage->Type == EFI_HII_VARSTORE_EFI_VARIABLE_BUFFER)) {
    if ((Storage->Buffer == NULL) || (Storage->EditBuffer == NULL) ||
        (Storage->Size == 0)) {
      return EFI_NOT_READY;
    }
  } else if (Storage->Type == EFI_HII_VARSTORE_NAME_VALUE) {
    for (Link = GetFirstNode (&Storage->NameValueListHead);
         !IsNull (&Storage->NameValueListHead, Link);
         Link = GetNextNode (&Storage->NameValueListHead, Link)) {
      Node = NAME_VALUE_NODE_FROM_LINK (Link);
      if (Node->Name == NULL) {
        *Changed = FALSE;
        return EFI_NOT_READY;
      }
      // Preserve native request selection without serializing secret values.
      // Native NewStringCat cannot report allocation failure and releases
      // intermediate strings without erasing them. Compare borrowed values
      // directly instead; continue checking baseline coverage after a change.
      if (StrStr (Request, Node->Name) != NULL) {
        if ((Node->Value == NULL) || (Node->EditValue == NULL)) {
          *Changed = FALSE;
          return EFI_NOT_READY;
        }
        if (StrCmp (Node->Value, Node->EditValue) != 0) {
          *Changed = TRUE;
        }
      }
    }
    return EFI_SUCCESS;
  } else {
    return EFI_UNSUPPORTED;
  }

  Before = NULL;
  After  = NULL;
  Status = StorageToConfigResp (Storage, &Before, Request, FALSE);
  if (!EFI_ERROR (Status)) {
    Status = StorageToConfigResp (Storage, &After, Request, TRUE);
  }
  if (!EFI_ERROR (Status)) {
    if ((Before == NULL) || (After == NULL)) {
      Status = EFI_NOT_READY;
    } else {
      *Changed = (BOOLEAN)(StrCmp (Before, After) != 0);
    }
  }
  if (Before != NULL) {
    ZeroMem (Before, StrSize (Before));
    FreePool (Before);
  }
  if (After != NULL) {
    ZeroMem (After, StrSize (After));
    FreePool (After);
  }
  return Status;
}

/** Enumerate request objects used by SubmitForForm/SubmitForFormSet.
  Visitor is browser-private: it MUST NOT retain pointers or mutate state.
  System scope intentionally fails closed until identity/refresh handling exists.
  This does NOT imply baseline provenance or complete review coverage.
**/
typedef EFI_STATUS (*MODERN_REVIEW_VISITOR)(BROWSER_STORAGE *, CHAR16 *, VOID *);

EFI_STATUS
ModernReviewVisitScope (
  IN FORM_BROWSER_FORMSET   *FormSet,
  IN FORM_BROWSER_FORM      *Form,
  IN BROWSER_SETTING_SCOPE  Scope,
  IN MODERN_REVIEW_VISITOR  Visitor,
  IN VOID                  *Context
  )
{
  LIST_ENTRY                   *Link;
  FORM_BROWSER_CONFIG_REQUEST  *Info;
  FORMSET_STORAGE              *FsStorage;
  EFI_STATUS                   Status;

  if ((Visitor == NULL) || (FormSet == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  if (Scope == FormLevel) {
    if (Form == NULL) {
      return EFI_INVALID_PARAMETER;
    }
    if (!IsNvUpdateRequiredForForm (Form)) {
      return EFI_SUCCESS;
    }
    for (Link = GetFirstNode (&Form->ConfigRequestHead);
         !IsNull (&Form->ConfigRequestHead, Link);
         Link = GetNextNode (&Form->ConfigRequestHead, Link)) {
      Info = FORM_BROWSER_CONFIG_REQUEST_FROM_LINK (Link);
      if ((Info->Storage->Type == EFI_HII_VARSTORE_EFI_VARIABLE) || (Info->ElementCount == 0)) {
        continue;
      }
      Status = Visitor (Info->Storage, Info->ConfigRequest, Context);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }
  } else if (Scope == FormSetLevel) {
    if (!IsNvUpdateRequiredForFormSet (FormSet)) {
      return EFI_SUCCESS;
    }
    for (Link = GetFirstNode (&FormSet->StorageListHead);
         !IsNull (&FormSet->StorageListHead, Link);
         Link = GetNextNode (&FormSet->StorageListHead, Link)) {
      FsStorage = FORMSET_STORAGE_FROM_LINK (Link);
      if ((FsStorage->BrowserStorage->Type == EFI_HII_VARSTORE_EFI_VARIABLE) || (FsStorage->ElementCount == 0)) {
        continue;
      }
      Status = Visitor (FsStorage->BrowserStorage, FsStorage->ConfigRequest, Context);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }
  } else {
    return EFI_UNSUPPORTED;
  }
  return EFI_SUCCESS;
}
