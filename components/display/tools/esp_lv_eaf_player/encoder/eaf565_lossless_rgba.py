# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""EAF codec 0x04 (LOSSLESS RGB565+A8) encoder and asset packer.

Each scene is a folder of RGBA PNG frames (sorted by file name). Frames are converted to
RGB565 with the 8x8 Bayer ordered dither used by the LVGL UI, kept bit-exact together with
the 8-bit alpha plane, and encoded as:
  - frame 0:  keyframe, non-transparent 4x4 tiles stored as RGB565 low/high + A8 planes
  - frame N:  changed 4x4 tiles only; one joint symbol per pixel for per-channel deltas in
              -1..1, everything else stored as absolute values in a separate escape stream
Both streams are raw deflate (zopfli by default), decoded on the device by the ROM inflater.

Outputs (under build/lossless565_pack by default):
  <scene>.eaf               one EAF file per scene
  eaf_assets.bin            all scenes, 64-byte aligned
  animation.json            offsets, sizes and per-frame CRC32 (RGB565 little-endian + A8)
  eaf_scenes.h              the same table as C

Usage:
  python3 -m encoder.eaf565_lossless_rgba --fast
  python3 -m encoder.eaf565_lossless_rgba --scene "MY_SCENE=path/to/pngs" [--scene ...] [--fast]
