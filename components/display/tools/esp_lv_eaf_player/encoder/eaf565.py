# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Video / Animation to Optimized EAF Converter for ESP32 (esp_lv_eaf_player)

Production codecs:
1. --mode lossless   : codec 0x04, bit-exact RGB565+A8 three-plane temporal delta
2. --mode bounded    : codec 0x05, error-bounded 4x4 BTC
3. --mode jpeg-delta : codec 0x06, changed-region Abbreviated JPEG

The encoder and decoder use canonical IDs without experimental bytecode aliases.
"""
import argparse
import glob
import io
import os
import struct
import subprocess
import tempfile
import zlib
from PIL import Image
import numpy as np


if __package__:
    from .names import (PUBLIC_CODEC_NAMES, BOUNDED_BLOCK_SKIP, BOUNDED_BLOCK_SOLID, BOUNDED_BLOCK_PALETTE2,
                        BOUNDED_BLOCK_PALETTE4, BOUNDED_BLOCK_LITERAL)
else:
    from names import (PUBLIC_CODEC_NAMES, BOUNDED_BLOCK_SKIP, BOUNDED_BLOCK_SOLID, BOUNDED_BLOCK_PALETTE2,
                       BOUNDED_BLOCK_PALETTE4, BOUNDED_BLOCK_LITERAL)


def extract_frames(video_path: str, out_dir: str, fps: int = 15, width: int = 240, height: int = 240):
    os.makedirs(out_dir, exist_ok=True)
    ext = os.path.splitext(video_path)[1].lower()
    if ext in ('.apng', '.png', '.webp', '.gif'):
        try:
            with Image.open(video_path) as im:
                n_frames = getattr(im, 'n_frames', 1)
                start = 1 if im.info.get('default_image', False) and n_frames > 1 else 0
                out_files = []
                for idx, f_idx in enumerate(range(start, n_frames)):
                    im.seek(f_idx)
                    frame = im.convert('RGBA')
                    if frame.size != (width, height):
                        frame = frame.resize((width, height), Image.BILINEAR)
                    fp = os.path.join(out_dir, f'frame_{idx:04d}.png')
                    frame.save(fp)
                    out_files.append(fp)
                if out_files:
                    return out_files
        except Exception:
            pass
    cmd = [
        'ffmpeg', '-y', '-v', 'error',
        '-i', video_path,
        '-vf', f'fps={fps},scale={width}:{height}',
        os.path.join(out_dir, 'frame_%04d.png')
    ]
    subprocess.run(cmd, check=True)
    return sorted(glob.glob(os.path.join(out_dir, 'frame_*.png')))


def extract_scan_only(jb: bytes) -> bytes:
    i = 2
    while i < len(jb):
        m = jb[i + 1]
        seg_len = (jb[i + 2] << 8) | jb[i + 3]
        if m == 0xDA:
            return jb[i + 2 + seg_len:-2]
        i += 2 + seg_len
    return b''


def rgb_to_565_arr(rgb):
    r = (rgb[..., 0] >> 3).astype(np.uint16)
    g = (rgb[..., 1] >> 2).astype(np.uint16)
    b = (rgb[..., 2] >> 3).astype(np.uint16)
    return (r << 11) | (g << 5) | b


def rgb565_components(colors):
    """Expand to the RGB888 domain using the decoder's quantized channels."""
    c = np.asarray(colors, dtype=np.int32)
    return np.stack(((c >> 11) * 8, ((c >> 5) & 63) * 4, (c & 31) * 8), axis=-1)


