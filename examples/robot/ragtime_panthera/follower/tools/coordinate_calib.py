# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
# /// script
# requires-python = ">=3.10"
# dependencies = ["numpy"]
# ///
"""Fit the pixel-to-world affine matrix and print the console command that stores it.

    tools/apriltag_capture.py --size 640x480 --calib-out calib_pixels.json
    tools/coordinate_calib.py --pixels calib_pixels.json
    tools/coordinate_calib.py --map 248.7,226.4
    tools/coordinate_calib.py --check
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    import numpy as np
except ModuleNotFoundError:
    sys.exit(
        f'numpy is missing from {sys.executable}.\n'
        f'  uv run tools/coordinate_calib.py'
    )

# Hand-measured tag positions in the arm's frame (metres). Origin and axes
# match what panthera_fk prints.
WORLD: dict[int, tuple[float, float, float]] = {
    0: (0.438, 0.15, 0.02),
    1: (0.438, 0.00, 0.02),
    2: (0.438, -0.11, 0.02),
    3: (0.321, 0.14, 0.02),
    4: (0.321, 0.011, 0.02),
    5: (0.321, -0.11, 0.02),
    6: (0.21, 0.13, 0.02),
    7: (0.21, 0.015, 0.02),
    8: (0.21, -0.115, 0.02),
}

# Fallback pixel centres at 640x480, from a previous --calib-out run.
PIXELS: dict[int, tuple[float, float]] = {
    0: (174.9, 151.0),
    1: (248.7, 226.4),
    2: (321.3, 304.0),
    3: (97.0, 225.7),
    4: (170.6, 300.7),
    5: (243.4, 376.2),
    6: (18.4, 300.2),
    7: (91.8, 373.7),
    8: (170.0, 450.7),
}

FIRMWARE_SIZE = (640, 480)


def load_pixels(path: Path | None) -> tuple[dict[int, tuple[float, float]], tuple[int, int] | None]:
    if path is None:
        print(f'using built-in PIXELS table ({len(PIXELS)} tags)')
        return dict(PIXELS), None

    blob = json.loads(path.read_text())
    centres = {int(k): (float(v[0]), float(v[1])) for k, v in blob['centres'].items()}
    size = tuple(blob['size']) if 'size' in blob else None
    where = f'{size[0]}x{size[1]}' if size else 'unknown size'
    print(f'{path}: {len(centres)} tags, {where}')

    frames = blob.get('frames')
    if frames:
        thin = [
            f'id {k} ({n}/{frames})'
            for k, n in sorted(blob.get('sightings', {}).items())
            if n < frames * 0.5
        ]
        if thin:
            print('  seen in under half the frames: ' + ', '.join(thin))
    return centres, size


def fit(pixels: dict[int, tuple[float, float]]) -> tuple[np.ndarray, list[int]]:
    ids = sorted(set(pixels) & set(WORLD))
    if len(ids) < 4:
        raise SystemExit(
            f'need at least 4 tags in both tables (3 gives an exact fit with no '
            f'error estimate). Have {len(ids)}: {ids}'
        )
    if extra := sorted(set(pixels) - set(WORLD)):
        print(f'  detected but unmeasured, ignored: {extra}')
    if absent := sorted(set(WORLD) - set(pixels)):
        print(f'  measured but undetected, ignored: {absent}')

    A = np.array([[pixels[i][0] for i in ids], [pixels[i][1] for i in ids]])
    B = np.array([[WORLD[i][j] for i in ids] for j in range(3)])

    M = np.zeros((3, 3))
    A_hom = np.vstack([A, np.ones(len(ids))])
    for row in range(3):
        M[row, :] = np.linalg.lstsq(A_hom.T, B[row, :], rcond=None)[0]
    return M, ids


def residuals(M: np.ndarray, pixels: dict[int, tuple[float, float]], ids: list[int]) -> dict[int, tuple[float, float]]:
    out = {}
    for i in ids:
        p = M @ np.array([pixels[i][0], pixels[i][1], 1.0])
        out[i] = ((p[0] - WORLD[i][0]) * 1000.0, (p[1] - WORLD[i][1]) * 1000.0)
    return out


def report(M: np.ndarray, pixels: dict[int, tuple[float, float]], ids: list[int]) -> None:
    res = residuals(M, pixels, ids)
    ex = np.array([res[i][0] for i in ids])
    ey = np.array([res[i][1] for i in ids])

    print('\n id | pixel (u, v)      | measured (x, y) | fitted (x, y)   | error mm')
    print('----+-------------------+-----------------+-----------------+------------')
    for i in ids:
        p = M @ np.array([pixels[i][0], pixels[i][1], 1.0])
        print(
            f' {i:2d} | {pixels[i][0]:8.1f},{pixels[i][1]:8.1f} |'
            f' {WORLD[i][0]:7.3f},{WORLD[i][1]:7.3f} | {p[0]:7.3f},{p[1]:7.3f} |'
            f' {res[i][0]:+6.1f},{res[i][1]:+6.1f}'
        )

    print(f'\nRMS      X {np.sqrt((ex ** 2).mean()):5.1f} mm   Y {np.sqrt((ey ** 2).mean()):5.1f} mm')
    print(f'max |e|  X {np.abs(ex).max():5.1f} mm   Y {np.abs(ey).max():5.1f} mm')
    print(f'worst point: id {ids[int(np.argmax(np.hypot(ex, ey)))]}')
    print('\nLarge errors are almost always the hand measurements, not the detection.')


def map_point(M: np.ndarray, pixels: dict[int, tuple[float, float]], ids: list[int],
              res: dict[int, tuple[float, float]], u: float, v: float) -> None:
    x, y, z = M @ np.array([u, v, 1.0])
    print(f'  ({u:7.1f}, {v:7.1f})  ->  x {x:7.4f}   y {y:7.4f}   z {z:6.4f} m')

    if not (0 <= u < FIRMWARE_SIZE[0] and 0 <= v < FIRMWARE_SIZE[1]):
        print(f'      outside the {FIRMWARE_SIZE[0]}x{FIRMWARE_SIZE[1]} frame')

    us = [pixels[i][0] for i in ids]
    vs = [pixels[i][1] for i in ids]
    if not (min(us) <= u <= max(us) and min(vs) <= v <= max(vs)):
        print(f'      EXTRAPOLATING: outside calibrated area '
              f'u {min(us):.0f}..{max(us):.0f}, v {min(vs):.0f}..{max(vs):.0f}')

    near = min(ids, key=lambda i: np.hypot(pixels[i][0] - u, pixels[i][1] - v))
    dist = np.hypot(pixels[near][0] - u, pixels[near][1] - v)
    print(f'      nearest tag id {near} at {dist:.0f} px, error {res[near][0]:+.1f}, {res[near][1]:+.1f} mm')


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--pixels', type=Path, default=None, metavar='PATH',
                        help='JSON from apriltag_capture.py --calib-out')
    parser.add_argument('--map', nargs='+', default=None, metavar='U,V',
                        help='convert pixel coordinates to world and exit')
    parser.add_argument('--check', action='store_true',
                        help='interactive prompt to convert pixels after fitting')
    args = parser.parse_args(argv)

    pixels, size = load_pixels(args.pixels)

    if size is not None and tuple(size) != FIRMWARE_SIZE:
        # Camera modes show different fields of view, not crops. Rescaling won't fix it.
        print(f'\nWARNING: captured at {size[0]}x{size[1]}, firmware streams '
              f'{FIRMWARE_SIZE[0]}x{FIRMWARE_SIZE[1]}. Re-capture at the right size.')

    outside = sorted(
        i for i, (u, v) in pixels.items()
        if not (0 <= u < FIRMWARE_SIZE[0] and 0 <= v < FIRMWARE_SIZE[1])
    )
    if outside:
        print(f'\nWARNING: tags outside {FIRMWARE_SIZE[0]}x{FIRMWARE_SIZE[1]}: {outside}')

    M, ids = fit(pixels)
    res = residuals(M, pixels, ids)

    if args.map:
        print()
        for token in args.map:
            try:
                u, v = (float(t) for t in token.replace('(', '').replace(')', '').split(','))
            except ValueError:
                print(f'  {token!r}: expected U,V')
                continue
            map_point(M, pixels, ids, res, u, v)
        return 0

    report(M, pixels, ids)

    print(f'\nMatrix (row-major, {len(ids)} tags):')
    print(repr(M))
    print('\nConsole command:')
    print('panthera_set_vision_matrix '
          + ' '.join(f'-{n} {v:.6f}' for n, v in enumerate(M.flatten(), start=1)))

    while args.check:
        try:
            raw = input('\npixel u,v (blank to quit): ').strip()
        except EOFError:
            break
        if not raw:
            break
        try:
            u, v = (float(t) for t in raw.replace('(', '').replace(')', '').split(','))
        except ValueError:
            print('  expected U,V')
            continue
        map_point(M, pixels, ids, res, u, v)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