"""
import argparse
import glob
import json
import re
import struct
import sys
import zlib
from pathlib import Path

import numpy as np
from PIL import Image

DEMO_DIR = Path(__file__).resolve().parents[1]
DEFAULT_SRC = DEMO_DIR / 'assets' / 'synthetic'
DEFAULT_SCENES = [
    ('ALPHA_GLOW', DEFAULT_SRC / 'alpha_glow'),
    ('FLAT_UI', DEFAULT_SRC / 'flat_ui'),
    ('MICRO_MOTION', DEFAULT_SRC / 'micro_motion'),
]

CODEC_LOSSLESS = 0x04
FLAGS_KEYFRAME = 0
FLAGS_JOINT_INTER = 3
SYM_ESCAPE = 255
BLOB_ALIGN = 64

BAYER8 = np.array([
    [0, 32, 8, 40, 2, 34, 10, 42],
    [48, 16, 56, 24, 50, 18, 58, 26],
    [12, 44, 4, 36, 14, 46, 6, 38],
    [60, 28, 52, 20, 62, 30, 54, 22],
    [3, 35, 11, 43, 1, 33, 9, 41],
    [51, 19, 59, 27, 49, 17, 57, 25],
    [15, 47, 7, 39, 13, 45, 5, 37],
    [63, 31, 55, 23, 61, 29, 53, 21],
], dtype=np.uint16)


def rgb888_to_rgb565_bayer8(rgb: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    """Same rounding as lvgl_util_make_gradient_px(); fully transparent pixels become 0."""
    h, w = alpha.shape
    th = np.tile(BAYER8, ((h + 7) // 8, (w + 7) // 8))[:h, :w]
    r = rgb[..., 0].astype(np.uint16)
    g = rgb[..., 1].astype(np.uint16)
    b = rgb[..., 2].astype(np.uint16)
    r5, g6, b5 = r >> 3, g >> 2, b >> 3
    r5 = np.where((r5 < 31) & (((r & 7) << 3) > th), r5 + 1, r5)
    g6 = np.where((g6 < 63) & (((g & 3) << 4) > th), g6 + 1, g6)
    b5 = np.where((b5 < 31) & (((b & 7) << 3) > th), b5 + 1, b5)
    c565 = (r5 << 11) | (g6 << 5) | b5
    return np.where(alpha == 0, 0, c565).astype(np.uint16)


class Deflater:
    def __init__(self, fast: bool, iterations: int):
        self.fast = fast
        self.iterations = iterations
        if not fast:
            try:
                import zopfli.zlib  # noqa: F401
            except ImportError:
                sys.exit('zopfli is not installed: pip install zopfli (or pass --fast for zlib level 9)')

    def __call__(self, raw: bytes) -> bytes:
        """Raw deflate stream without zlib header/trailer."""
        if self.fast:
            co = zlib.compressobj(level=9, wbits=-15)
            return co.compress(raw) + co.flush()
        import zopfli.zlib
        return zopfli.zlib.compress(raw, numiterations=self.iterations)[2:-4]


def to_tiles(plane: np.ndarray) -> np.ndarray:
    """(rows, width) -> (n_tiles, 16): 4x4 tiles in raster order, pixels row-major inside a tile."""
    h, w = plane.shape
    return plane.reshape(h // 4, 4, w // 4, 4).transpose(0, 2, 1, 3).reshape(-1, 16)


def joint_symbols(cur_c, cur_a, prev_c, prev_a):
    x = cur_c.astype(np.int16)
    y = prev_c.astype(np.int16)
    dr = ((x >> 11) & 31) - ((y >> 11) & 31)
    dg = ((x >> 5) & 63) - ((y >> 5) & 63)
    db = (x & 31) - (y & 31)
    da = cur_a.astype(np.int16) - prev_a.astype(np.int16)
    small = (np.abs(dr) <= 1) & (np.abs(dg) <= 1) & (np.abs(db) <= 1) & (np.abs(da) <= 1)
    sym = np.where(small, (dr + 1) * 27 + (dg + 1) * 9 + (db + 1) * 3 + (da + 1), SYM_ESCAPE).astype(np.uint8)
    esc_c = cur_c[~small]
    esc_a = cur_a[~small]
    esc = ((esc_c & 0xFF).astype(np.uint8).tobytes() + (esc_c >> 8).astype(np.uint8).tobytes() +
           esc_a.astype(np.uint8).tobytes())
    return sym.tobytes(), esc, int(esc_c.size)


def encode_block(cb, ab, pcb, pab, keyframe: bool, deflate: Deflater) -> bytes:
    if keyframe:
        chg = (np.max(cb, axis=1) != 0) | (np.max(ab, axis=1) != 0)
    else:
        chg = ~(np.all(cb == pcb, axis=1) & np.all(ab == pab, axis=1))
    n_changed = int(np.sum(chg))
    if n_changed == 0:
        return b''
    mask = np.packbits(chg.astype(np.uint8), bitorder='little').tobytes()
    if keyframe:
        dc, da = cb[chg], ab[chg]
        raw = (mask + (dc & 0xFF).astype(np.uint8).tobytes() + (dc >> 8).astype(np.uint8).tobytes() +
               da.astype(np.uint8).tobytes())
        return bytes([CODEC_LOSSLESS]) + struct.pack('<BIH', FLAGS_KEYFRAME, len(raw), n_changed) + deflate(raw)
    sym, esc, n_esc = joint_symbols(cb[chg], ab[chg], pcb[chg], pab[chg])
    sym_raw = mask + sym
    sym_comp = deflate(sym_raw)
    esc_comp = deflate(esc) if n_esc else b''
    return (bytes([CODEC_LOSSLESS]) +
            struct.pack('<BIHII', FLAGS_JOINT_INTER, len(sym_raw), n_changed, n_esc, len(sym_comp)) +
            sym_comp + esc_comp)


def frame_bytes(width: int, height: int, block_height: int, blocks: list) -> bytes:
    """Frame header `_S`, version, 24-bit depth, geometry, block length table, blocks."""
    return (b'_S\x0000V1.0' + bytes([24]) +
            struct.pack('<HHHH', width, height, len(blocks), block_height) +
            b''.join(struct.pack('<I', len(b)) for b in blocks) + b''.join(blocks))


def eaf_file_bytes(frames: list) -> bytes:
    """File header (magic, frame count, byte-sum checksum, length) + frame table + frames."""
    table = bytearray()
    payload = bytearray()
    for fb in frames:
        framed = b'\x5A\x5A' + fb
        table += struct.pack('<II', len(framed), len(payload))
        payload += framed
    body = bytes(table + payload)
    return b'\x89EAF' + struct.pack('<III', len(frames), sum(body) & 0xFFFFFFFF, len(body)) + body


def load_frames(src_dir: Path):
    files = sorted(glob.glob(str(src_dir / '*.png')))
    if not files:
        sys.exit(f'no PNG frames in {src_dir}')
    colors, alphas = [], []
    for fp in files:
        arr = np.array(Image.open(fp).convert('RGBA'))
        alpha = np.ascontiguousarray(arr[..., 3])
        colors.append(rgb888_to_rgb565_bayer8(arr[..., :3], alpha))
        alphas.append(alpha)
    h, w = alphas[0].shape
    for fp, a in zip(files, alphas):
        if a.shape != (h, w):
            sys.exit(f'{fp}: size {a.shape[1]}x{a.shape[0]} differs from first frame {w}x{h}')
    return colors, alphas


def encode_scene(colors, alphas, block_height: int, deflate: Deflater) -> bytes:
    h, w = alphas[0].shape
    prev_c = np.zeros((h, w), dtype=np.uint16)
    prev_a = np.zeros((h, w), dtype=np.uint8)
    frames = []
    for idx, (cur_c, cur_a) in enumerate(zip(colors, alphas)):
        blocks = []
        for y0 in range(0, h, block_height):
            rows = slice(y0, y0 + block_height)
            blocks.append(encode_block(to_tiles(cur_c[rows]), to_tiles(cur_a[rows]),
                                       to_tiles(prev_c[rows]), to_tiles(prev_a[rows]), idx == 0, deflate))
        frames.append(frame_bytes(w, h, block_height, blocks))
        prev_c, prev_a = cur_c, cur_a
        print(f'\r  frame {idx + 1}/{len(colors)}', end='', flush=True)
    print()
    return eaf_file_bytes(frames)


def frame_crc32(c565: np.ndarray, a8: np.ndarray) -> int:
    """CRC32 of the decoded RGB565A8 buffer: RGB565 plane (little-endian) followed by the A8 plane."""
    return zlib.crc32(c565.astype('<u2').tobytes() + a8.tobytes()) & 0xFFFFFFFF


def rel_path(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(DEMO_DIR))
    except ValueError:
        return str(path)


def c_ident(name: str) -> str:
    return re.sub(r'[^0-9A-Za-z]+', '_', name).strip('_').upper()


def write_header(path: Path, width: int, height: int, scenes: list, blob_bytes: int):
    out = [
        '/* Generated by tools/eaf_encode.py, do not edit. */',
        '#pragma once',
        '',
        '#include <stdint.h>',
        '',
        f'#define EAF_SCENE_WIDTH       {width}',
        f'#define EAF_SCENE_HEIGHT      {height}',
        f'#define EAF_SCENE_COUNT       {len(scenes)}',
        f"#define EAF_SCENE_MAX_FRAMES  {max(s['frame_count'] for s in scenes)}",
        f'#define EAF_ASSETS_BYTES      {blob_bytes}U',
        '',
        'typedef struct {',
        '    const char *name;',
        '    uint32_t offset;           /* byte offset inside the eaf_assets partition */',
        '    uint32_t eaf_bytes;',
        '    uint32_t frame_count;',
        '    const uint32_t *crc32;     /* per frame: CRC32 of RGB565 (LE) plane + A8 plane */',
        '} eaf_scene_t;',
        '',
    ]
    for s in scenes:
        out.append(f"static const uint32_t EAF_CRC32_{c_ident(s['name'])}[{s['frame_count']}] = {{")
        crcs = s['frame_crc32']
        for i in range(0, len(crcs), 6):
            out.append('    ' + ' '.join(f'0x{c:08X}U,' for c in crcs[i:i + 6]))
        out += ['};', '']
    out.append('static const eaf_scene_t EAF_SCENES[EAF_SCENE_COUNT] = {')
    for s in scenes:
        out.append(f"    {{\"{s['name']}\", {s['offset']}U, {s['eaf_bytes']}U, {s['frame_count']}U, "
                   f"EAF_CRC32_{c_ident(s['name'])}}},")
    out += ['};', '']
    path.write_text('\n'.join(out))


def parse_scene(arg: str):
    name, sep, folder = arg.partition('=')
    if not sep or not name or not folder:
        raise argparse.ArgumentTypeError('expected NAME=PNG_DIR')
    return name, Path(folder)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--scene', action='append', type=parse_scene, metavar='NAME=PNG_DIR',
                    help='scene name and PNG folder, repeat for several scenes (default: 3 public synthetic scenes)')
    ap.add_argument('--out', type=Path, default=DEMO_DIR / 'build' / 'lossless565_pack', help='output folder for .eaf/.bin/.json')
    ap.add_argument('--header', type=Path, default=DEMO_DIR / 'build' / 'lossless565_pack' / 'eaf_scenes.h', help='generated C scene table')
    ap.add_argument('--block-height', type=int, default=40, help='rows per block, multiple of 4 (default 40)')
    ap.add_argument('--fast', action='store_true', help='zlib level 9 instead of zopfli (~11%% larger, seconds instead of minutes)')
    ap.add_argument('--zopfli-iterations', type=int, default=5)
    args = ap.parse_args()

    scenes_in = args.scene or DEFAULT_SCENES
    deflate = Deflater(args.fast, args.zopfli_iterations)
    args.out.mkdir(parents=True, exist_ok=True)
    args.header.parent.mkdir(parents=True, exist_ok=True)

    blob = bytearray()
    scenes = []
    size = None
    for name, folder in scenes_in:
        print(f'[{name}] {folder}')
        colors, alphas = load_frames(folder)
        h, w = alphas[0].shape
        if size and size != (w, h):
            sys.exit(f'[{name}] {w}x{h} differs from previous scenes {size[0]}x{size[1]}')
        size = (w, h)
        if w % 4 or args.block_height % 4 or h % args.block_height:
            sys.exit(f'[{name}] width must be a multiple of 4 and height a multiple of --block-height '
                     f'(a multiple of 4); got {w}x{h}, block height {args.block_height}')
        if args.block_height * w // 16 > 0xFFFF:
            sys.exit(f'[{name}] too many 4x4 tiles per block, lower --block-height')

        eaf = encode_scene(colors, alphas, args.block_height, deflate)
        (args.out / f'{c_ident(name).lower()}.eaf').write_bytes(eaf)
        raw_bytes = len(colors) * w * h * 3
        print(f'[{name}] {len(colors)} frames, {len(eaf):,} bytes, {raw_bytes / len(eaf):.1f}x vs raw RGB565A8')

        offset = len(blob)
        blob += eaf
        blob += b'\xFF' * (-len(blob) % BLOB_ALIGN)
        scenes.append({
            'name': name,
            'folder': rel_path(folder),
            'offset': offset,
            'eaf_bytes': len(eaf),
            'frame_count': len(colors),
            'compression_ratio': round(raw_bytes / len(eaf), 2),
            'frame_crc32': [frame_crc32(c, a) for c, a in zip(colors, alphas)],
        })

    (args.out / 'eaf_assets.bin').write_bytes(blob)
    meta = {'width': size[0], 'height': size[1], 'block_height': args.block_height,
            'total_blob_bytes': len(blob), 'scenes': scenes}
    (args.out / 'animation.json').write_text(json.dumps(meta, indent=2))
    write_header(args.header, size[0], size[1], scenes, len(blob))
    print(f'eaf_assets.bin: {len(blob):,} bytes -> {args.out}')
    print(f'scene table     -> {args.header}')


if __name__ == '__main__':
    main()
