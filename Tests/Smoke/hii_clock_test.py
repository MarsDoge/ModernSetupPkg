#!/usr/bin/env python3
"""Compile the production HII wait loop with mocked Boot Services.

These host tests prove event routing/lifetime, not actual firmware rendering.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def function(text, name):
    start = text.index("UI_EVENT_TYPE\n" + name + " (")
    body = text.index("{", start)
    depth = 1
    end = body + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]

HARNESS = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#undef NULL
#define NULL 0
#define IN
#define TRUE 1
#define NULL_EVENT ((EFI_EVENT)0)
#define MAX_UINTN SIZE_MAX
#define EVT_TIMER 1
#define TimerRelative 0
#define TimerPeriodic 1
#define ONE_SECOND 10000000
#define EFI_ERROR(x) ((x) != 0)
#define ASSERT_EFI_ERROR(x) assert(!EFI_ERROR(x))
typedef int EFI_STATUS;
typedef uintptr_t UINTN;
typedef uint64_t UINT64;
typedef uintptr_t EFI_EVENT;
typedef enum { UIEventKey, UIEventDriver, UIEventTimeOut } UI_EVENT_TYPE;
static struct { EFI_EVENT FormRefreshEvent; } form, *gFormData = &form;
static UINT64 timeout;
static int creates, sets, closes, ticks, calls, wantTicks;
static int failCreate, failSet;
static EFI_EVENT clockEvent, exitEvent, target;
static int live[32];
static UINT64 FormExitTimeout(void *p) { (void)p; return timeout; }
static void ModernDisplayRefreshClock(void) { ticks++; }
static EFI_STATUS Create(int type, int tpl, void *cb, void *ctx, EFI_EVENT *out) {
  (void)type; (void)tpl; (void)cb; (void)ctx;
  creates++;
  if (creates == failCreate) return 1;
  *out = 10 + creates; live[*out] = 1; return 0;
}
static EFI_STATUS Set(EFI_EVENT event, int kind, UINT64 period) {
  sets++; assert(live[event]);
  if (sets == failSet) return 1;
  if (kind == TimerPeriodic) { assert(period == ONE_SECOND); clockEvent = event; }
  else { assert(period == timeout); exitEvent = event; }
  return 0;
}
static EFI_STATUS Close(EFI_EVENT event) {
  assert(live[event]); live[event] = 0; closes++; return 0;
}
static EFI_STATUS Wait(UINTN count, EFI_EVENT *events, UINTN *index) {
  EFI_EVENT desired = (calls++ < wantTicks) ? clockEvent : (target ? target : exitEvent);
  assert(calls < 20 && desired);
  for (UINTN i=0; i<count; i++) if (events[i] == desired) { *index=i; return 0; }
  assert(!"requested event missing from wait set"); return 1;
}
static struct { EFI_STATUS (*CreateEvent)(int,int,void*,void*,EFI_EVENT*);
  EFI_STATUS (*SetTimer)(EFI_EVENT,int,UINT64);
  EFI_STATUS (*CloseEvent)(EFI_EVENT);
  EFI_STATUS (*WaitForEvent)(UINTN,EFI_EVENT*,UINTN*);
} services={Create,Set,Close,Wait}, *gBS=&services;
'''
CASES = r'''
static void run(int refresh, int hasTimeout, EFI_EVENT resultEvent, int nTicks,
                int createFailure, int setFailure, UI_EVENT_TYPE expected) {
  form.FormRefreshEvent = refresh ? 2 : 0;
  timeout = hasTimeout ? 50000000 : 0;
  creates=sets=closes=ticks=calls=0;
  clockEvent=exitEvent=0;
  target=resultEvent; wantTicks=nTicks; failCreate=createFailure; failSet=setFailure;
  assert(UiWaitForEvent(1) == expected);
  assert(ticks == nTicks);
  for (int i=0;i<32;i++) assert(!live[i]);
  // Each timer is armed only once; clock ticks cannot postpone form timeout.
  assert(sets <= (hasTimeout ? 2 : 1));
}
int main(void) {
  run(0,0,1,3,0,0,UIEventKey);
  run(1,0,2,2,0,0,UIEventDriver);
  run(0,1,0,2,0,0,UIEventTimeOut);
  run(1,1,0,2,0,0,UIEventTimeOut);
  run(1,1,2,2,0,0,UIEventDriver);
  run(1,1,1,2,0,0,UIEventKey);
  run(0,0,1,0,1,0,UIEventKey);
  run(0,0,1,0,0,1,UIEventKey);
  run(1,1,2,0,2,0,UIEventDriver);
  run(0,1,1,0,0,2,UIEventKey);
  run(0,1,1,1,1,0,UIEventKey);
  run(0,1,1,1,0,1,UIEventKey);
  puts("PASS 12 HII clock wait cases: ticks, key/refresh/timeout, timer failures, cleanup");
}
'''

if __name__ == "__main__":
    code = function((ROOT / "Universal/ModernDisplayEngineDxe/FormDisplay.c").read_text(), "UiWaitForEvent")
    with tempfile.TemporaryDirectory(prefix="modernsetup-clock-test-") as tmp:
        source = Path(tmp) / "clock.c"
        binary = Path(tmp) / "clock-test"
        source.write_text(HARNESS + code + CASES)
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
