<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# DisplayEngine OVMF X64 Native-vs-Modern Visual Validation

This is the Phase35 foundation for repeatable native-vs-modern FormBrowser
screenshot evidence on OVMF X64. It establishes a deterministic artifact layout
and workflow; it does not claim visual equivalence unless QEMU screendumps are
actually captured and reviewed.

## Scope

The comparison target is the DisplayEngine product path:

`edk2 FormBrowser / EDKII_FORM_DISPLAY_ENGINE_PROTOCOL -> ModernDisplayEngineDxe -> ModernUiCustomizedDisplayLib -> private FormModel -> renderer`

The native side uses upstream edk2 `DisplayEngineDxe` and upstream
`CustomizedDisplayLib`. The modern side replaces those overlay entries with
`ModernDisplayEngineDxe` and `ModernUiCustomizedDisplayLib` through the existing
OVMF overlay generator.

This workflow intentionally does not promote `ModernUiHiiBridgeLib`, parse IFR
packages, implement ConfigAccess or varstore semantics, call `RouteConfig`,
`ExtractConfig`, `SetVariable`, `HiiSetBrowserData`, or directly invoke platform
ConfigAccess callbacks.

## Evidence Levels

Use precise evidence language when recording results:

- Static smoke: script/doc existence and contract tokens only.
- Generate-only: native and modern overlay DSC/FDF snapshots were generated and
  copied to the evidence directory.
- Build: each variant produced `OVMF_CODE.fd` and `OVMF_VARS.fd` snapshots.
- QEMU boot: QEMU launched far enough to run the capture helper.
- Visual screenshot: `screendump` PPM/PNG artifacts exist for both variants.
- Visual reviewed: a maintainer compared the intended FormBrowser surfaces.

Do not mark the path `Verified` based only on static smoke, generate-only output,
build success, or a QEMU boot without screenshots.

## Scripted Workflow

Run from the ModernSetupPkg repository or an edk2 workspace containing
ModernSetupPkg:

```sh
Scripts/capture-displayengine-ovmf-x64.sh --mode dry-run
```

Default output root:

```text
${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64
```

The dry-run prints planned native and modern commands and writes a README in the
output root. No overlay, build, QEMU, or visual evidence is produced.

Generate only the overlay snapshots:

```sh
Scripts/capture-displayengine-ovmf-x64.sh --mode generate-only
```

Expected artifacts:

```text
${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64/overlays/native/OvmfX64ModernSetup.dsc
${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64/overlays/native/OvmfX64ModernSetup.fdf
${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64/overlays/modern/OvmfX64ModernSetup.dsc
${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64/overlays/modern/OvmfX64ModernSetup.fdf
```

Build both firmware variants and copy stable FD snapshots:

```sh
Scripts/capture-displayengine-ovmf-x64.sh --mode build
```

Expected additional artifacts:

```text
${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64/firmware/native/OVMF_CODE.fd
${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64/firmware/native/OVMF_VARS.fd
${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64/firmware/modern/OVMF_CODE.fd
${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64/firmware/modern/OVMF_VARS.fd
```

Capture both variants with QEMU screendump:

```sh
BOOT_WAIT_SECONDS=12 \
Scripts/capture-displayengine-ovmf-x64.sh --mode capture
```

The default capture sequence sends `Esc,Enter` (`SENDKEY_SEQUENCE=esc,ret`) to
open the OVMF boot selector and enter the selected `EFI Firmware Setup` path.
If your host firmware timing differs, override `SENDKEY_SEQUENCE` and
`BOOT_WAIT_SECONDS` explicitly.

Expected additional artifact directories:

```text
${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64/native/
${TMPDIR:-/tmp}/modernsetup-qemu/displayengine-ovmf-x64/modern/
```

Each variant directory is populated by `Scripts/capture-ovmf-x64.sh`, normally
including PPM screendumps, optional PNG conversions, and serial logs under the
capture work directory.

Set `CAPTURE_OUT_DIR=Assets/Screenshots/manual/displayengine-ovmf-x64` only when
intentionally collecting assets for commit. The default path stays under
`${TMPDIR:-/tmp}` to avoid accidental repository changes.

## Native/Modern Selectors

The helper invokes the existing build script with both selectors:

```sh
MODERN_SETUP_DISPLAY_ENGINE=native GENERATE_ONLY=1 Scripts/build-ovmf-x64.sh
MODERN_SETUP_DISPLAY_ENGINE=modern GENERATE_ONLY=1 Scripts/build-ovmf-x64.sh
```

In build/capture mode it uses the same selectors with `GENERATE_ONLY=0` and then
copies the generated firmware files before switching variants, so later captures
use variant-distinct FD paths.

## Manual Review Checklist

For real visual review, capture the same FormBrowser surface in both variants and
record the exact `BOOT_WAIT_SECONDS`, `SENDKEY_SEQUENCE`, QEMU version, and host
GOP mode. Review at least:

- FrontPage/native UiApp landing surface after `Esc` during BDS when reachable.
- Device Manager or Boot Manager page.
- One popup or dialog if navigation is reliable on the host.
- Serial logs for ASSERTs, exceptions, GOP failures, or unexpected boot denial.

Expected result for Phase35 is not row polish. The useful outcome is a repeatable
artifact set that makes native and modern FormBrowser screenshots comparable.

## Known Limitations

The wrapper cannot guarantee host-independent automatic navigation into a
specific FormBrowser page. QEMU firmware timing, keyboard focus, boot policy, and
available OVMF setup entries can vary. `capture` mode produces screenshot
evidence only when QEMU and `screendump` complete; the script does not inspect
pixels, compare images, or assert visual equivalence.
