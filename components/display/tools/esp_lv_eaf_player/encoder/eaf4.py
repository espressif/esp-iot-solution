# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""EAF4 codec using frame modes 0..11 and declared pixel formats.

Frame modes 0..5, 7, 8, 10, 11 represent codec 0x04 (LOSSLESS); modes 6 and 9
represent codec 0x05 (BOUNDED). They are not top-level codec IDs.

  - Mode 0 (FRAME_FULL)          : Full-frame BGRA8888 Deflate (Keyframe, lossless)
  - Mode 1 (FRAME_XOR_BGRA8888)          : Full-frame BGRA8888 32-bit XOR Deflate (lossless)
  - Mode 2 (FRAME_XOR_PLANAR_BGRA8888)       : 4-plane (B,G,R,A) XOR Deflate (lossless)
  - Mode 3 (FRAME_TILE_XOR_PLANAR_BGRA8888)  : 4x4 tile mask + 4-plane XOR Deflate (lossless)
  - Mode 4 (FRAME_TILE_XOR_PACKED_BGRA8888)  : 4x4 tile mask + interleaved 32-bit BGRA word XOR Deflate (lossless, fast)
  - Mode 5 (FRAME_TILE_JOINT_DELTA_BGRA8888)  : joint ternary symbol (-1..1)^4 + 4-plane escape Deflate (lossless)
  - Mode 6 (FRAME_TILE_BOUNDED_BGRA8888) : 4x4 error-bounded BTC with 64B raw escape
