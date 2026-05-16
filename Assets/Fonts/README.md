# Font Assets

ModernSetupPkg uses a generated minimal bitmap subset for Simplified Chinese UI
text. The generated glyph table is committed as source code so normal edk2
builds do not depend on Python packages or external font files.

## Source Font

- Font: Noto Sans CJK SC Regular
- Upstream: https://github.com/notofonts/noto-cjk
- License: SIL Open Font License 1.1
- Current generated table: `Library/ModernUiRendererLib/ModernUiGlyphs.c`

The full OTF file is not committed to this repository. To regenerate the glyph
table, download `NotoSansCJKsc-Regular.otf` from the upstream repository and run:

```sh
python3 -m venv /tmp/modernsetup-font-venv
/tmp/modernsetup-font-venv/bin/python -m pip install Pillow
/tmp/modernsetup-font-venv/bin/python Scripts/generate-font-glyphs.py \
  --font /path/to/NotoSansCJKsc-Regular.otf
```

Only glyphs referenced by `Library/ModernUiStringLib/ModernUiStringLib.c` are
included in the generated table.
