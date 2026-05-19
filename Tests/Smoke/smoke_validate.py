#!/usr/bin/env python3
# Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
# Author: MarsDoge (Dongyan Qian)
# Open source: https://github.com/MarsDoge/ModernSetupPkg
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
"""Lightweight repository smoke validation for ModernSetupPkg.

This checker intentionally avoids full edk2 builds and QEMU. It validates host-
side invariants that are useful for multi-agent maintenance:

* shell syntax for Scripts/*.sh when bash is available;
* build-overlay scripts keep generated files under Build/ModernSetupPkgOverlay;
* native and modern DisplayEngine overlay paths remain separated;
* default firmware overlay generators do not pull in ModernSetupApp or the
  experimental HII bridge path;
* ModernSetupApp INF sources stay synchronized with app source files; and
* ModernSetupApp module boundaries keep dashboard drawing in its app module
  without direct experimental HII bridge or ConfigAccess coupling.
* overlay generation works against tiny synthetic edk2 source fixtures.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from typing import Iterable


REPO_MARKERS = ("ModernSetupPkg.dec", "Scripts", "Tests")
BUILD_SCRIPTS = ("build-armvirt.sh", "build-loongarchvirt.sh")
PROHIBITED_DEFAULT_OVERLAY_TOKENS = (
    "ModernSetupApp",
    "ModernUiHiiBridgeLib",
    "ModernUiPageAdapterLib",
    "ModernUiHiiBridge.h",
    "ModernUiPageAdapter.h",
)
MODERN_SETUP_APP_DIR = Path("Application") / "ModernSetupApp"
MODERN_SETUP_APP_INF = MODERN_SETUP_APP_DIR / "ModernSetupApp.inf"
PROHIBITED_APP_SOURCE_TOKENS = (
    "ModernUiHiiBridge.h",
    "ModernUiPageAdapter.h",
    "ModernUiHiiBridgeLib",
    "ModernUiPageAdapterLib",
    "EFI_HII_CONFIG_ACCESS_PROTOCOL",
    "ConfigAccess",
    "ExtractConfig",
    "RouteConfig",
)
APP_PROVIDER_SUMMARY_TOKENS = (
    "ModernUiPlatformDataGetSummary",
    "ModernUiSecurityDataGetSummary",
    "ModernUiFirmwareDataGetSummary",
    "ModernUiDiagnosticsDataGetSummary",
    "ModernUiManagementDataGetSummary",
    "ModernUiPowerDataGetSummary",
    "ModernUiPerformanceDataGetSummary",
)
APP_PROVIDER_SNAPSHOT_FIELDS = (
    "Platform",
    "Security",
    "Firmware",
    "Diagnostics",
    "Management",
    "Power",
    "Performance",
)


class SmokeFailure(Exception):
    """Raised when a smoke invariant fails."""


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[2]


def require_repo_root(path: Path) -> Path:
    root = path.resolve()
    missing = [marker for marker in REPO_MARKERS if not (root / marker).exists()]
    if missing:
        raise SmokeFailure(f"{root} is not ModernSetupPkg root; missing: {', '.join(missing)}")
    return root


def run(command: list[str], *, cwd: Path | None = None, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        command,
        cwd=str(cwd) if cwd else None,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        quoted = " ".join(command)
        detail = (result.stdout + result.stderr).strip()
        raise SmokeFailure(f"command failed ({result.returncode}): {quoted}\n{detail}")
    return result


def check_shell_syntax(root: Path) -> list[str]:
    bash = shutil.which("bash")
    if bash is None:
        return ["SKIP shell syntax: bash not found"]

    scripts = sorted((root / "Scripts").glob("*.sh"))
    if not scripts:
        raise SmokeFailure("no Scripts/*.sh files found")

    for script in scripts:
        run([bash, "-n", str(script)], cwd=root)
    return [f"PASS shell syntax: {len(scripts)} Scripts/*.sh files"]


def check_static_overlay_script_contracts(root: Path) -> list[str]:
    messages: list[str] = []
    for name in BUILD_SCRIPTS:
        path = root / "Scripts" / name
        text = path.read_text(encoding="utf-8")

        required_fragments = (
            "GENERATE_ONLY",
            "Build/ModernSetupPkgOverlay",
            "MODERN_SETUP_DISPLAY_ENGINE",
            "display_engine not in",
            "display_engine == \"modern\"",
        )
        for fragment in required_fragments:
            if fragment not in text:
                raise SmokeFailure(f"{path} missing overlay contract fragment: {fragment}")

        for token in PROHIBITED_DEFAULT_OVERLAY_TOKENS:
            if token in text:
                raise SmokeFailure(f"{path} default overlay generator references prohibited token: {token}")

        write_targets = re.findall(r"\(overlay / [^)]+\)\.write_text\(", text)
        if not write_targets:
            raise SmokeFailure(f"{path} has no overlay write_text targets")

        messages.append(f"PASS static overlay script contract: Scripts/{name}")
    return messages


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def armvirt_fixture(workspace: Path) -> None:
    (workspace / "MdePkg").mkdir(parents=True)
    write(
        workspace / "ArmVirtPkg" / "ArmVirtQemu.dsc",
        """[Defines]
  FLASH_DEFINITION               = ArmVirtPkg/ArmVirtQemu.fdf
  gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerMenuFile|{ 0x00 }

