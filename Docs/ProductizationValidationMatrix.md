<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Productization Validation Matrix

Language: English | [简体中文](ProductizationValidationMatrix.zh-CN.md)

This matrix is the Phase30 evidence checklist for XArch productization. It records what can be validated today from repository docs, smoke tests, scripts, and existing provider boundaries. It is not a feature promise and it is not an edk2 build-architecture abstraction.

XArch is cross-architecture validation/productization vocabulary for ModernSetupPkg. XArch does not replace edk2 ARCH values. Product integrations, DSC/FDF overlays, build scripts, and toolchains still use concrete edk2 ARCH values: `ARCH=X64`, `ARCH=AARCH64`, `ARCH=LOONGARCH64`, and `ARCH=RISCV64`. There is no supported `ARCH=XArch` or `TARGET=XArch` build implication.

Related source-of-truth docs:

- [XArch.md](XArch.md) for target mapping and validation language.
- [ProductizationFeatureMatrix.md](ProductizationFeatureMatrix.md) for feature ownership and App/provider scope.
- [CompatibilityMatrix.md](CompatibilityMatrix.md) for DisplayEngine/FormBrowser evidence.
- [Tests/Smoke/README.md](../Tests/Smoke/README.md) for the host-side smoke gate.

## Evidence Language

The validation terms below describe current evidence only:

- `Smoke`: host-side `Tests/Smoke/smoke_validate.py` static and overlay dry-run checks.
- `Script`: a repository script exists and is checked for syntax/metadata.
- `Manual`: local maintainer validation path is documented, but not CI-gated.
- `Captured`: screenshot/screendump evidence exists for a path.
- `Build/script validation`: the script/overlay path is validated without claiming graphical runtime evidence.
- `Planned`: documented target or behavior only; do not describe it as validated.

## XArch Target Validation Matrix

| XArch target | Concrete edk2 ARCH | Platform path | Primary scripts | Current maturity evidence | Productization validation notes |
| --- | --- | --- | --- | --- | --- |
| X64 / OVMF X64 | `X64` | `OvmfPkg/OvmfPkgX64` | `Scripts/build-ovmf-x64.sh`, `Scripts/run-ovmf-x64.sh`, `Scripts/capture-ovmf-x64.sh` | Manual OVMF build/run/capture path; smoke overlay generation; local/manual App validation. | Evidence supports target metadata, native/modern DisplayEngine overlay separation, and local screenshot capture path. |
| AARCH64 / ArmVirtQemu | `AARCH64` | `ArmVirtPkg/ArmVirtQemu` | `Scripts/build-armvirt.sh`, `Scripts/run-armvirt.sh`, `Scripts/capture-armvirt.sh`, `Scripts/build-modern-app.sh` | Captured ArmVirt before/after evidence; active build/run path; smoke overlay generation. | Primary compatibility capture path for native UiApp/FormBrowser plus ModernDisplayEngine. |
| LOONGARCH64 / LoongArchVirtQemu | `LOONGARCH64` | `OvmfPkg/LoongArchVirt/LoongArchVirtQemu` | `Scripts/build-loongarchvirt.sh`, `Scripts/run-loongarchvirt.sh` | Active build/run script path; smoke overlay generation. | Evidence covers generated overlays and documented manual run path; external cross toolchain remains product-team responsibility. |
| RISCV64 / RiscVVirtQemu | `RISCV64` | `OvmfPkg/RiscVVirt/RiscVVirtQemu` | `Scripts/build-riscvvirt.sh` | Build/script validation; smoke overlay generation. | RISCV64 remains Build/script validation in Phase30; graphical QEMU helper and captured UI evidence are not claimed. |

`Scripts/xarch-validate.sh --all --mode dry-run --format json` is the fast target metadata smoke companion. In Phase30 the smoke gate asserts four target `PASS` results and preserves the RISCV64 `Build/script validation` maturity wording.

## Product Class Validation Matrix

| Product class | Evidence-backed App role | Native owner / boundary | Current validation evidence |
| --- | --- | --- | --- |
| Desktop / workstation | Dashboard, Boot, Devices/HII, Security, Firmware, Diagnostics, Power/Thermal, Performance, PCIe capability summary, Preferences, Exit. | Boot order editing, Secure Boot key management, CPU/memory tuning, fan policy, PCIe policy, and chipset controls remain native HII/FormBrowser. | OVMF X64 and ArmVirt paths document desktop/workstation-style checks; smoke validates boundary tokens. |
| Server | Dashboard provider health, Management, Diagnostics, Performance, Hardware Health demo visualization, PCIe inventory/policy-entry hints, Exit/native entries. | BMC/IPMI/Redfish configuration, RAS/NUMA policy, PCIe resource policy, ACS/ARI/IOMMU policy, SEL/log clearing remain native or service-app owned. | Provider snapshot, server inventory, management, performance, PCIe, and diagnostics boundaries are smoke-checked. |
| Embedded / industrial | Device/HII entry list, boot/recovery posture, firmware update status, diagnostics evidence, security posture. | GPIO, serial, watchdog, provisioning, boot-pin behavior, board muxes, and power-restore policy remain platform HII/native. | ArmVirt, LoongArchVirt, and RiscVVirt script paths provide cross-arch metadata evidence; product specifics require platform validation. |
| Tablet / appliance | Minimal dashboard, Boot/recovery entry, Security, Firmware, Power/Thermal, Preferences, Exit. | Battery/adapter policy, display panel/backlight, thermal trip points, recovery writes, and appliance provisioning remain native/platform owned. | App/provider docs define read-only status and entry behavior; no product-specific runtime evidence is claimed. |

