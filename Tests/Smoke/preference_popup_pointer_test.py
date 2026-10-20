#!/usr/bin/env python3
"""Exercise shipped preference popup C with SDK types and mocked persistence.

Host geometry/interaction evidence, not firmware/GOP acceptance.
SPDX-License-Identifier: BSD-2-Clause-Patent
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / 'Application/ModernSetupApp'


def function(text, name):
    start = text.index('\n' + name + ' (')
    brace = text.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start + 1:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--edk2', required=True, type=Path)
    args = parser.parse_args()
    app = (APP / 'ModernSetupApp.c').read_text()
    pages = (APP / 'ModernSetupAppPages.c').read_text()
    actions = (APP / 'ModernSetupAppActions.c').read_text()
    # Guard both ordering and the consume/continue barrier in the real event loop.
    dispatch = re.search(
        r'if \(ModernSetupHandlePreferencePopupClick \(&Ui, PointerX, PointerY, StatusMessage, sizeof \(StatusMessage\)\)\) \{\s*'
        r'Redraw = TRUE;\s*continue;\s*\}', app)
    assert dispatch, 'open preference popup must consume clicks before background dispatch'
    pointer_start = app.index('if ((Event.Type == ModernUiInputPointer) && Event.PointerValid)')
    assert pointer_start < app.index('if (!Event.PointerPressed)', pointer_start) < dispatch.start()
    for background in ('ModernSetupHitTestTab', 'ModernSetupHitTestSecondaryNav',
                       'ModernSetupCatalogInput', 'ModernSetupHitTestDashboardCard',
                       'ModernSetupHitTestExitRow', 'ModernSetupHitTestPageListRow'):
        assert dispatch.end() < app.index(background, pointer_start), background
    draw = function(pages, 'DrawPreferences')
    assert 'PopupModel.Rect = ModernSetupPreferencePopupRect (Ui);' in draw
    assert 'RowModel.Rect      = ModernSetupPreferencePopupChoiceRect (PopupModel.Rect, Choice);' in draw
    print('PASS popup-first dispatch/consume barrier and shared painting geometry', flush=True)

    source = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#undef NULL
#include "ModernSetupAppInternal.h"
BOOLEAN mModernSetupPreferencePopupOpen;
UINTN mModernSetupPreferencePopupRow, mModernSetupPreferencePopupSelection;
MODERN_SETUP_PREFERENCE_POPUP_KIND mModernSetupPreferencePopupKind;
BOOLEAN mModernSetupPreferenceInputEdited;
UINTN mModernSetupPreferenceInputLength;
CHAR16 mModernSetupPreferenceInputBuffer[MODERN_UI_PREFERENCES_PROFILE_NAME_CHARS];
MODERN_UI_PREFERENCES mModernSetupPreferences;
static unsigned saves;
static MODERN_UI_PREFERENCES saved;
static MODERN_UI_RECT panel;
MODERN_UI_RECT ModernSetupContentRect(MODERN_UI_RENDER_CONTEXT *Ui) {
  (void)Ui; return panel;
}
VOID * EFIAPI CopyMem(VOID *d, CONST VOID *s, UINTN n) { return memcpy(d,s,n); }
UINTN EFIAPI UnicodeSPrint(CHAR16 *b, UINTN n, CONST CHAR16 *f, ...) {
  (void)f; if (b && n >= sizeof(*b)) b[0]=0; return 0;
}
CONST CHAR16 * EFIAPI ModernUiGetString(MODERN_UI_STRING_ID id) { (void)id; return L"saved"; }
EFI_STATUS EFIAPI ModernUiPreferencesSave(CONST MODERN_UI_PREFERENCES *p) {
  saves++; saved=*p; return EFI_SUCCESS;
}
'''
    for ret, name in [
        ('UINTN', 'ModernSetupGetPreferenceChoiceCount'),
        ('STATIC EFI_STATUS', 'PersistPreferencesAndStatus'),
        ('VOID', 'ModernSetupHandlePreferencePopupUp'),
        ('VOID', 'ModernSetupHandlePreferencePopupDown'),
        ('VOID', 'ModernSetupCancelPreferencePopup'),
        ('VOID', 'ModernSetupCommitPreferencePopup'),
        ('MODERN_UI_RECT', 'ModernSetupPreferencePopupRect'),
        ('MODERN_UI_RECT', 'ModernSetupPreferencePopupChoiceRect'),
        ('BOOLEAN', 'ModernSetupHandlePreferencePopupClick'),
    ]:
        source += '\n' + ret + '\n' + function(actions, name) + '\n'
    source += r'''
static MODERN_UI_RENDER_CONTEXT ui;
static CHAR16 status[128];
static void open_popup(UINTN row, MODERN_SETUP_PREFERENCE_POPUP_KIND kind) {
  memset(&mModernSetupPreferences, 0, sizeof(mModernSetupPreferences));
  mModernSetupPreferences.ThemeId=3;
  mModernSetupPreferences.DashboardDensity=1;
  mModernSetupPreferences.BootTimeoutSeconds=5;
  mModernSetupPreferencePopupOpen=TRUE;
  mModernSetupPreferencePopupRow=row;
  mModernSetupPreferencePopupSelection=1;
  mModernSetupPreferencePopupKind=kind;
  mModernSetupPreferenceInputEdited=TRUE;
  mModernSetupPreferenceInputLength=1;
  mModernSetupPreferenceInputBuffer[0]=L'7';
  saves=0;
}
static void click(UINTN x, UINTN y) {
  assert(ModernSetupHandlePreferencePopupClick(&ui,x,y,status,sizeof(status)));
}
int main(void) {
  /* Frozen panel fixture: popup occupies the audited Default click (1060,335). */
  panel=(MODERN_UI_RECT){224,188,1032,540};
  open_popup(MODERN_SETUP_PREFERENCE_ROW_THEME,ModernSetupPreferencePopupChoice);
  click(1060,335);
  assert(saves==1 && saved.ThemeId==0 && saved.DashboardDensity==1);
  assert(!mModernSetupPreferencePopupOpen);
  puts("PASS audited Default coordinate selects theme, not background density");
  for (UINTN width=800; width<=1280; width+=480) {
    panel=(MODERN_UI_RECT){24,188,width-48,540};
    for (UINTN row=0; row<=1; row++) {
      for (UINTN choice=0; choice<ModernSetupGetPreferenceChoiceCount(row); choice++) {
        for (UINTN corner=0; corner<4; corner++) {
          open_popup(row,ModernSetupPreferencePopupChoice);
          MODERN_UI_RECT p=ModernSetupPreferencePopupRect(&ui);
          assert(p.X==panel.X+26+(panel.Width-52)-240-12);
          assert(p.Y==panel.Y+72+(row+1)*42-8);
          assert(p.Height==40+ModernSetupGetPreferenceChoiceCount(row)*34);
          MODERN_UI_RECT r=ModernSetupPreferencePopupChoiceRect(p,choice);
          click(r.X+((corner&1)?r.Width-1:0),r.Y+((corner&2)?r.Height-1:0));
          assert(saves==1 && !mModernSetupPreferencePopupOpen);
          assert(mModernSetupPreferencePopupSelection==choice);
          assert(row==0 ? saved.ThemeId==choice && saved.DashboardDensity==1 :
                          saved.DashboardDensity==choice && saved.ThemeId==3);
        }
      }
      /* Exhaust every pixel inside the popup: only actual painted bands commit. */
      open_popup(row,ModernSetupPreferencePopupChoice);
      MODERN_UI_RECT p=ModernSetupPreferencePopupRect(&ui);
      for (UINTN y=0;y<p.Height;y++) for (UINTN x=0;x<p.Width;x++) {
        open_popup(row,ModernSetupPreferencePopupChoice);
        int hit=-1;
        for (UINTN c=0;c<ModernSetupGetPreferenceChoiceCount(row);c++)
          if (x>=6 && x<234 && y>=28+c*34 && y<58+c*34) hit=(int)c;
        click(p.X+x,p.Y+y);
        assert(saves==(hit>=0 ? 1u:0u));
        assert(mModernSetupPreferencePopupOpen==(hit<0));
        if(hit>=0) assert(mModernSetupPreferencePopupSelection==(UINTN)hit);
        else assert(mModernSetupPreferencePopupSelection==1);
      }
    }
  }
  puts("PASS every choice/corner and exhaustive title, margins, gaps and bottom padding at two widths");
  for (UINTN kind=ModernSetupPreferencePopupChoice;kind<=ModernSetupPreferencePopupStringInput;kind++) {
    UINTN row=(kind==ModernSetupPreferencePopupChoice)?0:kind;
    open_popup(row,kind);
    MODERN_UI_RECT p=ModernSetupPreferencePopupRect(&ui);
    if(kind!=ModernSetupPreferencePopupChoice) {
      for(UINTN y=0;y<p.Height;y++) for(UINTN x=0;x<p.Width;x++) {
        click(p.X+x,p.Y+y);
        assert(!saves && mModernSetupPreferencePopupOpen);
        assert(mModernSetupPreferences.BootTimeoutSeconds==5);
        assert(mModernSetupPreferenceInputBuffer[0]==L'7' && mModernSetupPreferenceInputEdited);
      }
    }
    UINTN outside[][2]={{0,0},{p.X-1,p.Y},{p.X+p.Width,p.Y},
                        {p.X,p.Y-1},{p.X,p.Y+p.Height},{(UINTN)-1,(UINTN)-1}};
    for(UINTN i=0;i<sizeof(outside)/sizeof(outside[0]);i++) {
      open_popup(row,kind);
      MODERN_UI_PREFERENCES before=mModernSetupPreferences;
      click(outside[i][0],outside[i][1]);
      assert(!saves && !mModernSetupPreferencePopupOpen && !mModernSetupPreferenceInputEdited);
      assert(memcmp(&before,&mModernSetupPreferences,sizeof(before))==0);
      assert(!ModernSetupHandlePreferencePopupClick(&ui,p.X,p.Y,status,sizeof(status)));
    }
  }
  puts("PASS numeric/string clicks never commit; outside cancels only; closed popup passes through");
  open_popup(0,ModernSetupPreferencePopupChoice);
  mModernSetupPreferencePopupSelection=0;
  ModernSetupHandlePreferencePopupUp(); assert(mModernSetupPreferencePopupSelection==3);
  ModernSetupHandlePreferencePopupDown(); assert(mModernSetupPreferencePopupSelection==0);
  ModernSetupCommitPreferencePopup(status,sizeof(status)); assert(saves==1 && saved.ThemeId==0);
  open_popup(2,ModernSetupPreferencePopupNumericInput);
  ModernSetupHandlePreferencePopupDown(); assert(mModernSetupPreferencePopupSelection==1);
  ModernSetupCommitPreferencePopup(status,sizeof(status)); assert(saves==1 && saved.BootTimeoutSeconds==7);
  open_popup(3,ModernSetupPreferencePopupStringInput);
  ModernSetupCommitPreferencePopup(status,sizeof(status)); assert(saves==1 && saved.ProfileName[0]==L'7');
  puts("PASS existing keyboard wrap/Enter commit unchanged for choice, numeric and string");
  return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='preference-popup-') as tmp:
        c = Path(tmp) / 'test.c'
        exe = Path(tmp) / 'test'
        c.write_text(source)
        cmd = shlex.split(os.environ.get('CC', 'cc')) + [
            '-std=c11', '-fshort-wchar', '-Wall', '-Wextra', '-Werror',
            '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
            '-I' + str(APP), '-I' + str(ROOT / 'Include'),
            '-I' + str(args.edk2 / 'MdePkg/Include'),
            '-I' + str(args.edk2 / 'MdePkg/Include/X64'),
            '-I' + str(args.edk2 / 'MdeModulePkg/Include'), str(c), '-o', str(exe)]
        subprocess.run(cmd, check=True)
        subprocess.run([str(exe)], check=True, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1'})


if __name__ == '__main__':
    main()
