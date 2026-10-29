#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Convert a TTF font to an esp_painter_basic_font_t C bitmap array.

Characters generated: printable ASCII 0x20 (space) through 0x7E (~).
The bitmap format matches the layout expected by esp_painter_draw_char:
  - Row-major, MSB-first
  - bytes_per_row = ceil(width / 8)
  - All characters share the same cell size (monospace)

Usage:
    python font_gen.py --font <font.ttf> --size <px> --name <var_name> [--output <dir>]
                       [--preview [CHAR ...]]

    --font     Path to a TrueType (.ttf) or OpenType (.otf) font file
    --size     Font pixel height (em size passed to FreeType)
    --name     C variable name for the generated font, e.g. my_font_24
    --output   Output directory (default: current directory)
    --preview  Print ASCII-art preview of rendered characters.
               Optionally list specific characters (e.g. --preview A b 3).
               Defaults to: A a 0 !

Dependencies:
    pip install pillow

Output:
    <name>.c   Bitmap data and esp_painter_basic_font_t definition
    <name>.h   Header with ESP_PAINTER_FONT_DECLARE(<name>) — #include this in your source

Example:
    python font_gen.py --font DejaVuSans.ttf --size 24 --name my_font_24 --output ../font/
    python font_gen.py --font DejaVuSans.ttf --size 16 --name my_font_16 --preview A b 0
