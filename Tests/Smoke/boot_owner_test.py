#!/usr/bin/env python3
"""Host regression for the actual lazy-loader C; no firmware execution claimed."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    loader = ROOT / 'Library/ModernBootMaintenanceLoaderLib'
    owner = ROOT / 'Universal/ModernBootMaintenanceDxe'
    source = (loader / 'ModernBootMaintenanceLoaderLib.c').read_text()
    inf = (owner / 'ModernBootMaintenanceDxe.inf').read_text()
    # Upstream NULL library supports DXE_DRIVER, not UEFI_DRIVER. FALSE prevents
    # automatic DXE dispatch while LoadImage/StartImage explicitly starts it.
    assert re.search(r'MODULE_TYPE\s*=\s*DXE_DRIVER', inf)
    assert re.search(r'\[Depex\]\s*FALSE', inf)
    assert 'UNLOAD_IMAGE' not in inf
    assert 'DESTRUCTOR' not in (loader / 'ModernBootMaintenanceLoaderLib.inf').read_text()
    guid = re.search(r'FILE_GUID\s*=\s*([^\s]+)', inf)[1]
    import uuid
    values = [int(x, 16) for x in re.findall(r'0x[0-9a-f]+', source.split('mOwnerGuid =')[1].split(';')[0])]
    assert uuid.UUID(guid).fields[:3] == tuple(values[:3])
    assert uuid.UUID(guid).bytes[8:] == bytes(values[3:])
    # Verify the baseline's entry-failure lifecycle, not an invented cleanup:
    # native constructors run before entry; a failed entry runs destructors.
    entry_path = ROOT / 'External/edk2/MdePkg/Library/UefiDriverEntryPoint/DriverEntryPoint.c'
    if entry_path.exists():
        from smoke_validate import extract_c_function_body
        entry = extract_c_function_body(entry_path.read_text(), '_ModuleEntryPoint')
        assert entry.index('ProcessLibraryConstructorList (') < entry.index('ProcessModuleEntryPointList (')
        assert re.search(r'if \(EFI_ERROR \(Status\)\)\s*\{\s*ProcessLibraryDestructorList', entry)
    assert 'UefiLib' in (loader / 'ModernBootMaintenanceLoaderLib.inf').read_text()
    assert '#include <Library/UefiLib.h>' in source
    owner_source = (owner / 'ModernBootMaintenanceDxe.c').read_text()
    assert source.split('mOwnerGuid =')[1].split(';')[0] == owner_source.split('mOwnerGuid =')[1].split(';')[0]
    prelude = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#define STATIC static
#define EFIAPI
#define IN
#define VOID void
#define FALSE 0
#define EFI_NATIVE_INTERFACE 0
#define EFI_SUCCESS 0
#define EFI_NOT_READY 1
#define EFI_NOT_FOUND 2
#define EFI_OUT_OF_RESOURCES 3
#define EFI_ERROR(s) ((s)!=0)
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define DEBUG(x) ((void)0)
typedef int EFI_STATUS;
typedef void *EFI_HANDLE;
typedef size_t UINTN;
typedef unsigned char UINT8;
typedef struct { uint32_t a; uint16_t b,c; uint8_t d[8]; } EFI_GUID;
typedef struct { void *DeviceHandle; } EFI_LOADED_IMAGE_PROTOCOL;
typedef struct { int dummy; } EFI_DEVICE_PATH_PROTOCOL;
typedef EFI_DEVICE_PATH_PROTOCOL MEDIA_FW_VOL_FILEPATH_DEVICE_PATH;
typedef struct { void *Mode; } CONSOLE;
typedef struct { CONSOLE *ConOut; } EFI_SYSTEM_TABLE;
static EFI_GUID gEfiHiiDatabaseProtocolGuid, gEfiHiiStringProtocolGuid,
 gEfiHiiConfigRoutingProtocolGuid, gEfiFormBrowser2ProtocolGuid,
 gEdkiiFormBrowserEx2ProtocolGuid, gEfiDevicePathToTextProtocolGuid,
 gEfiLoadedImageProtocolGuid;
static int ready, missing, loads, starts, unloads, allocfail, pathfail, handlefail, loadfail, startfail, nomarker, installfail;
static int owneralive, entryfailed, ownerprobes;
static EFI_LOADED_IMAGE_PROTOCOL image = {(void *)1};
static EFI_DEVICE_PATH_PROTOCOL path;
static EFI_GUID *marker;
static EFI_STATUS locate(EFI_GUID *g, void *r, void **out) {
 (void)r; *out = &image; return g == marker ? (ready ? 0 : 2) : missing;
}
static EFI_STATUS handle(EFI_HANDLE h, EFI_GUID *g, void **out) {
 (void)g; *out=&image;
 if (h==(void *)2) { ownerprobes++; return owneralive ? 0 : EFI_NOT_FOUND; }
 return handlefail;
}
static EFI_STATUS load(int b, EFI_HANDLE h, void *p, void *s, UINTN n, EFI_HANDLE *out) {
 (void)b;(void)h;(void)p;(void)s;(void)n; loads++; owneralive=1; *out=(void *)2; return loadfail;
}
static EFI_STATUS start(EFI_HANDLE h, void *a, void *b) {
 (void)h;(void)a;(void)b; starts++; if (startfail && entryfailed) owneralive=0;
 if (!startfail && !nomarker) { ready=1; }
 return startfail;
}
static EFI_STATUS unload(EFI_HANDLE h) { assert(h==(void *)2 && owneralive); owneralive=0; unloads++; return 0; }
static EFI_STATUS install(EFI_HANDLE *h, EFI_GUID *g, int t, void *p) {
 (void)h;(void)g;(void)t;(void)p; return installfail;
}
static struct { EFI_STATUS (*LocateProtocol)(EFI_GUID *,void *,void **);
 EFI_STATUS (*HandleProtocol)(EFI_HANDLE,EFI_GUID *,void **);
 EFI_STATUS (*LoadImage)(int,EFI_HANDLE,void *,void *,UINTN,EFI_HANDLE *);
 EFI_STATUS (*StartImage)(EFI_HANDLE,void *,void *);
 EFI_STATUS (*UnloadImage)(EFI_HANDLE);
 EFI_STATUS (*InstallProtocolInterface)(EFI_HANDLE *,EFI_GUID *,int,void *);
} bs={locate,handle,load,start,unload,install}, *gBS=&bs;
static void *DevicePathFromHandle(void *h) { (void)h; return pathfail ? NULL : &path; }
static void EfiInitializeFwVolDevicepathNode(void *p, EFI_GUID *g) { (void)p; assert(g==marker); }
static void *AppendDevicePathNode(void *a, void *b) { (void)a;(void)b; return allocfail ? NULL : malloc(1); }
#define FreePool free
'''
    # Keep actual production definitions; rename the second TU's static GUID.
    code = prelude + re.sub(r'^#include.*$', '', source, flags=re.M)
    code += re.sub(r'^#include.*$', '', owner_source, flags=re.M).replace('mOwnerGuid', 'mDriverOwnerGuid')
    code += r'''
int main(void) {
 CONSOLE console={(void *)1}; EFI_SYSTEM_TABLE st={&console}; marker=&mOwnerGuid;
 assert(ModernBootMaintenanceEnsureOwner((void *)1,&st)==0);
 assert(loads==1 && starts==1 && unloads==0);
 assert(ModernBootMaintenanceEnsureOwner((void *)3,&st)==0);
 assert(loads==1 && starts==1 && unloads==0);
 puts("PASS first load and nested/reopened app reuse resident owner");
 ready=0; loads=starts=0; st.ConOut=NULL;
 assert(ModernBootMaintenanceEnsureOwner((void *)1,&st)==EFI_NOT_READY);
 assert(ModernBootMaintenanceLoaderConstructor((void *)1,&st)==0);
 st.ConOut=&console; missing=2;
 assert(ModernBootMaintenanceLoaderConstructor((void *)1,&st)==0 && loads==0);
 missing=0; handlefail=2; assert(ModernBootMaintenanceEnsureOwner((void *)1,&st)==2); handlefail=0;
 pathfail=1; assert(ModernBootMaintenanceEnsureOwner((void *)1,&st)==2); pathfail=0;
 allocfail=1; assert(ModernBootMaintenanceEnsureOwner((void *)1,&st)==3); allocfail=0;
 loadfail=2; assert(ModernBootMaintenanceEnsureOwner((void *)1,&st)==2 && unloads==1 && starts==0);
 loadfail=0; startfail=EFI_OUT_OF_RESOURCES;
 /* Core fails before entry: loaded image survives and must be unloaded. */
 assert(ModernBootMaintenanceEnsureOwner((void *)1,&st)==EFI_OUT_OF_RESOURCES);
 assert(unloads==2 && !owneralive && ownerprobes==1);
 /* Repeated failures cannot accumulate surviving images. */
 assert(ModernBootMaintenanceEnsureOwner((void *)1,&st)==EFI_OUT_OF_RESOURCES);
 assert(unloads==3 && !owneralive && ownerprobes==2);
 /* Entry failed: Core already freed the image; never unload it twice. */
 entryfailed=1; startfail=EFI_NOT_FOUND;
 assert(ModernBootMaintenanceEnsureOwner((void *)1,&st)==EFI_NOT_FOUND);
 assert(unloads==3 && !owneralive && ownerprobes==3);
 entryfailed=0; startfail=0;
 assert(ModernBootMaintenanceEnsureOwner((void *)1,&st)==0);
 assert(owneralive && ready && unloads==3 && ownerprobes==3);
 ready=0; nomarker=1; assert(ModernBootMaintenanceEnsureOwner((void *)1,&st)==2 && unloads==3);
 puts("PASS pre-entry start failure cleanup, repeated failure, no double unload after entry failure, successful retry");
 installfail=2; assert(ModernBootMaintenanceEntryPoint((void *)2,&st)==2);
 installfail=0; assert(ModernBootMaintenanceEntryPoint((void *)2,&st)==0);
 puts("PASS dependency/load/start/marker failures; constructor fails soft; successful owner never unloaded");
 return 0;
}
'''
    with tempfile.TemporaryDirectory() as directory:
        p = Path(directory)
        (p / 'test.c').write_text(code)
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', str(p / 'test.c'), '-o', str(p / 'test')], check=True)
        subprocess.run([str(p / 'test')], check=True)
    print('PASS owner INF/DEPEX and FV-image/marker GUID identity')


if __name__ == '__main__':
    main()
