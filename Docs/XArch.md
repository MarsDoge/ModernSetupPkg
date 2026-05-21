<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# XArch Architecture Model

XArch is ModernSetupPkg's cross-architecture architecture model for keeping one
Setup UX, one HII/FormBrowser ownership boundary, and one validation vocabulary
across X64, AARCH64, LOONGARCH64, and RISCV64 targets.

XArch is project, product, and validation vocabulary. XArch does not replace edk2 ARCH values.
Build scripts, DSC/FDF overlays, toolchain selection, and edk2 build variables
continue to use concrete edk2 ARCH values such as `ARCH=X64`,
`ARCH=AARCH64`, `ARCH=LOONGARCH64`, and `ARCH=RISCV64`.

## Goals

- Keep the same ModernSetup visual language and front-page information
  architecture across supported architecture families.
- Keep firmware setup semantics owned by edk2 HII, UiApp, SetupBrowserDxe,
  FormBrowser2, ConfigAccess, callbacks, defaults, validation, and varstores.
- Keep platform integration concrete: every target still maps to an edk2 platform
  package, firmware image, build script, toolchain, and QEMU/manual validation
  path.
- Keep validation status explicit so a target can be documented as captured,
  manually validated, build/script validated, or planned without overclaiming
  maturity.

## Target Mapping

| XArch target | edk2 ARCH value | edk2/QEMU platform | Main scripts | Current role |
| --- | --- | --- | --- | --- |
| X64 / OVMF X64 | `X64` | `OvmfPkg/OvmfPkgX64` | `build-ovmf-x64.sh`, `run-ovmf-x64.sh`, `capture-ovmf-x64.sh` | Local/manual OVMF validation and screenshot capture path. |
| AARCH64 / ArmVirtQemu | `AARCH64` | `ArmVirtPkg/ArmVirtQemu` | `build-armvirt.sh`, `run-armvirt.sh`, `capture-armvirt.sh`, `build-modern-app.sh` | Primary compatibility capture path and primary App bring-up path. |
| LOONGARCH64 / LoongArchVirtQemu | `LOONGARCH64` | `OvmfPkg/LoongArchVirt/LoongArchVirtQemu` | `build-loongarchvirt.sh`, `run-loongarchvirt.sh` | Active build/run path with native UiApp plus ModernDisplayEngine and optional App ESP. |
| RISCV64 / RiscVVirtQemu | `RISCV64` | `OvmfPkg/RiscVVirt/RiscVVirtQemu` | `build-riscvvirt.sh` | Build/script overlay validation; graphical QEMU helper remains future work. |

## Validation Levels

XArch status language is shared by README, compatibility docs, productization
planning, and smoke tests:

- `Captured`: screenshot or screendump evidence exists for the relevant path.
- `Manual`: local run instructions exist and the path is intended for manual QEMU
  validation, but it is not a CI gate.
- `Active`: build/run path exists and is part of current maintainer validation.
- `Build/script validation`: scripts generate overlays or builds for the target,
  but graphical UI behavior is not yet validated.
- `Planned`: target or capability is documented as a future extension and should
  not be described as validated.

These levels describe evidence, not product quality. A target may support the
same UX model while still having a lower evidence level than another target.

## Lightweight Validation Runner

`Scripts/xarch-validate.sh` is the Phase20 runner for making the XArch target
model executable without starting a full firmware build or QEMU session. It is
dry-run only in this phase: the runner validates target metadata, checks that
the expected scripts and manual docs exist, and reports the current evidence
level for each target. It reports intended build/run/capture helpers but does
not invoke them.

```sh
# Validate all documented XArch targets with the default table report.
Scripts/xarch-validate.sh --all --mode dry-run

# Validate one target.
Scripts/xarch-validate.sh --target aarch64 --mode dry-run

# Machine-readable report for external tooling.
Scripts/xarch-validate.sh --target all --mode dry-run --format json

# Write Markdown/JSON artifacts for CI logs or maintainer handoff.
Scripts/xarch-validate.sh --all --mode dry-run --format markdown --output Build/Reports/xarch-validation.md
Scripts/xarch-validate.sh --all --mode dry-run --format json --output Build/XArchValidation.json
```

When `--output PATH` is provided, the runner creates parent directories as
needed, writes exactly the selected report format to `PATH`, and prints a short
`Wrote XArch validation artifact: PATH` status line to stdout. If `--output` is
omitted, the report is printed to stdout as before.

Current runner scope:

- Supported targets: `x64`, `aarch64`, `loongarch64`, `riscv64`, or `all`.
- edk2 ARCH values remain concrete: `X64`, `AARCH64`, `LOONGARCH64`, and
  `RISCV64`.
- Supported mode: `dry-run` only.
- Supported report formats: `table`, `markdown`, and `json`.
- Checked artifacts are the documented target scripts and manual validation
  docs. Heavy build, QEMU, and screenshot capture commands remain explicit
  maintainer actions outside this runner.

## Ownership Boundaries

XArch does not change firmware ownership boundaries:

- edk2 owns HII package registration, VFR/IFR parsing, expression evaluation,
  ConfigAccess callbacks, BrowserActions, validation, defaults, and varstore
  routing.
- `ModernDisplayEngineDxe` owns the GOP display-engine presentation path for
  forms already prepared by SetupBrowserDxe/FormBrowser2.
- `ModernSetupApp` owns only the standard front-page shell, navigation state,
  read-only summaries, App-private UX preferences, and entry points into native
  FormBrowser2.
- Provider libraries should report inventory, posture, capability, health, and
  native entry availability. They must not become platform policy engines.
- Real setup changes remain in the owning platform HII formset, native
  FormBrowser flow, or a platform/vendor utility designed for that policy.

## Current Maturity

| Area | X64 / OVMF X64 | AARCH64 / ArmVirtQemu | LOONGARCH64 / LoongArchVirtQemu | RISCV64 / RiscVVirtQemu |
| --- | --- | --- | --- | --- |
| Native UiApp + ModernDisplayEngine overlay | Manual | Captured | Active | Build/script validation |
| Before/after DisplayEngine evidence | Manual OVMF capture path | Captured ArmVirt evidence | Manual | Planned |
| ModernSetupApp front-page shell | Local/manual OVMF-compatible app build | Active bring-up path | Optional ESP path | Planned |
| QEMU graphical run helper | Manual | Active | Active | Planned |
| Smoke overlay generation | Active script contract | Active script contract | Active script contract | Active script contract |

## Next Extension Rules

- Add a target by first documenting its XArch mapping: concrete edk2 ARCH value,
  platform DSC/FDF, build script, run/capture helper if available, and validation
  level.
- Do not rename build variables or script families to XArch. Keep concrete edk2
  ARCH values and platform names in commands.
- Reuse the shared UX model, provider vocabulary, and ownership boundaries before
  adding target-specific App behavior.
- Prefer read-only provider summaries and native FormBrowser entry points over
  App-owned controls for platform policy.
- Update `Docs/CompatibilityMatrix.md`, `Docs/ProductizationFeatureMatrix.md`,
  and smoke static docs contracts whenever XArch targets or validation terms
  change.
