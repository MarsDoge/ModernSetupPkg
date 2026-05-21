<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Font Assets

ModernSetupPkg uses a generated minimal anti-aliased bitmap subset for
Simplified Chinese UI text and selected HII demo strings. The generated glyph
table is committed as source code so normal edk2 builds do not depend on Python
packages or external font files.

## Source Font

- Font: Noto Sans CJK SC Regular
- Upstream: https://github.com/notofonts/noto-cjk
- License: SIL Open Font License 1.1
- License file: `Assets/Fonts/LICENSE.NotoSansCJK.txt`
- Current generated table: `Library/ModernUiRendererLib/ModernUiGlyphs.c`
- Current glyph cell: 18x18 pixels, 8-bit alpha coverage

See the repository-level `THIRD_PARTY_NOTICES.md` for the consolidated Noto
Sans CJK attribution and third-party notice text.

The full OTF file is not committed to this repository. To regenerate the glyph
table, download `NotoSansCJKsc-Regular.otf` from the upstream repository and run:

```sh
python3 -m venv /tmp/modernsetup-font-venv
/tmp/modernsetup-font-venv/bin/python -m pip install Pillow
/tmp/modernsetup-font-venv/bin/python Scripts/generate-font-glyphs.py \
  --font /path/to/NotoSansCJKsc-Regular.otf \
  --source Library/ModernUiStringLib/ModernUiStringLib.c \
  --source /path/to/edk2/MdeModulePkg/Universal/DriverSampleDxe/VfrStrings.uni \
  --source /path/to/edk2/MdeModulePkg/Universal/DriverSampleDxe/InventoryStrings.uni
```

Only glyphs referenced by the selected source files are included in the
generated table. Add another `--source` argument when a new built-in page or HII
demo needs additional fixed firmware-resident glyphs.

The full font file is not committed. The generated bitmap subset is included so
firmware builds do not need external font tooling; attribution to Noto Sans CJK
SC Regular and the SIL Open Font License 1.1 boundary must be preserved when the
subset is regenerated or replaced.
