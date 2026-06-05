# LvglSpikePkg — experimental LVGL rendering-backend evaluation

**Status: experimental spike. Not shipped. Never in a default firmware overlay or
in `ModernSetupApp`.** Lives only on the `experimental/lvgl-spike` branch.

## Why this exists

To answer one question before considering LVGL as an open-source rendering
backend for ModernSetupPkg:

> Does LVGL (core + software renderer + its upstream UEFI port) **build and
> render under edk2 on a hard architecture** — LoongArch64 — not just on X64
> where downstream LVGL-UEFI ports already work?

This is the single objection that actually gated the "switch backend?" decision:
the package's entire value is *one* Setup UX across OVMF-X64 / ArmVirt-AArch64 /
LoongArch64 / RISC-V64. If LVGL could not survive that, the question was moot.

## Result

| Arch | Build (edk2 GCC) | Render | Notes |
|---|---|---|---|
| X64 | ✅ (proven upstream by LvglPkg) | ✅ upstream | not re-done here |
| AArch64 | ✅ already in LVGL's UEFI arch allowlist | — | no patch needed |
| **LoongArch64** | ✅ **PASS** — 465 objs, `LvglSpikeProbe.efi` PE32+ LoongArch | ✅ **PASS on real hardware** | the crux; see below |
| RISC-V64 | ⚠️ arch-gate compiles; full build blocked by local toolchain | — (repo is build-only for RISC-V) | environment limit, not LVGL |

**LoongArch64 is the headline:** `LvglSpikeProbe.efi` compiles, links, and was
run on **real LoongArch silicon**, drawing an LVGL UI (themed background, title,
rounded button) straight to the GOP framebuffer. Soft-float — the one concern
that static reading can't settle — is a non-issue: `LV_USE_FLOAT 0` keeps the SW
renderer integer-only and the whole closure linked cleanly.

### RISC-V64 caveat (environment, not LVGL)

The baseline's RISC-V branch compiles (the `lv_uefi.h` gate passes). The full
build fails only because the host has `riscv64-linux-gnu-gcc` (a glibc Linux
cross-compiler) instead of the bare-metal `riscv64-unknown-elf-gcc` edk2 expects:
LVGL's `#include <stdint.h>` chains into glibc, which then needs
`gnu/stubs-lp64.h` — absent because edk2 builds RISC-V with soft-float `-mabi=lp64`
while the installed glibc is `lp64d`. LoongArch avoided this only because its
glibc ABI happened to match edk2's flags. With a bare-metal RISC-V toolchain (or
by redirecting LVGL's libc includes to edk2 types) this clears; it is not an
LVGL problem.

## The upstream contribution (now in the baseline)

LVGL's UEFI port originally hard-`#error`ed on any arch outside
`{x86_64, i386, aarch64}`. Adding LoongArch64 + RISC-V64 was a small, coherent
change across exactly three sites:

| File | Role | Change |
|---|---|---|
| `src/drivers/uefi/lv_uefi.h` | the `#error` gate + 64-bit size assert | add `__loongarch_lp64` / `__riscv && __riscv_xlen==64` branches (separate, to keep the per-arch `__LV_UEFI_ARCH_*` markers correct) |
| `src/misc/lv_types.h` | `LV_ARCH_64` fallback list | append both arches (main path already covered via `__UINTPTR_MAX__`) |
| `tests/makefiles_uefi/efi.h` | CI standalone UEFI test types | fold both LP64 arches into the existing aarch64 type block |

This support has since landed **upstream** and the pinned `External/lvgl` baseline
is bumped to a commit that includes it. The submodule is consumed **pristine** —
there is no local patch and no build-time patching. (LVGL stays an unmodified
upstream submodule.)

## Integration notes learned

- **Memory:** `LV_USE_STDLIB_MALLOC = LV_STDLIB_BUILTIN` gives only a 64 KB static
  pool — too small for a display draw buffer; LVGL hangs on allocation. Use
  `LV_STDLIB_CUSTOM` + `LV_UEFI_USE_MEMORY_SERVICES 1` so malloc routes to UEFI
  `AllocatePool` (`lv_mem_core_uefi.c`).
- **Headers:** removed LvglPkg's libc-shim headers (`limits.h`, `stdint.h`, …)
  which shadowed the compiler's freestanding headers (`CHAR_BIT` undeclared).
- **lv_conf discovery:** `-DLV_CONF_INCLUDE_SIMPLE`, because `lvgl/` is a symlink
  out to `External/lvgl`, so LVGL's default `"../../lv_conf.h"` escapes the dir.
- **Render path:** this probe is a standalone UEFI application that draws
  `lv_draw_sw → EFI_GRAPHICS_OUTPUT_PROTOCOL.Blt()` directly. It does **not** go
  through HII / FormBrowser / any DisplayEngine — same shape as `ModernSetupApp`,
  not the DisplayEngine-replacement shape (that would be the separate
  `LvglDisplayEngineDxe` model, deliberately not vendored here — it carries
  AMI-"Aptio" branded chrome we must not reuse).

## Build

```sh
Scripts/bootstrap-edk2.sh                          # if not already done
Scripts/build-lvgl-spike-loongarch.sh              # -> Build/LvglSpike/.../LvglSpikeProbe.efi
TARGET=DEBUG Scripts/build-lvgl-spike-riscv.sh     # needs a bare-metal riscv64 toolchain
```
The pinned `External/lvgl` baseline already carries LoongArch64/RISC-V64 UEFI
support upstream, so the scripts consume the submodule pristine — no patching.

## Recommendation

The feasibility blocker — "an LVGL backend would break the XArch promise" — is
**empirically cleared on real LoongArch hardware** with only a ~3-site change
that is now upstream in LVGL. This does **not** by itself mandate switching; it means the
decision can now be made on merits (LVGL's widget/text-editing richness and AA
rendering vs. dependency weight, the boundary/maintenance cost, and the existing
`ModernUiEngineLib` investment) rather than being blocked on portability.

## Attribution

Builds on LVGL (lvgl/lvgl), LvglPkg (hamitcan99), and the original UEFI port by
YangGangUEFI — all MIT. See [`REFERENCES.md`](REFERENCES.md) for exactly what is
used from each and what is deliberately not reused.