[LibraryClasses.common]
  CustomizedDisplayLib|MdeModulePkg/Library/CustomizedDisplayLib/CustomizedDisplayLib.inf

[Components]
  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
  MdeModulePkg/Application/UiApp/UiApp.inf {
  }
""",
    )
    write(
        workspace / "ArmVirtPkg" / "ArmVirtQemu.fdf",
        """!include VarStore.fdf.inc
!include ArmVirtRules.fdf.inc
!include ArmVirtQemuFvMain.fdf.inc
""",
    )
    write(
        workspace / "ArmVirtPkg" / "ArmVirtQemuFvMain.fdf.inc",
        """!include ArmVirtRules.fdf.inc
  INF MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
  INF MdeModulePkg/Application/UiApp/UiApp.inf
""",
    )


def loongarch_fixture(workspace: Path) -> None:
    (workspace / "MdePkg").mkdir(parents=True, exist_ok=True)
    write(
        workspace / "OvmfPkg" / "LoongArchVirt" / "LoongArchVirtQemu.dsc",
        """[Defines]
  FLASH_DEFINITION               = OvmfPkg/LoongArchVirt/LoongArchVirtQemu.fdf
!include LoongArchVirt.fdf.inc

[LibraryClasses.common]
  CustomizedDisplayLib             | MdeModulePkg/Library/CustomizedDisplayLib/CustomizedDisplayLib.inf

[Components]
  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
  MdeModulePkg/Application/UiApp/UiApp.inf {
  }
""",
    )
    write(
        workspace / "OvmfPkg" / "LoongArchVirt" / "LoongArchVirtQemu.fdf",
        """!include LoongArchVirt.fdf.inc
