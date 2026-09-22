#!/usr/bin/env python3
"""Generate a pinned, copy-only native browser review overlay (single owner)."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

PIN = "b03a21a63e3bd001f52c527e5a57feddb53a690b"
MODULE = "MdeModulePkg/Universal/SetupBrowserDxe"
ROOT = Path(__file__).resolve().parents[1]
CAPABILITIES = {
    "revision": 1, "baseline": PIN, "runtime_review_enabled": True,
    "submit_gate_installed": True,
    "supported_scopes": ["form", "formset", "system"],
    "supported_values": ["numeric", "checkbox", "one-of", "public-string", "ordered-list"],
    "unsupported_policy": "known protected bytes redacted; unknown baseline or unsupported changed name/value scope blocks confirmation",
    "baseline_provenance": "successful native load and synchronization; failed extraction is unknown",
    "deployment": "replace the original SetupBrowserDxe INF in DSC/FDF; never add another owner",
}


def git(source, *args):
    return subprocess.check_output(["git", "-C", str(source), *args])


def generate(source, output, enable_review=True, cancel_guards=True, sdk=None):
    source, output = Path(source).resolve(), Path(output).resolve()
    sdk = Path(sdk).resolve() if sdk else source
    allowed = [(ROOT / "Build/ModernSetupPkgOverlay").resolve(), (source / "Build/ModernSetupPkgOverlay").resolve()]
    if not any(output.is_relative_to(base) and output != base for base in allowed):
        raise ValueError("output must be a child of Build/ModernSetupPkgOverlay")
    if git(sdk, "rev-parse", "HEAD").decode().strip() != PIN:
        raise ValueError("unrecognized reference SDK revision")
    names = git(sdk, "ls-tree", "-r", "--name-only", PIN, "--", MODULE).decode().splitlines()
    if not names:
        raise ValueError("missing pinned browser module")
    contents = {}
    for name in names:
        expected = git(sdk, "show", f"{PIN}:{name}")
        # New build workspaces may consist of package symlinks and have no .git.
        # Verify both reference SDK and actual consumed bytes against pinned git.
        if (sdk / name).read_bytes() != expected or (source / name).read_bytes() != expected:
            raise ValueError(f"modified pinned input: {name}")
        contents[Path(name).relative_to(MODULE)] = expected
    spec = importlib.util.spec_from_file_location("review_overlay", ROOT / "Scripts/SaveReview/runtime_overlay.py")
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    contents = module.apply(contents, ROOT)
    # Write only generated files; repeat generation is deterministic and safe.
    output.mkdir(parents=True, exist_ok=True)
    for name, data in contents.items():
        target = output / name
        if target.is_symlink():
            raise ValueError(f"refusing symlink output: {target}")
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    report = dict(CAPABILITIES, files={str(k): hashlib.sha256(v).hexdigest() for k, v in contents.items()})
    (output / "review-capabilities.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--edk2", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, help="real pinned git SDK when --edk2 is a symlink workspace")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = generate(args.edk2, args.output, sdk=args.sdk)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{error}\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
