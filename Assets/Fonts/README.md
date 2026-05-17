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
- Current generated table: `Library/ModernUiRendererLib/ModernUiGlyphs.c`
- Current glyph cell: 18x18 pixels, 8-bit alpha coverage

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