!include VarStore.fdf.inc
INF  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
INF  MdeModulePkg/Application/UiApp/UiApp.inf
""",
    )


def symlink_or_copy_repo(root: Path, link_path: Path) -> None:
    try:
        os.symlink(root, link_path, target_is_directory=True)
    except OSError:
        shutil.copytree(root, link_path, ignore=shutil.ignore_patterns(".git", "Build", "__pycache__"))


def generated_files_for(platform: str, workspace: Path) -> tuple[Path, ...]:
    overlay = workspace / "Build" / "ModernSetupPkgOverlay"
    if platform == "armvirt":
        return (
            overlay / "ArmVirtQemuModernSetup.dsc",
            overlay / "ArmVirtQemuModernSetup.fdf",
            overlay / "ArmVirtQemuModernSetupFvMain.fdf.inc",
        )
    return (
        overlay / "LoongArchVirtQemuModernSetup.dsc",
        overlay / "LoongArchVirtQemuModernSetup.fdf",
    )


def assert_contains(path: Path, needle: str) -> None:
    if needle not in path.read_text(encoding="utf-8"):
        raise SmokeFailure(f"{path} does not contain expected text: {needle}")


def assert_not_contains_any(path: Path, needles: Iterable[str]) -> None:
    text = path.read_text(encoding="utf-8")
    for needle in needles:
        if needle in text:
            raise SmokeFailure(f"{path} contains prohibited text: {needle}")


def strip_c_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*", "", text)
    return text


def parse_inf_sources(inf: Path) -> list[str]:
    sources: list[str] = []
    in_sources = False
    for raw_line in inf.read_text(encoding="utf-8").splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            section_names = [part.strip().lower() for part in line[1:-1].split(",")]
            in_sources = any(name.startswith("sources") for name in section_names)
            continue
        if in_sources:
            sources.append(line.split()[0])
    return sources


def check_modern_setup_app_inf_sources(root: Path) -> list[str]:
    app_dir = root / MODERN_SETUP_APP_DIR
    inf = root / MODERN_SETUP_APP_INF
    if not app_dir.exists():
        raise SmokeFailure(f"missing ModernSetupApp directory: {app_dir}")
    if not inf.exists():
        raise SmokeFailure(f"missing ModernSetupApp INF: {inf}")

    app_sources = sorted(path.name for path in app_dir.glob("ModernSetupApp*.c"))
    inf_sources = parse_inf_sources(inf)
    inf_c_sources = sorted(source for source in inf_sources if source.endswith(".c"))

    missing_from_inf = sorted(set(app_sources) - set(inf_c_sources))
    if missing_from_inf:
        raise SmokeFailure(
            "ModernSetupApp .c sources missing from INF [Sources]: " + ", ".join(missing_from_inf)
        )

    missing_files = sorted(source for source in inf_c_sources if not (app_dir / source).exists())
    if missing_files:
        raise SmokeFailure(
            "ModernSetupApp INF [Sources] lists missing .c files: " + ", ".join(missing_files)
        )

    return ["PASS ModernSetupApp INF source coverage"]


def c_function_definition_count(text: str, function_name: str) -> int:
    pattern = re.compile(
        rf"(^|\n)\s*(?:STATIC\s+)?[A-Z_][A-Z0-9_\s\*]+\s+{re.escape(function_name)}\s*\([^;]*?\)\s*\{{",
        re.DOTALL,
    )
    return len(pattern.findall(text))


def check_modern_setup_app_module_boundaries(root: Path) -> list[str]:
    app_dir = root / MODERN_SETUP_APP_DIR
    inf_sources = parse_inf_sources(root / MODERN_SETUP_APP_INF)
    dashboard = app_dir / "ModernSetupAppDashboard.c"
    pages = app_dir / "ModernSetupAppPages.c"
    provider = app_dir / "ModernSetupAppProvider.c"

    if not dashboard.exists():
        raise SmokeFailure("ModernSetupAppDashboard.c is missing")
    if "ModernSetupAppDashboard.c" not in inf_sources:
        raise SmokeFailure("ModernSetupAppDashboard.c is not listed in ModernSetupApp.inf [Sources]")
    if not pages.exists():
        raise SmokeFailure("ModernSetupAppPages.c is missing")
    if not provider.exists():
        raise SmokeFailure("ModernSetupAppProvider.c is missing")
    if "ModernSetupAppProvider.c" not in inf_sources:
        raise SmokeFailure("ModernSetupAppProvider.c is not listed in ModernSetupApp.inf [Sources]")

    definition_locations: list[str] = []
    for source in sorted(app_dir.glob("ModernSetupApp*.c")):
        body = strip_c_comments(source.read_text(encoding="utf-8"))
        if c_function_definition_count(body, "ModernSetupDrawDashboard"):
            definition_locations.append(source.name)
        for token in PROHIBITED_APP_SOURCE_TOKENS:
            if token in body:
                raise SmokeFailure(f"{source.relative_to(root)} directly references prohibited app boundary token: {token}")
        for token in APP_PROVIDER_SUMMARY_TOKENS:
            if token in body and source.name != "ModernSetupAppProvider.c":
                raise SmokeFailure(
                    f"{source.relative_to(root)} bypasses ModernSetupAppProvider.c for provider summary token: {token}"
                )
        if source.name != "ModernSetupAppProvider.c" and "MODERN_SETUP_PROVIDER_SNAPSHOT" in body:
            direct_provider_field = re.compile(
                rf"(?<!Providers\.)\b({'|'.join(APP_PROVIDER_SNAPSHOT_FIELDS)})\s*\."
            )
            for line_number, line in enumerate(body.splitlines(), start=1):
                match = direct_provider_field.search(line)
                if match:
                    raise SmokeFailure(
                        f"{source.relative_to(root)}:{line_number} uses stale direct provider field "
                        f"'{match.group(1)}.'; use MODERN_SETUP_PROVIDER_SNAPSHOT (Providers.{match.group(1)}.)"
                    )

    if definition_locations != ["ModernSetupAppDashboard.c"]:
        raise SmokeFailure(
            "ModernSetupDrawDashboard must be defined only in ModernSetupAppDashboard.c; found: "
            + (", ".join(definition_locations) if definition_locations else "none")
        )

    pages_body = strip_c_comments(pages.read_text(encoding="utf-8"))
    if "ModernSetupDrawDashboard" not in pages_body:
        raise SmokeFailure("ModernSetupAppPages.c does not call ModernSetupDrawDashboard")
    if c_function_definition_count(pages_body, "ModernSetupDrawDashboard"):
        raise SmokeFailure("ModernSetupAppPages.c must call, not define, ModernSetupDrawDashboard")

    provider_body = strip_c_comments(provider.read_text(encoding="utf-8"))
    if c_function_definition_count(provider_body, "ModernSetupGetProviderSnapshot") != 1:
        raise SmokeFailure("ModernSetupAppProvider.c must define ModernSetupGetProviderSnapshot exactly once")
    for token in APP_PROVIDER_SUMMARY_TOKENS:
        if token not in provider_body:
            raise SmokeFailure(f"ModernSetupAppProvider.c missing provider summary call: {token}")

    return ["PASS ModernSetupApp module boundary checks"]


def check_overlay_generation(root: Path) -> list[str]:
    bash = shutil.which("bash")
    if bash is None:
        return ["SKIP overlay generation: bash not found"]

    messages: list[str] = []
    with tempfile.TemporaryDirectory(prefix="modernsetup-smoke-") as tmp:
        base = Path(tmp)
        workspace = base / "edk2"
        workspace.mkdir()
        symlink_or_copy_repo(root, workspace / "ModernSetupPkg")
        armvirt_fixture(workspace)
        loongarch_fixture(workspace)

        cases = (
            ("armvirt", "build-armvirt.sh", "ArmVirtQemuModernSetup.dsc"),
            ("loongarch", "build-loongarchvirt.sh", "LoongArchVirtQemuModernSetup.dsc"),
        )
        for platform, script_name, dsc_name in cases:
            script = workspace / "ModernSetupPkg" / "Scripts" / script_name
            for engine in ("native", "modern"):
                overlay_dir = workspace / "Build" / "ModernSetupPkgOverlay"
                if overlay_dir.exists():
                    shutil.rmtree(overlay_dir)

                env = os.environ.copy()
                env.update(
                    {
                        "WORKSPACE": str(workspace),
                        "GENERATE_ONLY": "1",
                        "MODERN_SETUP_DISPLAY_ENGINE": engine,
                        "MODERN_SETUP_DEMO_DRIVER_SAMPLE": "1",
                    }
                )
                run([bash, str(script)], cwd=workspace / "ModernSetupPkg", env=env)

                generated = generated_files_for(platform, workspace)
                for generated_path in generated:
                    if not generated_path.exists():
                        raise SmokeFailure(f"missing generated overlay file: {generated_path}")
                    assert_not_contains_any(generated_path, PROHIBITED_DEFAULT_OVERLAY_TOKENS)

                dsc = workspace / "Build" / "ModernSetupPkgOverlay" / dsc_name
                if engine == "modern":
                    assert_contains(dsc, "ModernUiEngineLib|ModernSetupPkg/Library/ModernUiEngineLib/ModernUiEngineLib.inf")
                    assert_contains(dsc, "gModernSetupPkgTokenSpaceGuid.PcdModernSetupTheme|0x00")
                    if not any("ModernDisplayEngineDxe" in path.read_text(encoding="utf-8") for path in generated):
                        raise SmokeFailure(f"{platform} modern overlay did not reference ModernDisplayEngineDxe")
                else:
                    for generated_path in generated:
                        assert_not_contains_any(
                            generated_path,
                            (
                                "ModernDisplayEngineDxe",
                                "ModernUiEngineLib|ModernSetupPkg",
                                "ModernUiRendererLib|ModernSetupPkg",
                                "ModernUiThemeLib|ModernSetupPkg",
                                "ModernUiCustomizedDisplayLib",
                                "gModernSetupPkgTokenSpaceGuid.PcdModernSetupTheme",
                            ),
                        )

                messages.append(f"PASS {platform} {engine} overlay generation dry run")

    return messages


def main() -> int:
    parser = argparse.ArgumentParser(description="Run lightweight ModernSetupPkg smoke validation.")
    parser.add_argument("--repo-root", type=Path, default=repo_root_from_script(), help="ModernSetupPkg repository root")
    args = parser.parse_args()

    try:
        root = require_repo_root(args.repo_root)
        messages: list[str] = []
        messages.extend(check_shell_syntax(root))
        messages.extend(check_modern_setup_app_inf_sources(root))
        messages.extend(check_modern_setup_app_module_boundaries(root))
        messages.extend(check_static_overlay_script_contracts(root))
        messages.extend(check_overlay_generation(root))
    except SmokeFailure as exc:
        print(f"FAIL smoke validation: {exc}", file=sys.stderr)
        return 1

    for message in messages:
        print(message)
    print("PASS smoke validation")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
