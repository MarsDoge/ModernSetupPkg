#!/usr/bin/env python3
"""Real-C review geometry/input and native USB binding regression.

usb-tablet is not a boot-protocol mouse. UsbMouseAbsolutePointerDxe exposes
EFI absolute coordinates by integrating *relative* usb-mouse boot reports;
its name does not mean it accepts arbitrary USB absolute HID tablets.
Descriptor fixtures below match QEMU v8.2.2 hw/usb/dev-hid.c.
No QEMU acceptance or complete ReviewChanges event-loop coverage is claimed.
"""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SDK = Path(os.environ.get('SAVE_REVIEW_EDK2', str(ROOT / 'External/edk2')))


def function(text, name):
    start = text.index('\n' + name + ' (')
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start + 1:end]


def main():
    review = (ROOT / 'Universal/ModernDisplayEngineDxe/SaveReview.c').read_text()
    driver = (SDK / 'MdeModulePkg/Bus/Usb/UsbMouseAbsolutePointerDxe/UsbMouseAbsolutePointer.c').read_text()
    adapter = ROOT / 'Library/ModernUiInputLib/ModernUiInputLib.c'
    # Compile the actual production adapter plus exact production function bodies.
    source = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#undef NULL
#include <Uefi.h>
#include <Protocol/UsbIo.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <ModernUi/ModernUiInput.h>
#define CLASS_HID 3
#define SUBCLASS_BOOT 1
#define PROTOCOL_MOUSE 2
typedef struct { UINTN X, Y, Width, Height; } MODERN_UI_RECT;
EFI_BOOT_SERVICES *gBS;
EFI_SYSTEM_TABLE *gST;
EFI_GUID gEfiSimpleTextInputExProtocolGuid;
EFI_GUID gEfiAbsolutePointerProtocolGuid;
VOID * EFIAPI ZeroMem(VOID *p, UINTN n) { return memset(p, 0, n); }
UINT64 EFIAPI MultU64x64(UINT64 a, UINT64 b) { return a*b; }
UINT64 EFIAPI DivU64x64Remainder(UINT64 a, UINT64 b, UINT64 *r) {
  if (r) *r=a%b; return a/b;
}
''' + '\nBOOLEAN\n' + function(driver, 'IsUsbMouse') + '\nBOOLEAN\n' + function(review, 'ReviewHit') + '\nUINTN\n' + function(review, 'ReviewCoordinate') + '\n#include "' + str(adapter) + '"\n' + r'''
static EFI_USB_INTERFACE_DESCRIPTOR descriptor;
static EFI_STATUS EFIAPI get_descriptor(EFI_USB_IO_PROTOCOL *p, EFI_USB_INTERFACE_DESCRIPTOR *d) {
  (void)p; *d=descriptor; return EFI_SUCCESS;
}
static EFI_ABSOLUTE_POINTER_PROTOCOL pointer;
static EFI_ABSOLUTE_POINTER_MODE mode;
static EFI_SIMPLE_TEXT_INPUT_PROTOCOL keyboard;
static int pending, waits, reads, locate_calls;
static EFI_STATUS EFIAPI locate(EFI_GUID *g, VOID *r, VOID **p) {
  (void)r; assert(g == &gEfiAbsolutePointerProtocolGuid);
  locate_calls++; *p=&pointer; return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI handle(EFI_HANDLE h, EFI_GUID *g, VOID **p) {
  (void)h; (void)g; *p=NULL; return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI state(EFI_ABSOLUTE_POINTER_PROTOCOL *p, EFI_ABSOLUTE_POINTER_STATE *s) {
  assert(p==&pointer); reads++;
  if (!pending) return EFI_NOT_READY;
  pending=0; memset(s,0,sizeof(*s)); s->CurrentX=37225; s->CurrentY=52842;
  s->ActiveButtons=EFI_ABSP_TouchActive; return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI wait_event(UINTN n, EFI_EVENT *events, UINTN *index) {
  waits++; assert(n==2); assert(events[0]==keyboard.WaitForKey);
  assert(events[1]==pointer.WaitForInput); *index=0; return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI key(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p, EFI_INPUT_KEY *k) {
  assert(p==&keyboard); k->ScanCode=SCAN_ESC; k->UnicodeChar=0; return EFI_SUCCESS;
}
int main(void) {
  EFI_USB_IO_PROTOCOL usb={0}; EFI_BOOT_SERVICES bs={0}; EFI_SYSTEM_TABLE st={0};
  MODERN_UI_INPUT_CONTEXT input; MODERN_UI_INPUT_EVENT event;
  MODERN_UI_RECT Panel, Buttons[4]; UINTN I; UINTN width=1920, height=1080;
  usb.UsbGetInterfaceDescriptor=get_descriptor;
  descriptor.InterfaceClass=3; descriptor.InterfaceSubClass=0; descriptor.InterfaceProtocol=0;
  assert(!IsUsbMouse(&usb)); /* QEMU usb-tablet: no driver binding */
  descriptor.InterfaceSubClass=1; descriptor.InterfaceProtocol=2;
  assert(IsUsbMouse(&usb)); /* QEMU usb-mouse: EFI absolute via native driver */
  puts("PASS native USB binding: tablet rejected, boot mouse accepted");
  gBS=&bs; gST=&st; st.ConIn=&keyboard;
  bs.HandleProtocol=handle; bs.LocateProtocol=locate; bs.WaitForEvent=wait_event;
  pointer.Mode=&mode; pointer.GetState=state; pointer.WaitForInput=(VOID *)2;
  mode.AbsoluteMaxX=65536; mode.AbsoluteMaxY=65536;
  keyboard.WaitForKey=(VOID *)1; keyboard.ReadKeyStroke=key;
  assert(ModernUiInputInit(&input)==EFI_SUCCESS && input.Pointer==&pointer && locate_calls==1);
  pending=1;
  assert(ModernUiReadInput(&input,&event)==EFI_SUCCESS);
  assert(event.Type==ModernUiInputPointer && event.PointerValid && event.PointerPressed && waits==0);
  Panel.Width=MIN(width-48,1120); Panel.Height=MIN(height-48,800);
  Panel.X=(width-Panel.Width)/2; Panel.Y=(height-Panel.Height)/2;
''' + review[review.index('  for (I = 0; I < 4; I++) {', review.index('Status = ModernUiCaptureRect (&Ui, Panel, Saved);')):review.index('  Page = 0;', review.index('Status = ModernUiCaptureRect (&Ui, Panel, Saved);'))] + r'''
  assert(ReviewHit(Buttons[2],1090,870));
  assert(ReviewHit(Buttons[2],ReviewCoordinate(event.PointerX,0,65536,width),
                 ReviewCoordinate(event.PointerY,0,65536,height)));
  assert(!ReviewHit(Buttons[2],Buttons[2].X+Buttons[2].Width,870));
  assert(ReviewCoordinate(0,0,65536,width)==0);
  assert(ReviewCoordinate(65536,0,65536,width)==1919);
  assert(ReviewCoordinate(100,100,100,width)==0);
  assert(ReviewCoordinate(MAX_UINTN,0,MAX_UINT64,width)==1919);
  puts("PASS real adapter pointer report + real review coordinate/button hit at 1090,870");
  assert(ModernUiReadInput(&input,&event)==EFI_SUCCESS);
  assert(event.Type==ModernUiInputEscape && !event.PointerValid && waits==1 && reads==2);
  puts("PASS empty console pointer falls back to keyboard without protocol lifecycle changes");
  return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='review-pointer-') as td:
        c = Path(td) / 'test.c'
        exe = Path(td) / 'test'
        c.write_text(source)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-g', '-fshort-wchar',
                        '-fsanitize=undefined', '-fno-sanitize-recover=all',
                        '-I' + str(SDK / 'MdePkg/Include'),
                        '-I' + str(SDK / 'MdePkg/Include/X64'),
                        '-I' + str(ROOT / 'Include'), str(c), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
