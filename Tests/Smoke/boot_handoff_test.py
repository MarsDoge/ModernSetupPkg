#!/usr/bin/env python3
"""Compile the real App routing functions against mocked DeviceData/cache APIs.

No firmware execution is claimed: this verifies routing, status propagation and
post-handoff cache invalidation. Run with python3 Tests/Smoke/boot_handoff_test.py.
SPDX-License-Identifier: BSD-2-Clause-Patent
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

from smoke_validate import extract_c_function_body

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Application/ModernSetupApp/ModernSetupAppActions.c"


def main():
    """Compile unmodified production function definitions and run assertions."""
    source = SOURCE.read_text()
    match_guid = re.search(r"STATIC CONST EFI_GUID\s+mBootMaintenanceFormSetGuid = .*?;", source)
    assert match_guid is not None, "Boot Maintenance GUID declaration missing"
    guid = match_guid.group() + "\n" + re.search(r"STATIC CONST EFI_GUID\s+mSecureBootFormSetGuid = .*?;", source).group()
    prelude = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#define VOID void
typedef wchar_t CHAR16;
#define IN
#define OUT
#define STATIC static
#define BOOLEAN int
#define TRUE 1
#define FALSE 0
#define CONST const
#define EFI_SUCCESS 0
#define EFI_NOT_FOUND 1
#define EFI_INVALID_PARAMETER 2
#define EFI_OUT_OF_RESOURCES 3
#define EFI_DEVICE_ERROR 4
#define EFI_ERROR(s) ((s) != 0)
#define CopyMem memcpy
#define CompareGuid(a,b) (memcmp((a),(b),sizeof(EFI_GUID)) == 0)
typedef int EFI_STATUS;
typedef size_t UINTN;
typedef void *EFI_HANDLE;
typedef struct { uint32_t a; uint16_t b,c; uint8_t d[8]; } EFI_GUID;
typedef struct { void *HiiHandle; EFI_GUID FormSetGuid; int HasForm; } MODERN_UI_DEVICE_ENTRY;
static MODERN_UI_DEVICE_ENTRY entries[16];
static UINTN count;
static int boot_invalid, device_invalid, provider_invalid, opened, fallback;
static int discovery_status, open_status, fallback_status;
static void ModernSetupInvalidateBootOptionsCache(void) { boot_invalid++; }
static void ModernSetupInvalidateDeviceEntriesCache(void) { device_invalid++; }
static void ModernSetupInvalidateProviderSnapshotCache(void) { provider_invalid++; }
static EFI_STATUS ModernSetupGetCachedDeviceEntries(const MODERN_UI_DEVICE_ENTRY **out, UINTN *n) {
  *out = count ? entries : NULL; *n = count; return discovery_status;
}
static EFI_STATUS ModernUiDeviceDataOpenEntry(const MODERN_UI_DEVICE_ENTRY *entry) {
  assert((uintptr_t)entry < (uintptr_t)entries || (uintptr_t)entry >= (uintptr_t)(entries + 16));
  assert(entry->HiiHandle == (void *)42);
  assert(!boot_invalid && !provider_invalid); /* invalidation must follow handoff */
  opened++; return open_status;
}
static EFI_STATUS ModernSetupLaunchUiAppFallback(EFI_HANDLE image) {
  assert(image); fallback++; return fallback_status;
}
'''
    tests = r'''
static void reset(void) {
  memset(entries, 0, sizeof entries); count = 0;
  boot_invalid = device_invalid = provider_invalid = opened = fallback = 0;
  discovery_status = open_status = fallback_status = 0;
}
static void match(UINTN index) {
  entries[index].HasForm = 1; entries[index].HiiHandle = (void *)42;
  entries[index].FormSetGuid = mBootMaintenanceFormSetGuid;
}
int main(void) {
  reset(); assert(ModernSetupOpenBootConfigurationWithFallback((void *)1, FALSE) == EFI_NOT_FOUND);
  assert(!opened && !fallback && device_invalid == 1);
  reset(); count = 16; match(15); open_status = EFI_DEVICE_ERROR;
  assert(ModernSetupOpenBootConfigurationWithFallback((void *)1, FALSE) == EFI_DEVICE_ERROR);
  assert(opened == 1 && !fallback && boot_invalid == 1 && provider_invalid == 1);
  puts("PASS strict Boot Maintenance missing owner and error-return refresh");
  reset(); assert(ModernSetupOpenBootConfiguration(NULL) == EFI_INVALID_PARAMETER);
  assert(!device_invalid && !opened && !fallback);
  puts("PASS NULL handle preserves caches");
  reset(); assert(ModernSetupOpenBootConfiguration((void *)1) == EFI_SUCCESS);
  assert(fallback == 1 && !opened && device_invalid == 1);
  puts("PASS absent formset uses native fallback");
  reset(); count = 16; match(15);
  assert(ModernSetupOpenBootConfiguration((void *)1) == EFI_SUCCESS);
  assert(opened == 1 && !fallback && boot_invalid == 1 && device_invalid == 2 && provider_invalid == 1);
  puts("PASS GUID match beyond visible rows; all caches invalidated after return");
  reset(); count = 1; match(0); open_status = EFI_DEVICE_ERROR;
  assert(ModernSetupOpenBootConfiguration((void *)1) == EFI_DEVICE_ERROR);
  assert(opened == 1 && !fallback && boot_invalid == 1 && provider_invalid == 1);
  puts("PASS failed matched handoff preserves error, refreshes, never launches another UI");
  reset(); count = 3; match(0); entries[0].HasForm = 0;
  match(1); entries[1].HiiHandle = NULL; match(2); entries[2].FormSetGuid.a++;
  fallback_status = EFI_NOT_FOUND;
  assert(ModernSetupOpenBootConfiguration((void *)1) == EFI_NOT_FOUND);
  assert(fallback == 1 && !opened);
  puts("PASS inventory/null-handle/wrong-GUID rows skipped; fallback error propagated");
  reset(); discovery_status = EFI_OUT_OF_RESOURCES;
  assert(ModernSetupOpenBootConfiguration((void *)1) == EFI_OUT_OF_RESOURCES);
  assert(!opened && !fallback);
  puts("PASS enumeration failure does not open UI");
  reset(); count = 1; match(0); open_status = EFI_DEVICE_ERROR;
  assert(ModernSetupOpenSelectedDeviceEntry(0) == EFI_DEVICE_ERROR);
  assert(boot_invalid == 1 && device_invalid == 1 && provider_invalid == 1);
  puts("PASS generic Devices handoff refreshes boot/device/provider on error");
  reset(); assert(ModernSetupOpenSelectedDeviceEntry(0) == EFI_NOT_FOUND);
  assert(!opened && !boot_invalid && !provider_invalid);
  puts("PASS invalid Devices selection does not hand off");
  reset(); assert(ModernSetupFindSecureBootConfiguration(NULL) == EFI_INVALID_PARAMETER);
  assert(ModernSetupOpenSecureBootConfiguration() == EFI_NOT_FOUND);
  assert(!opened && !fallback && device_invalid == 1);
  assert(wcscmp(ModernSetupSecureBootEntryText(), L"Unavailable") == 0);
  puts("PASS missing Secure Boot does not open unrelated UI");
  reset(); count = 16; match(15); entries[15].FormSetGuid = mSecureBootFormSetGuid;
  assert(wcscmp(ModernSetupSecureBootEntryText(), L"Open setup") == 0);
  assert(!device_invalid); /* labels reuse cache, do not force enumeration */
  assert(ModernSetupOpenSecureBootConfiguration() == EFI_SUCCESS);
  assert(opened == 1 && !fallback && boot_invalid == 1 && device_invalid == 2 && provider_invalid == 1);
  puts("PASS Secure Boot beyond visible cap, cached availability and refresh on return");
  reset(); count = 1; match(0); entries[0].FormSetGuid = mSecureBootFormSetGuid;
  open_status = EFI_DEVICE_ERROR;
  assert(ModernSetupOpenSecureBootConfiguration() == EFI_DEVICE_ERROR);
  assert(opened == 1 && !fallback && boot_invalid == 1 && device_invalid == 2 && provider_invalid == 1);
  puts("PASS Secure Boot handoff error propagates and invalidates all caches");
  reset(); count = 3; match(0); entries[0].FormSetGuid = mSecureBootFormSetGuid; entries[0].HasForm = 0;
  match(1); entries[1].FormSetGuid = mSecureBootFormSetGuid; entries[1].HiiHandle = NULL;
  match(2); /* Boot Maintenance GUID is not Secure Boot */
  assert(ModernSetupOpenSecureBootConfiguration() == EFI_NOT_FOUND && !opened && !fallback);
  puts("PASS wrong GUID, inventory-only and null-handle Secure Boot entries rejected");
  reset(); discovery_status = EFI_OUT_OF_RESOURCES;
  assert(wcscmp(ModernSetupSecureBootEntryText(), L"Discovery error") == 0);
  assert(ModernSetupOpenSecureBootConfiguration() == EFI_OUT_OF_RESOURCES && !opened && !fallback);
  assert(ModernSetupQuickSettingsRowOffset(4) == 234);
  assert(ModernSetupQuickSettingsRowOffset(5) == 261);
  assert(ModernSetupQuickSettingsRowOffset(9) == 399);
  puts("PASS Secure Boot discovery errors and grouped row offsets");
  return 0;
}
'''
    # Compile the actual definitions, not a Python translation of their logic.
    code = prelude + guid + "\n" + "\n".join(
        extract_c_function_body(source, name) for name in (
            "ModernSetupOpenSelectedDeviceEntry", "ModernSetupOpenBootConfigurationWithFallback", "ModernSetupOpenBootConfiguration",
            "ModernSetupFindSecureBootConfiguration", "ModernSetupSecureBootEntryText",
            "ModernSetupOpenSecureBootConfiguration", "ModernSetupQuickSettingsRowOffset"
        )
    ) + tests
    with tempfile.TemporaryDirectory(prefix="modernsetup-boot-handoff-") as directory:
        path = Path(directory)
        (path / "test.c").write_text(code)
        subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
            "-std=c11", "-Wall", "-Wextra", "-Werror", str(path / "test.c"), "-o", str(path / "test")
        ], check=True)
        subprocess.run([str(path / "test")], check=True)


if __name__ == "__main__":
    main()
