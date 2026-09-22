"""Exact pinned-source transformation: native browser remains sole policy owner."""
from pathlib import Path
import importlib.util


def once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"review anchor count {text.count(old)}: {old[:90]!r}")
    return text.replace(old, new, 1)


def apply(contents, root):
    spec = importlib.util.spec_from_file_location("cancel_guards", root / "Scripts/SaveReview/CancelGuards/__init__.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    contents = module.apply(contents)
    text = {k: v.decode().replace("\r\n", "\n") for k, v in contents.items()}
    h = text[Path("Setup.h")]
    h = once(h, "  BOOLEAN           Initialized;", "  BOOLEAN           ModernReviewBaselineKnown; // successful native extract + sync only\n  BOOLEAN           Initialized;")
    text[Path("Setup.h")] = h
    s = text[Path("Setup.c")]
    # Include after private native helpers have been declared/defined.
    s = once(s, 'EFI_STATUS\nSubmitForForm (', '#include "ReviewRuntime.inc"\n\nEFI_STATUS\nSubmitForForm (')
    s = once(s, '  switch (SettingScope) {\n    case FormLevel:\n      Status = SubmitForForm (FormSet, Form);', '  Status = ModernReviewSubmit (FormSet, Form, SettingScope);\n  if (EFI_ERROR (Status)) {\n    return Status;\n  }\n\n  switch (SettingScope) {\n    case FormLevel:\n      Status = SubmitForForm (FormSet, Form);')
    s = once(s, '  SynchronizeStorage (Storage->BrowserStorage, NULL, TRUE);', '''  // Initialized is set before ExtractConfig; it is NOT baseline provenance.
  Storage->BrowserStorage->ModernReviewBaselineKnown = FALSE;
  if (!EFI_ERROR (Status)) {
    Status = SynchronizeStorage (Storage->BrowserStorage, NULL, TRUE);
    Storage->BrowserStorage->ModernReviewBaselineKnown = (BOOLEAN)(!EFI_ERROR (Status) && ModernReviewNamesSynchronized (Storage->BrowserStorage));
  } else if (Storage->BrowserStorage->Type != EFI_HII_VARSTORE_NAME_VALUE) {
    // A failed name/value setter can leave NULL EditValue; do not synchronize
    // an untrusted/partially converted node list (native sync assumes non-NULL).
    SynchronizeStorage (Storage->BrowserStorage, NULL, TRUE);
  }''')
    s = once(s, '      Status = ConfigRespToStorage (Storage->BrowserStorage, Result);', '      Status = ModernReviewLoadResponse (Storage, Result);')
    s = once(s, '      StrPtr = StrStr (Result, L"&GUID=");', '      StrPtr = Result == NULL ? NULL : StrStr (Result, L"&GUID=");')
    s = once(s, '      Status = ModernReviewLoadResponse (Storage, Result);\n      FreePool (Result);', '      Status = ModernReviewLoadResponse (Storage, Result);\n      if (Result != NULL) { FreePool (Result); }')
    s = once(s, '  gBrowserContextCount++;', '  mReviewContextEpoch++; // invalidate approvals even when nested SendForm returns\n  gBrowserContextCount++;')
    s = once(s, '      Storage->BrowserStorage->Initialized = FALSE;', '      Storage->BrowserStorage->ModernReviewBaselineKnown = FALSE;\n      Storage->BrowserStorage->Initialized = FALSE;')
    # Route failures never imply rollback, discard, success, page change or reset.
    start = s.index('    if (ConfirmSaveFail (Form->FormTitle, FormSet->HiiHandle) == BROWSER_ACTION_DISCARD) {')
    end = s.index('    //\n    // Free Form save fail list.', start)
    s = s[:start] + '''    Status = EFI_DEVICE_ERROR;
    Link = GetFirstNode (&gBrowserSaveFailFormSetList);
    while (!IsNull (&gBrowserSaveFailFormSetList, Link)) {
      ConfigInfo = FORM_BROWSER_CONFIG_REQUEST_FROM_SAVE_FAIL_LINK (Link);
      Link = GetNextNode (&gBrowserSaveFailFormSetList, Link);
      if (ConfigInfo->SyncConfigRequest != NULL) {
        SynchronizeStorage (ConfigInfo->Storage, ConfigInfo->SyncConfigRequest, TRUE);
        FreePool (ConfigInfo->SyncConfigRequest);
        ConfigInfo->SyncConfigRequest = NULL;
      }
      if (ConfigInfo->RestoreConfigRequest != NULL) {
        FreePool (ConfigInfo->RestoreConfigRequest);
        ConfigInfo->RestoreConfigRequest = NULL;
      }
    }

''' + s[end:]
    start = s.index('      if (ConfirmSaveFail (Form->FormTitle, FormSet->HiiHandle) == BROWSER_ACTION_DISCARD) {')
    end = s.index('      //\n      // Free FormSet save fail list.', start)
    s = s[:start] + '''      Status = EFI_DEVICE_ERROR;
      Link = GetFirstNode (&FormSet->SaveFailStorageListHead);
      while (!IsNull (&FormSet->SaveFailStorageListHead, Link)) {
        FormSetStorage = FORMSET_STORAGE_FROM_SAVE_FAIL_LINK (Link);
        Link = GetNextNode (&FormSet->SaveFailStorageListHead, Link);
        if (FormSetStorage->SyncConfigRequest != NULL) {
          SynchronizeStorage (FormSetStorage->BrowserStorage, FormSetStorage->SyncConfigRequest, TRUE);
          FreePool (FormSetStorage->SyncConfigRequest);
          FormSetStorage->SyncConfigRequest = NULL;
        }
        if (FormSetStorage->RestoreConfigRequest != NULL) {
          FreePool (FormSetStorage->RestoreConfigRequest);
          FormSetStorage->RestoreConfigRequest = NULL;
        }
      }

''' + s[end:]
    # A route failure may not map to any question; do not ASSERT or dereference it.
    s = once(s, '        ASSERT (Form != NULL && Question != NULL);', '        // Opaque route failures need not identify a question.')
    s = once(s, '  if (Form != NULL) {\n    if (!SkipProcessFail) {', '  if (SubmitFormSetFail) {\n    if (!SkipProcessFail) {')
    # System calls the same native per-formset submit with local failure cleanup.
    start = s.index('EFI_STATUS\nSubmitForSystem (')
    end = s.index('/**', start)
    s = s[:start] + '''EFI_STATUS
SubmitForSystem (
  VOID
  )
{
  EFI_STATUS Status;
  EFI_STATUS Result;
  LIST_ENTRY *Link;
  FORM_BROWSER_FORMSET *LocalFormSet;
  Result = EFI_SUCCESS;
  mSystemSubmit = TRUE;
  Link = GetFirstNode (&gBrowserFormSetList);
  while (!IsNull (&gBrowserFormSetList, Link)) {
    LocalFormSet = FORM_BROWSER_FORMSET_FROM_LINK (Link);
    Link = GetNextNode (&gBrowserFormSetList, Link);
    if (!ValidateFormSet (LocalFormSet)) { continue; }
    Status = SubmitForFormSet (LocalFormSet, FALSE);
    if (EFI_ERROR (Status)) {
      Result = Status;
      break;
    }
    if (!IsHiiHandleInBrowserContext (LocalFormSet->HiiHandle)) {
      CleanBrowserStorage (LocalFormSet);
      RemoveEntryList (&LocalFormSet->Link);
      DestroyFormSet (LocalFormSet);
    }
  }
  mSystemSubmit = FALSE;
  return Result;
}

''' + s[end:]
    # Report attempted native save errors with the same review UI, no saved claim.
    marker = '      Status = SubmitForSystem ();\n      break;'
    assert marker in s
    start = s.index('EFI_STATUS\nSubmitForm (')
    end = s.index('/**', start)
    chunk = s[start:end]
    chunk = once(chunk, '  return Status;', '  return Status;') if False else chunk
    # Gate errors return directly; only actual persistence errors get a notice.
    pos = chunk.rfind('  return Status;')
    chunk = chunk[:pos] + '  ModernReviewFinish ();\n  if (EFI_ERROR (Status)) { ModernReviewSaveError (); }\n' + chunk[pos:]
    s = s[:start] + chunk + s[end:]
    for name, request, flag, cleanup in [
        ('SubmitForForm', 'ConfigInfo->ConfigRequest', 'SubmitFormFail', '  //\n  // 4. Process the save failed storage.'),
        ('SubmitForFormSet', 'FormSetStorage->ConfigRequest', 'SubmitFormSetFail', '  //\n  // 4. Has save fail storage need to handle.'),
    ]:
        start = s.index('EFI_STATUS\n' + name + ' (')
        end = s.index('/**', start)
        part = s[start:end]
        serial = '    Status = StorageToConfigResp ('
        at = part.index(serial)
        tail = part[at:]
        tail = once(tail, '      return Status;', '      ' + flag + ' = TRUE;\n      goto ReviewCleanup;')
        route = '    Status = mHiiConfigRouting->RouteConfig ('
        tail = once(tail, route, '    Status = ModernReviewRouteGuard (Storage, ' + request + ', ConfigResp);\n    if (EFI_ERROR (Status)) {\n      FreePool (ConfigResp);\n      ' + flag + ' = TRUE;\n      goto ReviewCleanup;\n    }\n' + route)
        part = part[:at] + tail
        part = once(part, cleanup, 'ReviewCleanup:\n' + cleanup)
        # Preserve serialization/guard errors while draining earlier route failures.
        part = part.replace('Status = EFI_DEVICE_ERROR;', 'if (!EFI_ERROR (Status)) { Status = EFI_DEVICE_ERROR; }')
        s = s[:start] + part + s[end:]
    text[Path("Setup.c")] = s
    p = text[Path("Presentation.c")]
    text[Path("Presentation.c")] = p
    inf = text[Path("SetupBrowserDxe.inf")]
    inf = once(inf, '[Packages]', '[Packages]\n  ModernSetupPkg/ModernSetupPkg.dec')
    text[Path("SetupBrowserDxe.inf")] = inf
    text[Path("ReviewRuntime.inc")] = (root / "Scripts/SaveReview/ReviewRuntime.inc").read_text()
    return {k: v.replace('\n', '\r\n').encode() for k, v in text.items()}