## App / Provider Validation Matrix

| Area | Evidence-backed App/provider behavior | Boundary tokens that must remain true | Phase30 validation evidence |
| --- | --- | --- | --- |
| Dashboard | Shows provider-backed platform, boot, device, firmware, power/thermal, performance, and provider-health summaries. | Dashboard consumes `ModernSetupAppProvider.c` snapshots; it does not call provider LibraryClasses directly. | Smoke checks Dashboard draw ownership, quick-card count, provider snapshot use, and density layout. |
| Boot | Lists boot inventory and launches selected boot options when available. | Boot policy editing and Boot Maintenance remain native; no App-owned platform policy writes. | Smoke covers App source boundaries; feature matrix documents display/entry behavior. |
| Devices / HII | Lists HII/device entries and opens real setup pages through `EFI_FORM_BROWSER2_PROTOCOL.SendForm()`. | Native `FormBrowser2`, native HII, VFR/IFR parsing, `ConfigAccess`, callbacks, validation, defaults, and varstore writes own semantics. ModernSetupApp must not parse IFR, implement ConfigAccess, write HII varstores, or write platform policy. | Smoke checks HII bridge read-only preview boundaries and prohibited App source tokens. |
| Security | Shows read-only Secure Boot, Setup Mode, key presence, and TPM/TCG/TCM posture when discoverable. | Key enrollment, password, physical-presence, measured-boot policy, and chassis security policy remain native. | Smoke validates provider snapshot boundaries and App mutation-token exclusion. |
| Firmware | Shows capsule/update/recovery availability and native entry hints. | Capsule construction, flash programming, rollback policy, and recovery writes remain native/platform utility owned. | Feature and validation docs use evidence language only. |
| Diagnostics | Shows table/log/provider-health summaries and service/debug evidence. | POST log management, error clearing, vendor diagnostics, and repair flows remain native or service-app owned. | Smoke checks provider health derivation and diagnostics inclusion. |
| Management | Shows BMC/IPMI/Redfish presence and management host hints. | BMC networking, users, KVM/media, SEL policy, and remote update configuration remain BMC/native owned. | Smoke checks provider snapshot boundary and server inventory summary. |
| Power / Thermal | Shows ACPI/chassis/power-supply status and can display demo Hardware Health curves. | Fan curves, pump headers, thermal trip points, acoustic profiles, battery, and adapter policy remain native HII/FormBrowser-owned. | Smoke checks Power provider wiring through App boundaries. |
| Hardware Health | Demo-only/read-only provider for deterministic temperature trend UX. | The Hardware Health demo provider does not claim real sensors and does not program SMBus, I2C, IPMI, SuperIO, MMIO, PCI, fan, or trip-point policy. | Smoke checks demo provider files, demo text, read-only docs, and prohibited hardware/mutation tokens. |
| Performance | Shows CPU/memory inventory and tuning/RAS entry availability. | CPU frequency/voltage, memory timing/profile, NUMA/RAS, and workload profile policy remain native. | Smoke checks provider snapshot use and server inventory summary. |
| PCIe | Shows PCIe inventory and read-only capability/native policy-entry hints for ReBAR, Above 4G, SR-IOV, ASPM, bifurcation, hot-plug, ACS, ARI, and IOMMU. | ReBAR, Above 4G, SR-IOV, ASPM, bifurcation, hot-plug, ACS, ARI, IOMMU, BAR/resource allocation, and fabric policy changes stay native HII/FormBrowser-owned. | Smoke checks PCIe provider wiring, mutation-token exclusion, and docs language. |
| Preferences | App-owned UX preferences use `ModernUiPreferencesLib`. | App-owned preferences are not platform policy; platform variables such as BootOrder, SecureBoot, CPU, fan, chipset, and PCIe policy stay out of the preferences library. | Smoke checks `ModernUiPreferencesLib`, app usage, schema/version fields, and no runtime variable access. |
| Exit | Provides session actions, app/version info, language/theme preference access, native UiApp/native setup entries where available. | Save/discard/defaults workflows for real setup variables remain native FormBrowser/platform HII. | Smoke checks App source boundaries and preference routing. |

## Phase30 Smoke Gate Requirements

The smoke gate must remain fast and deterministic. For Productization Validation it checks:

- English and zh-CN validation matrix files exist and link each other.
- X64, AARCH64, LOONGARCH64, and RISCV64 appear with platform paths and scripts.
- XArch does not replace edk2 ARCH values, and no `ARCH=XArch` or `TARGET=XArch` build implication appears.
- ModernSetupApp boundaries: no IFR parsing, no ConfigAccess implementation, no HII varstore writes, no platform policy writes.
- `EFI_FORM_BROWSER2_PROTOCOL.SendForm()`, FormBrowser2/native HII/ConfigAccess ownership, and native semantics are documented.
- Hardware Health remains demo-only/read-only.
- App-owned preferences route through `ModernUiPreferencesLib`.
- PCIe policy tokens remain native-owned: ReBAR, Above 4G, SR-IOV, ASPM, bifurcation, hot-plug, ACS, ARI, and IOMMU.
- `Scripts/xarch-validate.sh --all --mode dry-run --format json` reports four target `PASS` results and preserves RISCV64 as `Build/script validation`.
