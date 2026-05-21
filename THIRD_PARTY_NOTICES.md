<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Third-Party Notices

This document summarizes third-party attribution and asset provenance for
ModernSetupPkg. The project license is `BSD-2-Clause-Patent`; third-party
components remain under their original licenses as noted below.

## TianoCore edk2

ModernSetupPkg is designed to build against the TianoCore edk2 firmware tree,
which is carried in this repository as the `External/edk2` submodule for pinned
baseline validation. edk2 remains an upstream dependency; ModernSetupPkg does
not claim ownership of upstream edk2 source.

Some ModernSetupPkg DisplayEngine/customized display integration code follows
or adapts edk2 DisplayEngine interfaces and behavior so it can interoperate with
`SetupBrowserDxe` / `FormBrowser2`. edk2-derived portions retain their original
copyright notices where present.

- Upstream project: https://github.com/tianocore/edk2
- Local submodule path: `External/edk2`
- License: BSD-2-Clause-Patent

## Noto Sans CJK / Generated Glyph Subset

ModernSetupPkg includes a generated, minimal 18px anti-aliased bitmap glyph
subset for selected Simplified Chinese UI text and HII demo strings. The bitmap
glyph table is generated from Noto Sans CJK SC Regular and committed as C source
at `Library/ModernUiRendererLib/ModernUiGlyphs.c` so firmware builds do not need
the full font file or Python font tooling.

The full Noto Sans CJK font file is not committed to this repository. The
project's own source remains under `BSD-2-Clause-Patent`; the generated glyph
bitmap subset and the source font attribution are documented under the SIL Open
Font License 1.1 boundary.

- Source font: Noto Sans CJK SC Regular
- Upstream project: https://github.com/notofonts/noto-cjk
- License: SIL Open Font License 1.1
- Local license copy: `Assets/Fonts/LICENSE.NotoSansCJK.txt`
- Local font asset documentation: `Assets/Fonts/README.md`

## Screenshots

Project screenshots under `Assets/Screenshots/` are captured from ModernSetupPkg
running with edk2/QEMU outputs unless a file or documentation note explicitly
states otherwise. They are repository presentation and design-review artifacts,
not copied commercial firmware screenshots.

Commercial firmware screenshots, vendor UI assets, vendor icons, wallpapers, and
other closed assets must not be copied into this repository.

## Brand and Logo Assets

Brand assets under `Assets/Brand/` are repository-provided project visual assets
for ModernSetupPkg presentation unless future per-file metadata states
otherwise. They are not copied from third-party firmware UIs, commercial logos,
icons, wallpapers, or vendor asset packs.

Future replacements or additions should include source/provenance metadata and
license/permission notes before being used in firmware images or public
showcases.

## Trademarks

Product names, company names, project names, and trademarks mentioned in this
repository are the property of their respective owners. They are used for
identification, compatibility, attribution, and reference only. Their use does
not imply endorsement, sponsorship, affiliation, or approval by the respective
trademark holders.
