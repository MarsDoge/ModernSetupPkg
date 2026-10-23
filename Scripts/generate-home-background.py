#!/usr/bin/env python3
"""Build-only PNG -> bounded RGB565 C resource (Pillow required only when opted in).

The original is never modified. Generated text lives in the build overlay, not
in the source tree. Firmware embeds RGB565 bytes, not a PNG decoder.
SPDX-License-Identifier: BSD-2-Clause-Patent
"""
import argparse
import hashlib
from pathlib import Path
from PIL import Image


def generate(source, output):
    source, output = Path(source), Path(output)
    if source.resolve() == output.resolve():
        raise ValueError('output must not replace the source')
    with Image.open(source) as original:
        if original.format != 'PNG':
            raise ValueError('expected a PNG')
        image = original.convert('RGB')
    image.thumbnail((1280, 800), Image.Resampling.LANCZOS)
    # RGB565 retains local colour detail without a global palette. Explicit
    # little-endian bytes make the generated resource independent of the host.
    pixels = bytearray()
    for r, g, b in image.getdata():
        value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        pixels.extend((value & 255, value >> 8))
    lines = ['/* Generated build-only resource. SPDX-License-Identifier: BSD-2-Clause-Patent */',
             f'/* Source SHA256: {hashlib.sha256(source.read_bytes()).hexdigest()} */',
             f'#define HOME_BACKGROUND_WIDTH {image.width}',
             f'#define HOME_BACKGROUND_HEIGHT {image.height}',
             'STATIC CONST UINT8 mHomeRgb565[] =']
    for pos in range(0, len(pixels), 64):
        lines.append('  "' + ''.join(f'\\{value:03o}' for value in pixels[pos:pos + 64]) + '"')
    lines += ['  ;', '']
    output.parent.mkdir(parents=True, exist_ok=True)
    text = '\n'.join(lines)
    if not output.exists() or output.read_text() != text:
        output.write_text(text)
    print(f'Home background: {image.width}x{image.height}, {len(pixels)} resource bytes; {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source')
    parser.add_argument('output')
    args = parser.parse_args()
    generate(args.source, args.output)