"""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

import numpy as np
from PIL import Image

from .names import (BOUNDED_BLOCK_SKIP, BOUNDED_BLOCK_SOLID, BOUNDED_BLOCK_PALETTE2,
                    BOUNDED_BLOCK_PALETTE4, BOUNDED_BLOCK_LITERAL)

from .bounded_bgra8888 import encode_bounded_bgra8888_frame, restore_bounded_bgra8888_frame

HEADER_SIZE = 64
INDEX_SIZE = 24
HEADER = struct.Struct('<4sHHHHIIIIIIHHIII12s')
INDEX = struct.Struct('<IIIIBBHI')

from .names import (
    FRAME_FULL,
    FRAME_XOR_BGRA8888,
    FRAME_XOR_PLANAR_BGRA8888,
    FRAME_TILE_XOR_PLANAR_BGRA8888,
    FRAME_TILE_XOR_PACKED_BGRA8888,
    FRAME_TILE_JOINT_DELTA_BGRA8888,
    FRAME_TILE_BOUNDED_BGRA8888,
    FRAME_TILE_DELTA_RGB565,
    FRAME_TILE_JOINT_DELTA_RGB565,
    FRAME_TILE_BOUNDED_RGB565,
    FRAME_HSUB_BGRA8888,
    FRAME_TILE_SUB_PACKED_BGRA8888,
    FRAME_MODE_NAMES,
    PIXFMT_BGRA8888,
    PIXFMT_RGB565,
    PIXFMT_RGB565A8,
)


def load_bgra(path: str | Path) -> np.ndarray:
    with Image.open(path) as image:
        rgba = np.array(image.convert('RGBA'), dtype=np.uint8)
    return np.ascontiguousarray(rgba[..., [2, 1, 0, 3]])


def encode_joint_delta(current: np.ndarray, previous: np.ndarray) -> tuple[bytes, int]:
    """Mode 5: 4x4 changed-tile bitmap + ternary joint delta symbol (0..80, 255=escape) + 4-plane escapes."""
    height, width, _ = current.shape
    cur_tiles = current.reshape(height // 4, 4, width // 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 4)
    old_tiles = previous.reshape(height // 4, 4, width // 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 4)
    changed = np.any(cur_tiles != old_tiles, axis=(1, 2))
    mask_bytes = np.packbits(changed, bitorder='little').tobytes()
    now = cur_tiles[changed].reshape(-1, 4)
    old = old_tiles[changed].reshape(-1, 4)
    if len(now) == 0:
        raw = mask_bytes
        return zlib.compress(raw, 6), len(raw)
    delta = now.astype(np.int16) - old.astype(np.int16)
    small = np.all((delta >= -1) & (delta <= 1), axis=1)
    symbols = np.full(len(now), 255, dtype=np.uint8)
    d = delta[small] + 1
    symbols[small] = (d[:, 0] * 27 + d[:, 1] * 9 + d[:, 2] * 3 + d[:, 3]).astype(np.uint8)
    escapes = np.ascontiguousarray(now[~small].T).tobytes()
    raw = mask_bytes + symbols.tobytes() + escapes
    return zlib.compress(raw, 6), len(raw)


def encode_hsub(current: np.ndarray) -> tuple[bytes, int]:
    """Mode 10: Full-frame horizontal byte-wise SUB delta (keyframe-capable)."""
    hs = current.copy()
    hs[:, 1:, :] = (current[:, 1:, :].astype(np.int16) - current[:, :-1, :].astype(np.int16)) & 0xFF
    raw = hs.tobytes()
    return zlib.compress(raw, 6), len(raw)


def candidates(current: np.ndarray, previous: np.ndarray) -> dict[int, tuple[bytes, int]]:
    """Return {mode: (deflate_blob, uncompressed_size)} for lossless BGRA8888 modes."""
    height, width, _ = current.shape
    difference = current ^ previous
    tiles = difference.reshape(height // 4, 4, width // 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 4)
    changed = np.any(tiles != 0, axis=(1, 2))
    mask_bytes = np.packbits(changed, bitorder='little').tobytes()
    changed_tiles = tiles[changed]
    tile_planar_blob = mask_bytes + changed_tiles.transpose(2, 0, 1).tobytes()
    raw_list = [
        (FRAME_FULL, current.tobytes()),
        (FRAME_XOR_BGRA8888, difference.tobytes()),
        (FRAME_XOR_PLANAR_BGRA8888, difference.transpose(2, 0, 1).tobytes()),
        (FRAME_TILE_XOR_PLANAR_BGRA8888, tile_planar_blob),
    ]
    out: dict[int, tuple[bytes, int]] = {
        mode_id: (zlib.compress(payload, 6), len(payload)) for mode_id, payload in raw_list
    }
    tile_packed_xor = mask_bytes + np.ascontiguousarray(changed_tiles).tobytes()
    out[FRAME_TILE_XOR_PACKED_BGRA8888] = (zlib.compress(tile_packed_xor, 6), len(tile_packed_xor))
    out[FRAME_TILE_JOINT_DELTA_BGRA8888] = encode_joint_delta(current, previous)
    out[FRAME_HSUB_BGRA8888] = encode_hsub(current)
    diff_sub = ((current.astype(np.int16) - previous.astype(np.int16)) & 0xFF).astype(np.uint8)
    tiles_sub = diff_sub.reshape(height // 4, 4, width // 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 4)
    tile_packed_sub = mask_bytes + np.ascontiguousarray(tiles_sub[changed]).tobytes()
    out[FRAME_TILE_SUB_PACKED_BGRA8888] = (zlib.compress(tile_packed_sub, 6), len(tile_packed_sub))
    return out


def restore(blob: bytes, mode: int, previous: np.ndarray) -> np.ndarray:
    height, width, _ = previous.shape
    if mode == FRAME_TILE_BOUNDED_BGRA8888:
        return restore_bounded_bgra8888_frame(blob, previous)
    data = zlib.decompress(blob)
    if mode == FRAME_FULL:
        return np.frombuffer(data, dtype=np.uint8).reshape(height, width, 4).copy()
    if mode == FRAME_HSUB_BGRA8888:
        hs = np.frombuffer(data, dtype=np.uint8).reshape(height, width, 4)
        return np.ascontiguousarray((np.cumsum(hs.astype(np.uint16), axis=1) & 0xFF).astype(np.uint8))
    if mode == FRAME_XOR_BGRA8888:
        return np.frombuffer(data, dtype=np.uint8).reshape(height, width, 4) ^ previous
    if mode == FRAME_XOR_PLANAR_BGRA8888:
        return np.frombuffer(data, dtype=np.uint8).reshape(4, height, width).transpose(1, 2, 0) ^ previous
    tile_count = (height // 4) * (width // 4)
    mask_bytes = (tile_count + 7) // 8
    mask = np.unpackbits(np.frombuffer(data[:mask_bytes], dtype=np.uint8), bitorder='little')[:tile_count].astype(bool)
    changed = int(mask.sum())
    if mode == FRAME_TILE_XOR_PLANAR_BGRA8888:
        tiles = np.zeros((tile_count, 16, 4), dtype=np.uint8)
        if changed:
            tiles[mask] = np.frombuffer(data[mask_bytes:], dtype=np.uint8).reshape(4, changed, 16).transpose(1, 2, 0)
        restored = tiles.reshape(height // 4, width // 4, 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(height, width, 4)
        return restored ^ previous
    if mode == FRAME_TILE_XOR_PACKED_BGRA8888:
        tiles = np.zeros((tile_count, 16, 4), dtype=np.uint8)
        if changed:
            tiles[mask] = np.frombuffer(data[mask_bytes:], dtype=np.uint8).reshape(changed, 16, 4)
        restored = tiles.reshape(height // 4, width // 4, 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(height, width, 4)
        return restored ^ previous
    if mode == FRAME_TILE_SUB_PACKED_BGRA8888:
        tiles = np.zeros((tile_count, 16, 4), dtype=np.uint8)
        if changed:
            tiles[mask] = np.frombuffer(data[mask_bytes:], dtype=np.uint8).reshape(changed, 16, 4)
        restored = tiles.reshape(height // 4, width // 4, 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(height, width, 4)
        return np.ascontiguousarray(((previous.astype(np.uint16) + restored.astype(np.uint16)) & 0xFF).astype(np.uint8))
    if mode == FRAME_TILE_JOINT_DELTA_BGRA8888:
        old_tiles = previous.reshape(height // 4, 4, width // 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 4).copy()
        if changed == 0:
            return previous.copy()
        sym_end = mask_bytes + changed * 16
        sym = np.frombuffer(data[mask_bytes:sym_end], dtype=np.uint8)
        baseline = old_tiles[mask].reshape(-1, 4).astype(np.int16)
        sm = sym != 255
        for c_idx, weight in enumerate((27, 9, 3, 1)):
            baseline[sm, c_idx] += ((sym[sm] // weight) % 3).astype(np.int16) - 1
        escaped = int((~sm).sum())
        if escaped:
            esc = np.frombuffer(data[sym_end:], dtype=np.uint8).reshape(4, escaped).T
            baseline[~sm] = esc
        old_tiles[mask] = (baseline & 0xFF).astype(np.uint8).reshape(changed, 16, 4)
        return np.ascontiguousarray(
            old_tiles.reshape(height // 4, width // 4, 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(height, width, 4)
        )
    raise ValueError(f'unknown EAF4 mode {mode}')


def _pack(
    records: list[tuple[int, int, int, int, int]],
    payloads: list[bytes],
    width: int,
    height: int,
    period_ms: int,
    pixel_format: int = PIXFMT_BGRA8888,
    codec_id: int = 0,
) -> bytes:
    bpp = 4 if pixel_format == PIXFMT_BGRA8888 else (3 if pixel_format == PIXFMT_RGB565A8 else 2)
    index_offset = HEADER_SIZE
    data_offset = HEADER_SIZE + len(records) * INDEX_SIZE
    offset = data_offset
    index = bytearray()
    for length, decoded_size, frame_crc, mode, payload_crc in records:
        index.extend(INDEX.pack(offset, length, decoded_size, frame_crc, mode, 0, 0, payload_crc))
        offset += length
    reserved = bytearray(12)
    reserved[0] = pixel_format & 0xFF
    reserved[1] = codec_id & 0xFF
    header = bytearray(
        HEADER.pack(
            b'EAF4',
            1,
            HEADER_SIZE,
            width,
            height,
            len(records),
            period_ms,
            width * height * bpp,
            index_offset,
            data_offset,
            offset,
            4,
            4,
            0,
            zlib.crc32(index) & 0xFFFFFFFF,
            1,
            bytes(reserved),
        )
    )
    struct.pack_into('<I', header, 40, zlib.crc32(header) & 0xFFFFFFFF)
    return bytes(header) + bytes(index) + b''.join(payloads)


def _bgra_to_565(bgra: np.ndarray) -> np.ndarray:
    b = (bgra[..., 0].astype(np.uint16) >> 3)
    g = (bgra[..., 1].astype(np.uint16) >> 2)
    r = (bgra[..., 2].astype(np.uint16) >> 3)
    return (r << 11) | (g << 5) | b


def _565_to_bgra(c565: np.ndarray, a8: np.ndarray | None = None) -> np.ndarray:
    h, w = c565.shape
    out = np.empty((h, w, 4), dtype=np.uint8)
    r5 = (c565 >> 11) & 31
    g6 = (c565 >> 5) & 63
    b5 = c565 & 31
    out[..., 0] = (b5 * 8).astype(np.uint8)
    out[..., 1] = (g6 * 4).astype(np.uint8)
    out[..., 2] = (r5 * 8).astype(np.uint8)
    out[..., 3] = a8 if a8 is not None else 255
    return out


def _encode_565a8_frame_candidates(
    cur_c: np.ndarray,
    cur_a: np.ndarray,
    prev_c: np.ndarray,
    prev_a: np.ndarray,
    with_alpha: bool = True,
) -> list[tuple[int, bytes, int]]:
    """Return [(mode, zlib_blob, raw_len), ...] for 565+A8 lossless inter-frame."""
    h, w = cur_c.shape
    ty, tx = h // 4, w // 4
    c_tiles = cur_c.reshape(ty, 4, tx, 4).transpose(0, 2, 1, 3).reshape(-1, 16)
    pc_tiles = prev_c.reshape(ty, 4, tx, 4).transpose(0, 2, 1, 3).reshape(-1, 16)
    a_tiles = cur_a.reshape(ty, 4, tx, 4).transpose(0, 2, 1, 3).reshape(-1, 16)
    pa_tiles = prev_a.reshape(ty, 4, tx, 4).transpose(0, 2, 1, 3).reshape(-1, 16)

    chg = ~(np.all(c_tiles == pc_tiles, axis=1) & np.all(a_tiles == pa_tiles, axis=1))
    mask_bytes = np.packbits(chg.astype(np.uint8), bitorder='little').tobytes()
    if not np.any(chg):
        return [(FRAME_TILE_DELTA_RGB565, zlib.compress(mask_bytes, 6), len(mask_bytes))]

    dc = c_tiles[chg] ^ pc_tiles[chg]
    da = ((a_tiles[chg].astype(np.int16) - pa_tiles[chg].astype(np.int16)) & 0xFF).astype(np.uint8)
    tiles_raw = (
        mask_bytes
        + (dc & 0xFF).astype(np.uint8).tobytes()
        + (dc >> 8).astype(np.uint8).tobytes()
        + (da.tobytes() if with_alpha else b'')
    )

    # Joint ternary symbol candidate (mode 8)
    now_c = c_tiles[chg].reshape(-1).astype(np.int16)
    old_c = pc_tiles[chg].reshape(-1).astype(np.int16)
    now_a = a_tiles[chg].reshape(-1).astype(np.int16)
    old_a = pa_tiles[chg].reshape(-1).astype(np.int16)
    dr = ((now_c >> 11) & 31) - ((old_c >> 11) & 31)
    dg = ((now_c >> 5) & 63) - ((old_c >> 5) & 63)
    db = (now_c & 31) - (old_c & 31)
    da_j = now_a - old_a
    small = (np.abs(dr) <= 1) & (np.abs(dg) <= 1) & (np.abs(db) <= 1) & (np.abs(da_j) <= 1)
    sym = np.where(small, (dr + 1) * 27 + (dg + 1) * 9 + (db + 1) * 3 + (da_j + 1), 255).astype(np.uint8)
    esc_c = now_c[~small].astype(np.uint16)
    esc_a = now_a[~small].astype(np.uint8)
    joint_raw = (
        mask_bytes
        + sym.tobytes()
        + (esc_c & 0xFF).astype(np.uint8).tobytes()
        + (esc_c >> 8).astype(np.uint8).tobytes()
        + (esc_a.tobytes() if with_alpha else b'')
    )
    return [
        (FRAME_TILE_DELTA_RGB565, zlib.compress(tiles_raw, 6), len(tiles_raw)),
        (FRAME_TILE_JOINT_DELTA_RGB565, zlib.compress(joint_raw, 6), len(joint_raw)),
    ]


def _restore_565_frame(
    blob: bytes,
    mode: int,
    prev_bytes: bytes,
    width: int,
    height: int,
    pixel_format: int,
) -> bytes:
    data = zlib.decompress(blob)
    pixels = width * height
    has_alpha = pixel_format == PIXFMT_RGB565A8
    bpp = 3 if has_alpha else 2
    if mode == FRAME_FULL:
        if len(data) != pixels * bpp:
            raise ValueError('bad 565 raw frame size')
        return bytes(data)

    buf = bytearray(prev_bytes)
    fb16 = np.frombuffer(buf, dtype='<u2', count=pixels)
    alpha = np.frombuffer(buf, dtype=np.uint8, offset=pixels * 2, count=pixels) if has_alpha else None
    tiles = pixels // 16
    mask_size = (tiles + 7) // 8
    cols = width // 4

    if mode == FRAME_TILE_DELTA_RGB565:
        mask = np.unpackbits(np.frombuffer(data[:mask_size], dtype=np.uint8), bitorder='little')[:tiles].astype(bool)
        changed = int(mask.sum())
        c_low = np.frombuffer(data, dtype=np.uint8, offset=mask_size, count=changed * 16)
        c_high = np.frombuffer(data, dtype=np.uint8, offset=mask_size + changed * 16, count=changed * 16)
        a_src = (
            np.frombuffer(data, dtype=np.uint8, offset=mask_size + changed * 32, count=changed * 16)
            if has_alpha
            else None
        )
        ord_i = 0
        for t in range(tiles):
            if not mask[t]:
                continue
            ty, tx = (t // cols) * 4, (t % cols) * 4
            for r in range(4):
                row_off = (ty + r) * width + tx
                for c in range(4):
                    p = ord_i * 16 + r * 4 + c
                    w = int(c_low[p]) | (int(c_high[p]) << 8)
                    fb16[row_off + c] ^= w
                    if alpha is not None and a_src is not None:
                        alpha[row_off + c] = (int(alpha[row_off + c]) + int(a_src[p])) & 0xFF
            ord_i += 1
        return bytes(buf)

    if mode == FRAME_TILE_JOINT_DELTA_RGB565:
        mask = np.unpackbits(np.frombuffer(data[:mask_size], dtype=np.uint8), bitorder='little')[:tiles].astype(bool)
        changed = int(mask.sum())
        if changed == 0:
            return bytes(buf)
        sym_cnt = changed * 16
        syms = np.frombuffer(data, dtype=np.uint8, offset=mask_size, count=sym_cnt)
        escaped = int((syms == 255).sum())
        esc_lo = np.frombuffer(data, dtype=np.uint8, offset=mask_size + sym_cnt, count=escaped)
        esc_hi = np.frombuffer(data, dtype=np.uint8, offset=mask_size + sym_cnt + escaped, count=escaped)
        esc_a = (
            np.frombuffer(data, dtype=np.uint8, offset=mask_size + sym_cnt + 2 * escaped, count=escaped)
            if has_alpha
            else None
        )
        ord_i = 0
        esc_i = 0
        for t in range(tiles):
            if not mask[t]:
                continue
            ty, tx = (t // cols) * 4, (t % cols) * 4
            for p in range(16):
                off = (ty + p // 4) * width + tx + p % 4
                s = int(syms[ord_i])
                ord_i += 1
                if s == 255:
                    fb16[off] = int(esc_lo[esc_i]) | (int(esc_hi[esc_i]) << 8)
                    if alpha is not None and esc_a is not None:
                        alpha[off] = esc_a[esc_i]
                    esc_i += 1
                else:
                    dr = (s // 27) - 1
                    dg = ((s // 9) % 3) - 1
                    db = ((s // 3) % 3) - 1
                    da = (s % 3) - 1
                    v = int(fb16[off])
                    d = ((dr & 31) << 11) | ((dg & 63) << 5) | (db & 31)
                    fb16[off] = (((v & 0xF800) + (d & 0xF800)) & 0xF800) | (((v & 0x07E0) + (d & 0x07E0)) & 0x07E0) | (((v & 0x001F) + (d & 0x001F)) & 0x001F)
                    if alpha is not None:
                        alpha[off] = (int(alpha[off]) + da) & 0xFF
        return bytes(buf)

    if mode == FRAME_TILE_BOUNDED_RGB565:
        modes = np.frombuffer(data[:tiles], dtype=np.uint8)
        pos = tiles
        for t in range(tiles):
            m = int(modes[t])
            if m == BOUNDED_BLOCK_SKIP:
                continue
            ty, tx = (t // cols) * 4, (t % cols) * 4
            if m == BOUNDED_BLOCK_SOLID:
                c0 = int.from_bytes(data[pos : pos + 2], 'little')
                pos += 2
                pal = [c0, c0, c0, c0]
                mask = 0
            elif m == BOUNDED_BLOCK_PALETTE2:
                c0 = int.from_bytes(data[pos : pos + 2], 'little')
                c1 = int.from_bytes(data[pos + 2 : pos + 4], 'little')
                mask = int.from_bytes(data[pos + 4 : pos + 6], 'little')
                pos += 6
                pal = [c0, c1, c0, c1]
            elif m == BOUNDED_BLOCK_PALETTE4:
                c0 = int.from_bytes(data[pos : pos + 2], 'little')
                c3 = int.from_bytes(data[pos + 2 : pos + 4], 'little')
                mask = int.from_bytes(data[pos + 4 : pos + 8], 'little')
                pos += 8
                r0, g0, b0 = (c0 >> 11) & 31, (c0 >> 5) & 63, c0 & 31
                r3, g3, b3 = (c3 >> 11) & 31, (c3 >> 5) & 63, c3 & 31
                c1 = (((2 * r0 + r3) // 3) << 11) | (((2 * g0 + g3) // 3) << 5) | ((2 * b0 + b3) // 3)
                c2 = (((r0 + 2 * r3) // 3) << 11) | (((g0 + 2 * g3) // 3) << 5) | ((b0 + 2 * b3) // 3)
                pal = [c0, c1, c2, c3]
            else:
                raw_px = np.frombuffer(data, dtype='<u2', offset=pos, count=16)
                pos += 32
                for p in range(16):
                    fb16[(ty + p // 4) * width + tx + p % 4] = raw_px[p]
                continue
            for p in range(16):
                idx = 0 if m == BOUNDED_BLOCK_SOLID else ((mask >> p) & 1 if m == BOUNDED_BLOCK_PALETTE2 else (mask >> (2 * p)) & 3)
                fb16[(ty + p // 4) * width + tx + p % 4] = pal[idx]
        if pos != len(data):
            raise ValueError('BTC565 trailing bytes')
        return bytes(buf)

    raise ValueError(f'unsupported 565 mode {mode}')


def encode_565(
    frames: list[np.ndarray],
    period_ms: int = 67,
    key_interval: int = 30,
    codec: str = 'lossless',
    max_error: int = 8,
    with_alpha: bool | None = None,
) -> tuple[bytes, dict]:
    """Encode BGRA8888 frames into the unified EAF4 container using RGB565/RGB565A8."""
    from .eaf565 import encode_btc_block, reconstruct_btc_block, rgb565_components

    if not frames or not 10 <= period_ms <= 10000 or key_interval < 1:
        raise ValueError('invalid EAF4 565 parameters')
    height, width, _ = frames[0].shape
    if width % 4 or height % 4 or not 0 < width <= 1024 or not 0 < height <= 1024:
        raise ValueError(f'invalid dimensions {width}x{height}')

    if codec not in ('lossless', 'bounded'):
        raise ValueError(f'unsupported RGB565 codec: {codec}')
    is_btc = codec == 'bounded'
    if with_alpha is None:
        with_alpha = not is_btc
    if is_btc and with_alpha:
        raise ValueError('RGB565 Bounded does not support an A8 plane')
    pixfmt = PIXFMT_RGB565A8 if with_alpha else PIXFMT_RGB565
    top_codec = 0x05 if is_btc else 0x04
    bpp = 3 if with_alpha else 2
    pixels = width * height

    payloads: list[bytes] = []
    records: list[tuple[int, int, int, int, int]] = []
    counts = [0] * 10
    frame_crcs: list[int] = []
    prev_c = np.zeros((height, width), dtype=np.uint16)
    prev_a = np.zeros((height, width), dtype=np.uint8)
    prev_bytes = bytes(pixels * bpp)

    for index, bgra in enumerate(frames):
        cur_c = _bgra_to_565(bgra)
        cur_a = np.ascontiguousarray(bgra[..., 3]) if with_alpha else np.zeros((height, width), dtype=np.uint8)
        is_key = index % key_interval == 0

        if is_btc:
            mode = FRAME_TILE_BOUNDED_RGB565
            ty, tx = height // 4, width // 4
            blocks = cur_c.reshape(ty, 4, tx, 4).transpose(0, 2, 1, 3).reshape(-1, 16)
            refs = prev_c.reshape(ty, 4, tx, 4).transpose(0, 2, 1, 3).reshape(-1, 16).copy()
            modes_arr = bytearray(len(blocks))
            data_arr = bytearray()
            for t, blk in enumerate(blocks):
                target = rgb565_components(blk)
                err = np.abs(target - rgb565_components(refs[t]))
                if not is_key and err.max() <= max_error:
                    continue
                m, ep, mask = encode_btc_block(target, max_error)
                rebuilt = reconstruct_btc_block(m, ep, mask)
                if np.max(np.abs(target - rgb565_components(rebuilt))) > max_error:
                    m, ep, mask, rebuilt = BOUNDED_BLOCK_LITERAL, blk.astype('<u2').tobytes(), b'', blk
                modes_arr[t] = m
                data_arr += ep + mask
                refs[t] = rebuilt
            prev_c = refs.reshape(ty, tx, 4, 4).transpose(0, 2, 1, 3).reshape(height, width)
            raw = bytes(modes_arr + data_arr)
            blob = zlib.compress(raw, 6)
            decoded_size = len(raw)
            frame_raw_bytes = prev_c.astype('<u2').tobytes()
        else:
            frame_raw_bytes = cur_c.astype('<u2').tobytes() + (cur_a.tobytes() if with_alpha else b'')
            raw_cand = (FRAME_FULL, zlib.compress(frame_raw_bytes, 6), len(frame_raw_bytes))
            if is_key:
                mode, blob, decoded_size = raw_cand
            else:
                inter_cands = _encode_565a8_frame_candidates(cur_c, cur_a, prev_c, prev_a, with_alpha=with_alpha)
                mode, blob, decoded_size = min([raw_cand, *inter_cands], key=lambda item: len(item[1]))
            prev_c = cur_c
            prev_a = cur_a

        verify_bytes = _restore_565_frame(blob, mode, prev_bytes, width, height, pixfmt)
        if verify_bytes != frame_raw_bytes:
            raise ValueError(f'565 roundtrip mismatch at frame {index} (mode {mode})')
        prev_bytes = verify_bytes
        frame_crc = zlib.crc32(frame_raw_bytes) & 0xFFFFFFFF
        records.append((len(blob), decoded_size, frame_crc, mode, zlib.crc32(blob) & 0xFFFFFFFF))
        payloads.append(blob)
        counts[mode] += 1
        frame_crcs.append(frame_crc)

    out_blob = _pack(records, payloads, width, height, period_ms, pixel_format=pixfmt, codec_id=top_codec)
    summary = {
        'width': width,
        'height': height,
        'frames': len(records),
        'period_ms': period_ms,
        'key_interval': key_interval,
        'pixel_format': pixfmt,
        'bytes': len(out_blob),
        'mode_counts_all': counts,
        'frame_crc32': frame_crcs,
    }
    return out_blob, summary


def encode(
    frames: list[np.ndarray],
    period_ms: int = 67,
    key_interval: int = 30,
    codec: str = 'lossless',
    forced_mode: int | None = None,
    max_error: int = 8,
    clean_alpha: bool = True,
    temporal_deadzone: int = 0,
) -> tuple[bytes, dict]:
    """Encode BGRA8888 frames into an EAF4 container.

    Lossless uses one adaptive strategy; forced_mode is for format tests.
    Transparent RGB is normalized by default; disable clean_alpha for byte-exact input.
    """
    if codec not in ('lossless', 'bounded'):
        raise ValueError(f'unsupported BGRA8888 codec: {codec}')
    if not frames or not 10 <= period_ms <= 10000 or key_interval < 1 or not 0 <= temporal_deadzone <= 255:
        raise ValueError('invalid EAF4 parameters')
    if temporal_deadzone and (codec == 'bounded' or forced_mode == FRAME_TILE_BOUNDED_BGRA8888):
        raise ValueError('temporal_deadzone requires lossless encoding')
    payloads: list[bytes] = []
    records: list[tuple[int, int, int, int, int]] = []
    previous = None
    counts = [0] * 12
    frame_crcs: list[int] = []
    width = height = 0
    is_lossless = codec != 'bounded' and forced_mode != FRAME_TILE_BOUNDED_BGRA8888

    for index, current in enumerate(frames):
        if current.dtype != np.uint8 or current.ndim != 3 or current.shape[2] != 4:
            raise ValueError(f'frame {index} is not uint8 BGRA')
        height, width, _ = current.shape
        if width % 4 or height % 4 or not 0 < width <= 1024 or not 0 < height <= 1024:
            raise ValueError(f'invalid dimensions {width}x{height}')
        current = np.ascontiguousarray(current)
        if clean_alpha or temporal_deadzone > 0:
            current = current.copy()
            if clean_alpha:
                current[current[..., 3] == 0, :3] = 0
        if previous is None:
            previous = np.zeros_like(current)
        if current.shape != previous.shape:
            raise ValueError('frame dimensions differ')

        is_key = index % key_interval == 0

        if codec == 'bounded' or forced_mode == FRAME_TILE_BOUNDED_BGRA8888:
            mode = FRAME_TILE_BOUNDED_BGRA8888
            blob, decoded_size, decoded = encode_bounded_bgra8888_frame(
                current, previous, keyframe=is_key, max_error=max_error
            )
            verify = restore(blob, mode, previous)
            if not np.array_equal(decoded, verify):
                raise ValueError(f'BTC8888 decoder mismatch at frame {index}')
            max_err = int(np.max(np.abs(current.astype(np.int16) - decoded.astype(np.int16))))
            if max_err > max_error:
                raise ValueError(f'BTC8888 error bound {max_err} > {max_error} at frame {index}')
        else:
            def _select_lossless(cur_img: np.ndarray) -> tuple[int, bytes, int]:
                ch = candidates(cur_img, previous)
                if is_key:
                    if forced_mode == FRAME_HSUB_BGRA8888:
                        m = FRAME_HSUB_BGRA8888
                    elif forced_mode is None:
                        full_l = len(ch[FRAME_FULL][0])
                        hsub_l = len(ch[FRAME_HSUB_BGRA8888][0])
                        m = FRAME_HSUB_BGRA8888 if hsub_l + 256 <= full_l else FRAME_FULL
                    else:
                        m = FRAME_FULL
                elif forced_mode is not None:
                    m = forced_mode
                else:
                    fast_modes = (
                        FRAME_FULL,
                        FRAME_HSUB_BGRA8888,
                        FRAME_TILE_XOR_PACKED_BGRA8888,
                        FRAME_TILE_SUB_PACKED_BGRA8888,
                    )
                    best_fast = min(fast_modes, key=lambda k: len(ch[k][0]))
                    fast_len = len(ch[best_fast][0])
                    joint_len = len(ch[FRAME_TILE_JOINT_DELTA_BGRA8888][0])
                    if joint_len + 256 <= fast_len and joint_len <= int(fast_len * 0.92):
                        m = FRAME_TILE_JOINT_DELTA_BGRA8888
                    else:
                        m = best_fast
                return m, ch[m][0], ch[m][1]

            mode, blob, decoded_size = _select_lossless(current)
            if not is_key and temporal_deadzone > 0:
                diff_abs = np.max(np.abs(current.astype(np.int16) - previous.astype(np.int16)), axis=-1)
                keep = diff_abs <= temporal_deadzone
                if np.any(keep):
                    cand_dz = current.copy()
                    cand_dz[keep] = previous[keep]
                    m_dz, b_dz, d_dz = _select_lossless(cand_dz)
                    if len(b_dz) < len(blob):
                        current = cand_dz
                        mode, blob, decoded_size = m_dz, b_dz, d_dz

            decoded = restore(blob, mode, previous)
            if not np.array_equal(current, decoded):
                raise ValueError(f'roundtrip failed at frame {index} (mode {mode})')

        frame_crc = zlib.crc32(decoded.tobytes()) & 0xFFFFFFFF
        records.append((len(blob), decoded_size, frame_crc, mode, zlib.crc32(blob) & 0xFFFFFFFF))
        payloads.append(blob)
        counts[mode] += 1
        frame_crcs.append(frame_crc)
        previous = decoded

    blob = _pack(records, payloads, width, height, period_ms)
    summary = {
        'width': width,
        'height': height,
        'frames': len(records),
        'period_ms': period_ms,
        'key_interval': key_interval,
        'codec': codec,
        'bytes': len(blob),
        'mode_counts': counts if any(counts[4:]) else counts[:4],
        'mode_counts_all': counts,
        'frame_crc32': frame_crcs,
        'clean_alpha': clean_alpha,
        'temporal_deadzone': temporal_deadzone,
        'roundtrip_exact': is_lossless and not clean_alpha and temporal_deadzone == 0,
    }
    return blob, summary


def encode_paths(
    paths: list[str | Path],
    output: str | Path,
    period_ms: int = 67,
    key_interval: int = 30,
    codec: str = 'lossless',
    forced_mode: int | None = None,
    max_error: int = 8,
    clean_alpha: bool = True,
    temporal_deadzone: int = 0,
) -> dict:
    frames = [load_bgra(path) for path in paths]
    blob, summary = encode(
        frames,
        period_ms=period_ms,
        key_interval=key_interval,
        codec=codec,
        forced_mode=forced_mode,
        max_error=max_error,
        clean_alpha=clean_alpha,
        temporal_deadzone=temporal_deadzone,
    )
    destination = Path(output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open('wb') as handle:
        handle.write(blob)
    return summary


def decode(data: bytes) -> list[np.ndarray]:
    """Reference decoder for Modes 0..11 across BGRA8888, RGB565, and RGB565A8."""
    if len(data) < HEADER_SIZE or data[:4] != b'EAF4':
        raise ValueError('not an EAF4 file')
    header = bytearray(data[:HEADER_SIZE])
    expected = struct.unpack_from('<I', header, 40)[0]
    header[40:44] = b'\x00\x00\x00\x00'
    if (zlib.crc32(header) & 0xFFFFFFFF) != expected:
        raise ValueError('header crc mismatch')
    version, header_size, width, height = struct.unpack_from('<HHHH', data, 4)
    frames, _period_ms, frame_bytes, index_offset, data_offset, file_size = struct.unpack_from('<IIIIII', data, 12)
    tile_w, tile_h, _header_crc, index_crc, flags = struct.unpack_from('<HHIII', data, 36)
    if (version, header_size, tile_w, tile_h, flags) != (1, 64, 4, 4, 1):
        raise ValueError('unsupported EAF4 header')
    if file_size != len(data) or index_offset != 64 or data_offset != 64 + frames * INDEX_SIZE:
        raise ValueError('bad EAF4 layout')
    pixfmt = data[52]
    bpp = 4 if pixfmt == PIXFMT_BGRA8888 else (3 if pixfmt == PIXFMT_RGB565A8 else 2)
    if pixfmt > PIXFMT_RGB565A8 or frame_bytes != width * height * bpp or data[54:64] != bytes(10):
        raise ValueError('bad EAF4 geometry')
    index = data[HEADER_SIZE:data_offset]
    if (zlib.crc32(index) & 0xFFFFFFFF) != index_crc:
        raise ValueError('index crc mismatch')
    output: list[np.ndarray] = []
    if pixfmt == PIXFMT_BGRA8888:
        previous = np.zeros((height, width, 4), dtype=np.uint8)
        for frame in range(frames):
            offset, length, decoded_size, frame_crc, mode, _pad, _reserved16, payload_crc = INDEX.unpack_from(
                index, frame * INDEX_SIZE
            )
            blob = data[offset : offset + length]
            if (zlib.crc32(blob) & 0xFFFFFFFF) != payload_crc:
                raise ValueError(f'payload crc mismatch at frame {frame}')
            if frame == 0 and mode not in (FRAME_FULL, FRAME_HSUB_BGRA8888, FRAME_TILE_BOUNDED_BGRA8888):
                raise ValueError('frame 0 must be a keyframe')
            restored = restore(blob, mode, previous)
            if restored.shape != previous.shape or decoded_size != len(zlib.decompress(blob)):
                raise ValueError(f'bad frame {frame}')
            if (zlib.crc32(restored.tobytes()) & 0xFFFFFFFF) != frame_crc:
                raise ValueError(f'frame crc mismatch at frame {frame}')
            output.append(np.ascontiguousarray(restored))
            previous = output[-1]
        return output

    pixels = width * height
    prev_bytes = bytes(frame_bytes)
    for frame in range(frames):
        offset, length, decoded_size, frame_crc, mode, _pad, _reserved16, payload_crc = INDEX.unpack_from(
            index, frame * INDEX_SIZE
        )
        blob = data[offset : offset + length]
        if (zlib.crc32(blob) & 0xFFFFFFFF) != payload_crc:
            raise ValueError(f'payload crc mismatch at frame {frame}')
        if frame == 0 and mode not in (FRAME_FULL, FRAME_TILE_BOUNDED_RGB565):
            raise ValueError('frame 0 must be a keyframe')
        if decoded_size != len(zlib.decompress(blob)):
            raise ValueError(f'bad frame {frame}')
        prev_bytes = _restore_565_frame(blob, mode, prev_bytes, width, height, pixfmt)
        if (zlib.crc32(prev_bytes) & 0xFFFFFFFF) != frame_crc:
            raise ValueError(f'frame crc mismatch at frame {frame}')
        c565 = np.frombuffer(prev_bytes, dtype='<u2', count=pixels).reshape(height, width)
        a8 = (
            np.frombuffer(prev_bytes, dtype=np.uint8, offset=pixels * 2, count=pixels).reshape(height, width)
            if pixfmt == PIXFMT_RGB565A8
            else None
        )
        output.append(_565_to_bgra(c565, a8))
    return output
