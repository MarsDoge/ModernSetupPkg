#!/usr/bin/env python3
# Copyright (c) 2026, MarsDoge. All rights reserved.
# Author: MarsDoge (Dongyan Qian)
# Open source: https://github.com/MarsDoge/ModernSetupPkg
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
# Rasterize the original OEM watermark (Assets/Branding/oem-watermark.svg content)
# into an 8-bit alpha coverage map and emit it as a C array the LVGL renderer wraps
# in an A8 lv_image. A8 keeps it tiny (1 byte/pixel) and lets the firmware tint it
# with the active theme color. We re-draw the (simple, monochrome) design with PIL
# because no SVG rasterizer is available here; the SVG remains the canonical source
# and the layout constants below mirror it. Regenerate with:
#
#   python3 Scripts/gen-oem-watermark.py
#
# Ships NO third-party / IBV art -- 100% original geometry + the project URL text.

import os
from PIL import Image, ImageDraw, ImageFont

W, H = 620, 92
PKG = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(PKG, "Library", "ModernUiLvglRendererLib")

FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONT_REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

SUB_TEXT = "MODERN UEFI SETUP · OPEN SOURCE"
URL_TEXT = "github.com/MarsDoge/ModernSetupPkg"


def build_alpha() -> Image.Image:
    img = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(img)

    # Original brand mark: rounded panel + scan lines + a corner-cut accent.
    d.rounded_rectangle([8, 16, 68, 76], radius=12, outline=255, width=3)
    d.line([24, 34, 44, 34], fill=255, width=3)
    d.line([24, 46, 52, 46], fill=255, width=3)
    d.line([24, 58, 47, 58], fill=255, width=3)
    d.line([52, 16, 68, 32], fill=255, width=3)

    sub = ImageFont.truetype(FONT_REG, 15)
    url = ImageFont.truetype(FONT_BOLD, 24)
    # Letter-spacing the sub-label by drawing char by char.
    x = 88
    for ch in SUB_TEXT:
        d.text((x, 20), ch, font=sub, fill=210)
        x += d.textlength(ch, font=sub) + 2
    d.text((88, 46), URL_TEXT, font=url, fill=255)
    return img


def emit_c(img: Image.Image) -> None:
    data = img.tobytes()
    assert len(data) == W * H

    h_path = os.path.join(OUT_DIR, "OemWatermarkData.h")
    c_path = os.path.join(OUT_DIR, "OemWatermarkData.c")

    with open(h_path, "w") as f:
        f.write("/** @file\n")
        f.write("  Generated OEM watermark alpha coverage map. DO NOT EDIT.\n")
        f.write("  Regenerate via Scripts/gen-oem-watermark.py.\n\n")
        f.write("  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>\n")
        f.write("  SPDX-License-Identifier: BSD-2-Clause-Patent\n**/\n\n")
        f.write("#ifndef OEM_WATERMARK_DATA_H_\n#define OEM_WATERMARK_DATA_H_\n\n")
        f.write("#include <Uefi.h>\n\n")
        f.write(f"#define OEM_WATERMARK_WIDTH   {W}\n")
        f.write(f"#define OEM_WATERMARK_HEIGHT  {H}\n\n")
        f.write("//\n// Row-major 8-bit alpha coverage (0 = transparent, 255 = solid).\n//\n")
        f.write("extern CONST UINT8  gOemWatermarkAlpha[OEM_WATERMARK_WIDTH * OEM_WATERMARK_HEIGHT];\n\n")
        f.write("#endif // OEM_WATERMARK_DATA_H_\n")

    with open(c_path, "w") as f:
        f.write("/** @file\n")
        f.write("  Generated OEM watermark alpha coverage map. DO NOT EDIT.\n")
        f.write("  Regenerate via Scripts/gen-oem-watermark.py.\n\n")
        f.write("  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>\n")
        f.write("  SPDX-License-Identifier: BSD-2-Clause-Patent\n**/\n\n")
        f.write('#include "OemWatermarkData.h"\n\n')
        f.write("CONST UINT8  gOemWatermarkAlpha[OEM_WATERMARK_WIDTH * OEM_WATERMARK_HEIGHT] = {\n")
        for i in range(0, len(data), 20):
            chunk = data[i:i + 20]
            f.write("  " + ",".join(str(b) for b in chunk) + ",\n")
        f.write("};\n")

    print(f"wrote {h_path}")
    print(f"wrote {c_path}  ({len(data)} bytes)")


if __name__ == "__main__":
    emit_c(build_alpha())
