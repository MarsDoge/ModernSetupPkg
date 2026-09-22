#!/usr/bin/env python3
"""Run actual review wrapping/redaction C with deterministic font metrics.

This validates line integrity and bounds, not GOP appearance or page input.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

PRELUDE = r'''
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef uint16_t CHAR16;
typedef size_t UINTN;
typedef int BOOLEAN;
#define STATIC static
#define CONST const
#define VOID void
#define OPTIONAL
#define TRUE 1
#define FALSE 0
#define MODERN_SETUP_REVIEW_REDACTED 1
#define MODERN_SETUP_REVIEW_OPAQUE 2
#define MODERN_SETUP_REVIEW_BASELINE_KNOWN 4
typedef struct { unsigned Flags; const CHAR16 *Path,*OldValue,*NewValue; } MODERN_SETUP_REVIEW_ITEM;
typedef struct { UINTN ItemCount; MODERN_SETUP_REVIEW_ITEM *Items; } MODERN_SETUP_REVIEW_SNAPSHOT;
static void ZeroMem(void *p, UINTN n) { memset(p,0,n); }
/* Fixed metrics deliberately distinguish Latin and CJK cells. */
static UINTN ModernUiMeasureText(const CHAR16 *s) {
 UINTN w=0; while (*s) { w += *s++ < 128 ? 8 : 16; } return w;
}
static UINTN Len(const CHAR16 *s) { UINTN n=0; while(s[n]) n++;return n; }
'''
TEST = r'''
int main(void) {
 CHAR16 old[601],newv[401],path[201],outOld[601]={0},outNew[401]={0},outPath[201]={0};
 for(UINTN i=0;i<600;i++)old[i]=(i%2)?'a':0x4e2d;old[600]=0;
 for(UINTN i=0;i<400;i++)newv[i]=(i%2)?'b':0x6587;newv[400]=0;
 for(UINTN i=0;i<200;i++)path[i]='P';path[200]=0;
 MODERN_SETUP_REVIEW_ITEM item={MODERN_SETUP_REVIEW_BASELINE_KNOWN,path,old,newv};
 MODERN_SETUP_REVIEW_SNAPSHOT snapshot={1,&item};
 /* Narrowest supported panel has Width 552; also exercise 800/1920 widths. */
 UINTN widths[]={552,712,1080};
 for(UINTN k=0;k<3;k++) {
  UINTN count=ReviewMakeLines(&snapshot,TRUE,widths[k],NULL);
  assert(count>11); REVIEW_LINE *lines=calloc(count,sizeof(*lines));assert(lines);
  assert(ReviewMakeLines(&snapshot,TRUE,widths[k],lines)==count);
  UINTN a=0,b=0,c=0;
  for(UINTN i=0;i<count;i++) {
   assert(Len(lines[i].Left)<REVIEW_LINE_CHARS && Len(lines[i].Right)<REVIEW_LINE_CHARS);
   UINTN width=lines[i].Path?widths[k]:(widths[k]-40)/2;
   assert(ModernUiMeasureText(lines[i].Left)<=width);
   assert(ModernUiMeasureText(lines[i].Right)<=width);
   if(lines[i].Path){UINTN n=Len(lines[i].Left);memcpy(outPath+c,lines[i].Left,n*2);c+=n;}
   else {UINTN n=Len(lines[i].Left);memcpy(outOld+a,lines[i].Left,n*2);a+=n;
         n=Len(lines[i].Right);memcpy(outNew+b,lines[i].Right,n*2);b+=n;}
  }
  assert(a==600 && b==400 && c==200);
  assert(memcmp(outOld,old,sizeof(old))==0 && memcmp(outNew,newv,sizeof(newv))==0);
  assert(memcmp(outPath,path,sizeof(path))==0);free(lines);
 }
 CHAR16 controls[]={0x202e,0x2066,0xfff1,'A',0},line[REVIEW_LINE_CHARS];const CHAR16 *p=controls;
 ReviewWrap(&p,line,100);assert(line[0]==' '&&line[1]==' '&&line[2]==' '&&line[3]=='A'&&*p==0);
 item.OldValue=(void*)1;item.NewValue=(void*)1;
 unsigned flags[]={0,MODERN_SETUP_REVIEW_REDACTED,MODERN_SETUP_REVIEW_OPAQUE};
 for(UINTN i=0;i<3;i++){item.Flags=flags[i];assert(ReviewValidText(ReviewValue(&item,TRUE,TRUE)));assert(ReviewValidText(ReviewValue(&item,FALSE,FALSE)));}
 CHAR16 *limit=calloc(REVIEW_TEXT_LIMIT+2,2);assert(limit);
 for(UINTN i=0;i<REVIEW_TEXT_LIMIT;i++)limit[i]='x';assert(ReviewValidText(limit));
 limit[REVIEW_TEXT_LIMIT]='x';assert(!ReviewValidText(limit));free(limit);
 puts("PASS: review long values preserved, narrow widths bounded, controls sanitized, secrets redacted before access");
 return 0;
}
'''

class ReviewLayoutTest(unittest.TestCase):
    def test_shipped_helpers(self):
        text = (ROOT / 'Universal/ModernDisplayEngineDxe/SaveReview.c').read_text()
        helpers = text[text.index('#define REVIEW_TEXT_LIMIT'):text.index('STATIC BOOLEAN\nReviewHit')]
        helpers = '\n'.join(line for line in helpers.splitlines() if not line.startswith('STATIC EFI_GUID'))
        with tempfile.TemporaryDirectory(prefix='modern-review-layout-') as td:
            source = Path(td) / 'test.c'
            source.write_text(PRELUDE + helpers + TEST)
            exe = Path(td) / 'test'
            subprocess.run(['gcc','-std=c11','-fshort-wchar','-fsanitize=address,undefined','-g',str(source),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

if __name__ == '__main__':
    unittest.main()
