#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Verify that plus/XZ bootloader code landed in iram_loader_seg on IDF 6.x.

Looks under <example_dir> for bootloader/bootloader.map files produced by CI
builds, then checks that _xz_decompressor_* symbols (or plus/xz archive
objects) fall inside the iram_loader_seg memory region.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


MEM_RE = re.compile(
    r'^iram_loader_seg\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\b',
    re.MULTILINE,
)
# GNU ld maps may print "ADDR  symbol" or "symbol  ADDR".
XZ_START_RE = re.compile(
    r'(?:^\s*(0x[0-9a-fA-F]+)\s+_xz_decompressor_start\b)|'
    r'(?:^\s*_xz_decompressor_start\s+(0x[0-9a-fA-F]+)\b)',
    re.MULTILINE,
)
XZ_END_RE = re.compile(
    r'(?:^\s*(0x[0-9a-fA-F]+)\s+_xz_decompressor_end\b)|'
    r'(?:^\s*_xz_decompressor_end\s+(0x[0-9a-fA-F]+)\b)',
    re.MULTILINE,
)
PLUS_XZ_OBJ_RE = re.compile(
    r'^\s*\.(?:literal|text|text\.[^\s]+)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+'
    r'(?:libbootloader_support_plus|libespressif__bootloader_support_plus|'
    r'libxz|libespressif__xz)\.a\(',
    re.MULTILINE,
)


def _parse_int(value: str) -> int:
    return int(value, 0)


def check_map(map_path: Path) -> None:
    text = map_path.read_text(encoding='utf-8', errors='replace')
    mem = MEM_RE.search(text)
    if not mem:
        raise AssertionError(f'{map_path}: iram_loader_seg not found in MEMORY CONFIGURATION')

    origin = _parse_int(mem.group(1))
    length = _parse_int(mem.group(2))
    end = origin + length

    def _match_addr(match: re.Match[str]) -> int:
        return _parse_int(next(g for g in match.groups() if g))

    start_m = XZ_START_RE.search(text)
    end_m = XZ_END_RE.search(text)
    if start_m and end_m:
        xz_start = _match_addr(start_m)
        xz_end = _match_addr(end_m)
        if xz_end <= xz_start:
            raise AssertionError(
                f'{map_path}: _xz_decompressor range is empty '
                f'({xz_start:#x} .. {xz_end:#x})'
            )
        if not (origin <= xz_start < end and origin < xz_end <= end):
            raise AssertionError(
                f'{map_path}: _xz_decompressor '
                f'[{xz_start:#x}, {xz_end:#x}) is outside iram_loader_seg '
                f'[{origin:#x}, {end:#x})'
            )
        print(
            f'{map_path}: OK _xz_decompressor '
            f'[{xz_start:#x}, {xz_end:#x}) in iram_loader_seg '
            f'[{origin:#x}, {end:#x})'
        )
        return

    # Fallback when symbols were stripped from the map: require at least one
    # plus/xz text object inside iram_loader_seg.
    hits = []
    for match in PLUS_XZ_OBJ_RE.finditer(text):
        addr = _parse_int(match.group(1))
        size = _parse_int(match.group(2))
        if size == 0:
            continue
        if origin <= addr < end:
            hits.append((addr, size))
    if not hits:
        raise AssertionError(
            f'{map_path}: no plus/XZ code found in iram_loader_seg '
            f'[{origin:#x}, {end:#x}); injection/layout likely failed'
        )
    print(
        f'{map_path}: OK {len(hits)} plus/XZ object(s) in iram_loader_seg '
        f'[{origin:#x}, {end:#x})'
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        'example_dir',
        type=Path,
        help='Example directory that contains build*/bootloader/bootloader.map',
    )
    args = parser.parse_args()
    example_dir = args.example_dir.resolve()
    if not example_dir.is_dir():
        print(f'example directory not found: {example_dir}', file=sys.stderr)
        return 2

    maps = sorted(example_dir.glob('**/bootloader/bootloader.map'))
    if not maps:
        print(
            f'no bootloader/bootloader.map found under {example_dir}',
            file=sys.stderr,
        )
        return 1

    failures = 0
    for map_path in maps:
        try:
            check_map(map_path)
        except AssertionError as exc:
            print(str(exc), file=sys.stderr)
            failures += 1

    if failures:
        print(f'{failures} bootloader map check(s) failed', file=sys.stderr)
        return 1
    print(f'checked {len(maps)} bootloader map(s)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
