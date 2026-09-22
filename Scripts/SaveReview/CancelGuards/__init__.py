"""Pinned-source, copy-only submit caller guards. No review gate is installed."""
from pathlib import Path


def replace_once(data, old, new, label):
    old = old.replace("\n", "\r\n").encode("ascii")
    new = new.replace("\n", "\r\n").encode("ascii")
    if data.count(old) != 1:
        raise ValueError(f"unexpected cancel-guard anchor: {label}")
    return data.replace(old, new, 1)


def apply(contents):
    """Return modified in-memory copies; fail before any output is written."""
    result = dict(contents)
    presentation = result[Path("Presentation.c")]
    edits = [
        ("action locals", '''ProcessAction (
  IN UINT32  Action,
  IN UINT16  DefaultId
  )
{
''', '''ProcessAction (
  IN UINT32  Action,
  IN UINT16  DefaultId
  )
{
  EFI_STATUS  Status;

  // Reject contradictory save/discard or save/ESC requests before mutation.
  if (((Action & BROWSER_ACTION_SUBMIT) != 0) &&
      ((Action & (BROWSER_ACTION_DISCARD | BROWSER_ACTION_FORM_EXIT)) != 0))
  {
    gCurrentSelection->Action = UI_ACTION_REFRESH_FORM;
    return EFI_SUCCESS;
  }

'''),
        ("action submit", '''    SubmitForm (gCurrentSelection->FormSet, gCurrentSelection->Form, gBrowserSettingScope);
''', '''    Status = SubmitForm (gCurrentSelection->FormSet, gCurrentSelection->Form, gBrowserSettingScope);
    if (EFI_ERROR (Status)) {
      // DisplayForm errors terminate SetupBrowser. This is a handled action,
      // not a display failure: retain native submit diagnostics and edit state.
      gCurrentSelection->Action = UI_ACTION_REFRESH_FORM;
      return EFI_SUCCESS;
    }
'''),
        ("callback reset local", '''  BOOLEAN                         SubmitFormIsRequired;
''', '''  BOOLEAN                         ResetRequested;
  BOOLEAN                         SubmitFormIsRequired;
'''),
        ("callback reset init", '''  ConfigAccess          = FormSet->ConfigAccess;
''', '''  ResetRequested        = FALSE;
  ConfigAccess          = FormSet->ConfigAccess;
'''),
        ("defer callback reset", '''              gResetRequiredFormLevel   = TRUE;
              gResetRequiredSystemLevel = TRUE;
''', '''              ResetRequested           = TRUE;
'''),
        ("callback submit", '''  if (SubmitFormIsRequired && !SkipSaveOrDiscard) {
    SubmitForm (FormSet, Form, SettingLevel);
  }
''', '''  if (SubmitFormIsRequired && !SkipSaveOrDiscard) {
    InternalStatus = SubmitForm (FormSet, Form, SettingLevel);
    if (EFI_ERROR (InternalStatus)) {
      // Cancel pending exit/discard/reconnect, not prior or partial-save reset state.
      // SubmitForm may already have committed a subset: this is not rollback.
      gCallbackReconnect        = FALSE;
      Selection->Action         = UI_ACTION_REFRESH_FORM;
      // Handled rejection is nonfatal, but must not enable success-only flags.
      return EFI_WARN_WRITE_FAILURE;
    }
  }
'''),
        ("commit callback reset", '''  if (DiscardFormIsRequired && !SkipSaveOrDiscard) {
''', '''  if (ResetRequested) {
    gResetRequiredFormLevel   = TRUE;
    gResetRequiredSystemLevel = TRUE;
  }

  if (DiscardFormIsRequired && !SkipSaveOrDiscard) {
'''),
        ("changed callback status", '''          ProcessCallBackFunction (Selection, Selection->FormSet, Selection->Form, Statement, EFI_BROWSER_ACTION_CHANGED, FALSE);
''', '''          Status = ProcessCallBackFunction (Selection, Selection->FormSet, Selection->Form, Statement, EFI_BROWSER_ACTION_CHANGED, FALSE);
'''),
    ]
    for label, old, new in edits:
        presentation = replace_once(presentation, old, new, label)
    result[Path("Presentation.c")] = presentation
    result[Path("Setup.c")] = replace_once(
        result[Path("Setup.c")],
        '''      SubmitForm (NULL, NULL, SystemLevel);
      DataSavedAction = BROWSER_SAVE_CHANGES;
''',
        '''      if (EFI_ERROR (SubmitForm (NULL, NULL, SystemLevel))) {
        // The reminder reports an action, not EFI_STATUS; never claim saved.
        return BROWSER_KEEP_CURRENT;
      }

      DataSavedAction = BROWSER_SAVE_CHANGES;
''',
        "save reminder",
    )
    return result
