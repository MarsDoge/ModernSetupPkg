# References & attribution — LvglSpikePkg

This experimental package builds on prior open-source work. All third-party code
used here is MIT-licensed; this file records what we use from each project and
what we deliberately do **not** reuse. Copyright headers in copied files are kept
intact, as MIT requires.

## LVGL — https://github.com/lvgl/lvgl

- **License:** MIT.
- **Use:** vendored directly as the `External/lvgl` git submodule, pinned to a
  tagged **v9.5.0**-derived baseline that already carries LoongArch64/RISC-V64
  UEFI support upstream. This is the actual rendering engine (core, software
  renderer, fonts, widgets, and the upstream UEFI port under
  `src/drivers/uefi`). The submodule keeps LVGL's own `LICENCE.txt`.
- **Upstream contribution back:** the LoongArch64 / RISC-V64 arch-gate change
  was contributed to lvgl/lvgl and is now in the pinned baseline. The submodule
  is consumed **pristine** — no local patch, no build-time patching; LVGL stays
  an unmodified upstream submodule.

## LvglPkg — https://github.com/hamitcan99/LvglPkg

- **License:** MIT.
- **Use:** EDK2-integration reference. The `Library/LvglLib` baseline in this
  package (the `lv_conf.h`, the INF source-list shape, and the UEFI port glue)
  was adapted from LvglPkg's `Library/LvglLib`; copied files retain their
  original `Copyright (c) 2024, Yang Gang` headers. The forthcoming
  `LvglDisplayEngineDxe` (LVGL behind `EDKII_FORM_DISPLAY_ENGINE_PROTOCOL`,
  mapping `FORM_DISPLAY_ENGINE_STATEMENT` to LVGL widgets) follows the pattern of
  LvglPkg's `LvglDisplayEngineDxe` / `LvglFormRenderer.c`.
- **Deliberately NOT reused:** LvglPkg's `AptioWallpaper.c` / `LvglAptioChrome.*`.
  "Aptio" is AMI's commercial BIOS brand; per this repo's "no commercial-IBV
  asset reuse" rule we use original chrome only.

## YangGangUEFI — https://github.com/YangGangUEFI

- **License:** MIT.
- **Use:** Yang Gang is the original author of the LVGL-on-UEFI port that LvglPkg
  forked from. Credited here as the origin of the EDK2/LVGL integration approach
  this spike learns from.

---

*Visual/architectural references are fine; copied artwork, fonts, icons, strings,
or vendor-branded assets are not. See the package `README.md` for the spike
findings and `CLAUDE.md` for the boundary rules.*
