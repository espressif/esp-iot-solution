# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""BGRA8888 Bounded (EAF4 submode 6, unified codec 0x05) for 8-bit BGRA8888.

Uncompressed frame payload layout (before zlib):
  - modes[num_tiles] : 1 byte per 4x4 tile in raster order
      0 = Skip       (0 bytes)  : keep previous reconstructed 4x4 tile
      1 = Flat       (4 bytes)  : 1x BGRA8888 color replicated to all 16 pixels
      2 = 2-Color    (10 bytes) : 2x BGRA8888 endpoints (8B) + uint16 1bpp mask (2B)
      3 = 4-Color    (12 bytes) : 2x BGRA8888 endpoints c0,c3 (8B) + uint32 2bpp mask (4B),
                                  palette = [c0, (2*c0+c3)//3, (c0+2*c3)//3, c3]
      4 = Raw Escape (64 bytes) : 16x BGRA8888 pixels verbatim (guarantees max_err <= limit)
  - data[...]        : concatenated tile payloads for tiles with mode != 0
"""

from __future__ import annotations

import zlib
import numpy as np

from .names import (BOUNDED_BLOCK_SKIP, BOUNDED_BLOCK_SOLID, BOUNDED_BLOCK_PALETTE2,
                    BOUNDED_BLOCK_PALETTE4, BOUNDED_BLOCK_LITERAL)
PAYLOAD_BYTES = (0, 4, 10, 12, 64)


def encode_bounded_bgra8888_frame(
    current: np.ndarray,
    previous: np.ndarray,
    keyframe: bool = False,
    max_error: int = 8,
) -> tuple[bytes, int, np.ndarray]:
    """Encode one BGRA8888 frame using 4x4 BGRA8888 Bounded and return (zlib_blob, raw_len, reconstructed)."""
    h, w, _ = current.shape
    ty, tx = h // 4, w // 4
    num_tiles = ty * tx
    cur_tiles = current.reshape(ty, 4, tx, 4, 4).transpose(0, 2, 1, 3, 4).reshape(num_tiles, 16, 4)
    prev_tiles = previous.reshape(ty, 4, tx, 4, 4).transpose(0, 2, 1, 3, 4).reshape(num_tiles, 16, 4)
    rebuilt_tiles = prev_tiles.copy()

    cur_i = cur_tiles.astype(np.int32)
    prev_i = prev_tiles.astype(np.int32)

    modes = np.zeros(num_tiles, dtype=np.uint8)
    if not keyframe:
        temporal_err = np.max(np.abs(cur_i - prev_i), axis=(1, 2))
        active_mask = temporal_err > max_error
    else:
        active_mask = np.ones(num_tiles, dtype=bool)

    active_indices = np.nonzero(active_mask)[0]
    if active_indices.size == 0:
        raw = modes.tobytes()
        return zlib.compress(raw, 6), len(raw), previous.copy()

    act = cur_i[active_indices]  # (M, 16, 4)
    M = act.shape[0]

    # Candidate 1: Flat (mean rounded)
    flat_color = np.clip(np.rint(act.mean(axis=1)), 0, 255).astype(np.int32)  # (M, 4)
    flat_err = np.max(np.abs(act - flat_color[:, None, :]), axis=(1, 2))
    is_flat = flat_err <= max_error

    # Principal channel axis for endpoints on remaining blocks
    ptp = np.ptp(act, axis=1)  # (M, 4)
    axis = np.argmax(ptp, axis=1)  # (M,)
    m_idx = np.arange(M)
    axis_vals = act[m_idx[:, None], np.arange(16)[None, :], axis[:, None]]  # (M, 16)

    # Candidate 2: 1-bit Two-Color (low/high split around midpoint)
    mid = (axis_vals.min(axis=1) + axis_vals.max(axis=1))[:, None] / 2.0
    hi_bit = axis_vals > mid  # (M, 16) bool
    hi_count = np.maximum(1, hi_bit.sum(axis=1))[:, None]
    lo_count = np.maximum(1, (~hi_bit).sum(axis=1))[:, None]
    c_lo = np.clip(np.rint((act * (~hi_bit)[:, :, None]).sum(axis=1) / lo_count), 0, 255).astype(np.int32)
    c_hi = np.clip(np.rint((act * hi_bit[:, :, None]).sum(axis=1) / hi_count), 0, 255).astype(np.int32)
    recon_two = np.where(hi_bit[:, :, None], c_hi[:, None, :], c_lo[:, None, :])
    two_err = np.max(np.abs(act - recon_two), axis=(1, 2))
    is_two = (~is_flat) & (two_err <= max_error)

    # Candidate 3: 2-bit Four-Color linear interpolation between min/max pixel along dominant channel
    min_pos = np.argmin(axis_vals, axis=1)
    max_pos = np.argmax(axis_vals, axis=1)
    c0 = act[m_idx, min_pos]  # (M, 4)
    c3 = act[m_idx, max_pos]  # (M, 4)
    c1 = (2 * c0 + c3) // 3
    c2 = (c0 + 2 * c3) // 3
    pal4 = np.stack([c0, c1, c2, c3], axis=1)  # (M, 4, 4)
    # Choose palette entry minimizing max channel error (chebyshev) then L2
    diff4 = np.abs(act[:, :, None, :] - pal4[:, None, :, :])  # (M, 16, 4, 4)
    cheb4 = np.max(diff4, axis=3) * 1024 + np.sum(diff4 * diff4, axis=3)
    idx4 = np.argmin(cheb4, axis=2)  # (M, 16)
    recon_four = pal4[m_idx[:, None], idx4]  # (M, 16, 4)
    four_err = np.max(np.abs(act - recon_four), axis=(1, 2))
    is_four = (~is_flat) & (~is_two) & (four_err <= max_error)

    # Pack masks for 2-color and 4-color
    bit_weights16 = (1 << np.arange(16, dtype=np.uint16))[None, :]
    mask2_arr = (hi_bit.astype(np.uint16) * bit_weights16).sum(axis=1, dtype=np.uint16)

    shift32 = (2 * np.arange(16, dtype=np.uint32))[None, :]
    mask4_arr = ((idx4.astype(np.uint32) << shift32)).sum(axis=1, dtype=np.uint32)

    payload_parts: list[bytes] = [b''] * (num_tiles + 1)
    for k, tile_id in enumerate(active_indices):
        if is_flat[k]:
            modes[tile_id] = BOUNDED_BLOCK_SOLID
            col_u8 = flat_color[k].astype(np.uint8)
            rebuilt_tiles[tile_id] = col_u8
            payload_parts[k + 1] = col_u8.tobytes()
        elif is_two[k]:
            modes[tile_id] = BOUNDED_BLOCK_PALETTE2
            rebuilt_tiles[tile_id] = recon_two[k].astype(np.uint8)
            ep = np.stack([c_lo[k], c_hi[k]]).astype(np.uint8).tobytes()
            payload_parts[k + 1] = ep + int(mask2_arr[k]).to_bytes(2, 'little')
        elif is_four[k]:
            modes[tile_id] = BOUNDED_BLOCK_PALETTE4
            rebuilt_tiles[tile_id] = recon_four[k].astype(np.uint8)
            ep = np.stack([c0[k], c3[k]]).astype(np.uint8).tobytes()
            payload_parts[k + 1] = ep + int(mask4_arr[k]).to_bytes(4, 'little')
        else:
            modes[tile_id] = BOUNDED_BLOCK_LITERAL
            rebuilt_tiles[tile_id] = cur_tiles[tile_id]
            payload_parts[k + 1] = cur_tiles[tile_id].tobytes()

    payload_parts[0] = modes.tobytes()
    raw = b''.join(payload_parts)
    reconstructed = rebuilt_tiles.reshape(ty, tx, 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(h, w, 4)
    return zlib.compress(raw, 6), len(raw), np.ascontiguousarray(reconstructed)


def restore_bounded_bgra8888_frame(blob: bytes, previous: np.ndarray) -> np.ndarray:
    """Reference decoder for Mode 6 (BGRA8888 Bounded)."""
    h, w, _ = previous.shape
    ty, tx = h // 4, w // 4
    num_tiles = ty * tx
    data = zlib.decompress(blob)
    if len(data) < num_tiles:
        raise ValueError('BGRA8888 Bounded stream truncated')
    modes = np.frombuffer(data[:num_tiles], dtype=np.uint8)
    pos = num_tiles
    tiles = previous.reshape(ty, 4, tx, 4, 4).transpose(0, 2, 1, 3, 4).reshape(num_tiles, 16, 4).copy()
    for t in range(num_tiles):
        m = int(modes[t])
        if m == BOUNDED_BLOCK_SKIP:
            continue
        elif m == BOUNDED_BLOCK_SOLID:
            tiles[t] = np.frombuffer(data[pos : pos + 4], dtype=np.uint8)
            pos += 4
        elif m == BOUNDED_BLOCK_PALETTE2:
            c0 = np.frombuffer(data[pos : pos + 4], dtype=np.uint8)
            c1 = np.frombuffer(data[pos + 4 : pos + 8], dtype=np.uint8)
            mask = int.from_bytes(data[pos + 8 : pos + 10], 'little')
            pos += 10
            for p in range(16):
                tiles[t, p] = c1 if ((mask >> p) & 1) else c0
        elif m == BOUNDED_BLOCK_PALETTE4:
            c0 = np.frombuffer(data[pos : pos + 4], dtype=np.uint8).astype(np.uint16)
            c3 = np.frombuffer(data[pos + 4 : pos + 8], dtype=np.uint8).astype(np.uint16)
            mask = int.from_bytes(data[pos + 8 : pos + 12], 'little')
            pos += 12
            pal = np.stack(
                [
                    c0,
                    (2 * c0 + c3) // 3,
                    (c0 + 2 * c3) // 3,
                    c3,
                ],
                axis=0,
            ).astype(np.uint8)
            for p in range(16):
                tiles[t, p] = pal[(mask >> (2 * p)) & 3]
        elif m == BOUNDED_BLOCK_LITERAL:
            tiles[t] = np.frombuffer(data[pos : pos + 64], dtype=np.uint8).reshape(16, 4)
            pos += 64
        else:
            raise ValueError(f'invalid Bounded block mode {m}')
    if pos != len(data):
        raise ValueError('BTC8888 trailing bytes')
    return np.ascontiguousarray(tiles.reshape(ty, tx, 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(h, w, 4))
