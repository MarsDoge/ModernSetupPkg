#!/usr/bin/env python3
"""Execute the actual native framework entry with display-only mocks.
SPDX-License-Identifier: BSD-2-Clause-Patent
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / 'Library/ModernUiCustomizedDisplayLib/CustomizedDisplayLibInternal.c').read_text()
body = source[source.index('VOID\nPrintFramework ('):]
body = body[:body.index('/**')]
stub = r'''
#include <assert.h>
#include <stddef.h>
#define VOID void
#define IN
#define EFI_ERROR(x) ((x)!=0)
#define FORMSET_CLASS_PLATFORM_SETUP 1
#define STATUS_BAR_HEIGHT 1
#define NONE_FRONT_PAGE_HEADER_HEIGHT 5
#define KEYHELP_TEXT 7
#define KEYHELP_BACKGROUND 16
#define TITLE_TEXT 8
#define TITLE_BACKGROUND 32
#define FIELD_BACKGROUND 64
#define PcdBrowserFieldTextColor 3
#define PcdGet8(x) (x)
typedef size_t UINTN;
typedef int FORM_DISPLAY_ENGINE_FORM;
typedef struct { UINTN X,Y,Width,Height; } MODERN_UI_RECT;
static struct { UINTN Width,Height; } mModernRenderContext={1280,800};
static struct { UINTN LeftColumn,RightColumn,TopRow,BottomRow; } gScreenDimensions={0,80,0,25};
static UINTN mModernCursorColumn,mModernCursorRow;
static int gClassOfVfr=1,gFooterHeight=3,ready_error,backdrop,clears,chrome,restores;
static UINTN attribute,cursor_x,cursor_y;
struct output;
static void attr(struct output *o,UINTN a) { (void)o;attribute=a; }
static void cursor(struct output *o,UINTN x,UINTN y) { (void)o;cursor_x=x;cursor_y=y; }
static struct output { void (*SetAttribute)(struct output*,UINTN);void (*SetCursorPosition)(struct output*,UINTN,UINTN); } out={attr,cursor};
static struct { struct output *ConOut; } table={&out},*gST=&table;
static int ModernDisplayEnsureRenderer(void) { return ready_error; }
static int ModernUiBackdropRedraw(void *ui,MODERN_UI_RECT r) {
 (void)ui;assert(r.X==0&&r.Y==0&&r.Width==1280&&r.Height==800);restores++;return backdrop;
}
static void ModernDisplayDrawPageChrome(FORM_DISPLAY_ENGINE_FORM *f) { (void)f;chrome++; }
static void ClearLines(UINTN l,UINTN r,UINTN t,UINTN b,UINTN a) {
 clears++;attribute=a;cursor_x=l;cursor_y=t;mModernCursorColumn=r;mModernCursorRow=b;
}
'''
main = r'''
int main(void) {
 int form=0;
 for(int enabled=0;enabled<=1;enabled++) {
  backdrop=enabled;clears=chrome=restores=0;PrintFramework(&form);
  assert(clears==(enabled?0:3));assert(chrome==1&&restores==1);
  assert(attribute==(KEYHELP_TEXT|KEYHELP_BACKGROUND));
  assert(cursor_x==0&&cursor_y==21);
  assert(mModernCursorColumn==80&&mModernCursorRow==23);
 }
 ready_error=1;clears=chrome=restores=0;PrintFramework(&form);
 assert(clears==3&&chrome==1&&restores==0);
 ready_error=0;gClassOfVfr=0;clears=chrome=restores=0;PrintFramework(&form);
 assert(clears==1&&chrome==0&&restores==0);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='native-backdrop-frame-') as tmp:
    p=Path(tmp)
    (p/'test.c').write_text(stub+body+main)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS actual PrintFramework: artwork skips opaque preclears; OFF/failure/non-platform paths and final native console/cursor state preserved')
