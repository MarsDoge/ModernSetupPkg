#!/usr/bin/env python3
# Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
# Author: MarsDoge (Dongyan Qian)
# Open source: https://github.com/MarsDoge/ModernSetupPkg
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
"""Generate a minimal anti-aliased glyph table for ModernUiRendererLib."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def collect_c_chars(source: Path) -> set[str]:
    """Return non-ASCII characters used by UCS-2 string literals."""

    text = source.read_text(encoding="utf-8")
    chars: set[str] = set()
    for match in re.finditer(r'L"((?:[^"\\]|\\.)*)"', text):
        for char in match.group(1):
            if ord(char) > 0x7F:
                chars.add(char)
    return chars


def collect_uni_chars(source: Path) -> set[str]:
    """Return non-ASCII characters used by edk2 UNI string lines."""

    text = source.read_text(encoding="utf-8", errors="ignore")
    chars: set[str] = set()
    for match in re.finditer(r'"([^"]*)"', text):
        for char in match.group(1):
            if ord(char) > 0x7F:
                chars.add(char)
    return chars


def collect_chars(sources: list[Path]) -> list[str]:
    """Return sorted non-ASCII characters from C and UNI source files."""

    chars: set[str] = set()
    for source in sources:
        if source.suffix.lower() == ".uni":
            chars.update(collect_uni_chars(source))
        elif source.name.endswith('.generated.h'):
            # Catalog CHAR8 metadata uses octal UTF-8 bytes, not wide literals.
            for literal in re.findall(r'"((?:[^"\\]|\\.)*)"', source.read_text(encoding='ascii')):
                raw = re.sub(r'\\([0-7]{3})', lambda m: chr(int(m[1], 8)), literal)
                decoded = raw.encode('latin-1').decode('utf-8')
                chars.update(c for c in decoded if ord(c) > 0x7F)
        else:
            chars.update(collect_c_chars(source))
    return sorted(chars, key=ord)


def render_bitmap(font: ImageFont.FreeTypeFont, char: str, size: int) -> list[int]:
    """Render one character into fixed-size 8-bit alpha bitmap pixels."""

    image = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(image)
    bbox = draw.textbbox((0, 0), char, font=font)
    width = bbox[2] - bbox[0]
    height = bbox[3] - bbox[1]
    x = (size - width) // 2 - bbox[0]
    y = (size - height) // 2 - bbox[1]
    draw.text((x, y), char, fill=255, font=font)

    pixels: list[int] = []
    for y_pos in range(size):
        for x_pos in range(size):
            pixels.append(image.getpixel((x_pos, y_pos)))
    return pixels


def write_c_file(output: Path, chars: list[str], pixels_by_char: dict[str, list[int]], size: int) -> None:
    """Write the renderer's generated C glyph table to the output path."""

    lines: list[str] = [
        "/** @file",
        "  Generated minimal anti-aliased glyph table for ModernUiRendererLib.",
        "",
        "  Generated bitmap subset from Noto Sans CJK SC Regular. The source font is",
        "  licensed under the SIL Open Font License 1.1; preserve Noto Sans CJK",
        "  attribution and the OFL notice when regenerating or replacing this subset.",
        "  See Assets/Fonts/README.md, Assets/Fonts/LICENSE.NotoSansCJK.txt, and",
        "  THIRD_PARTY_NOTICES.md for attribution and license boundary details.",
        "  Regenerate with Scripts/generate-font-glyphs.py.",
        "",
        "  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>",
        "  Author: MarsDoge (Dongyan Qian)",
        "  Open source: https://github.com/MarsDoge/ModernSetupPkg",
        "",
        "  SPDX-License-Identifier: BSD-2-Clause-Patent",
        "**/",
        "",
        '#include "ModernUiGlyphs.h"',
        "",
        "STATIC CONST MODERN_UI_BUILTIN_GLYPH  mModernUiBuiltinGlyphs[] = {",
    ]
    for char in chars:
        pixels = ", ".join(f"{pixel:3d}" for pixel in pixels_by_char[char])
        lines.append(f"  {{ 0x{ord(char):04X}, {size}, {size}, {size}, {{ {pixels} }} }},  // {char}")
    lines.extend(
        [
            "};",
            "",
            "/**",
            "  Find one built-in bitmap glyph.",
            "",
            "  @param[in] CodePoint  UCS-2 code point to look up.",
            "",
            "  @return Pointer to immutable glyph data, or NULL when the glyph is absent.",
            "**/",
            "CONST MODERN_UI_BUILTIN_GLYPH *",
            "ModernUiFindBuiltinGlyph (",
            "  IN CHAR16  CodePoint",
            "  )",
            "{",
            "  UINTN  Index;",
            "",
            "  for (Index = 0; Index < ARRAY_SIZE (mModernUiBuiltinGlyphs); Index++) {",
            "    if (mModernUiBuiltinGlyphs[Index].CodePoint == CodePoint) {",
            "      return &mModernUiBuiltinGlyphs[Index];",
            "    }",
            "  }",
            "",
            "  return NULL;",
            "}",
            "",
        ]
    )
    output.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    """Parse command line arguments and regenerate the C glyph table."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", required=True, type=Path, help="Path to NotoSansCJKsc-Regular.otf")
    parser.add_argument(
        "--source",
        action="append",
        default=[Path("Library/ModernUiStringLib/ModernUiStringLib.c")],
        type=Path,
        help="Source file containing localized CHAR16 or UNI strings. May be repeated.",
    )
    parser.add_argument(
        "--output",
        default=Path("Library/ModernUiRendererLib/ModernUiGlyphs.c"),
        type=Path,
        help="Generated C output path.",
    )
    parser.add_argument("--glyph-size", default=18, type=int, help="Fixed glyph bitmap size in pixels.")
    parser.add_argument("--font-size", default=17, type=int, help="FreeType font size used for glyph rendering.")
    parser.add_argument('--font-index', default=0, type=int,
                        help='Collection face index (Noto CJK Regular TTC uses 2 for SC).')
    parser.add_argument('--preserve-existing', action='store_true',
                        help='Keep existing glyph bitmaps and add missing source characters only.')
    args = parser.parse_args()

    chars = collect_chars(args.source)
    font = ImageFont.truetype(str(args.font), args.font_size, index=args.font_index)
    existing = {}
    if args.preserve_existing and args.output.exists():
        existing = {chr(int(m[1], 16)): m[0] for m in re.finditer(
            r'^  \{ 0x([0-9A-Fa-f]+),.*$', args.output.read_text(encoding='utf-8'), re.M)}
        if not existing:
            raise ValueError('No existing glyphs found; refusing lossy regeneration')
    missing = [char for char in chars if char not in existing]
    pixels_by_char = {char: render_bitmap(font, char, args.glyph_size) for char in missing}
    write_c_file(args.output, missing, pixels_by_char, args.glyph_size)
    generated = args.output.read_text(encoding='utf-8')
    new_rows = {chr(int(m[1], 16)): m[0] for m in re.finditer(
        r'^  \{ 0x([0-9A-Fa-f]+),.*$', generated, re.M)}
    existing.update(new_rows)
    start = generated.index('mModernUiBuiltinGlyphs[] = {') + len('mModernUiBuiltinGlyphs[] = {')
    end = generated.index('};', start)
    chars = sorted(existing, key=ord)
    generated = generated[:start] + '\n' + '\n'.join(existing[c] for c in chars) + '\n' + generated[end:]
    args.output.write_text(generated, encoding='utf-8')
    print(f"Wrote {args.output} with {len(chars)} glyphs")


if __name__ == "__main__":
    main()