"""

import argparse
import math
import os
import sys
from datetime import datetime

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit('Error: Pillow is required — install with:  pip install pillow')

CHAR_FIRST = 0x20   # space
CHAR_LAST  = 0x7E   # ~
CHARS      = [chr(c) for c in range(CHAR_FIRST, CHAR_LAST + 1)]


def _text_draw(draw, font, char, x, y):
    """Draw a single character. Use 'lt' anchor when available (Pillow >= 8.0)."""
    try:
        draw.text((x, y), char, font=font, fill=255, anchor='lt')
    except TypeError:
        draw.text((x, y), char, font=font, fill=255)


def _cell_dimensions(font):
    """Return (cell_w, cell_h) for a monospace cell based on font metrics."""
    ascent, descent = font.getmetrics()
    cell_h = ascent + descent

    # Use advance width of 'M' as the reference for cell width.
    try:
        cell_w = math.ceil(font.getlength('M'))
    except AttributeError:
        # Pillow < 9.2: getlength() not available, fall back to getsize()
        cell_w = font.getsize('M')[0]  # type: ignore[attr-defined]

    return cell_w, cell_h


def preview_bitmap(font, cell_w, cell_h, chars):
    """Print ASCII-art preview of the specified characters."""
    print('\nPreview:')
    for char in chars:
        if char not in CHARS:
            print(f"  '{char}': (not in printable ASCII range, skipped)")
            continue
        img  = Image.new('L', (cell_w, cell_h), 0)
        draw = ImageDraw.Draw(img)
        _text_draw(draw, font, char, 0, 0)
        print(f"  '{char}':")
        for row in range(cell_h):
            line = ''.join('#' if img.getpixel((col, row)) > 127 else '.' for col in range(cell_w))
            print(f'    {line}')


def build_bitmap(font_path: str, size: int, name: str, output_dir: str,
                 preview_chars=None) -> None:
    try:
        font = ImageFont.truetype(font_path, size)
    except OSError as exc:
        sys.exit(f'Cannot load font: {exc}')

    cell_w, cell_h = _cell_dimensions(font)
    bytes_per_row  = (cell_w + 7) // 8
    bytes_per_char = cell_h * bytes_per_row
    total_chars    = len(CHARS)

    print(f'Font : {os.path.basename(font_path)}  size={size}px')
    print(f'Cell : {cell_w} x {cell_h} px')
    print(f'Data : {bytes_per_char} bytes/char × {total_chars} chars = {bytes_per_char * total_chars} bytes total')

    if preview_chars is not None:
        preview_bitmap(font, cell_w, cell_h, preview_chars)

    all_bytes: list[int] = []

    for char in CHARS:
        img  = Image.new('L', (cell_w, cell_h), 0)
        draw = ImageDraw.Draw(img)
        _text_draw(draw, font, char, 0, 0)

        for y in range(cell_h):
            for b in range(bytes_per_row):
                byte = 0
                for bit in range(8):
                    x = b * 8 + bit
                    if x < cell_w and img.getpixel((x, y)) > 127:
                        byte |= 0x80 >> bit
                all_bytes.append(byte)

    # Format bytes as a C hex array, 16 values per line
    COLS = 16
    hex_lines: list[str] = []
    for i in range(0, len(all_bytes), COLS):
        chunk = all_bytes[i:i + COLS]
        hex_lines.append('\t' + ', '.join(f'0x{b:02X}' for b in chunk) + ',')

    c_path = os.path.join(output_dir, f'{name}.c')
    h_path = os.path.join(output_dir, f'{name}.h')
    year   = datetime.now().year
    guard  = f'{name.upper()}_H'

    with open(c_path, 'w', newline='\n') as f:
        f.write(
            f'/*\n'
            f' * SPDX-FileCopyrightText: {year} Espressif Systems (Shanghai) CO LTD\n'
            f' *\n'
            f' * SPDX-License-Identifier: CC0-1.0\n'
            f' */\n'
            f'\n'
            f'#include "{name}.h"\n'
            f'\n'
            f'static const uint8_t s_{name}_bitmap[] =\n'
            f'{{\n'
        )
        f.write('\n'.join(hex_lines))
        f.write(
            f'\n}};\n'
            f'\n'
            f'const esp_painter_basic_font_t {name} = {{\n'
            f'    .bitmap = s_{name}_bitmap,\n'
            f'    .width  = {cell_w},\n'
            f'    .height = {cell_h},\n'
            f'}};\n'
        )

    with open(h_path, 'w', newline='\n') as f:
        f.write(
            f'/*\n'
            f' * SPDX-FileCopyrightText: {year} Espressif Systems (Shanghai) CO LTD\n'
            f' *\n'
            f' * SPDX-License-Identifier: CC0-1.0\n'
            f' */\n'
            f'\n'
            f'#pragma once\n'
            f'\n'
            f'#include "esp_painter_font.h"\n'
            f'\n'
            f'ESP_PAINTER_FONT_DECLARE({name});\n'
        )

    print(f'\nOutput : {c_path}')
    print(f'         {h_path}')
    print(f"\nAdd the font source to your component's CMakeLists.txt:")
    print(f'    idf_component_register(')
    print(f'        SRCS "main.c" "{name}.c"')
    print(f'        INCLUDE_DIRS ".")')
    print(f'\nThen include the header and pass the font to any draw function:')
    print(f'    #include "{name}.h"')
    print(f'    esp_painter_draw_string(..., &{name}, ...);')


def main() -> None:
    p = argparse.ArgumentParser(
        description='Convert a TTF/OTF font to an esp_painter_basic_font_t C bitmap array.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)
    p.add_argument('--font',   required=True, help='Path to TTF/OTF font file')
    p.add_argument('--size',   required=True, type=int, help='Font pixel height (em size)')
    p.add_argument('--name',   required=True, help='C variable name, e.g. my_font_24')
    p.add_argument('--output', default='.',   help='Output directory (default: current dir)')
    p.add_argument('--preview', nargs='*', metavar='CHAR',
                   help='Print ASCII-art preview of rendered chars. '
                        'Optionally specify chars (e.g. --preview A b 3); '
                        'defaults to: A a 0 !')
    args = p.parse_args()

    if not os.path.isfile(args.font):
        sys.exit(f'Font file not found: {args.font}')
    if args.size < 8 or args.size > 256:
        sys.exit(f'Font size {args.size} is out of the supported range [8, 256]')

    preview_chars = None
    if args.preview is not None:
        preview_chars = args.preview if args.preview else list('Aa0!')

    os.makedirs(args.output, exist_ok=True)
    build_bitmap(args.font, args.size, args.name, args.output, preview_chars)


if __name__ == '__main__':
    main()
