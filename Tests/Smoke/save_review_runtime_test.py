#!/usr/bin/env python3
"""Compile the shipped runtime against pinned native browser types; run real buffer edits."""
import argparse
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
def main():
    p = argparse.ArgumentParser()
    p.add_argument('--workspace', type=Path, help='Optional symlink-based build workspace')
    p.add_argument('--sdk', type=Path, default=Path(os.environ.get('SAVE_REVIEW_EDK2', str(ROOT / 'External/edk2'))))
    a = p.parse_args()
    spec = importlib.util.spec_from_file_location('overlay', ROOT / 'Scripts/setup-browser-review-overlay.py')
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    out = (a.workspace or ROOT) / 'Build/ModernSetupPkgOverlay/ReviewHostTest'
    mod.generate(a.workspace or a.sdk, out, sdk=a.sdk)
    def extract(file, name, ret):
        text = (out/file).read_text()
        start = text.index(ret+'\n'+name+' (')
        end = text.index('\n}\n', start)+3
        return text[start:end]
    (out/'HostNative.inc').write_text(extract('Setup.c','SetValueByName','EFI_STATUS') + '\n' + extract('Setup.c','ConfigRespToStorage','EFI_STATUS') + '\n' + extract('Presentation.c','ProcessAction','EFI_STATUS') + '\n' + extract('Setup.c','LoadStorage','VOID') + '\n' + extract('Setup.c','SubmitForForm','EFI_STATUS') + '\n' + extract('Setup.c','SubmitForFormSet','EFI_STATUS'))
    presentation = (out/'Presentation.c').read_text()
    begin = presentation.index('  if (SubmitFormIsRequired && !SkipSaveOrDiscard) {')
    end = presentation.index('  if (ResetRequested)', begin)
    callback = presentation[begin:end]
    begin = presentation.index('      if ((Status == EFI_SUCCESS) &&')
    end = presentation.index('    //\n    // Check whether Exit flag', begin)
    flags = presentation[begin:end].rsplit('    }', 1)[0]
    (out/'HostCallback.inc').write_text('STATIC EFI_STATUS HostCallback(UI_MENU_SELECTION *Selection) { EFI_STATUS InternalStatus; BOOLEAN SubmitFormIsRequired=TRUE, SkipSaveOrDiscard=FALSE; FORM_BROWSER_FORMSET *FormSet=Selection->FormSet; FORM_BROWSER_FORM *Form=Selection->Form; BROWSER_SETTING_SCOPE SettingLevel=FormSetLevel;\n' + callback + 'return EFI_SUCCESS; }\nSTATIC EFI_STATUS HostChanged(UI_MENU_SELECTION *Selection, FORM_BROWSER_STATEMENT *Statement) { EFI_STATUS Status=HostCallback(Selection);\n' + flags + '\nreturn Status; }\n')
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / 'test'
        cmd = ['gcc', '-g', '-fsanitize=address,undefined', '-std=gnu11', '-fshort-wchar', '-fno-builtin', '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-DNDEBUG', '-I'+str(out), '-I'+str(ROOT/'Include')]
        for inc in ['MdePkg/Include', 'MdePkg/Include/X64', 'MdeModulePkg/Include']:
            cmd += ['-I'+str(a.sdk/inc)]
        cmd += [str(ROOT/'Tests/Smoke/save_review_runtime_test.c'), '-o', str(exe)]
        subprocess.run(cmd, check=True)
        subprocess.run([str(exe)], check=True)
if __name__ == '__main__': main()
