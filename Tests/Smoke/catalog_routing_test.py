#!/usr/bin/env python3
"""Compile actual stable-ID routing; guard separate help/native affordances."""
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
from smoke_validate import extract_c_function_body

ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / "Application/ModernSetupApp"


def main():
    source = (APP / "ModernSetupAppCatalog.c").read_text()
    route = extract_c_function_body(source, "CatalogNativeRoute")
    metadata = json.loads((ROOT / "Config/SetupSettings.json").read_text())
    items = metadata["settings"]
    ids = {item["id"] for item in items}
    mapped = set(re.findall(r'"((?:boot|security)\.[a-z_]+)"', route))
    assert mapped <= ids, mapped - ids
    assert len(mapped) == 13
    activation = extract_c_function_body(source, "CatalogActivate")
    assert "mModernSetupImageHandle, FALSE" in activation
    assert "ModernSetupOpenSecureBootConfiguration ();" in activation
    assert "if (Route == 0)" in activation
    assert "value remains N/A" in activation
    assert "mHelpOffset = 0" in activation
    inputs = extract_c_function_body(source, "ModernSetupCatalogInput")
    assert "Type == ModernUiInputEnter" in inputs
    assert "mLevel == 2 && Y >= Panel.Y + 32 && Y < Panel.Y + 64" in inputs
    assert inputs.count("CatalogActivate ();") == 3
    assert 'L"[Enter / Click] Open native setup >"' in source
    assert 'Row.Value = (mLevel == 0) ? ModernSetupCatalogUi (L"Browse >", L"浏览 >") : L"N/A";' in source
    assert not any(token in source for token in ("SetVariable", "RouteConfig", "HiiSetBrowserData"))
    prelude = '''#include <assert.h>
#include <string.h>
#include <stddef.h>
#define STATIC static
#define CONST const
#define CHAR8 char
#define UINTN size_t
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define AsciiStrCmp strcmp
'''
    tests = '\nint main(void) {\n'
    for item in items:
        ident = item["id"]
        expected = (1 if ident.startswith("boot.") else 2) if ident in mapped else 0
        tests += f'assert(CatalogNativeRoute({json.dumps(ident)}) == {expected});\n'
    tests += 'assert(CatalogNativeRoute("boot.timeout.extra") == 0); return 0; }\n'
    with tempfile.TemporaryDirectory(prefix="modernsetup-catalog-routing-") as directory:
        path = Path(directory)
        (path / "test.c").write_text(prelude + route + tests)
        subprocess.run(shlex.split(os.environ.get("CC", "cc")) + ["-std=c11", "-Wall", "-Wextra", "-Werror", str(path / "test.c"), "-o", str(path / "test")], check=True)
        subprocess.run([str(path / "test")], check=True)
    print("PASS real-C explicit catalog routes, unmapped IDs, strict handoff and keyboard/pointer source guards")


if __name__ == "__main__":
    main()
