#!/usr/bin/env python3
"""Execute real artwork and page-base C with sanitizer/fault/pixel checks.
SPDX-License-Identifier: BSD-2-Clause-Patent
"""
from pathlib import Path
import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--page-source', type=Path, help='legacy page source for RED regression run')
p.add_argument('--legacy-header-clear', action='store_true', help='negative control: reproduce unconditional header clear')
a = p.parse_args()
STUB = r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define EFIAPI
#define EFI_SUCCESS 0
typedef int RETURN_STATUS;
#define IN
#define STATIC static
#define CONST const
#define VOID void
#define MODERN_SETUP_NOINLINE
#define TRUE 1
#define FALSE 0
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define EFI_ERROR(x) ((x) != 0)
typedef size_t UINTN;
typedef uint8_t UINT8;
typedef uint16_t CHAR16;
typedef int BOOLEAN;
typedef int EFI_STATUS;
typedef int SETUP_FOCUS;
typedef enum {PageDashboard,PageCatalog,PageSystemInfo,PageBoot,PageDevices,
 PageSecurity,PageFirmware,PageDiagnostics,PageManagement,PagePower,
 PagePerformance,PageQuickSettings,PageServerInventory,PagePreferences,PageExit} SETUP_PAGE;
typedef struct { UINT8 Blue, Green, Red, Reserved; } EFI_GRAPHICS_OUTPUT_BLT_PIXEL;
typedef struct { UINTN Width, Height; } MODERN_UI_RENDER_CONTEXT;
typedef struct { UINTN X,Y,Width,Height; } MODERN_UI_RECT;
typedef struct { EFI_GRAPHICS_OUTPUT_BLT_PIXEL Background; } MODERN_UI_THEME;
static int fail_alloc, fail_restore, calls, frees, clears, fills, surface_test;
static EFI_GRAPHICS_OUTPUT_BLT_PIXEL captured[100], screen[100];
static void *AllocatePool(UINTN n) { return fail_alloc ? NULL : malloc(n); }
static void FreePool(void *p) { frees++; free(p); }
static MODERN_UI_RECT ModernSetupDashboardContentRect(MODERN_UI_RENDER_CONTEXT *ui) {
 (void)ui;return (MODERN_UI_RECT){2,3,4,2};
}
static void ModernUiClear(MODERN_UI_RENDER_CONTEXT *ui, EFI_GRAPHICS_OUTPUT_BLT_PIXEL c) {
 clears++;for(UINTN i=0;i<ui->Width*ui->Height;i++) screen[i]=c;
}
static void ModernUiFillRect(MODERN_UI_RENDER_CONTEXT *ui, MODERN_UI_RECT r,
 EFI_GRAPHICS_OUTPUT_BLT_PIXEL c) {
 fills++;
 for(UINTN y=r.Y;y<r.Y+r.Height;y++) for(UINTN x=r.X;x<r.X+r.Width;x++) {
  assert(x<ui->Width && y<ui->Height);
  // Chrome clearing must never touch the artwork, even transiently.
  screen[y*ui->Width+x]=c;
 }
}
static EFI_STATUS ModernUiRestoreRect(MODERN_UI_RENDER_CONTEXT *ui, MODERN_UI_RECT r,
 const EFI_GRAPHICS_OUTPUT_BLT_PIXEL *v) {
 calls++;assert(r.Width*r.Height<=100);
 memcpy(captured,v,r.Width*r.Height*sizeof(*v));
 if(!fail_restore) for(UINTN y=0;y<r.Height;y++) for(UINTN x=0;x<r.Width;x++)
  screen[(y+r.Y)*ui->Width+x+r.X]=v[y*r.Width+x];
 return fail_restore;
}
'''
MAIN = r'''
int main(void) {
 MODERN_UI_RENDER_CONTEXT ui={10,10};
 MODERN_UI_THEME theme={{0,0,0,0}};
 MODERN_UI_RECT r={2,3,4,2};
#if MODERN_SETUP_HOME_BACKGROUND
 assert(ModernSetupDrawHomeBackground(&ui,r));
 for(int i=0;i<8;i++) assert(captured[i].Blue==(i*255+15)/31);
 r.Width=8;r.Height=2;
 assert(ModernSetupDrawHomeBackground(&ui,r));
 assert(captured[0].Blue==0 && captured[7].Blue==25);
 assert(captured[1].Blue>0 && captured[1].Blue<8); // actual interpolation
 r.Width=1;r.Height=4;
 assert(ModernSetupDrawHomeBackground(&ui,r));
 assert(captured[0].Blue==8 && captured[3].Blue==41);
 assert(captured[1].Blue>8 && captured[1].Blue<41);
 int old=calls;
 fail_alloc=1;assert(!ModernSetupDrawHomeBackground(&ui,r));assert(calls==old);
 fail_alloc=0;fail_restore=1;
 assert(!ModernSetupDrawHomeBackground(&ui,r));assert(frees==calls);
 fail_restore=0;
 assert(!ModernSetupDrawHomeBackground(NULL,r));
 r.Width=0;assert(!ModernSetupDrawHomeBackground(&ui,r));
 r.Width=101;assert(!ModernSetupDrawHomeBackground(&ui,r));
 r.Width=1;r.X=(UINTN)-1;assert(!ModernSetupDrawHomeBackground(&ui,r));
 r.X=0;r.Width=4097;ui.Width=10000;
 assert(!ModernSetupDrawHomeBackground(&ui,r));ui.Width=10;
 // Execute the actual page dispatcher, not only a new helper. Both repaint
 // and cursor save-under use the same shadow-canvas restore contract.
 clears=0;fills=0;
 EFI_GRAPHICS_OUTPUT_BLT_PIXEL pristine[100];
 assert(ModernSetupDrawHomeBackground(&ui,(MODERN_UI_RECT){0,0,10,10}));
 memcpy(pristine,screen,sizeof(pristine));
 for(int page=PageDashboard;page<=PageExit;page++) for(int n=0;n<8;n++) {
  memset(screen,255,sizeof(screen));
  ModernSetupDrawCurrentPage(&ui,&theme,page,n%2,n,0,0,0,0,NULL);
  assert(clears==0 && fills==0);
  assert(memcmp(pristine,screen,sizeof(pristine))==0);
 }
 fail_alloc=1;assert(!ModernSetupDrawPageBase(&ui,&theme,PageDashboard));assert(clears==1);
 fail_alloc=0;fail_restore=1;
 assert(!ModernSetupDrawPageBase(&ui,&theme,PageDashboard));assert(clears==2);
 fail_restore=0;
 assert(ModernSetupDrawPageBase(&ui,&theme,PageBoot));assert(clears==2);
 ModernSetupReleaseHomeBackground();
#else
 assert(!ModernSetupDrawHomeBackground(&ui,r));assert(calls==0 && frees==0);
 assert(!ModernSetupDrawPageBase(&ui,&theme,PageDashboard));assert(clears==1);
 assert(!ModernSetupDrawPageBase(&ui,&theme,PageBoot));assert(clears==2);
 assert(calls==0 && fills==0);
#endif
 // Surfaces never sample previous compositing/text, including overlapping fills.
 surface_test=1;
 r=(MODERN_UI_RECT){2,3,4,2};
 EFI_GRAPHICS_OUTPUT_BLT_PIXEL color={20,40,60,0};
#if MODERN_SETUP_HOME_BACKGROUND
 assert(ModernSetupDrawHomeBackground(&ui,r));
 for(int n=0;n<32;n++) {
  memset(screen,255,sizeof(screen)); // poison rendered text/previous panels
  ModernSetupDrawHomeSurface(&ui,r,color,140);
  for(int i=0;i<8;i++) {
   EFI_GRAPHICS_OUTPUT_BLT_PIXEL px=screen[(3+i/4)*10+2+i%4];
   assert(px.Blue==(20*140+((i*255+15)/31)*115+127)/255);
   assert(px.Green==(40*140+127)/255 && px.Red==(60*140+127)/255);
  }
 }
 MODERN_UI_RECT nested={3,3,2,1};
 ModernSetupDrawHomeSurface(&ui,nested,color,180);
 assert(screen[33].Blue==(20*180+8*75+127)/255);
 ModernSetupDrawHomeSurface(&ui,r,color,0);assert(screen[33].Blue==8);
 ModernSetupDrawHomeSurface(&ui,r,color,255);assert(screen[33].Blue==20);
 fail_alloc=1;ModernSetupDrawHomeSurface(&ui,r,color,140);
 assert(screen[33].Blue==20);fail_alloc=0;
 fail_restore=1;ModernSetupDrawHomeSurface(&ui,r,color,140);
 assert(screen[33].Blue==20);fail_restore=0;
 // Same owner after a mode/size change must not index the old cache.
 ui.Width=9;
 ModernSetupDrawHomeSurface(&ui,r,color,140);assert(screen[3*9+3].Blue==20);
 ui.Width=10;
 MODERN_UI_RENDER_CONTEXT foreign={10,10};
 ModernSetupDrawHomeSurface(&foreign,r,color,140);assert(screen[33].Blue==20);
 ModernSetupDrawHomeSurface(&ui,(MODERN_UI_RECT){0,0,2,2},color,140);
 assert(screen[0].Blue==20); // outside cached crop uses the original solid fill
 assert(Native_ModernUiBackdropPrepare(&foreign,r));
 fail_alloc=1; // reuse must not resample/allocate, even after canvas poisoning
 memset(screen,255,sizeof(screen));
 assert(Native_ModernUiBackdropRedraw(&foreign,r));assert(screen[33].Blue==8);
 foreign.Width=9;
 assert(!Native_ModernUiBackdropRedraw(&foreign,r)); // changed mode discards cache
 assert(Native_mHomePristine==NULL);
 fail_alloc=0;foreign.Width=10;
 assert(Native_ModernUiBackdropRedraw(&foreign,r));
 fail_restore=1;assert(!Native_ModernUiBackdropRedraw(&foreign,r));
 assert(Native_mHomePristine==NULL);fail_restore=0;
 assert(Native_ModernUiBackdropRedraw(&foreign,r));
 ModernSetupReleaseHomeBackground();assert(mHomePristine==NULL);
 Native_ModernUiBackdropSurface(&foreign,r,color,0);
 assert(screen[33].Blue==8); // native cache survives app cleanup/unload
 assert(Native_mHomePristine!=NULL);
 Native_ModernUiBackdropDestructor();assert(Native_mHomePristine==NULL);
 ModernSetupReleaseHomeBackground(); // idempotent cleanup
#endif
 int surface_calls=calls;
 ModernSetupDrawHomeSurface(&ui,r,color,140); // disabled or released: original opaque fill
 assert(calls==surface_calls && screen[33].Blue==20 && screen[33].Red==60);
 // Existing native API always clears; the additive API never does.
 MODERN_UI_PAGE_MODEL model=0;
 int before=clears;
 assert(ModernUiEngineDrawPage(&ui,&model,&theme)==0);
 assert(clears==before+1);
 assert(ModernUiEngineDrawPageOnBackground(&ui,&model,&theme)==0);
 assert(clears==before+1);
 assert(ModernUiEngineDrawPageOnBackground(NULL,&model,&theme)!=0);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='home-background-') as tmp:
    path=Path(tmp)
    shared=(ROOT/'Library/ModernUiEngineLib/ModernUiBackdrop.c').read_text()
    shared=re.sub(r'^#include(?! <ModernSetupHomeBackground).*$', '', shared, flags=re.M)
    adapter=(ROOT/'Application/ModernSetupApp/ModernSetupAppBackground.c').read_text()
    adapter=re.sub(r'^#include.*$', '', adapter, flags=re.M)
    # Two independently linked-image instances of the exact C, simulated by
    # symbol namespacing. Static artwork data comes from the same source bytes.
    symbols=['mHomePristine','mHomeRegion','mHomeOwner','mHomeWidth','mHomeHeight',
             'HomePixel','HomeInterpolate','mHomeRgb565','ModernUiBackdropPrepare',
             'ModernUiBackdropSurface','ModernUiBackdropRelease','ModernUiBackdropRedraw','ModernUiBackdropDestructor']
    native='\n'.join('#define '+x+' Native_'+x for x in symbols)+'\n'+shared+'\n'+\
           '\n'.join('#undef '+x for x in symbols)
    (path/'background.c').write_text('#include "ModernSetupAppInternal.h"\n'+shared+'\n'+native+'\n'+adapter)
    page=(a.page_source or ROOT/'Application/ModernSetupApp/ModernSetupAppPages.c').read_text()
    page=page[page.index('VOID\nMODERN_SETUP_NOINLINE\nModernSetupDrawCurrentPage'):]
    names=set(re.findall(r'\b((?:ModernSetupDraw|Draw)\w+)\s*\(',page))
    mocks='\n'.join(f'#define {name}(...) ((void)0)' for name in names-{'ModernSetupDrawCurrentPage','ModernSetupDrawPageBase','ModernSetupDrawHeader'})
    # Execute the real engine clear decision and public wrappers beneath the
    # real App header dispatch. Header drawing primitives are outside this
    # pixel-buffer test; the firmware build compiles their complete bodies.
    engine=(ROOT/'Library/ModernUiEngineLib/ModernUiEngineLib.c').read_text()
    decision=engine[engine.index('  if ((Context == NULL)', engine.index('DrawPageInternal (')):]
    decision=decision[:decision.index('  Status = DrawHeaderGradient')]
    wrappers=engine[engine.index('EFI_STATUS\nEFIAPI\nModernUiEngineDrawPage ('):]
    wrappers=wrappers[:wrappers.index('/**')]
    chrome=(ROOT/'Application/ModernSetupApp/ModernSetupAppChrome.c').read_text()
    dispatch=chrome[chrome.index('  if (PreserveBackground)',chrome.index('ModernSetupDrawHeader (')):]
    dispatch=dispatch[:dispatch.index('  // Frozen build identity')]
    if a.legacy_header_clear:
        dispatch=dispatch.replace('if (PreserveBackground)', 'if (FALSE)')
    shim='''
#define EFIAPI
#define EFI_SUCCESS 0
#define EFI_INVALID_PARAMETER 2
typedef int MODERN_UI_PAGE_MODEL;
static void *mClockContext;
static EFI_STATUS DrawPageInternal(MODERN_UI_RENDER_CONTEXT *Context,
 const MODERN_UI_PAGE_MODEL *Model, const MODERN_UI_THEME *Theme,
 BOOLEAN PreserveBackground) { EFI_STATUS Status;
'''+decision+'''return EFI_SUCCESS; }\n'''+wrappers+'''
static void ModernSetupDrawHeader(MODERN_UI_RENDER_CONTEXT *Ui,
 const MODERN_UI_THEME *Theme, BOOLEAN PreserveBackground) {
 MODERN_UI_PAGE_MODEL PageModel=0;
'''+dispatch+'}\n'
    (path/'ModernSetupAppInternal.h').write_text(STUB.replace('static void ModernUiClear(', 'static EFI_STATUS ModernUiClear(').replace('screen[i]=c;\n}', 'screen[i]=c;\n return 0;\n}'))
    (path/'test.c').write_text('#include "background.c"\n'+shim+mocks+'\n'+page+'\n'+MAIN)
    for enabled in (0,1):
        if enabled:
            (path/'ModernSetupHomeBackground.generated.h').write_text(
                '#define HOME_BACKGROUND_WIDTH 4\n#define HOME_BACKGROUND_HEIGHT 2\n'
                'STATIC CONST UINT8 mHomeRgb565[]={0,0,1,0,2,0,3,0,4,0,5,0,6,0,7,0};\n')
        binary=path/f'test-{enabled}'
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Wno-unused-function',
                        '-Wno-unused-parameter','-Wno-unused-but-set-variable','-g',
                        '-fsanitize=address,undefined',f'-DMODERN_SETUP_HOME_BACKGROUND={enabled}',
                        '-I',str(path),str(path/'test.c'),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
        print(f'PASS real C enabled={enabled}: RGB565/bilinear/crop/bounds/faults/page repaint; pristine surface invariance, alpha endpoints, overlap, owner and OFF fallback',flush=True)
    source,output=path/'source.png',path/'asset.h'
    Image.new('RGB',(2400,1200),(10,20,30)).save(source)
    before=source.read_bytes()
    command=[sys.executable,str(ROOT/'Scripts/generate-home-background.py'),str(source),str(output)]
    subprocess.run(command,check=True)
    first=output.read_bytes(),output.stat().st_mtime_ns
    subprocess.run(command,check=True)
    assert first==(output.read_bytes(),output.stat().st_mtime_ns)
    assert source.read_bytes()==before
    text=output.read_text()
    assert '#define HOME_BACKGROUND_WIDTH 1280' in text
    assert '#define HOME_BACKGROUND_HEIGHT 640' in text
    assert 'mHomeRgb565' in text and 'mHomePalette' not in text
    encoded=re.findall(r'\\([0-7]{3})',text)
    assert len(encoded)==1280*640*2
    assert [int(x,8) for x in encoded[:2]]==[163,8]
    print('PASS converter: RGB565/bounded/deterministic/original preserved')