def encode_btc_block(block, error_limit=13):
    """Select flat / 2-color / 4-color using actual RGB565 reconstruction error."""
    flat = int(rgb_to_565_arr(np.mean(block, axis=0).astype(np.int32)))
    if np.max(np.abs(block - rgb565_components(flat))) <= error_limit:
        return BOUNDED_BLOCK_SOLID, struct.pack('<H', flat), b''
    # A chroma edge can have zero luma contrast. Split on the widest RGB axis.
    axis = int(np.argmax(np.ptp(block, axis=0)))
    hi = block[:, axis] > np.mean(block[:, axis])
    lo_color = block[~hi].mean(axis=0) if np.any(~hi) else block[0]
    hi_color = block[hi].mean(axis=0) if np.any(hi) else block[0]
    endpoints2 = rgb_to_565_arr(np.array([lo_color, hi_color], dtype=np.int32))
    palette2 = rgb565_components(endpoints2)
    dist2 = np.sum((block[:, None] - palette2) ** 2, axis=2)
    idx2 = dist2.argmin(axis=1)
    error2 = np.max(np.abs(block - palette2[idx2]))

    ends4 = rgb_to_565_arr(block[[np.argmin(block[:, axis]), np.argmax(block[:, axis])]])
    channels = rgb565_components(ends4) // np.array([8, 4, 8])
    c0, c3 = channels
    palette4 = np.array([c0, (2*c0+c3)//3, (c0+2*c3)//3, c3]) * [8, 4, 8]
    dist4 = np.sum((block[:, None] - palette4) ** 2, axis=2)
    idx4 = dist4.argmin(axis=1)
    if error2 <= error_limit or dist2[np.arange(16), idx2].sum() <= dist4[np.arange(16), idx4].sum():
        mask = sum(int(v) << p for p, v in enumerate(idx2))
        return BOUNDED_BLOCK_PALETTE2, struct.pack('<HH', *map(int, endpoints2)), struct.pack('<H', mask)
    mask = sum(int(v) << (2*p) for p, v in enumerate(idx4))
    return BOUNDED_BLOCK_PALETTE4, struct.pack('<HH', *map(int, ends4)), struct.pack('<I', mask)


def reconstruct_btc_block(mode, endpoints, mask):
    colors = np.frombuffer(endpoints, dtype='<u2')
    if mode == BOUNDED_BLOCK_SOLID:
        return np.full(16, colors[0], dtype=np.uint16)
    if mode == BOUNDED_BLOCK_PALETTE2:
        indices = [(int.from_bytes(mask, 'little') >> p) & 1 for p in range(16)]
        return colors[indices]
    a, b = rgb565_components(colors) // [8, 4, 8]
    channels = np.array([a, (2*a+b)//3, (a+2*b)//3, b])
    palette = ((channels[:,0] << 11) | (channels[:,1] << 5) | channels[:,2]).astype(np.uint16)
    indices = [(int.from_bytes(mask, 'little') >> (2*p)) & 3 for p in range(16)]
    return palette[indices]


def jpeg_block(array, quality, optimize=False, subsampling=0):
    buf = io.BytesIO()
    Image.fromarray(array.astype(np.uint8)).save(
        buf, format='JPEG', quality=quality, optimize=optimize, subsampling=subsampling)
    return b'\x02' + buf.getvalue()


def frame_bytes(width, height, block_height, blocks):
    return (b'_S\x0000V1.0' + bytes([24]) +
            struct.pack('<HHHH', width, height, len(blocks), block_height) +
            b''.join(struct.pack('<I', len(b)) for b in blocks) + b''.join(blocks))


def build_eaf_file(frames_bytes_list: list, out_path: str) -> int:
    total_frames = len(frames_bytes_list)
    table = bytearray()
    payload = bytearray()
    offset = 0
    for fb in frames_bytes_list:
        f_with_magic = b'\x5A\x5A' + fb
        table += struct.pack('<II', len(f_with_magic), offset)
        payload += f_with_magic
        offset += len(f_with_magic)
    table_and_data = table + payload
    chk = sum(table_and_data) & 0xFFFFFFFF
    header = b'\x89EAF' + struct.pack('<III', total_frames, chk, len(table_and_data))
    with open(out_path, 'wb') as f:
        f.write(header + table_and_data)
    return os.path.getsize(out_path)


def video_preprocess_soft_shrink(arrs, edge_weight=0.35, noise_floor=9.0):
    base = np.median(np.stack(arrs, axis=0), axis=0)
    H, W, _ = base.shape
    pad_b = np.pad(base, ((1, 1), (1, 1), (0, 0)), mode='edge')
    b_min = np.minimum.reduce([pad_b[dy:dy+H, dx:dx+W] for dy in range(3) for dx in range(3)])
    b_max = np.maximum.reduce([pad_b[dy:dy+H, dx:dx+W] for dy in range(3) for dx in range(3)])
    local_edge_span = np.max(b_max - b_min, axis=2, keepdims=True)
    T_map = noise_floor + edge_weight * local_edge_span
    clean_arrs = []
    for a in arrs:
        delta = a - base
        mag = np.abs(delta)
        shrunk = np.sign(delta) * np.minimum(mag, np.maximum(0.0, (mag - T_map) * 1.25))
        clean_arrs.append(np.clip(base + shrunk, 0, 255).astype(np.int16))
    return clean_arrs


def convert_to_eaf(
    files: list,
    out_path: str,
    mode: str = 'bounded',
    quality: int = 82,
    mean_th: float = 0.8,
    p95_th: float = 6.0,
    btc_th: int = 8,
    denoise_median: bool = False,
    denoise_th: int = 14,
    circle_roi: bool = False,
    auto_full_frame: bool = True,
    keyframe_interval: int = 60,
    jpeg_subsampling: int = 0,
    max_error: int = 8,
):
    if mode not in PUBLIC_CODEC_NAMES:
        raise ValueError(f'Unknown mode: {mode}')
    if jpeg_subsampling not in (0, 1, 2) or not 0 <= max_error <= 255:
        raise ValueError('Invalid subsampling or BTC error bound')
    if not files:
        raise ValueError('No input frames')
    if not 1 <= quality <= 95 or keyframe_interval < 0:
        raise ValueError('Invalid quality or keyframe interval')
    if min(mean_th, p95_th, btc_th, denoise_th) < 0:
        raise ValueError('Thresholds must be nonnegative')
    arrs = [np.array(Image.open(f).convert('RGB'), dtype=np.float32) for f in files]
    if any(a.shape != arrs[0].shape for a in arrs):
        raise ValueError('All frames must have the same dimensions')
    if denoise_median and len(arrs) > 2:
        arrs = video_preprocess_soft_shrink(arrs, edge_weight=0.35, noise_floor=float(denoise_th) * 0.65)
    else:
        arrs = [a.astype(np.int16) for a in arrs]

    H, W, _ = arrs[0].shape
    if not (0 < W <= 65535 and 0 < H <= 65535):
        raise ValueError('Dimensions must fit the EAF uint16 header')
    alignment = 4 if mode in ('bounded', 'lossless') else 16 if mode in ('jpeg-delta',) else 1
    if W % alignment or H % alignment:
        raise ValueError(f'{mode} requires dimensions divisible by {alignment}')
    yy, xx = np.ogrid[:H, :W]
    r_max = min(H, W) / 2.0 + 0.5
    circle_mask = ((yy - (H - 1) / 2.0) ** 2 + (xx - (W - 1) / 2.0) ** 2) <= r_max ** 2
    frames_data = []

    if mode == 'bounded':
        # Codec 0x05: uint32 raw length + raw Deflate. After inflation:
        # one mode byte per 4x4 tile, then interleaved tile payloads.
        # 0=skip, 1=flat(2B), 2=two colors(6B), 3=four colors(8B),
        # 4=verbatim RGB565(32B). Escape guarantees a per-channel error bound.
        tx = W // 4
        stripe_h = min(H // 4, 65535 // (33 * tx)) * 4
        if stripe_h == 0:
            raise ValueError('BTC width exceeds the block payload limit')
        ref = np.zeros((H, W), dtype=np.uint16)
        for i, frame in enumerate(arrs):
            keyframe = i == 0 or (keyframe_interval and i % keyframe_interval == 0)
            src = rgb_to_565_arr(frame)
            payloads = []
            for y in range(0, H, stripe_h):
                sh = min(stripe_h, H - y)
                ty = sh // 4
                blocks = src[y:y+sh].reshape(ty,4,tx,4).transpose(0,2,1,3).reshape(-1,16)
                refs = ref[y:y+sh].reshape(ty,4,tx,4).transpose(0,2,1,3).reshape(-1,16).copy()
                roi = circle_mask[y:y+sh].reshape(ty,4,tx,4).transpose(0,2,1,3).reshape(-1,16).any(axis=1)
                modes = bytearray(len(blocks))
                data = bytearray()
                for t, block in enumerate(blocks):
                    target = rgb565_components(block)
                    error = np.abs(target - rgb565_components(refs[t]))
                    if not keyframe and ((circle_roi and not roi[t]) or
                            (error.max() <= max_error and error.mean() <= mean_th)):
                        continue
                    m, ep, mask = encode_btc_block(target, min(btc_th, max_error))
                    rebuilt = reconstruct_btc_block(m, ep, mask)
                    if np.max(np.abs(target-rgb565_components(rebuilt))) > max_error:
                        m, ep, mask, rebuilt = BOUNDED_BLOCK_LITERAL, block.astype('<u2').tobytes(), b'', block
                    modes[t] = m
                    data += ep + mask
                    refs[t] = rebuilt
                ref[y:y+sh] = refs.reshape(ty,tx,4,4).transpose(0,2,1,3).reshape(sh,W)
                if not any(modes):
                    payloads.append(b'')
                    continue
                raw = bytes(modes + data)
                co = zlib.compressobj(level=9, wbits=-15)
                payloads.append(b'\x05' + struct.pack('<I', len(raw)) + co.compress(raw) + co.flush())
            frames_data.append(frame_bytes(W, H, stripe_h, payloads))

    elif mode == 'jpeg-delta':
        assert W % 16 == 0 and H % 16 == 0, 'Width and height must be multiples of 16'
        tiles_x = W // 16
        tiles_y = H // 16
        total_tiles = tiles_x * tiles_y
        mask_len = (total_tiles + 7) // 8
        ref = arrs[0].copy()

        for i, f in enumerate(arrs):
            if i == 0 or (keyframe_interval and i % keyframe_interval == 0):
                ref = f.copy()
                buf = io.BytesIO()
                Image.fromarray(f.astype(np.uint8)).save(buf, format='JPEG', quality=quality, optimize=False, subsampling=jpeg_subsampling)
                block_data = b'\x02' + buf.getvalue()
            else:
                mask_bits = bytearray(mask_len)
                nibbles_list = []
                blocks_8x8 = []
                for idx_t in range(total_tiles):
                    ty = (idx_t // tiles_x) * 16
                    tx = (idx_t % tiles_x) * 16
                    if circle_roi and not np.any(circle_mask[ty:ty+16, tx:tx+16]):
                        continue
                    sub = f[ty:ty+16, tx:tx+16]
                    ref_sub = ref[ty:ty+16, tx:tx+16]
                    q_mask = 0
                    q_blocks = []
                    for q in range(4):
                        qy, qx = (q >> 1) * 8, (q & 1) * 8
                        q_sub = sub[qy:qy+8, qx:qx+8]
                        q_ref = ref_sub[qy:qy+8, qx:qx+8]
                        diff = np.abs(q_sub - q_ref)
                        if np.mean(diff) > mean_th or np.max(diff) > p95_th:
                            q_mask |= (1 << q)
                            q_blocks.append((qy, qx, q_sub))
                    if q_mask == 0:
                        continue
                    mask_bits[idx_t >> 3] |= (1 << (idx_t & 7))
                    nibbles_list.append(q_mask)
                    for qy, qx, q_sub in q_blocks:
                        blocks_8x8.append(q_sub)
                        ref[ty+qy:ty+qy+8, tx+qx:tx+qx+8] = q_sub.copy()
                if len(nibbles_list) == 0:
                    block_data = b'\x06' + bytes(mask_bits)
                else:
                    nib_bytes = bytearray((len(nibbles_list) + 1) // 2)
                    for c_idx, nib in enumerate(nibbles_list):
                        if c_idx & 1:
                            nib_bytes[c_idx >> 1] |= ((nib & 0x0F) << 4)
                        else:
                            nib_bytes[c_idx >> 1] |= (nib & 0x0F)
                    mcus = []
                    for k in range(0, len(blocks_8x8), 4):
                        chunk = blocks_8x8[k:k+4]
                        while len(chunk) < 4:
                            chunk.append(chunk[-1])
                        quad = np.zeros((16, 16, 3), dtype=np.uint8)
                        quad[0:8, 0:8] = chunk[0].astype(np.uint8)
                        quad[0:8, 8:16] = chunk[1].astype(np.uint8)
                        quad[8:16, 0:8] = chunk[2].astype(np.uint8)
                        quad[8:16, 8:16] = chunk[3].astype(np.uint8)
                        mcus.append(quad)
                    # Pad MCU count so strip width (16 * N) aligns with S31/P4 HW JPEG 2D-DMA block width (hb=48 for 4:4:4, 32 for 4:2:2/4:2:0)
                    mcu_align = 3 if jpeg_subsampling == 0 else 2
                    while len(mcus) % mcu_align != 0:
                        mcus.append(mcus[-1])
                    atlas = np.hstack(mcus)
                    buf = io.BytesIO()
                    Image.fromarray(atlas).save(buf, format='JPEG', quality=quality, optimize=False, subsampling=jpeg_subsampling)
                    scan_bytes = extract_scan_only(buf.getvalue())
                    block_data = b'\x06' + bytes(mask_bits) + bytes(nib_bytes) + scan_bytes
            if i > 0 and auto_full_frame:
                full = jpeg_block(f, quality, subsampling=jpeg_subsampling)
                if len(full) < len(block_data):
                    block_data = full
                    ref = f.copy()
            hdr = bytearray(b'_S\x0000V1.0')
            hdr.append(24)
            hdr += struct.pack('<HHHH', W, H, 1, H)
            hdr += struct.pack('<I', len(block_data))
            frames_data.append(bytes(hdr) + block_data)
    elif mode == 'lossless':
        rgba_arrs = [np.array(Image.open(f).convert('RGBA'), dtype=np.int32) for f in files]
        frames_rgba = np.stack(rgba_arrs, axis=0)
        N = len(frames_rgba)
        c565 = rgb_to_565_arr(frames_rgba[..., :3]).astype(np.uint16)
        a8 = frames_rgba[..., 3].astype(np.uint8)
        if circle_roi:
            c565[:, ~circle_mask] = 0
            a8[:, ~circle_mask] = 0

        c_blocks = c565.reshape(N, H // 4, 4, W // 4, 4).transpose(0, 1, 3, 2, 4).reshape(N, -1, 16)
        a_blocks = a8.reshape(N, H // 4, 4, W // 4, 4).transpose(0, 1, 3, 2, 4).reshape(N, -1, 16)
        num_tiles = c_blocks.shape[1]
        mask_len = (num_tiles + 7) // 8

        prev_c = np.zeros_like(c_blocks[0])
        prev_a = np.zeros_like(a_blocks[0])

        for f_idx in range(N):
            cur_c = c_blocks[f_idx]
            cur_a = a_blocks[f_idx]
            is_key = (f_idx == 0) or (keyframe_interval and (f_idx % keyframe_interval == 0))
            if is_key:
                flags = 0
                chg = (np.max(cur_c, axis=1) != 0) | (np.max(cur_a, axis=1) != 0)
                if f_idx > 0:
                    chg = np.ones(num_tiles, dtype=bool)
                dc = cur_c[chg]
                da = cur_a[chg]
            else:
                flags = 1
                skip = np.all(cur_c == prev_c, axis=1) & np.all(cur_a == prev_a, axis=1)
                chg = ~skip
                dc = cur_c[chg] ^ prev_c[chg]
                da = ((cur_a[chg].astype(np.int16) - prev_a[chg].astype(np.int16)) & 0xFF).astype(np.uint8)

            prev_c = cur_c.copy()
            prev_a = cur_a.copy()

            n_changed = int(np.sum(chg))
            if n_changed == 0 and not is_key:
                frames_data.append(frame_bytes(W, H, H, [b'']))
                continue

            mask_bits = np.packbits(chg.astype(np.uint8), bitorder='little')
            assert len(mask_bits) == mask_len
            c_low = (dc & 0xFF).astype(np.uint8).tobytes()
            c_high = (dc >> 8).astype(np.uint8).tobytes()
            a_bytes = da.astype(np.uint8).tobytes()
            raw = mask_bits.tobytes() + c_low + c_high + a_bytes
            co = zlib.compressobj(level=9, wbits=-15)
            comp = co.compress(raw) + co.flush()
            block = b'\x04' + struct.pack('<BIH', flags, len(raw), n_changed) + comp
            frames_data.append(frame_bytes(W, H, H, [block]))
    else:
        raise ValueError(f'Unknown mode: {mode}')

    return build_eaf_file(frames_data, out_path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Convert video to optimized EAF for ESP32')
    parser.add_argument('input', help='Input video file (.avi, .mp4, .gif, .apng)')
    parser.add_argument('-o', '--output', required=True, help='Output .eaf file path')
    parser.add_argument(
        '--mode',
        choices=PUBLIC_CODEC_NAMES,
        default='bounded',
        metavar='{lossless,bounded,jpeg-delta}',
        help='public modes: lossless (0x04), bounded (0x05), jpeg-delta (0x06)',
    )
    parser.add_argument('--fps', type=int, default=15)
    parser.add_argument('--width', type=int, default=240)
    parser.add_argument('--height', type=int, default=240)
    parser.add_argument('--quality', type=int, default=82)
    parser.add_argument('--denoise-median', action='store_true')
    parser.add_argument('--circle-roi', action='store_true', help='Skip tiles outside a circular display (default: full rectangle)')
    parser.add_argument('--no-auto-full-frame', action='store_true', help='Disable size-based full JPEG fallback')
    parser.add_argument('--keyframe-interval', type=int, default=60, help='Full refresh every N frames; 0 disables periodic refresh')
    parser.add_argument('--jpeg-subsampling', type=int, choices=[0, 1, 2], default=0, help='0=4:4:4 (default), 1=4:2:2, 2=4:2:0')
    parser.add_argument('--max-error', dest='max_error', type=int, default=8,
                        help='bounded maximum RGB channel error versus RGB565 source')
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmpdir:
        frame_files = extract_frames(args.input, tmpdir, fps=args.fps, width=args.width, height=args.height)
        sz = convert_to_eaf(
            frame_files,
            args.output,
            mode=args.mode,
            quality=args.quality,
            denoise_median=args.denoise_median,
            circle_roi=args.circle_roi,
            auto_full_frame=not args.no_auto_full_frame,
            keyframe_interval=args.keyframe_interval,
            jpeg_subsampling=args.jpeg_subsampling,
            max_error=args.max_error,
        )
        print(f'Generated {args.output}: {sz / 1024:.1f} KB ({len(frame_files)} frames, mode={args.mode})')
