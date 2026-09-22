#!/usr/bin/env python3
"""Replace the single native browser in freshly generated modern DSC/FDF files."""
import importlib.util
from pathlib import Path
import sys
import os


def wire(workspace, engine, files):
    enabled = os.environ.get("MODERN_SETUP_SAVE_REVIEW", "1")
    if enabled not in ("0", "1"):
        raise ValueError("MODERN_SETUP_SAVE_REVIEW must be 0 or 1")
    if engine == "native" or enabled == "0":
        print("Native staged-save review: disabled (explicit opt-out or native engine)")
        return
    workspace = Path(workspace).resolve()
    root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location("browser_overlay", root / "Scripts/setup-browser-review-overlay.py")
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    sdk = (workspace / "MdeModulePkg").resolve().parent
    output = workspace / "Build/ModernSetupPkgOverlay/SetupBrowserReview"
    old = "MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf"
    new = "Build/ModernSetupPkgOverlay/SetupBrowserReview/SetupBrowserDxe.inf"
    targets = [workspace / "Build/ModernSetupPkgOverlay" / name for name in files]
    texts = [p.read_text() for p in targets]
    if sum(t.count(old) for t in texts) != 2:
        raise ValueError("expected exactly one browser component and one FV browser INF")
    module.generate(workspace, output, sdk=sdk)
    for p, text in zip(targets, texts):
        p.write_text(text.replace(old, new))
    print("Native staged-save review: enabled, single browser owner")


if __name__ == "__main__":
    wire(sys.argv[1], sys.argv[2], sys.argv[3:])
