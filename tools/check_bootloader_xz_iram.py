#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Verify plus/XZ bootloader placement for injected layouts.

Injection is required for:
  - every target on IDF >= 6.0
  - ESP32-P4 on IDF >= 5.5.5

Those maps must define a non-empty [_xz_decompressor_start, _xz_decompressor_end)
fully inside iram_loader_seg. Do not infer "injection required" from whether
those symbols already exist: a failed PRE_LINK inject would then be skipped.

Other chips on IDF < 6.0 still use linker.lf / iram_loader1_seg and are skipped.
Pass --idf-version and/or --expected-injection; the script also tries to read
the IDF major.minor and target from idf_build_apps paths like 6.1/build_esp32c3_.
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

# Longest names first so "esp32" does not match inside "esp32c3".
_TARGETS = (
    'esp32s31',
    'esp32c61',
    'esp32c6',
    'esp32c5',
    'esp32c3',
    'esp32c2',
    'esp32h21',
    'esp32h2',
    'esp32h4',
    'esp32p4',
    'esp32s3',
    'esp32s2',
    'esp32',
)


def _parse_int(value: str) -> int:
    return int(value, 0)


def _match_addr(match: re.Match[str]) -> int:
    return _parse_int(next(g for g in match.groups() if g))


def _parse_version(ver: str) -> tuple[int, int, int]:
    bits = [int(x) for x in ver.split('.')]
    while len(bits) < 3:
        bits.append(0)
    return bits[0], bits[1], bits[2]


def _infer_target(map_path: Path) -> str | None:
    text = map_path.as_posix().lower()
    # idf_build_apps: {idf_ver}/build_@t_@w/...
    m = re.search(r'(?:^|/)build_([a-z0-9]+)(?:_|/)', text)
    if m:
        return m.group(1)
    for target in _TARGETS:
        if re.search(rf'(?:^|/){re.escape(target)}(?:/|$)', text):
            return target
    return None


def _infer_idf_version(map_path: Path) -> str | None:
    for part in map_path.parts:
        if re.fullmatch(r'\d+\.\d+(?:\.\d+)?', part):
            return part
    return None


def _needs_injection(
    idf_version: str | None,
    target: str | None,
    *,
    expected_injection: bool,
) -> bool:
    if expected_injection:
        return True
    if idf_version is not None:
        ver = _parse_version(idf_version)
        if ver >= (6, 0, 0):
            return True
        # P4 maps only exist for IDF >= 5.5.5; build dirs may be "5.5".
        if target == 'esp32p4' and ver >= (5, 5, 0):
            return True
        return False
    # No version: only ESP32-P4 is known to require injection whenever this
    # checker runs. Pass --idf-version or --expected-injection for IDF 6.x
    # trees that do not embed the version in the map path.
    return target == 'esp32p4'


def _region(text: str, pattern: re.Pattern[str], map_path: Path, name: str) -> tuple[int, int]:
    mem = pattern.search(text)
    if not mem:
        raise AssertionError(f'{map_path}: {name} not found in MEMORY CONFIGURATION')
    origin = _parse_int(mem.group(1))
    end = origin + _parse_int(mem.group(2))
    return origin, end


def _check_injected(map_path: Path, text: str) -> None:
    origin, end = _region(text, MEM_RE, map_path, 'iram_loader_seg')
    start_m = XZ_START_RE.search(text)
    end_m = XZ_END_RE.search(text)
    if not (start_m and end_m):
        raise AssertionError(
            f'{map_path}: _xz_decompressor_start/end missing. '
            f'Injection into .iram_loader.text is required for this '
            f'IDF/target; a missing PRE_LINK inject must fail CI'
        )
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


def check_map(
    map_path: Path,
    *,
    idf_version: str | None = None,
    expected_injection: bool = False,
) -> None:
    text = map_path.read_text(encoding='utf-8', errors='replace')
    target = _infer_target(map_path)
    version = idf_version or _infer_idf_version(map_path)
    if _needs_injection(version, target, expected_injection=expected_injection):
        _check_injected(map_path, text)
        return
    print(f'{map_path}: skip legacy linker.lf layout')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        'example_dir',
        type=Path,
        help='Example directory that contains build*/bootloader/bootloader.map',
    )
    parser.add_argument(
        '--idf-version',
        default=None,
        help='ESP-IDF version used for the build (e.g. 6.1, 5.5.5). '
             'Overrides any version inferred from the map path.',
    )
    parser.add_argument(
        '--expected-injection',
        action='store_true',
        help='Require _xz_decompressor_start/end on every map (IDF >= 6.0 jobs).',
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
            check_map(
                map_path,
                idf_version=args.idf_version,
                expected_injection=args.expected_injection,
            )
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
