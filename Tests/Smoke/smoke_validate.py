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
  without direct experimental HII bridge or ConfigAccess coupling;
* provider health/readiness remains app-private and derived from the provider
  snapshot boundary;
* the Dashboard quick-card expansion stays backed by provider snapshots and a
  single app-private selectable-card count; and
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
BUILD_SCRIPTS = (
    "build-armvirt.sh",
    "build-loongarchvirt.sh",
    "build-ovmf-x64.sh",
    "build-riscvvirt.sh",
)
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
    "SetVariable",
    "HiiSetBrowserData",
    "HiiUpdateForm",
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
PCIE_PROVIDER_SUMMARY_TOKEN = "ModernUiPcieDataGetSummary"
PCIE_PROVIDER_REQUIRED_FILES = (
    Path("Include") / "ModernUi" / "ModernUiPcieData.h",
    Path("Library") / "ModernUiPcieDataLib" / "ModernUiPcieDataLib.c",
    Path("Library") / "ModernUiPcieDataLib" / "ModernUiPcieDataLib.inf",
)
PCIE_FORBIDDEN_MUTATION_TOKENS = (
    "SetVariable",
    "RouteConfig",
    "ExtractConfig",
    "HiiSetBrowserData",
    "HiiUpdateForm",
    "EFI_HII_CONFIG_ACCESS_PROTOCOL",
    "SetBarAttributes",
)
PCIE_APP_CATALOG_TOKENS = (
    "Providers.Pcie",
    "Providers.PcieStatus",
    "ControllerCount",
    "RootBridgeCount",
    "EndpointCount",
    "BridgeCount",
    "AspmPolicyEntryPresent",
    "BifurcationPolicyEntryPresent",
    "HotPlugPolicyEntryPresent",
    "IommuPolicyEntryPresent",
    "ResizableBarDeviceCount",
    "SriovDeviceCount",
    "AspmCapableLinkCount",
)
PCIE_DOC_KEYWORDS = (
    "pcie",
    "pci express",
    "rebar",
    "resizable bar",
    "above4g",
    "above 4g",
    "sr-iov",
    "sriov",
    "aspm",
    "bifurcation",
)
PCIE_DOC_FORBIDDEN_CLAIM_PATTERNS = (
    re.compile(r"\b(edit|edits|editing|modify|modifies|modifying|write|writes|writing|toggle|toggles|toggling)\b", re.IGNORECASE),
)
PCIE_DOC_REQUIRED_FILES = (
    Path("Docs") / "ISSUE_BACKLOG.md",
    Path("Docs") / "AGENT_OWNERSHIP.md",
    Path("Docs") / "ProductizationFeatureMatrix.md",
    Path("Docs") / "IbvAndPlatformSetupSurvey.md",
    Path("README.md"),
    Path("CHANGELOG.md"),
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
DASHBOARD_EXPANDED_CARD_TOKENS = (
    "Provider Health",
    "Firmware",
    "Power / Thermal",
    "Performance",
    "BootDetailText",
    "DeviceDetailText",
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


def riscvvirt_fixture(workspace: Path) -> None:
    (workspace / "MdePkg").mkdir(parents=True, exist_ok=True)
    write(
        workspace / "OvmfPkg" / "RiscVVirt" / "RiscVVirtQemu.dsc",
        """[Defines]
  FLASH_DEFINITION               = OvmfPkg/RiscVVirt/RiscVVirtQemu.fdf

[LibraryClasses.common]
  CustomizedDisplayLib|MdeModulePkg/Library/CustomizedDisplayLib/CustomizedDisplayLib.inf

[Components]
  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
  MdeModulePkg/Application/UiApp/UiApp.inf {
  }
""",
    )
    write(
        workspace / "OvmfPkg" / "RiscVVirt" / "RiscVVirtQemu.fdf",
        """!include RiscVVirt.fdf.inc
!include VarStore.fdf.inc
INF  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
INF  MdeModulePkg/Application/UiApp/UiApp.inf
""",
    )


def ovmf_x64_fixture(workspace: Path) -> None:
    (workspace / "MdePkg").mkdir(parents=True, exist_ok=True)
    write(
        workspace / "OvmfPkg" / "OvmfPkgX64.dsc",
        """[Defines]
  FLASH_DEFINITION               = OvmfPkg/OvmfPkgX64.fdf

[LibraryClasses.common]
  CustomizedDisplayLib|MdeModulePkg/Library/CustomizedDisplayLib/CustomizedDisplayLib.inf

[Components]
  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
  MdeModulePkg/Application/UiApp/UiApp.inf {
  }
""",
    )
    write(
        workspace / "OvmfPkg" / "OvmfPkgX64.fdf",
        """INF  MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf
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
    if platform == "ovmf-x64":
        return (
            overlay / "OvmfX64ModernSetup.dsc",
            overlay / "OvmfX64ModernSetup.fdf",
        )
    if platform == "riscvvirt":
        return (
            overlay / "RiscVVirtQemuModernSetup.dsc",
            overlay / "RiscVVirtQemuModernSetup.fdf",
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
    if c_function_definition_count(provider_body, "ModernSetupGetProviderHealthSummary") != 1:
        raise SmokeFailure("ModernSetupAppProvider.c must define ModernSetupGetProviderHealthSummary exactly once")
    if c_function_definition_count(provider_body, "ModernSetupGetProviderHealthStateText") != 1:
        raise SmokeFailure("ModernSetupAppProvider.c must define ModernSetupGetProviderHealthStateText exactly once")
    for token in APP_PROVIDER_SUMMARY_TOKENS:
        if token not in provider_body:
            raise SmokeFailure(f"ModernSetupAppProvider.c missing provider summary call: {token}")
    if "MODERN_SETUP_PROVIDER_HEALTH_SUMMARY" not in provider_body:
        raise SmokeFailure("ModernSetupAppProvider.c missing app-private provider health summary derivation")

    dashboard_body = strip_c_comments(dashboard.read_text(encoding="utf-8"))
    app_body = strip_c_comments((app_dir / "ModernSetupApp.c").read_text(encoding="utf-8"))
    actions_body = strip_c_comments((app_dir / "ModernSetupAppActions.c").read_text(encoding="utf-8"))
    if "ModernSetupGetProviderHealthSummary" not in dashboard_body:
        raise SmokeFailure("ModernSetupAppDashboard.c must render health derived from the provider snapshot")
    if "ModernSetupGetProviderHealthStateText" not in dashboard_body:
        raise SmokeFailure("ModernSetupAppDashboard.c must show provider health state text")
    if "ProviderHealth." not in dashboard_body:
        raise SmokeFailure("ModernSetupAppDashboard.c must consume the app-private provider health summary")
    internal_body = strip_c_comments((app_dir / "ModernSetupAppInternal.h").read_text(encoding="utf-8"))
    count_match = re.search(r"#define\s+DASHBOARD_QUICK_CARD_COUNT\s+(\d+)", internal_body)
    if count_match is None:
        raise SmokeFailure("ModernSetupAppInternal.h missing DASHBOARD_QUICK_CARD_COUNT")
    if int(count_match.group(1)) < 6:
        raise SmokeFailure("Dashboard quick-card expansion must expose at least six cards")
    if "DASHBOARD_QUICK_CARD_COUNT" not in dashboard_body:
        raise SmokeFailure("ModernSetupAppDashboard.c must layout cards from DASHBOARD_QUICK_CARD_COUNT")
    if "DASHBOARD_QUICK_VALUE_MIN_HEIGHT" not in dashboard_body:
        raise SmokeFailure("ModernSetupAppDashboard.c must keep Dashboard card values visible in compact layouts")
    if "ModernSetupGetDashboardQuickGrid" not in actions_body or "MODERN_SETUP_DASHBOARD_QUICK_GRID" not in internal_body:
        raise SmokeFailure("Dashboard quick-card layout must use a shared grid helper contract")
    if "ModernSetupGetDashboardCategoryRoute" not in actions_body or "MODERN_SETUP_DASHBOARD_ROUTE" not in internal_body:
        raise SmokeFailure("Dashboard category landing routes must use the shared helper contract")
    if "ModernSetupGetDashboardCategoryRoute (DashboardSelection" not in app_body:
        raise SmokeFailure("Dashboard Enter handling must resolve category landing routes through the shared helper")
    if "mDashboardCategoryRoutes[DASHBOARD_QUICK_CARD_COUNT]" not in actions_body:
        raise SmokeFailure("Dashboard category route table must stay aligned with the visible card count")
    if actions_body.count("SetupFocusContent") < 2 or actions_body.count("SetupFocusNav") < 4:
        raise SmokeFailure("Dashboard category routes must preserve content focus for Boot/Devices and nav focus for overview pages")
    if "DashboardSelection >= DashboardGrid.CardsPerRow" not in app_body:
        raise SmokeFailure("Dashboard Up navigation must move by grid row before returning to navigation")
    if "DashboardSelection + DashboardGrid.CardsPerRow" not in app_body:
        raise SmokeFailure("Dashboard Down navigation must move by grid row")
    for token in DASHBOARD_EXPANDED_CARD_TOKENS:
        if token not in dashboard_body:
            raise SmokeFailure(f"ModernSetupAppDashboard.c missing expanded Dashboard card token: {token}")
    for provider_field in ("Firmware", "Power", "Performance", "Diagnostics"):
        if f"Providers.{provider_field}." not in dashboard_body:
            raise SmokeFailure(
                f"ModernSetupAppDashboard.c expanded cards must use provider snapshot field: Providers.{provider_field}."
            )
    if "Provider Health" not in pages_body:
        raise SmokeFailure("ModernSetupAppPages.c diagnostics summary must include provider health details")

    return ["PASS ModernSetupApp module boundary checks"]


def check_pcie_provider_foundation(root: Path) -> list[str]:
    for relative in PCIE_PROVIDER_REQUIRED_FILES:
        if not (root / relative).exists():
            raise SmokeFailure(f"missing PCIe provider foundation file: {relative}")

    header = root / "Include" / "ModernUi" / "ModernUiPcieData.h"
    lib_c = root / "Library" / "ModernUiPcieDataLib" / "ModernUiPcieDataLib.c"
    lib_inf = root / "Library" / "ModernUiPcieDataLib" / "ModernUiPcieDataLib.inf"
    dec = root / "ModernSetupPkg.dec"
    experimental_dsc = root / "Experimental" / "ModernSetupApp.dsc"
    app_inf = root / MODERN_SETUP_APP_INF

    assert_contains(header, "MODERN_UI_PCIE_SUMMARY")
    assert_contains(header, PCIE_PROVIDER_SUMMARY_TOKEN)
    assert_contains(lib_c, "#include <ModernUi/ModernUiPcieData.h>")
    assert_contains(lib_c, PCIE_PROVIDER_SUMMARY_TOKEN)
    assert_contains(lib_inf, "BASE_NAME                      = ModernUiPcieDataLib")
    assert_contains(lib_inf, "LIBRARY_CLASS                  = ModernUiPcieDataLib|UEFI_APPLICATION")
    assert_contains(dec, "ModernUiPcieDataLib|Include/ModernUi/ModernUiPcieData.h")
    assert_contains(experimental_dsc, "ModernUiPcieDataLib|ModernSetupPkg/Library/ModernUiPcieDataLib/ModernUiPcieDataLib.inf")
    assert_contains(experimental_dsc, "ModernSetupPkg/Library/ModernUiPcieDataLib/ModernUiPcieDataLib.inf")
    if app_inf.exists():
        assert_contains(app_inf, "ModernUiPcieDataLib")

    for source in (lib_c, header):
        body = strip_c_comments(source.read_text(encoding="utf-8"))
        for token in PCIE_FORBIDDEN_MUTATION_TOKENS:
            if token in body:
                raise SmokeFailure(f"{source.relative_to(root)} contains prohibited PCIe mutation token: {token}")

    app_dir = root / MODERN_SETUP_APP_DIR
    for source in sorted(app_dir.glob("ModernSetupApp*.c")):
        body = strip_c_comments(source.read_text(encoding="utf-8"))
        if PCIE_PROVIDER_SUMMARY_TOKEN in body and source.name != "ModernSetupAppProvider.c":
            raise SmokeFailure(
                f"{source.relative_to(root)} bypasses ModernSetupAppProvider.c for PCIe provider summary"
            )
        if re.search(r"\b(Pcie|PCIe|PciExpress|ReBAR|ResizableBar|Above4G|Sriov|SRIOV|ASPM|Bifurcation)\b", body):
            for token in PCIE_FORBIDDEN_MUTATION_TOKENS:
                if token in body:
                    raise SmokeFailure(f"{source.relative_to(root)} contains prohibited app PCIe mutation token: {token}")

    pages_body = strip_c_comments((app_dir / "ModernSetupAppPages.c").read_text(encoding="utf-8"))
    if PCIE_PROVIDER_SUMMARY_TOKEN in pages_body:
        raise SmokeFailure("ModernSetupAppPages.c must consume Providers.Pcie from the app snapshot, not call PCIe provider summary")
    for token in PCIE_APP_CATALOG_TOKENS:
        if token not in pages_body:
            raise SmokeFailure(f"ModernSetupAppPages.c missing Performance PCIe catalog token: {token}")

    return ["PASS PCIe provider foundation wiring and read-only boundary checks"]


def check_pcie_docs_language(root: Path) -> list[str]:
    missing_docs = [str(path) for path in PCIE_DOC_REQUIRED_FILES if not (root / path).exists()]
    if missing_docs:
        raise SmokeFailure("missing PCIe documentation files: " + ", ".join(missing_docs))

    required_markers = {
        Path("Docs") / "ISSUE_BACKLOG.md": "Phase 7 PCIe policy provider foundation",
        Path("Docs") / "AGENT_OWNERSHIP.md": "PCIe policy summary",
        Path("Docs") / "ProductizationFeatureMatrix.md": "ModernUiPcieDataLib",
        Path("Docs") / "IbvAndPlatformSetupSurvey.md": "PCIe policy",
        Path("README.md"): "ModernUiPcieDataLib",
        Path("CHANGELOG.md"): "ModernUiPcieDataLib",
    }
    for relative, marker in required_markers.items():
        assert_contains(root / relative, marker)

    for relative in PCIE_DOC_REQUIRED_FILES:
        path = root / relative
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            normalized = line.lower()
            if not any(keyword in normalized for keyword in PCIE_DOC_KEYWORDS):
                continue
            if "remain" in normalized and ("native" in normalized or "hii" in normalized or "formbrowser" in normalized):
                continue
            for pattern in PCIE_DOC_FORBIDDEN_CLAIM_PATTERNS:
                if pattern.search(line):
                    raise SmokeFailure(
                        f"{relative}:{line_number} may claim App-owned PCIe mutation instead of read-only/entry behavior: {line.strip()}"
                    )

    return ["PASS PCIe docs read-only/capability/entry language checks"]


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
        ovmf_x64_fixture(workspace)
        riscvvirt_fixture(workspace)

        cases = (
            ("armvirt", "build-armvirt.sh", "ArmVirtQemuModernSetup.dsc"),
            ("loongarch", "build-loongarchvirt.sh", "LoongArchVirtQemuModernSetup.dsc"),
            ("ovmf-x64", "build-ovmf-x64.sh", "OvmfX64ModernSetup.dsc"),
            ("riscvvirt", "build-riscvvirt.sh", "RiscVVirtQemuModernSetup.dsc"),
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
        messages.extend(check_pcie_provider_foundation(root))
        messages.extend(check_pcie_docs_language(root))
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
