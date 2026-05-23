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
* the pinned edk2 baseline submodule, bootstrap helper, workspace helper, and
  baseline docs stay discoverable by smoke tests; and
* overlay generation works against tiny synthetic edk2 source fixtures.
"""

from __future__ import annotations

import argparse
import json
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
EDK2_BASELINE_SHA = "b03a21a63e3bd001f52c527e5a57feddb53a690b"
EDK2_BASELINE_REQUIRED_SCRIPT_REFS = (
    Path("Scripts") / "build-modern-app.sh",
    Path("Scripts") / "build-ovmf-x64.sh",
    Path("Scripts") / "run-ovmf-x64.sh",
    Path("Scripts") / "capture-ovmf-x64.sh",
)
OVMF_CAPTURE_HELPER = Path("Scripts") / "capture-ovmf-x64.sh"
OVMF_CAPTURE_DOC = Path("Tests") / "Manual" / "OvmfX64Qemu.md"
DISPLAYENGINE_OVMF_VISUAL_HELPER = Path("Scripts") / "capture-displayengine-ovmf-x64.sh"
DISPLAYENGINE_OVMF_VISUAL_DOC = Path("Tests") / "Manual" / "DisplayEngineOvmfX64Visual.md"
PROHIBITED_DEFAULT_OVERLAY_TOKENS = (
    "ModernSetupApp",
    "ModernUiHiiBridgeLib",
    "ModernUiPageAdapterLib",
    "ModernUiHiiBridge.h",
    "ModernUiPageAdapter.h",
)
PROHIBITED_THEME_ALIAS_TOKENS = (
    "\x61\x6f\x72\x75\x73",
    "\x61\x73\x75\x73",
    "\x72\x6f\x67",
    "\x74\x75\x66",
    "\x67\x69\x67\x61\x62\x79\x74\x65",
    "\x6d\x73\x69",
)
MODERN_SETUP_APP_DIR = Path("Application") / "ModernSetupApp"
MODERN_SETUP_APP_INF = MODERN_SETUP_APP_DIR / "ModernSetupApp.inf"
APP_NOINLINE_DRAW_HELPERS = (
    "DrawProviderSummaryPage",
    "DrawBoot",
    "DrawHiiReadOnlyPreview",
    "DrawDevices",
    "DrawSecurity",
    "DrawFirmware",
    "DrawDiagnostics",
    "DrawManagement",
    "DrawPower",
    "DrawPerformance",
    "DrawServerInventorySummary",
    "DrawPreferences",
    "DrawExit",
)
PROHIBITED_APP_SOURCE_TOKENS = (
    "ModernUiPageAdapter.h",
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
    "ModernUiHardwareHealthDataGetSummary",
    "ModernUiPerformanceDataGetSummary",
    "ModernUiPcieDataGetSummary",
)
HARDWARE_HEALTH_PROVIDER_SUMMARY_TOKEN = "ModernUiHardwareHealthDataGetSummary"
HARDWARE_HEALTH_PROVIDER_REQUIRED_FILES = (
    Path("Include") / "ModernUi" / "ModernUiHardwareHealthData.h",
    Path("Library") / "ModernUiHardwareHealthDataLib" / "ModernUiHardwareHealthDataLib.c",
    Path("Library") / "ModernUiHardwareHealthDataLib" / "ModernUiHardwareHealthDataLib.inf",
)
HARDWARE_HEALTH_FORBIDDEN_TOKENS = (
    "SetVariable",
    "RouteConfig",
    "ExtractConfig",
    "HiiSetBrowserData",
    "HiiUpdateForm",
    "EFI_HII_CONFIG_ACCESS_PROTOCOL",
    "SMBus",
    "I2C",
    "IPMI",
    "SuperIO",
    "MMIO",
    "PCI",
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
PREFERENCES_REQUIRED_FILES = (
    Path("Include") / "ModernUi" / "ModernUiPreferences.h",
    Path("Library") / "ModernUiPreferencesLib" / "ModernUiPreferencesLib.c",
    Path("Library") / "ModernUiPreferencesLib" / "ModernUiPreferencesLib.inf",
)
PREFERENCES_APP_TOKENS = (
    "ModernUiPreferencesLoad",
    "ModernUiPreferencesSave",
    "ModernUiPreferencesResetToDefaults",
    "mModernSetupPreferences",
    "ModernSetupAppPreferences",
)
PREFERENCES_FORBIDDEN_PLATFORM_TOKENS = (
    "BootOrder",
    "Boot####",
    "SecureBoot",
    "SetupMode",
    "Cpu",
    "Fan",
    "Chipset",
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
HII_BRIDGE_FORBIDDEN_WRITE_TOKENS = (
    "SetVariable",
    "ExtractConfig",
    "RouteConfig",
    "HiiSetBrowserData",
    "HiiUpdateForm",
    "Callback",
    "ConfigAccess",
    "ConfigRouting",
    "EFI_HII_CONFIG_ACCESS_PROTOCOL",
    "EFI_HII_CONFIG_ROUTING_PROTOCOL",
    "gEfiHiiConfigAccessProtocolGuid",
    "gEfiHiiConfigRoutingProtocolGuid",
)
HII_BRIDGE_REQUIRED_TEXT_FIELDS = (
    "Title",
    "Prompt",
    "Help",
    "ValueText",
)
RENDERER_HII_NEUTRAL_TOKENS = (
    "ModernUiHiiBridge.h",
    "ModernUiHiiBridgeLib",
    "MODERN_UI_HII_VIEW",
    "MODERN_UI_HII_FORMSET",
    "MODERN_UI_HII_PAGE",
    "MODERN_UI_HII_ITEM",
    "EFI_HII_CONFIG_ACCESS_PROTOCOL",
    "EFI_HII_CONFIG_ROUTING_PROTOCOL",
    "ConfigAccess",
    "RouteConfig",
)
APP_PROVIDER_SNAPSHOT_FIELDS = (
    "Platform",
    "Security",
    "Firmware",
    "Diagnostics",
    "Management",
    "Power",
    "HardwareHealth",
    "Performance",
    "Pcie",
)
DASHBOARD_EXPANDED_CARD_TOKENS = (
    "Provider Health",
    "Firmware",
    "Power / Thermal",
    "Performance",
    "BootDetailText",
    "DeviceDetailText",
)
XARCH_DOC_REQUIRED_FILES = (
    Path("Docs") / "XArch.md",
    Path("README.md"),
    Path("Docs") / "CompatibilityMatrix.md",
    Path("Docs") / "ProductizationFeatureMatrix.md",
)
XARCH_ARCH_TOKENS = ("X64", "AARCH64", "LOONGARCH64", "RISCV64")
XARCH_TARGET_TOKENS = ("x64", "aarch64", "loongarch64", "riscv64")
XARCH_RUNNER = Path("Scripts") / "xarch-validate.sh"
XARCH_RUNNER_FORBIDDEN_MUTATION_TOKENS = (
    "SetVariable",
    "HiiSetBrowserData",
    "HiiUpdateForm",
    "RouteConfig",
    "ExtractConfig",
    "EFI_HII_CONFIG_ACCESS_PROTOCOL",
)
BILINGUAL_DOC_PAIRS = (
    (Path("README.md"), Path("README.zh-CN.md")),
    (Path("Docs") / "XArch.md", Path("Docs") / "XArch.zh-CN.md"),
    (Path("Docs") / "ProductizationFeatureMatrix.md", Path("Docs") / "ProductizationFeatureMatrix.zh-CN.md"),
    (Path("Docs") / "ProductizationValidationMatrix.md", Path("Docs") / "ProductizationValidationMatrix.zh-CN.md"),
    (Path("Docs") / "MODULE_BOUNDARIES.md", Path("Docs") / "MODULE_BOUNDARIES.zh-CN.md"),
    (Path("Docs") / "DEVELOPMENT.md", Path("Docs") / "DEVELOPMENT.zh-CN.md"),
    (Path("Docs") / "IbvAndPlatformSetupSurvey.md", Path("Docs") / "IbvAndPlatformSetupSurvey.zh-CN.md"),
)
DOC_INDEX_PAIRS = (
    (Path("Docs") / "README.md", Path("Docs") / "README.zh-CN.md"),
)
IBV_CHINESE_TAXONOMY_HEADING = "广义 IBV / 平台 Setup 功能分类"
IBV_ENGLISH_TAXONOMY_HEADING = "Broad IBV / Platform Setup Taxonomy"
PRODUCTIZATION_ZH_PARITY_TOKENS = (
    "XArch 产品目标能力矩阵",
    "Battery and adapter policy",
    "RAS/NUMA/PCIe policy",
    "ARCH=LOONGARCH64",
)
PRODUCTIZATION_VALIDATION_DOCS = (
    Path("Docs") / "ProductizationValidationMatrix.md",
    Path("Docs") / "ProductizationValidationMatrix.zh-CN.md",
)
PRODUCTIZATION_VALIDATION_LINK_SOURCES = (
    Path("README.md"),
    Path("README.zh-CN.md"),
    Path("Docs") / "README.md",
    Path("Docs") / "README.zh-CN.md",
    Path("Docs") / "XArch.md",
    Path("Docs") / "XArch.zh-CN.md",
    Path("Docs") / "ProductizationFeatureMatrix.md",
    Path("Docs") / "ProductizationFeatureMatrix.zh-CN.md",
    Path("Tests") / "README.md",
    Path("Tests") / "Smoke" / "README.md",
    Path("CHANGELOG.md"),
)
PRODUCTIZATION_VALIDATION_TARGET_TOKENS = (
    "X64",
    "AARCH64",
    "LOONGARCH64",
    "RISCV64",
    "OvmfPkg/OvmfPkgX64",
    "ArmVirtPkg/ArmVirtQemu",
    "OvmfPkg/LoongArchVirt/LoongArchVirtQemu",
    "OvmfPkg/RiscVVirt/RiscVVirtQemu",
    "Scripts/build-ovmf-x64.sh",
    "Scripts/build-armvirt.sh",
    "Scripts/build-loongarchvirt.sh",
    "Scripts/build-riscvvirt.sh",
)
PRODUCTIZATION_VALIDATION_DOC_CONTRACTS = (
    (
        Path("Docs") / "ProductizationValidationMatrix.md",
        {
            "XArch-not-ARCH boundary": (
                "XArch",
                "edk2 ARCH",
                "ARCH=X64",
                "ARCH=AARCH64",
                "ARCH=LOONGARCH64",
                "ARCH=RISCV64",
            ),
            "native HII/SendForm ownership": (
                "EFI_FORM_BROWSER2_PROTOCOL.SendForm()",
                "FormBrowser2",
                "native HII",
                "ConfigAccess",
                "IFR",
                "HII varstores",
                "platform policy",
            ),
            "Hardware Health demo-only/read-only boundary": (
                "Hardware Health",
                "demo-only/read-only",
                "does not claim real sensors",
                "does not program",
            ),
            "ModernUiPreferencesLib ownership": (
                "Preferences",
                "ModernUiPreferencesLib",
                "not platform policy",
            ),
            "PCIe native policy ownership": (
                "PCIe",
                "ReBAR",
                "Above 4G",
                "SR-IOV",
                "ASPM",
                "bifurcation",
                "hot-plug",
                "ACS",
                "ARI",
                "IOMMU",
                "native HII",
            ),
        },
    ),
    (
        Path("Docs") / "ProductizationValidationMatrix.zh-CN.md",
        {
            "XArch-not-ARCH boundary": (
                "XArch",
                "不会替代",
                "edk2 ARCH",
                "ARCH=X64",
                "ARCH=AARCH64",
                "ARCH=LOONGARCH64",
                "ARCH=RISCV64",
            ),
            "native HII/SendForm ownership": (
                "EFI_FORM_BROWSER2_PROTOCOL.SendForm()",
                "FormBrowser2",
                "native HII",
                "ConfigAccess",
                "不得解析 IFR",
                "不得写 HII varstores",
                "不得写 platform policy",
            ),
            "Hardware Health demo-only/read-only boundary": (
                "Hardware Health",
                "demo-only/read-only",
                "不声明真实传感器",
                "不编程",
                "只读",
            ),
            "ModernUiPreferencesLib ownership": (
                "Preferences",
                "ModernUiPreferencesLib",
                "不是 platform policy",
            ),
            "PCIe native policy ownership": (
                "PCIe",
                "ReBAR",
                "Above 4G",
                "SR-IOV",
                "ASPM",
                "bifurcation",
                "hot-plug",
                "ACS",
                "ARI",
                "IOMMU",
                "native HII",
            ),
        },
    ),
)
PRODUCTIZATION_XARCH_NEGATION_CUES = (
    "no supported",
    "not supported",
    "does not",
    "do not",
    "not an edk2 build-architecture abstraction",
    "no `ARCH=XArch`",
    "no `TARGET=XArch`",
    "不存在",
    "不会",
    "不是 edk2 构建架构抽象",
    "不支持",
    "不应",
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
                is_replace_uiapp_opt_in = (
                    "MODERN_SETUP_REPLACE_UIAPP" in text
                    and token in {"ModernSetupApp", "ModernUiHiiBridgeLib"}
                )
                if is_replace_uiapp_opt_in:
                    continue
                raise SmokeFailure(f"{path} default overlay generator references prohibited token: {token}")

        lowered = text.lower()
        for token in PROHIBITED_THEME_ALIAS_TOKENS:
            if f'"{token}"' in lowered or f"'{token}'" in lowered:
                raise SmokeFailure(f"{path} uses prohibited vendor theme alias: {token}")

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
  gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerMenuFile                | { 0x21, 0xaa, 0x2c, 0x46, 0x14, 0x76, 0x03, 0x45, 0x83, 0x6e, 0x8a, 0xb6, 0xf4, 0x66, 0x23, 0x31 }
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


def check_xarch_docs_contract(root: Path) -> list[str]:
    for relative in XARCH_DOC_REQUIRED_FILES:
        if not (root / relative).exists():
            raise SmokeFailure(f"missing XArch docs contract file: {relative}")

    for relative in XARCH_DOC_REQUIRED_FILES:
        assert_contains(root / relative, "XArch")

    xarch_doc = root / "Docs" / "XArch.md"
    for token in XARCH_ARCH_TOKENS:
        assert_contains(xarch_doc, token)
    assert_contains(xarch_doc, "XArch does not replace edk2 ARCH values")
    assert_contains(xarch_doc, "--output PATH")
    assert_contains(xarch_doc, "Wrote XArch validation artifact")

    return ["PASS XArch docs/model static contract"]


def check_xarch_runner_contract(root: Path) -> list[str]:
    runner = root / XARCH_RUNNER
    if not runner.exists():
        raise SmokeFailure(f"missing XArch validation runner: {XARCH_RUNNER}")

    text = runner.read_text(encoding="utf-8")
    if not os.access(runner, os.X_OK):
        bash = shutil.which("bash")
        if bash is None:
            raise SmokeFailure(f"{XARCH_RUNNER} is not executable and bash is unavailable for syntax coverage")
        run([bash, "-n", str(runner)], cwd=root)

    for token in XARCH_ARCH_TOKENS + XARCH_TARGET_TOKENS:
        if token not in text:
            raise SmokeFailure(f"{XARCH_RUNNER} missing XArch runner token: {token}")

    for token in ("--target", "--all", "--mode", "dry-run", "--format", "--output"):
        if token not in text:
            raise SmokeFailure(f"{XARCH_RUNNER} missing CLI contract token: {token}")

    for token in ("build-ovmf-x64.sh", "build-armvirt.sh", "build-loongarchvirt.sh", "build-riscvvirt.sh"):
        if token not in text:
            raise SmokeFailure(f"{XARCH_RUNNER} missing intended script reference: {token}")

    for prohibited in XARCH_RUNNER_FORBIDDEN_MUTATION_TOKENS:
        if prohibited in text:
            raise SmokeFailure(f"{XARCH_RUNNER} contains prohibited firmware-mutation token: {prohibited}")

    forbidden_runtime_patterns = (
        r"\bbuild\s+-p\b",
        r"\bqemu-system-",
    )
    for pattern in forbidden_runtime_patterns:
        if re.search(pattern, text):
            raise SmokeFailure(f"{XARCH_RUNNER} appears to invoke a heavy validation command: {pattern}")

    return ["PASS XArch validation runner static contract"]


def check_xarch_runner_artifact_output(root: Path) -> list[str]:
    bash = shutil.which("bash")
    if bash is None:
        return ["SKIP XArch validation artifact output: bash not found"]

    runner = root / XARCH_RUNNER
    with tempfile.TemporaryDirectory(prefix="xarch-validation-smoke-") as tmp:
        out_dir = Path(tmp) / "reports" / "nested"
        markdown_path = out_dir / "xarch-validation.md"
        json_path = out_dir / "xarch-validation.json"

        markdown_result = run(
            [
                bash,
                str(runner),
                "--all",
                "--mode",
                "dry-run",
                "--format",
                "markdown",
                "--output",
                str(markdown_path),
            ],
            cwd=root,
        )
        expected_markdown_status = f"Wrote XArch validation artifact: {markdown_path}"
        if markdown_result.stdout.strip() != expected_markdown_status:
            raise SmokeFailure("XArch markdown artifact status line mismatch")
        markdown_text = markdown_path.read_text(encoding="utf-8")
        for token in ("# XArch validation report", "| Target |", "X64 / OVMF X64", "RISCV64"):
            if token not in markdown_text:
                raise SmokeFailure(f"XArch markdown artifact missing token: {token}")

        json_result = run(
            [
                bash,
                str(runner),
                "--all",
                "--mode",
                "dry-run",
                "--format",
                "json",
                "--output",
                str(json_path),
            ],
            cwd=root,
        )
        expected_json_status = f"Wrote XArch validation artifact: {json_path}"
        if json_result.stdout.strip() != expected_json_status:
            raise SmokeFailure("XArch JSON artifact status line mismatch")
        payload = json.loads(json_path.read_text(encoding="utf-8"))
        if payload.get("mode") != "dry-run":
            raise SmokeFailure("XArch JSON artifact mode mismatch")
        targets = payload.get("targets")
        if not isinstance(targets, list) or len(targets) != len(XARCH_TARGET_TOKENS):
            raise SmokeFailure("XArch JSON artifact target list mismatch")
        for target in targets:
            if target.get("result") != "PASS":
                raise SmokeFailure(f"XArch JSON artifact target did not pass: {target}")

    return ["PASS XArch validation artifact output smoke"]


def check_edk2_baseline_contract(root: Path) -> list[str]:
    gitmodules = root / ".gitmodules"
    workspace_helper = root / "Scripts" / "edk2-workspace.sh"
    bootstrap_helper = root / "Scripts" / "bootstrap-edk2.sh"
    baseline_doc = root / "Docs" / "BASELINE.md"

    for path in (gitmodules, workspace_helper, bootstrap_helper, baseline_doc):
        if not path.exists():
            raise SmokeFailure(f"missing edk2 baseline contract file: {path.relative_to(root)}")

    gitmodules_text = gitmodules.read_text(encoding="utf-8")
    for token in ("External/edk2", "https://github.com/tianocore/edk2.git"):
        if token not in gitmodules_text:
            raise SmokeFailure(f".gitmodules missing edk2 baseline token: {token}")

    workspace_text = workspace_helper.read_text(encoding="utf-8")
    for token in ("DetectWorkspace", "External/edk2", "ConfigureModernSetupPackagePath"):
        if token not in workspace_text:
            raise SmokeFailure(f"Scripts/edk2-workspace.sh missing contract token: {token}")

    bootstrap_text = bootstrap_helper.read_text(encoding="utf-8")
    for token in (
        "git submodule update --init -- External/edk2",
        "submodule update --init --checkout",
        "OpenSSL",
        "optional nested",
        "MbedTLS",
        "framework",
        "BUILD_BASETOOLS",
    ):
        if token not in bootstrap_text:
            raise SmokeFailure(f"Scripts/bootstrap-edk2.sh missing contract token: {token}")

    baseline_text = baseline_doc.read_text(encoding="utf-8")
    baseline_text_lower = baseline_text.lower()
    for token in (EDK2_BASELINE_SHA, "External/edk2"):
        if token not in baseline_text:
            raise SmokeFailure(f"Docs/BASELINE.md missing baseline token: {token}")
    for token in ("reproducible", "build", "qemu"):
        if token not in baseline_text_lower:
            raise SmokeFailure(f"Docs/BASELINE.md missing baseline concept: {token}")

    for relative in EDK2_BASELINE_REQUIRED_SCRIPT_REFS:
        script = root / relative
        if not script.exists():
            raise SmokeFailure(f"missing edk2 workspace consumer script: {relative}")
        if "edk2-workspace.sh" not in script.read_text(encoding="utf-8"):
            raise SmokeFailure(f"{relative} does not source/reference edk2-workspace.sh")

    return ["PASS edk2 baseline submodule/docs/script contract"]


def check_ovmf_capture_helper_contract(root: Path) -> list[str]:
    script = root / OVMF_CAPTURE_HELPER
    baseline_doc = root / "Docs" / "BASELINE.md"
    manual_doc = root / OVMF_CAPTURE_DOC

    for path in (script, baseline_doc, manual_doc):
        if not path.exists():
            raise SmokeFailure(f"missing OVMF capture contract file: {path.relative_to(root)}")

    script_text = script.read_text(encoding="utf-8")
    required_script_tokens = (
        "edk2-workspace.sh",
        "DetectWorkspace",
        "OVMF_CODE",
        "OVMF_VARS",
        "Build/OvmfX64*/",
        "ESP_DIR",
        "EFI/BOOT/BOOTX64.EFI",
        "BOOT_APP",
        "QEMU_BIN",
        "CAPTURE_OUT_DIR",
        "TMPDIR",
        "CAPTURE_WORK_DIR",
        "Build/ModernSetupPkgCapture/OvmfX64",
        "CAPTURE_PREFIX",
        "^[A-Za-z0-9._-]+$",
        "CLEANUP_QEMU",
        "RESET_VARS",
        "monitor.sock",
        "serial.log",
        "-daemonize",
        "-pidfile",
        "-display none",
        "-vga std",
        "usb-kbd",
        "BOOT_WAIT_SECONDS",
        "SENDKEY_SEQUENCE",
        "screendump",
        "pnmtopng",
        "magick",
        "convert",
        "sips",
        "system OVMF",
        "Secure Boot",
    )
    for token in required_script_tokens:
        if token not in script_text:
            raise SmokeFailure(f"{OVMF_CAPTURE_HELPER} missing capture helper token: {token}")

    prohibited_fragments = (
        "SetVariable",
        "HiiSetBrowserData",
        "HiiUpdateForm",
        "rm -rf",
        "rm -f ${CAPTURE_OUT_DIR}",
        "rm -f \"${CAPTURE_OUT_DIR}",
    )
    for fragment in prohibited_fragments:
        if fragment in script_text:
            raise SmokeFailure(f"{OVMF_CAPTURE_HELPER} contains prohibited capture helper fragment: {fragment}")

    docs_text = "\n".join(
        (baseline_doc.read_text(encoding="utf-8"), manual_doc.read_text(encoding="utf-8"))
    )
    for token in ("capture-ovmf-x64.sh", "screendump", "BOOT_WAIT_SECONDS", "SENDKEY_SEQUENCE", "TMPDIR", "Assets/Screenshots/manual"):
        if token not in docs_text:
            raise SmokeFailure(f"OVMF capture docs missing token: {token}")

    return ["PASS OVMF X64 QEMU screendump capture helper/docs contract"]


def check_displayengine_ovmf_visual_validation_contract(root: Path) -> list[str]:
    script = root / DISPLAYENGINE_OVMF_VISUAL_HELPER
    manual_doc = root / DISPLAYENGINE_OVMF_VISUAL_DOC
    validation_doc = root / "Docs" / "ProductizationValidationMatrix.md"
    validation_doc_zh = root / "Docs" / "ProductizationValidationMatrix.zh-CN.md"

    for path in (script, manual_doc, validation_doc, validation_doc_zh):
        if not path.exists():
            raise SmokeFailure(f"missing Phase35 DisplayEngine visual validation file: {path.relative_to(root)}")

    script_text = script.read_text(encoding="utf-8")
    required_script_tokens = (
        "edk2-workspace.sh",
        "DetectWorkspace",
        "Scripts/build-ovmf-x64.sh",
        "Scripts/capture-ovmf-x64.sh",
        "MODERN_SETUP_DISPLAY_ENGINE=native",
        "MODERN_SETUP_DISPLAY_ENGINE=modern",
        "variants=(native modern)",
        "GENERATE_ONLY",
        "Build/ModernSetupPkgOverlay",
        "CAPTURE_OUT_DIR",
        "${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64",
        "CAPTURE_WORK_DIR",
        "Build/ModernSetupPkgCapture/DisplayEngineOvmfX64",
        "overlays/${variant}",
        "firmware/${variant}",
        "displayengine-ovmf-x64-${variant}",
        "--mode dry-run|generate-only|build|capture",
        "BOOT_WAIT_SECONDS",
        "SENDKEY_SEQUENCE",
        "screendump",
        "does not inspect pixels",
    )
    for token in required_script_tokens:
        if token not in script_text:
            raise SmokeFailure(f"{DISPLAYENGINE_OVMF_VISUAL_HELPER} missing Phase35 token: {token}")

    prohibited_script_fragments = (
        "rm -rf",
        "SetVariable",
        "HiiSetBrowserData",
        "HiiUpdateForm",
        "RouteConfig",
        "ExtractConfig",
        "EFI_HII_CONFIG_ACCESS_PROTOCOL",
    )
    for fragment in prohibited_script_fragments:
        if fragment in script_text:
            raise SmokeFailure(f"{DISPLAYENGINE_OVMF_VISUAL_HELPER} contains prohibited Phase35 fragment: {fragment}")

    doc_text = "\n".join(
        (
            manual_doc.read_text(encoding="utf-8"),
            validation_doc.read_text(encoding="utf-8"),
            validation_doc_zh.read_text(encoding="utf-8"),
        )
    )
    required_doc_tokens = (
        "capture-displayengine-ovmf-x64.sh",
        "MODERN_SETUP_DISPLAY_ENGINE=native",
        "MODERN_SETUP_DISPLAY_ENGINE=modern",
        "${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64",
        "overlays/native",
        "overlays/modern",
        "firmware/native",
        "firmware/modern",
        "Static smoke",
        "Generate-only",
        "Build",
        "QEMU boot",
        "Visual screenshot",
        "does not inspect pixels",
        "not mark visual equivalence as verified",
    )
    for token in required_doc_tokens:
        if token not in doc_text:
            raise SmokeFailure(f"Phase35 DisplayEngine visual docs missing token: {token}")

    visual_verified_patterns = (
        re.compile(r"Phase35[^\n]{0,80}\bVerified\b", re.IGNORECASE),
        re.compile(r"DisplayEngine[^\n]{0,80}\bVerified\b", re.IGNORECASE),
    )
    for pattern in visual_verified_patterns:
        if pattern.search(doc_text):
            raise SmokeFailure("Phase35 DisplayEngine visual docs must not mark visual validation as Verified")

    return ["PASS Phase35 DisplayEngine OVMF X64 native-vs-modern visual validation foundation"]


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


def extract_c_function_body(text: str, function_name: str) -> str:
    pattern = re.compile(
        rf"(^|\n)\s*(?:STATIC\s+)?[A-Z_][A-Z0-9_\s\*]+\s+{re.escape(function_name)}\s*\([^;]*?\)\s*\{{",
        re.DOTALL,
    )
    match = pattern.search(text)
    if match is None:
        raise SmokeFailure(f"could not isolate C function body: {function_name}")

    depth = 0
    for index in range(match.end() - 1, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[match.start() : index + 1]

    raise SmokeFailure(f"unterminated C function body: {function_name}")


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
    if "#define MODERN_SETUP_NOINLINE" not in pages_body:
        raise SmokeFailure("ModernSetupAppPages.c missing MODERN_SETUP_NOINLINE guard for large draw helpers")
    if not re.search(r"\bVOID\s+MODERN_SETUP_NOINLINE\s+ModernSetupDrawCurrentPage\s*\(", pages_body):
        raise SmokeFailure("ModernSetupDrawCurrentPage must be MODERN_SETUP_NOINLINE to keep its dispatch frame bounded")
    for helper in APP_NOINLINE_DRAW_HELPERS:
        if not re.search(rf"\bSTATIC\s+VOID\s+MODERN_SETUP_NOINLINE\s+{helper}\s*\(", pages_body):
            raise SmokeFailure(f"ModernSetupAppPages.c must mark {helper} MODERN_SETUP_NOINLINE")
    if re.search(r"\bMODERN_UI_HII_VIEW\s+View\s*;", pages_body):
        raise SmokeFailure("DrawHiiReadOnlyPreview must not place MODERN_UI_HII_VIEW on the UEFI stack")
    if "AllocateZeroPool (sizeof (*View))" not in pages_body:
        raise SmokeFailure("DrawHiiReadOnlyPreview must heap-allocate MODERN_UI_HII_VIEW with AllocateZeroPool")
    if "ModernUiHiiBridgeClearView (View)" not in pages_body or "FreePool (View)" not in pages_body:
        raise SmokeFailure("DrawHiiReadOnlyPreview must clear and free its heap-allocated MODERN_UI_HII_VIEW")

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
    layout_defines = {
        match.group(1): int(match.group(2))
        for match in re.finditer(
            r"#define\s+(DASHBOARD_(?:SECTION_TITLE_TOP|QUICK_CARD_TOP|QUICK_CARD_GAP|QUICK_GROUP_LABEL_OFFSET))\s+(\d+)",
            internal_body,
        )
    }
    for layout_token in (
        "DASHBOARD_SECTION_TITLE_TOP",
        "DASHBOARD_QUICK_CARD_TOP",
        "DASHBOARD_QUICK_CARD_GAP",
        "DASHBOARD_QUICK_GROUP_LABEL_OFFSET",
    ):
        if layout_token not in layout_defines:
            raise SmokeFailure(f"ModernSetupAppInternal.h missing Dashboard layout constant: {layout_token}")
    if "DASHBOARD_SECTION_TITLE_TOP" not in dashboard_body or "DASHBOARD_QUICK_GROUP_LABEL_OFFSET" not in dashboard_body:
        raise SmokeFailure("ModernSetupAppDashboard.c must use named Dashboard title/group spacing constants")
    if layout_defines["DASHBOARD_QUICK_GROUP_LABEL_OFFSET"] < 24:
        raise SmokeFailure("Dashboard group labels need at least 24px clearance above quick cards")
    if layout_defines["DASHBOARD_QUICK_CARD_GAP"] - layout_defines["DASHBOARD_QUICK_GROUP_LABEL_OFFSET"] < 8:
        raise SmokeFailure("Dashboard lower-row group labels need a clear lane below the previous row")
    if (
        layout_defines["DASHBOARD_QUICK_CARD_TOP"] - layout_defines["DASHBOARD_QUICK_GROUP_LABEL_OFFSET"]
        < layout_defines["DASHBOARD_SECTION_TITLE_TOP"] + 24
    ):
        raise SmokeFailure("Dashboard group labels must sit clearly below the section title before cards begin")
    if "ModernSetupGetDashboardQuickGrid" not in actions_body or "MODERN_SETUP_DASHBOARD_QUICK_GRID" not in internal_body:
        raise SmokeFailure("Dashboard quick-card layout must use a shared grid helper contract")
    if "MODERN_SETUP_PAGE_LIST_LAYOUT" not in internal_body:
        raise SmokeFailure("ModernSetupAppInternal.h missing shared page-list layout contract")
    for helper in ("DrawBoot", "DrawDevices", "DrawProviderSummaryPage"):
        helper_body = extract_c_function_body(pages_body, helper)
        if "ModernSetupGetPageListLayout" not in helper_body:
            raise SmokeFailure(f"{helper} must use the shared page-list layout helper")
    for helper, forbidden_tokens in {
        "DrawBoot": ("* 58", "+ 62", "Panel.Width - 40", "Panel.X + 20"),
        "DrawDevices": ("Panel.Width - 40", "Panel.X + 20", ">= 720"),
    }.items():
        helper_body = extract_c_function_body(pages_body, helper)
        for token in forbidden_tokens:
            if token in helper_body:
                raise SmokeFailure(f"{helper} still contains hardcoded page-list geometry token: {token}")
    page_layout_body = extract_c_function_body(actions_body, "ModernSetupGetPageListLayout")
    if page_layout_body.count("Compact ?") < 2:
        raise SmokeFailure("ModernSetupGetPageListLayout must expose compact and comfortable density branches")
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


def check_phase25_server_inventory_summary(root: Path) -> list[str]:
    app_dir = root / MODERN_SETUP_APP_DIR
    pages = app_dir / "ModernSetupAppPages.c"
    dashboard = app_dir / "ModernSetupAppDashboard.c"
    chrome = app_dir / "ModernSetupAppChrome.c"
    actions = app_dir / "ModernSetupAppActions.c"
    internal = app_dir / "ModernSetupAppInternal.h"
    provider = app_dir / "ModernSetupAppProvider.c"

    pages_body = strip_c_comments(pages.read_text(encoding="utf-8"))
    dashboard_body = strip_c_comments(dashboard.read_text(encoding="utf-8"))
    chrome_body = strip_c_comments(chrome.read_text(encoding="utf-8"))
    actions_body = strip_c_comments(actions.read_text(encoding="utf-8"))
    internal_body = strip_c_comments(internal.read_text(encoding="utf-8"))
    provider_body = strip_c_comments(provider.read_text(encoding="utf-8"))

    if not re.search(r"\bSTATIC\s+VOID\s+MODERN_SETUP_NOINLINE\s+DrawServerInventorySummary\s*\(", pages_body):
        raise SmokeFailure("DrawServerInventorySummary must exist and be MODERN_SETUP_NOINLINE")

    start = pages_body.find("DrawServerInventorySummary (")
    end = pages_body.find("DrawPreferences (", start)
    if start < 0 or end < 0:
        raise SmokeFailure("could not isolate DrawServerInventorySummary body")
    server_body = pages_body[start:end]

    for token in (
        "ModernSetupGetProviderSnapshot (&Providers)",
        "ModernSetupGetProviderHealthSummary (&Providers, &ProviderHealth)",
        "Providers.Management",
        "Providers.Performance",
        "Providers.Pcie",
        "Providers.Diagnostics",
        "Providers.Platform",
        "Providers.Firmware",
        "ProviderHealth.",
        "Read-only",
        "Native HII/FormBrowser owns policy changes",
    ):
        if token not in server_body:
            raise SmokeFailure(f"Server Inventory summary missing required token: {token}")

    if not re.search(r"EFI_ERROR\s*\(\s*Providers\.PcieStatus\s*\)", server_body):
        raise SmokeFailure("Server Inventory must gate PCIe counts on Providers.PcieStatus")
    for token in ("ModernUiPcieDataGetSummary", "ModernUiManagementDataGetSummary", "ModernUiPerformanceDataGetSummary"):
        if token in pages_body or token in dashboard_body or token in actions_body:
            raise SmokeFailure(f"UI page/dashboard/actions bypass provider snapshot with direct provider call: {token}")
        if token not in provider_body:
            raise SmokeFailure(f"ModernSetupAppProvider.c missing provider summary call: {token}")

    for token in (
        "PageServerInventory",
        "ModernUiStringPageServerInventory",
        "ModernUiStringPageServerInventoryHint",
    ):
        if token not in internal_body + chrome_body:
            raise SmokeFailure(f"Server Inventory page/chrome missing route token: {token}")
    if "case PageServerInventory:" not in pages_body or "DrawServerInventorySummary (Ui, Theme, Focus)" not in pages_body:
        raise SmokeFailure("Server Inventory page dispatch is missing")
    if "PageServerInventory" not in actions_body or "mDashboardCategoryRoutes[DASHBOARD_QUICK_CARD_COUNT]" not in actions_body:
        raise SmokeFailure("Server Inventory dashboard route is missing or route table is not count-aligned")
    if "Server Inventory" not in dashboard_body or "ServerValueText" not in dashboard_body:
        raise SmokeFailure("Dashboard missing Server Inventory card")

    dec_text = (root / "ModernSetupPkg.dec").read_text(encoding="utf-8")
    for token in ("PcdServerInventory", "ServerInventoryVar", "ServerInventoryPolicy"):
        if token in dec_text:
            raise SmokeFailure(f"writable/setup token unexpectedly added for Server Inventory: {token}")

    return ["PASS Phase25 Server Inventory read-only summary/dashboard contract"]


def check_modern_setup_app_preferences_boundary(root: Path) -> list[str]:
    for required in PREFERENCES_REQUIRED_FILES:
        if not (root / required).exists():
            raise SmokeFailure(f"missing preferences framework file: {required}")

    header_text = (root / PREFERENCES_REQUIRED_FILES[0]).read_text(encoding="utf-8")
    lib_text = strip_c_comments((root / PREFERENCES_REQUIRED_FILES[1]).read_text(encoding="utf-8"))
    dec_text = (root / "ModernSetupPkg.dec").read_text(encoding="utf-8")
    dsc_text = (root / "Experimental" / "ModernSetupApp.dsc").read_text(encoding="utf-8")
    app_inf_text = (root / MODERN_SETUP_APP_INF).read_text(encoding="utf-8")
    app_text = "\n".join(
        strip_c_comments(path.read_text(encoding="utf-8"))
        for path in sorted((root / MODERN_SETUP_APP_DIR).glob("ModernSetupApp*.c"))
    )
    combined = "\n".join((header_text, lib_text, dec_text, dsc_text, app_inf_text, app_text))

    for token in PREFERENCES_APP_TOKENS:
        if token not in combined:
            raise SmokeFailure(f"preferences framework missing token: {token}")
    for token in PREFERENCES_APP_TOKENS[:3]:
        if token not in app_text:
            raise SmokeFailure(f"ModernSetupApp UI does not use preferences API: {token}")

    if "EFI_VARIABLE_RUNTIME_ACCESS" in lib_text:
        raise SmokeFailure("preferences library must not use runtime variable access for app-owned UX preferences")
    if "SetVariable" not in lib_text or "GetVariable" not in lib_text:
        raise SmokeFailure("preferences variable IO must be centralized in ModernUiPreferencesLib")
    for token in PREFERENCES_FORBIDDEN_PLATFORM_TOKENS:
        if token in lib_text:
            raise SmokeFailure(f"preferences library references prohibited platform-policy token: {token}")

    return ["PASS ModernSetupApp app-owned preferences boundary"]


def check_phase26_interactive_app_owned_preferences(root: Path) -> list[str]:
    app_dir = root / MODERN_SETUP_APP_DIR
    app_main = strip_c_comments((app_dir / "ModernSetupApp.c").read_text(encoding="utf-8"))
    actions = strip_c_comments((app_dir / "ModernSetupAppActions.c").read_text(encoding="utf-8"))
    chrome = strip_c_comments((app_dir / "ModernSetupAppChrome.c").read_text(encoding="utf-8"))
    pages = strip_c_comments((app_dir / "ModernSetupAppPages.c").read_text(encoding="utf-8"))
    internal = strip_c_comments((app_dir / "ModernSetupAppInternal.h").read_text(encoding="utf-8"))
    app_sources = "\n".join((app_main, actions, chrome, pages, internal))

    required_tokens = (
        "MODERN_SETUP_PREFERENCE_ROW_THEME",
        "MODERN_SETUP_PREFERENCE_ROW_DASHBOARD_DENSITY",
        "MODERN_SETUP_PREFERENCE_ROW_REMEMBER_LAST_PAGE",
        "MODERN_SETUP_PREFERENCE_ROW_SHOW_ADVANCED_HINTS",
        "MODERN_SETUP_PREFERENCE_ROW_CONFIRM_RESET",
        "MODERN_SETUP_PREFERENCE_ROW_COUNT",
        "mModernSetupPreferencePopupOpen",
        "mModernSetupPreferencePopupSelection",
        "ModernSetupHandlePreferencePopupUp",
        "ModernSetupHandlePreferencePopupDown",
        "ModernSetupCancelPreferencePopup",
        "ModernSetupCommitPreferencePopup",
        "ModernSetupGetPreferenceChoiceCount",
        "ModernSetupGetPreferenceChoiceName",
        "ModernSetupGetPreferenceValueName",
        "ModernSetupPreferenceCheckboxValueText",
        "ModernUiValueOneOf",
        "ModernUiValueCheckbox",
        "ModernUiEngineDrawPopup",
    )
    for token in required_tokens:
        if token not in app_sources:
            raise SmokeFailure(f"Phase26 interactive Preferences missing token: {token}")

    for field in ("ThemeId", "DashboardDensity", "RememberLastPage", "ShowAdvancedHints", "ConfirmReset"):
        if f"mModernSetupPreferences.{field}" not in app_sources:
            raise SmokeFailure(f"Phase26 Preferences UI does not bind field: {field}")

    if "return MODERN_SETUP_PREFERENCE_ROW_COUNT" not in actions:
        raise SmokeFailure("Preferences selectable count must use MODERN_SETUP_PREFERENCE_ROW_COUNT")
    if "ModernUiPreferencesSave (&mModernSetupPreferences)" not in actions:
        raise SmokeFailure("Preferences controls must persist through ModernUiPreferencesSave")
    if actions.count("PersistPreferencesAndStatus (StatusMessage, StatusSize)") < 4:
        raise SmokeFailure("Preferences one-of and checkbox controls must share the save path")
    if "ModernSetupCommitPreferencePopup (StatusMessage, sizeof (StatusMessage))" not in app_main:
        raise SmokeFailure("Enter must commit an open Preferences one-of popup")
    if "ModernSetupCancelPreferencePopup ()" not in app_main:
        raise SmokeFailure("Esc/navigation must cancel an open Preferences popup")
    if "ModernSetupHandlePreferencePopupUp ()" not in app_main or "ModernSetupHandlePreferencePopupDown ()" not in app_main:
        raise SmokeFailure("Up/Down must move an open Preferences popup selection")
    if "ModernSetupGetCompactTabLabel" not in chrome or "L\"Perf\"" not in chrome or "L\"Mgmt\"" not in chrome:
        raise SmokeFailure("Phase26 top navigation must use compact IBV-style tab labels")
    if "Tabs[Index].Text = ModernUiGetString (mPages[Index].Title)" in chrome:
        raise SmokeFailure("Phase26 top navigation must not use full page titles as tab labels")

    return ["PASS Phase26 interactive Preferences controls and compact top navigation contract"]


def check_phase27_app_owned_input_preferences(root: Path) -> list[str]:
    app_dir = root / MODERN_SETUP_APP_DIR
    app_main = strip_c_comments((app_dir / "ModernSetupApp.c").read_text(encoding="utf-8"))
    actions = strip_c_comments((app_dir / "ModernSetupAppActions.c").read_text(encoding="utf-8"))
    pages = strip_c_comments((app_dir / "ModernSetupAppPages.c").read_text(encoding="utf-8"))
    internal = strip_c_comments((app_dir / "ModernSetupAppInternal.h").read_text(encoding="utf-8"))
    header = strip_c_comments((root / "Include" / "ModernUi" / "ModernUiPreferences.h").read_text(encoding="utf-8"))
    lib = strip_c_comments((root / "Library" / "ModernUiPreferencesLib" / "ModernUiPreferencesLib.c").read_text(encoding="utf-8"))
    app_sources = "\n".join((app_main, actions, pages, internal))
    prefs_sources = "\n".join((header, lib))

    for token in (
        "MODERN_SETUP_PREFERENCE_ROW_BOOT_TIMEOUT",
        "MODERN_SETUP_PREFERENCE_ROW_PROFILE_NAME",
        "ModernSetupPreferencePopupNumericInput",
        "ModernSetupPreferencePopupStringInput",
        "ModernSetupHandlePreferenceInputKey",
        "mModernSetupPreferenceInputBuffer",
        "mModernSetupPreferenceInputLength",
        "ModernUiValueNumeric",
        "ModernUiValueString",
        "UI Boot Countdown",
        "Setup Profile Name",
        "Digits only, range 0..30",
        "Printable ASCII, max 31 chars",
    ):
        if token not in app_sources:
            raise SmokeFailure(f"Phase27 input Preferences UI missing token: {token}")

    for token in (
        "BootTimeoutSeconds",
        "ProfileName",
        "MODERN_UI_PREFERENCES_BOOT_TIMEOUT_MAX",
        "MODERN_UI_PREFERENCES_BOOT_TIMEOUT_DEFAULT",
        "MODERN_UI_PREFERENCES_PROFILE_NAME_CHARS",
        "Default Profile",
        "ValidateProfileName",
    ):
        if token not in prefs_sources:
            raise SmokeFailure(f"Phase27 persisted input preference schema missing token: {token}")

    if "#define MODERN_UI_PREFERENCES_VERSION        2" not in header:
        raise SmokeFailure("Phase27 preference schema must bump the typed version")
    if "EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS" not in lib:
        raise SmokeFailure("Preferences variable attributes must remain NV+BS only")
    if "EFI_VARIABLE_RUNTIME_ACCESS" in lib:
        raise SmokeFailure("Preferences variable attributes must reject runtime access")
    if "Event.Type == ModernUiInputOther" not in app_main:
        raise SmokeFailure("Printable/backspace input must be routed to Preferences input popup")
    if "CHAR_BACKSPACE" not in actions:
        raise SmokeFailure("Preferences input popup must handle backspace")
    if "ModernSetupCommitPreferencePopup (StatusMessage, sizeof (StatusMessage))" not in app_main:
        raise SmokeFailure("Enter must commit numeric/string Preferences input popups")
    if "ModernSetupCancelPreferencePopup ()" not in app_main:
        raise SmokeFailure("Esc/navigation must cancel Preferences input popups")
    if "ModernUiPreferencesSave (&mModernSetupPreferences)" not in actions:
        raise SmokeFailure("Input preference commit must persist through ModernUiPreferencesLib")
    if "mModernSetupPreferences.BootTimeoutSeconds" not in actions or "mModernSetupPreferences.ProfileName" not in actions:
        raise SmokeFailure("Input preference commit must bind both persisted fields")

    for path in sorted(app_dir.glob("ModernSetupApp*.c")):
        body = strip_c_comments(path.read_text(encoding="utf-8"))
        if "SetVariable" in body:
            raise SmokeFailure(f"{path.relative_to(root)} directly calls/references SetVariable")
    for token in ("BootOrder", "Boot####", "SecureBoot", "PCIe", "CPU", "MemoryPolicy", "Fan", "Chipset"):
        if token in lib:
            raise SmokeFailure(f"preferences library references prohibited platform-policy token: {token}")
    if "ModernSetupGetCompactTabLabel" not in strip_c_comments((app_dir / "ModernSetupAppChrome.c").read_text(encoding="utf-8")):
        raise SmokeFailure("Compact top navigation guard is missing")

    return ["PASS Phase27 app-owned numeric/string input Preferences contract"]


def check_phase28_runtime_theme_switching(root: Path) -> list[str]:
    app_dir = root / MODERN_SETUP_APP_DIR
    app_main = strip_c_comments((app_dir / "ModernSetupApp.c").read_text(encoding="utf-8"))
    actions = strip_c_comments((app_dir / "ModernSetupAppActions.c").read_text(encoding="utf-8"))
    theme_header = strip_c_comments((root / "Include" / "ModernUi" / "ModernUiTheme.h").read_text(encoding="utf-8"))
    prefs_header = strip_c_comments((root / "Include" / "ModernUi" / "ModernUiPreferences.h").read_text(encoding="utf-8"))
    theme_lib = strip_c_comments((root / "Library" / "ModernUiThemeLib" / "ModernUiThemeLib.c").read_text(encoding="utf-8"))

    for token in (
        "MODERN_UI_PREFERENCES_THEME_GRAPHITE_GOLD",
        "MODERN_UI_PREFERENCES_THEME_MAX",
        "ModernUiGetThemeForPreference",
    ):
        if token not in (prefs_header + theme_header + theme_lib + app_main):
            raise SmokeFailure(f"Phase28 runtime theme switching missing token: {token}")
    if "MODERN_UI_PREFERENCES_THEME_GRAPHITE_GOLD" not in prefs_header:
        raise SmokeFailure("Phase28 preference schema must append the Graphite Gold theme id")
    if "MODERN_UI_PREFERENCES_THEME_MAX" not in prefs_header or "MODERN_UI_PREFERENCES_THEME_GRAPHITE_GOLD" not in prefs_header:
        raise SmokeFailure("Phase28 preference schema max must include the premium theme")
    if "mGraphiteGoldTheme" not in theme_lib:
        raise SmokeFailure("Phase28 premium Graphite Gold palette is missing")
    if "case MODERN_UI_PREFERENCES_THEME_GRAPHITE_GOLD:" not in theme_lib:
        raise SmokeFailure("Phase28 runtime theme resolver must map Graphite Gold")
    if "Graphite Gold" not in actions or "return 4;" not in actions:
        raise SmokeFailure("Phase28 Preferences theme choices must expose Graphite Gold")
    if app_main.count("ModernUiGetThemeForPreference (mModernSetupPreferences.ThemeId)") < 2:
        raise SmokeFailure("ModernSetupApp must resolve runtime theme on initial load and redraw")
    if "ModernUiGetTheme ();" in app_main:
        raise SmokeFailure("ModernSetupApp must not ignore app-owned ThemeId with direct ModernUiGetTheme use")
    for vendor_token in PROHIBITED_THEME_ALIAS_TOKENS:
        if vendor_token in (theme_lib + actions + prefs_header).lower():
            raise SmokeFailure(f"Phase28 runtime theme uses prohibited vendor alias: {vendor_token}")

    return ["PASS Phase28 runtime Theme preference applies app-owned palettes"]


def check_phase29_dashboard_density_layout(root: Path) -> list[str]:
    app_dir = root / MODERN_SETUP_APP_DIR
    app_main = strip_c_comments((app_dir / "ModernSetupApp.c").read_text(encoding="utf-8"))
    actions = strip_c_comments((app_dir / "ModernSetupAppActions.c").read_text(encoding="utf-8"))
    dashboard = strip_c_comments((app_dir / "ModernSetupAppDashboard.c").read_text(encoding="utf-8"))
    internal = strip_c_comments((app_dir / "ModernSetupAppInternal.h").read_text(encoding="utf-8"))

    if "ModernSetupGetDashboardQuickGrid (" not in internal or "DashboardDensity" not in internal:
        raise SmokeFailure("Phase29 Dashboard grid helper must accept DashboardDensity")
    if "ModernUiDashboardDensityCompact" not in actions:
        raise SmokeFailure("Phase29 Dashboard grid helper must branch on Compact density")
    for token in ("Compact ? 20 : DASHBOARD_QUICK_CARD_GAP", "Compact ? 42 : DASHBOARD_QUICK_CARD_TOP", "Compact ? 10 : 16"):
        if token not in actions:
            raise SmokeFailure(f"Phase29 compact Dashboard grid missing layout token: {token}")
    if "Compact ? ((Content.Height >= 460) ? 236 : 204)" not in actions:
        raise SmokeFailure("Phase29 compact Dashboard must reduce the top summary height")
    if app_main.count("mModernSetupPreferences.DashboardDensity") < 4:
        raise SmokeFailure("Phase29 Dashboard navigation must use the same density-aware grid as rendering")
    if "ModernSetupGetDashboardQuickGrid (Ui, mModernSetupPreferences.DashboardDensity, &Grid)" not in dashboard:
        raise SmokeFailure("Phase29 Dashboard rendering must use the density-aware grid")
    if "mModernSetupPreferences.DashboardDensity == ModernUiDashboardDensityCompact" not in dashboard:
        raise SmokeFailure("Phase29 Dashboard top summary layout must react to Compact density")
    if any(token in dashboard + actions for token in ("SetVariable", "ExtractConfig", "RouteConfig", "HiiSetBrowserData")):
        raise SmokeFailure("Phase29 DashboardDensity layout must not introduce HII or variable writes")

    return ["PASS Phase29 DashboardDensity controls Dashboard layout density"]


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


def check_hardware_health_demo_provider(root: Path) -> list[str]:
    for relative in HARDWARE_HEALTH_PROVIDER_REQUIRED_FILES:
        if not (root / relative).exists():
            raise SmokeFailure(f"missing Hardware Health demo provider file: {relative}")

    header = root / "Include" / "ModernUi" / "ModernUiHardwareHealthData.h"
    lib_c = root / "Library" / "ModernUiHardwareHealthDataLib" / "ModernUiHardwareHealthDataLib.c"
    lib_inf = root / "Library" / "ModernUiHardwareHealthDataLib" / "ModernUiHardwareHealthDataLib.inf"
    dec = root / "ModernSetupPkg.dec"
    experimental_dsc = root / "Experimental" / "ModernSetupApp.dsc"
    app_inf = root / MODERN_SETUP_APP_INF
    app_dir = root / MODERN_SETUP_APP_DIR
    provider = app_dir / "ModernSetupAppProvider.c"
    pages = app_dir / "ModernSetupAppPages.c"
    dashboard = app_dir / "ModernSetupAppDashboard.c"
    docs = root / "Docs" / "ProductizationFeatureMatrix.md"

    assert_contains(header, "MODERN_UI_HARDWARE_HEALTH_MAX_SENSORS")
    assert_contains(header, "MODERN_UI_HARDWARE_HEALTH_MAX_SAMPLES")
    assert_contains(header, "MODERN_UI_HARDWARE_SENSOR_TYPE")
    assert_contains(header, "MODERN_UI_HARDWARE_HEALTH_SENSOR")
    assert_contains(header, "MODERN_UI_HARDWARE_HEALTH_SUMMARY")
    assert_contains(header, HARDWARE_HEALTH_PROVIDER_SUMMARY_TOKEN)
    assert_contains(lib_c, "#include <ModernUi/ModernUiHardwareHealthData.h>")
    assert_contains(lib_c, HARDWARE_HEALTH_PROVIDER_SUMMARY_TOKEN)
    assert_contains(lib_c, "Demo provider")
    assert_contains(lib_c, "CPU Package")
    assert_contains(lib_c, "Board Ambient")
    assert_contains(lib_c, "VRM Zone")
    assert_contains(lib_inf, "BASE_NAME                      = ModernUiHardwareHealthDataLib")
    assert_contains(lib_inf, "LIBRARY_CLASS                  = ModernUiHardwareHealthDataLib|UEFI_APPLICATION")
    assert_contains(dec, "ModernUiHardwareHealthDataLib|Include/ModernUi/ModernUiHardwareHealthData.h")
    assert_contains(experimental_dsc, "ModernUiHardwareHealthDataLib|ModernSetupPkg/Library/ModernUiHardwareHealthDataLib/ModernUiHardwareHealthDataLib.inf")
    assert_contains(experimental_dsc, "ModernSetupPkg/Library/ModernUiHardwareHealthDataLib/ModernUiHardwareHealthDataLib.inf")
    assert_contains(app_inf, "ModernUiHardwareHealthDataLib")

    for source in (header, lib_c):
        body = strip_c_comments(source.read_text(encoding="utf-8"))
        for token in HARDWARE_HEALTH_FORBIDDEN_TOKENS:
            if re.search(rf"\b{re.escape(token)}\b", body):
                raise SmokeFailure(f"{source.relative_to(root)} contains prohibited Hardware Health token: {token}")

    for source in sorted(app_dir.glob("ModernSetupApp*.c")):
        body = strip_c_comments(source.read_text(encoding="utf-8"))
        if HARDWARE_HEALTH_PROVIDER_SUMMARY_TOKEN in body and source.name != "ModernSetupAppProvider.c":
            raise SmokeFailure(
                f"{source.relative_to(root)} bypasses ModernSetupAppProvider.c for Hardware Health provider summary"
            )

    provider_body = strip_c_comments(provider.read_text(encoding="utf-8"))
    pages_body = strip_c_comments(pages.read_text(encoding="utf-8"))
    dashboard_body = strip_c_comments(dashboard.read_text(encoding="utf-8"))
    if provider_body.count(HARDWARE_HEALTH_PROVIDER_SUMMARY_TOKEN) != 1:
        raise SmokeFailure("ModernSetupAppProvider.c must call Hardware Health provider summary exactly once")
    for token in ("HardwareHealth", "HardwareHealthStatus", "Hardware Health"):
        assert_contains(provider, token)
    for token in ("Providers.HardwareHealth", "DrawTemperatureTrendSparkline", "DrawHardwareHealthSensorRow"):
        if token not in pages_body:
            raise SmokeFailure(f"ModernSetupAppPages.c missing Hardware Health UI token: {token}")
    if "Providers.HardwareHealth" not in dashboard_body:
        raise SmokeFailure("ModernSetupAppDashboard.c must consume Hardware Health through the provider snapshot")

    doc_text = docs.read_text(encoding="utf-8").lower()
    for token in ("hardware health", "read-only", "demo", "native", "formbrowser"):
        if token not in doc_text:
            raise SmokeFailure(f"ProductizationFeatureMatrix.md missing Hardware Health documentation token: {token}")

    return ["PASS Hardware Health demo provider wiring and read-only boundary checks"]


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


def check_bilingual_documentation_contract(root: Path) -> list[str]:
    pairs = BILINGUAL_DOC_PAIRS + DOC_INDEX_PAIRS
    for english, chinese in pairs:
        english_path = root / english
        chinese_path = root / chinese
        if not english_path.exists():
            raise SmokeFailure(f"missing English bilingual doc: {english}")
        if not chinese_path.exists():
            raise SmokeFailure(f"missing zh-CN bilingual doc: {chinese}")

        english_text = english_path.read_text(encoding="utf-8")
        chinese_text = chinese_path.read_text(encoding="utf-8")
        if chinese.name not in english_text:
            raise SmokeFailure(f"{english} missing reciprocal zh-CN link to {chinese.name}")
        if english.name not in chinese_text:
            raise SmokeFailure(f"{chinese} missing reciprocal English link to {english.name}")

    readme = (root / "README.md").read_text(encoding="utf-8")
    for target in ("Docs/README.md", "Docs/README.zh-CN.md"):
        if target not in readme:
            raise SmokeFailure(f"README.md missing development-docs index link: {target}")

    english_ibv = root / "Docs" / "IbvAndPlatformSetupSurvey.md"
    english_ibv_text = english_ibv.read_text(encoding="utf-8")
    if IBV_CHINESE_TAXONOMY_HEADING in english_ibv_text:
        raise SmokeFailure("English IBV survey still contains Chinese-only taxonomy heading")
    if IBV_ENGLISH_TAXONOMY_HEADING not in english_ibv_text:
        raise SmokeFailure("English IBV survey missing Broad IBV / Platform Setup Taxonomy section")

    productization_zh = (root / "Docs" / "ProductizationFeatureMatrix.zh-CN.md").read_text(encoding="utf-8")
    for token in PRODUCTIZATION_ZH_PARITY_TOKENS:
        if token not in productization_zh:
            raise SmokeFailure(f"ProductizationFeatureMatrix.zh-CN.md missing parity token: {token}")

    return [f"PASS bilingual documentation pairs and IBV taxonomy split: {len(pairs)} pairs"]


def check_xarch_build_tokens_are_negated(relative: Path, text: str) -> None:
    for line_number, line in enumerate(text.splitlines(), start=1):
        for token in ("ARCH=XArch", "TARGET=XArch"):
            if token not in line:
                continue
            lowered = line.lower()
            if not any(cue.lower() in lowered for cue in PRODUCTIZATION_XARCH_NEGATION_CUES):
                raise SmokeFailure(
                    f"{relative}:{line_number} mentions {token} without an explicit same-line rejection"
                )


def check_phase30_productization_validation_matrix(root: Path) -> list[str]:
    english = root / PRODUCTIZATION_VALIDATION_DOCS[0]
    chinese = root / PRODUCTIZATION_VALIDATION_DOCS[1]
    for relative in PRODUCTIZATION_VALIDATION_DOCS:
        if not (root / relative).exists():
            raise SmokeFailure(f"missing Phase30 productization validation doc: {relative}")

    english_text = english.read_text(encoding="utf-8")
    chinese_text = chinese.read_text(encoding="utf-8")
    combined = english_text + "\n" + chinese_text

    if PRODUCTIZATION_VALIDATION_DOCS[1].name not in english_text:
        raise SmokeFailure("ProductizationValidationMatrix.md missing zh-CN cross-link")
    if PRODUCTIZATION_VALIDATION_DOCS[0].name not in chinese_text:
        raise SmokeFailure("ProductizationValidationMatrix.zh-CN.md missing English cross-link")

    for relative in PRODUCTIZATION_VALIDATION_LINK_SOURCES:
        if "ProductizationValidationMatrix" not in (root / relative).read_text(encoding="utf-8"):
            raise SmokeFailure(f"{relative} missing Phase30 validation matrix link/reference")

    for token in PRODUCTIZATION_VALIDATION_TARGET_TOKENS:
        if token not in combined:
            raise SmokeFailure(f"Phase30 validation matrix missing target/platform/script token: {token}")

    for relative, contracts in PRODUCTIZATION_VALIDATION_DOC_CONTRACTS:
        text = (root / relative).read_text(encoding="utf-8")
        for area, tokens in contracts.items():
            for token in tokens:
                if token not in text:
                    raise SmokeFailure(f"{relative} missing Phase30 {area} token: {token}")
        check_xarch_build_tokens_are_negated(relative, text)

    for required_heading in (
        "XArch Target Validation Matrix",
        "Product Class Validation Matrix",
        "App / Provider Validation Matrix",
        "XArch 目标验证矩阵",
        "产品类别验证矩阵",
        "App / Provider 验证矩阵",
    ):
        if required_heading not in combined:
            raise SmokeFailure(f"Phase30 validation matrix missing section: {required_heading}")

    for area in (
        "Dashboard",
        "Boot",
        "Devices / HII",
        "Security",
        "Firmware",
        "Diagnostics",
        "Management",
        "Power / Thermal",
        "Hardware Health",
        "Performance",
        "PCIe",
        "Preferences",
        "Exit",
    ):
        if area not in combined:
            raise SmokeFailure(f"Phase30 validation matrix missing App/provider area: {area}")

    bash = shutil.which("bash")
    if bash is None:
        return ["PASS Phase30 productization validation matrix docs; SKIP xarch JSON smoke: bash not found"]

    with tempfile.TemporaryDirectory(prefix="phase30-xarch-smoke-") as tmp:
        json_path = Path(tmp) / "xarch-validation.json"
        run(
            [
                bash,
                str(root / XARCH_RUNNER),
                "--all",
                "--mode",
                "dry-run",
                "--format",
                "json",
                "--output",
                str(json_path),
            ],
            cwd=root,
        )
        payload = json.loads(json_path.read_text(encoding="utf-8"))
        targets = payload.get("targets")
        if not isinstance(targets, list) or len(targets) != 4:
            raise SmokeFailure("Phase30 xarch JSON smoke expected four targets")
        for target in targets:
            if target.get("result") != "PASS":
                raise SmokeFailure(f"Phase30 xarch JSON target did not pass: {target}")
        riscv = [target for target in targets if target.get("edk2_arch") == "RISCV64"]
        if len(riscv) != 1 or riscv[0].get("validation_level") != "Build/script validation":
            raise SmokeFailure("Phase30 xarch JSON smoke must keep RISCV64 at Build/script validation")

    return ["PASS Phase30 XArch/productization validation matrix docs and xarch JSON smoke"]


def check_hii_bridge_view_model_boundary(root: Path) -> list[str]:
    header = root / "Include" / "ModernUi" / "ModernUiHiiBridge.h"
    source = root / "Library" / "ModernUiHiiBridgeLib" / "ModernUiHiiBridgeLib.c"
    inf = root / "Library" / "ModernUiHiiBridgeLib" / "ModernUiHiiBridgeLib.inf"
    device_data = root / "Library" / "ModernUiDeviceDataLib" / "ModernUiDeviceDataLib.c"
    app_pages = root / MODERN_SETUP_APP_DIR / "ModernSetupAppPages.c"
    app_main = root / MODERN_SETUP_APP_DIR / "ModernSetupApp.c"
    app_actions = root / MODERN_SETUP_APP_DIR / "ModernSetupAppActions.c"

    for path in (header, source, inf):
        if not path.exists():
            raise SmokeFailure(f"missing HII bridge foundation file: {path.relative_to(root)}")

    header_text = strip_c_comments(header.read_text(encoding="utf-8"))
    source_text = strip_c_comments(source.read_text(encoding="utf-8"))
    inf_text = inf.read_text(encoding="utf-8")
    combined = "\n".join((header_text, source_text, inf_text))

    for token in (
        "MODERN_UI_TEXT_REF",
        "ModernUiTextRefHiiString",
        "MODERN_UI_SETUP_SOURCE_REF",
        "MODERN_UI_DISPLAY_KIND",
        "MODERN_UI_EDIT_POLICY",
        "MODERN_UI_DISPLAY_POLICY",
        "ModernUiHiiBridgeClearView",
        "ModernUiHiiBridgeBuildView",
        "ModernUiHiiBridgeResolveText",
    ):
        if token not in combined:
            raise SmokeFailure(f"HII bridge view model missing token: {token}")

    item_match = re.search(r"typedef\s+struct\s*\{(?P<body>.*?)\}\s*MODERN_UI_HII_ITEM\s*;", header_text, re.DOTALL)
    if item_match is None:
        raise SmokeFailure("HII bridge header missing MODERN_UI_HII_ITEM struct")
    item_body = item_match.group("body")
    for field in HII_BRIDGE_REQUIRED_TEXT_FIELDS:
        if not re.search(rf"MODERN_UI_TEXT_REF\s+{field}\s*;", item_body):
            raise SmokeFailure(f"MODERN_UI_HII_ITEM must model {field} as MODERN_UI_TEXT_REF")
    if "MODERN_UI_TEXT_REF  Text;" not in header_text:
        raise SmokeFailure("MODERN_UI_HII_OPTION must model option text as MODERN_UI_TEXT_REF")

    for token in ("ApplyNextValue", "NotifyForm", "RunCallback", "RefreshValues", "ConfigAccess", "VarStore"):
        if token in header_text:
            raise SmokeFailure(f"HII bridge public API/model contains prohibited legacy token: {token}")
    for token in HII_BRIDGE_FORBIDDEN_WRITE_TOKENS:
        if token in combined:
            raise SmokeFailure(f"HII bridge foundation contains prohibited write/path token: {token}")
    for token in ("EFI_IFR_FLAG_CALLBACK", "ModernUiEditNativeOnly", "RequiresNativeFallback", "NativeOnly"):
        if token not in source_text:
            raise SmokeFailure(f"HII bridge default display policy missing fallback token: {token}")

    device_text = strip_c_comments(device_data.read_text(encoding="utf-8"))
    for token in ("ModernUiDeviceDataOpenEntry", "gEfiFormBrowser2ProtocolGuid", "SendForm"):
        if token not in device_text:
            raise SmokeFailure(f"native DeviceData/FormBrowser fallback missing token: {token}")

    app_pages_text = strip_c_comments(app_pages.read_text(encoding="utf-8"))
    for token in (
        "MODERN_UI_HII_VIEW",
        "ModernUiHiiBridgeBuildView",
        "ModernUiHiiBridgeResolveText",
        "Read-only HII preview",
        "preview does not edit settings",
        "HiiPreviewPolicyReasonText",
        "Preview:",
        "Firmware-owned behavior",
        "Native fallback required",
        "Unsupported IFR construct",
        "press Enter for native FormBrowser",
    ):
        if token not in app_pages_text:
            raise SmokeFailure(f"Devices HII bridge read-only preview missing Phase13/14 token: {token}")

    app_main_text = strip_c_comments(app_main.read_text(encoding="utf-8"))
    app_actions_text = strip_c_comments(app_actions.read_text(encoding="utf-8"))
    if "ModernSetupOpenSelectedDeviceEntry (DeviceSelection" not in app_main_text:
        raise SmokeFailure("Devices Enter handling must continue through ModernSetupOpenSelectedDeviceEntry(DeviceSelection)")
    if "ModernUiDeviceDataOpenEntry" not in app_actions_text:
        raise SmokeFailure("ModernSetupAppActions.c must keep native DeviceData open path")

    app_bridge_text = "\n".join(
        strip_c_comments(path.read_text(encoding="utf-8"))
        for path in sorted((root / MODERN_SETUP_APP_DIR).glob("ModernSetupApp*.c"))
    )
    for token in HII_BRIDGE_FORBIDDEN_WRITE_TOKENS:
        if token in app_bridge_text:
            raise SmokeFailure(f"ModernSetupApp HII preview contains prohibited write/path token: {token}")

    renderer_paths = [
        root / "Include" / "ModernUi" / "ModernUiRenderer.h",
        root / "Library" / "ModernUiRendererLib" / "ModernUiRendererLib.c",
        root / "Library" / "ModernUiRendererLib" / "ModernUiRendererLib.inf",
    ]
    for renderer in renderer_paths:
        if renderer.exists():
            renderer_text = strip_c_comments(renderer.read_text(encoding="utf-8"))
            for token in RENDERER_HII_NEUTRAL_TOKENS:
                if token in renderer_text:
                    raise SmokeFailure(f"renderer must remain HII-neutral; {renderer.relative_to(root)} contains {token}")

    return ["PASS HII bridge view-model/default-display-policy boundary"]


def check_phase33_display_form_view_model_boundary(root: Path) -> list[str]:
    lib_dir = root / "Library" / "ModernUiCustomizedDisplayLib"
    inf = lib_dir / "ModernUiCustomizedDisplayLib.inf"
    header = lib_dir / "ModernDisplayFormModel.h"
    source = lib_dir / "ModernDisplayFormModel.c"
    customized = lib_dir / "CustomizedDisplayLib.c"
    internal = lib_dir / "CustomizedDisplayLibInternal.h"
    internal_c = lib_dir / "CustomizedDisplayLibInternal.c"

    for path in (inf, header, source, customized, internal, internal_c):
        if not path.exists():
            raise SmokeFailure(f"missing Phase33 display form model file: {path.relative_to(root)}")

    inf_sources = parse_inf_sources(inf)
    for source_name in ("ModernDisplayFormModel.h", "ModernDisplayFormModel.c"):
        if source_name not in inf_sources:
            raise SmokeFailure(f"ModernUiCustomizedDisplayLib INF missing private form model source: {source_name}")

    model_text = "\n".join(
        strip_c_comments(path.read_text(encoding="utf-8")) for path in (header, source)
    )
    for token in (
        "MODERN_DISPLAY_FORM_MODEL",
        "MODERN_DISPLAY_FORM_ROW",
        "MODERN_DISPLAY_FORM_ROW_KIND",
        "MODERN_DISPLAY_FORM_ROW_STATE",
        "ModernDisplayFormModelBuild",
        "ModernDisplayFormModelClear",
        "ModernDisplayClassifyStatement",
        "FORM_DISPLAY_ENGINE_FORM",
        "FORM_DISPLAY_ENGINE_STATEMENT",
    ):
        if token not in model_text:
            raise SmokeFailure(f"Phase33 private form model missing token: {token}")

    for token in ("RouteConfig", "ExtractConfig", "SetVariable", "HiiSetBrowserData"):
        if token in model_text:
            raise SmokeFailure(f"Phase33 private form model contains prohibited browser/storage token: {token}")

    lib_text = "\n".join(
        strip_c_comments(path.read_text(encoding="utf-8"))
        for path in (inf, header, source, customized, internal, internal_c)
    )
    if "ModernUiHiiBridgeLib" in lib_text or "ModernUiHiiBridge.h" in lib_text:
        raise SmokeFailure("ModernUiCustomizedDisplayLib must not depend on ModernUiHiiBridgeLib")

    customized_text = strip_c_comments(customized.read_text(encoding="utf-8"))
    if "ModernDisplayFormModelBuild" not in extract_c_function_body(customized_text, "DisplayPageFrame"):
        raise SmokeFailure("DisplayPageFrame must build the private form model")
    refresh_body = extract_c_function_body(customized_text, "RefreshKeyHelp")
    for token in ("ModernDisplayClassifyStatement", "ModernDisplayFormRowIsChoiceLike", "ModernDisplayFormRowIsActionLike"):
        if token not in refresh_body:
            raise SmokeFailure(f"RefreshKeyHelp must consume the Phase33 row model/helper: {token}")

    row_surface_body = extract_c_function_body(
        strip_c_comments(internal_c.read_text(encoding="utf-8")),
        "ModernDisplayDrawStatementRow",
    )
    for token in (
        "ModernDisplayClassifyStatementForForm",
        "ModernDisplayFormRowGetVisualRole",
        "MODERN_DISPLAY_FORM_ROW",
        "ModernDisplayDrawStatementRowAccents",
    ):
        if token not in row_surface_body:
            raise SmokeFailure(f"DisplayEngine row surface must consume the Phase34/36 row model/helper: {token}")

    internal_text = strip_c_comments(internal_c.read_text(encoding="utf-8"))
    for token in (
        "ModernDisplayFormRowAccentColor",
        "ModernDisplayFormRowStateChanged",
        "ModernDisplayFormRowStateInvalid",
        "ModernDisplayFormRowStateDisabled",
        "ModernDisplayFormRowStateReadOnly",
        "ModernDisplayFormRowIsTextOnly",
        "ModernUiStrokeRect",
        "ModernDisplayStatementTextInset",
        "ModernDisplayDrawRightRailDivider",
        "ModernDisplayPageStatusText",
        "TextInset",
    ):
        if token not in internal_text:
            raise SmokeFailure(f"Phase36 DisplayEngine row polish missing FormModel-driven accent token: {token}")

    form_display = root / "Universal" / "ModernDisplayEngineDxe" / "FormDisplay.c"
    form_display_text = strip_c_comments(form_display.read_text(encoding="utf-8"))
    display_one_menu = extract_c_function_body(form_display_text, "DisplayOneMenu")
    if "ModernDisplayDrawStatementRow" not in display_one_menu:
        raise SmokeFailure("DisplayOneMenu must keep the Modern row surface hook")
    for stale_token in ("IsActionRow", "IsSubtitleRow"):
        if stale_token in display_one_menu:
            raise SmokeFailure(f"DisplayOneMenu must not reintroduce scattered row role state: {stale_token}")

    for token in ("ConfigAccess", "RouteConfig", "ExtractConfig", "SetVariable", "HiiSetBrowserData"):
        if token in row_surface_body:
            raise SmokeFailure(f"Phase34 row rendering hook contains prohibited browser/storage token: {token}")

    return ["PASS Phase33/34 private DisplayEngine form view-model boundary"]


def check_modern_ui_builtin_glyph_subset(root: Path) -> list[str]:
    string_source = root / "Library" / "ModernUiStringLib" / "ModernUiStringLib.c"
    glyph_source = root / "Library" / "ModernUiRendererLib" / "ModernUiGlyphs.c"

    strings = string_source.read_text(encoding="utf-8")
    required_chars: set[str] = set()
    for match in re.finditer(r'L"((?:[^"\\]|\\.)*)"', strings):
        required_chars.update(char for char in match.group(1) if ord(char) > 0x7F)

    glyphs = glyph_source.read_text(encoding="utf-8")
    available_chars = {
        chr(int(match.group(1), 16))
        for match in re.finditer(r"\{ 0x([0-9A-Fa-f]{4}),", glyphs)
    }

    missing_chars = sorted(required_chars - available_chars, key=ord)
    if missing_chars:
        missing = "".join(missing_chars)
        raise SmokeFailure(f"ModernUiGlyphs.c missing built-in glyphs for ModernUiStringLib: {missing}")

    return [f"PASS ModernUiRendererLib built-in glyph subset covers {len(required_chars)} localized string glyphs"]


def check_ip_hygiene_notices(root: Path) -> list[str]:
    notices_path = root / "THIRD_PARTY_NOTICES.md"
    if not notices_path.exists():
        raise SmokeFailure("missing THIRD_PARTY_NOTICES.md")

    notices = notices_path.read_text(encoding="utf-8")
    for token in (
        "External/edk2",
        "BSD-2-Clause-Patent",
        "Noto Sans CJK SC Regular",
        "SIL Open Font License 1.1",
        "Assets/Fonts/LICENSE.NotoSansCJK.txt",
        "Screenshots",
        "Assets/Brand",
        "Trademarks",
    ):
        if token not in notices:
            raise SmokeFailure(f"THIRD_PARTY_NOTICES.md missing token: {token}")

    for readme in (root / "README.md", root / "README.zh-CN.md"):
        text = readme.read_text(encoding="utf-8")
        if "THIRD_PARTY_NOTICES.md" not in text:
            raise SmokeFailure(f"{readme.relative_to(root)} missing THIRD_PARTY_NOTICES.md link")

    glyph_text = (root / "Library" / "ModernUiRendererLib" / "ModernUiGlyphs.c").read_text(encoding="utf-8")
    for token in ("Noto Sans CJK SC Regular", "SIL Open Font License 1.1", "THIRD_PARTY_NOTICES.md"):
        if token not in glyph_text:
            raise SmokeFailure(f"ModernUiGlyphs.c missing glyph attribution token: {token}")

    asset_docs = {
        Path("Assets") / "Fonts" / "README.md": ("Noto Sans CJK", "SIL Open Font License 1.1", "THIRD_PARTY_NOTICES.md"),
        Path("Assets") / "Brand" / "README.md": ("provenance", "third-party firmware", "THIRD_PARTY_NOTICES.md"),
        Path("Assets") / "Screenshots" / "README.md": ("edk2/QEMU", "commercial firmware screenshots", "THIRD_PARTY_NOTICES.md"),
    }
    for relative, tokens in asset_docs.items():
        text = (root / relative).read_text(encoding="utf-8")
        for token in tokens:
            if token not in text:
                raise SmokeFailure(f"{relative} missing IP hygiene/provenance token: {token}")

    return ["PASS IP hygiene notices and asset provenance docs"]


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

        overlay_dir = workspace / "Build" / "ModernSetupPkgOverlay"
        if overlay_dir.exists():
            shutil.rmtree(overlay_dir)

        env = os.environ.copy()
        env.update(
            {
                "WORKSPACE": str(workspace),
                "GENERATE_ONLY": "1",
                "MODERN_SETUP_DISPLAY_ENGINE": "modern",
                "MODERN_SETUP_DEMO_DRIVER_SAMPLE": "0",
                "MODERN_SETUP_REPLACE_UIAPP": "1",
            }
        )
        script = workspace / "ModernSetupPkg" / "Scripts" / "build-loongarchvirt.sh"
        run([bash, str(script)], cwd=workspace / "ModernSetupPkg", env=env)

        dsc = workspace / "Build" / "ModernSetupPkgOverlay" / "LoongArchVirtQemuModernSetup.dsc"
        fdf = workspace / "Build" / "ModernSetupPkgOverlay" / "LoongArchVirtQemuModernSetup.fdf"
        assert_contains(dsc, "ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf")
        assert_contains(dsc, "MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf")
        assert_contains(
            dsc,
            "gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerMenuFile                | { 0x21, 0xaa, 0x2c, 0x46, 0x14, 0x76, 0x03, 0x45, 0x83, 0x6e, 0x8a, 0xb6, 0xf4, 0x66, 0x23, 0x31 }",
        )
        assert_contains(fdf, "INF  MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf")
        assert_contains(fdf, "INF  RuleOverride = MODERN_SETUP_UIAPP ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf")
        assert_contains(fdf, "FILE APPLICATION = 462CAA21-7614-4503-836E-8AB6F4662331")
        messages.append("PASS loongarch replace-uiapp opt-in overlay generation dry run")

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
        messages.extend(check_phase25_server_inventory_summary(root))
        messages.extend(check_modern_setup_app_preferences_boundary(root))
        messages.extend(check_phase26_interactive_app_owned_preferences(root))
        messages.extend(check_phase27_app_owned_input_preferences(root))
        messages.extend(check_phase28_runtime_theme_switching(root))
        messages.extend(check_phase29_dashboard_density_layout(root))
        messages.extend(check_pcie_provider_foundation(root))
        messages.extend(check_hardware_health_demo_provider(root))
        messages.extend(check_pcie_docs_language(root))
        messages.extend(check_bilingual_documentation_contract(root))
        messages.extend(check_phase30_productization_validation_matrix(root))
        messages.extend(check_hii_bridge_view_model_boundary(root))
        messages.extend(check_phase33_display_form_view_model_boundary(root))
        messages.extend(check_modern_ui_builtin_glyph_subset(root))
        messages.extend(check_ip_hygiene_notices(root))
        messages.extend(check_edk2_baseline_contract(root))
        messages.extend(check_ovmf_capture_helper_contract(root))
        messages.extend(check_displayengine_ovmf_visual_validation_contract(root))
        messages.extend(check_xarch_docs_contract(root))
        messages.extend(check_xarch_runner_contract(root))
        messages.extend(check_xarch_runner_artifact_output(root))
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
