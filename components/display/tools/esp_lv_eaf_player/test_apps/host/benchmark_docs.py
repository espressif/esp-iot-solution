# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Reproduce documentation sizes using procedural inputs and production C decoding."""
import contextlib
import hashlib
import io
import json
import math
import platform
import sys
import tempfile
import zlib
from datetime import datetime
from zoneinfo import ZoneInfo
from pathlib import Path
import numpy as np
from PIL import Image, __version__ as pillow_version
from test_host_core import Player, ROOT, to565
from encoder.eaf4 import encode, encode_565
from encoder.eaf565 import convert_to_eaf, rgb565_components


def fixtures():
    y, x = np.mgrid[:160, :160]
    rng = np.random.default_rng(20261112)
    texture = np.empty((160, 192, 4), dtype=np.uint8)
    yy, xx = np.mgrid[:160, :192]
    for c in range(3):
        texture[..., c] = np.clip(120 + 60 * np.sin(xx / (9 + c * 3)) + 50 * np.cos(yy / (7 + c * 2)) + rng.normal(0, 8, xx.shape), 0, 255)
    texture[..., 3] = 255
    cases = {'flat_ui': [], 'alpha_sprite': [], 'textured_pan': []}
    for i in range(24):
        frame = np.full((160, 160, 4), (35, 25, 15, 255), dtype=np.uint8)
        frame[12:26, 12:148] = (80, 130, 220, 255)
        frame[55:87, 8 + 4*i:32 + 4*i] = (30, 220, 250, 255)
        cases['flat_ui'].append(frame)
        sprite = np.zeros((160, 160, 4), dtype=np.uint8)
        sprite[..., :3] = (20, 30, 40)
        radius = np.sqrt((x - (30 + i*4))**2 + (y - 80)**2)
        mask = radius < 24
        sprite[mask, :3] = (80, 180, 240)
        sprite[..., 3] = np.clip((24-radius)*14, 0, 200).astype(np.uint8)
        cases['alpha_sprite'].append(sprite)
        cases['textured_pan'].append(texture[:, i:160+i].copy())
    return cases


def verify(blob, source, kind, alpha=False):
    error = 0
    squared_error = 0.0
    samples = 0
    with Player(blob) as player:
        assert player.info.count == len(source)
        for i, frame in enumerate(source):
            assert player.seek(i) == 0
            raw = player.pixels()
            if kind == 'bgra':
                assert raw == frame.tobytes(), ('BGRA mismatch', i)
            elif kind == 'bounded':
                restored = np.frombuffer(raw, np.uint8).reshape(frame.shape)
                error = max(error, int(np.abs(restored.astype(np.int16)-frame.astype(np.int16)).max()))
                assert error <= 4, ('error bound', error)
            elif kind == '565':
                expected = to565(frame).tobytes() + (frame[..., 3].tobytes() if alpha else b'')
                assert raw == expected, ('RGB565/A8 mismatch', i)
            else:
                assert len(raw) == frame.shape[0]*frame.shape[1]*2
                pixels = np.frombuffer(raw, '<u2').reshape(frame.shape[:2])
                rgb = rgb565_components(pixels).astype(np.float64)
                reference = frame[..., [2, 1, 0]].astype(np.float64)
                squared_error += float(np.square(rgb-reference).sum())
                samples += rgb.size
    result = {'max_channel_error': error if kind == 'bounded' else None}
    if kind == 'jpeg':
        result['psnr_rgb_db'] = round(10*math.log10(255**2/(squared_error/samples)), 2) if squared_error else None
    return result


def main():
    rows = []
    for name, frames in fixtures().items():
        alpha = name == 'alpha_sprite'
        for codec in ('lossless_bgra', 'bounded_bgra', 'lossless_565', 'jpeg_delta'):
            if alpha and codec == 'jpeg_delta':
                continue
            if codec == 'lossless_bgra':
                blob, report = encode(frames, period_ms=80, key_interval=12, clean_alpha=False)
                fmt, kind, bpp = 'bgra8888', 'bgra', 4
            elif codec == 'bounded_bgra':
                blob, report = encode(frames, period_ms=80, key_interval=12, codec='bounded', clean_alpha=False, max_error=4)
                fmt, kind, bpp = 'bgra8888', 'bounded', 4
            elif codec == 'lossless_565':
                blob, report = encode_565(frames, period_ms=80, key_interval=12, with_alpha=alpha)
                fmt, kind, bpp = ('rgb565a8' if alpha else 'rgb565'), '565', (3 if alpha else 2)
            else:
                with tempfile.TemporaryDirectory() as directory:
                    p = Path(directory)
                    paths = []
                    for i, frame in enumerate(frames):
                        image = p/f'{i:04d}.png'
                        Image.fromarray(frame[..., [2, 1, 0, 3]], 'RGBA').save(image)
                        paths.append(str(image))
                    with contextlib.redirect_stdout(io.StringIO()):
                        convert_to_eaf(paths, str(p/'clip.eaf'), mode='jpeg-delta', quality=82, jpeg_subsampling=0, keyframe_interval=12, mean_th=0.8, p95_th=6.0, denoise_median=False, circle_roi=False, auto_full_frame=True)
                    blob = (p/'clip.eaf').read_bytes()
                report = {}
                fmt, kind, bpp = 'rgb565', 'jpeg', 2
            verification = verify(blob, frames, kind, alpha)
            raw_bytes = 160*160*24*bpp
            row = {'fixture': name, 'codec': codec, 'pixel_format': fmt, 'file_bytes': len(blob), 'raw_bytes': raw_bytes, 'ratio': round(raw_bytes/len(blob), 3), 'sha256': hashlib.sha256(blob).hexdigest(), 'mode_counts': report.get('mode_counts_all'), 'c_decode_verified': True}
            row.update(verification)
            rows.append(row)
            print(name, codec, len(blob), row['ratio'], flush=True)
    result = {'schema_version': 1, 'date': datetime.now(ZoneInfo('Asia/Shanghai')).date().isoformat(), 'fixture_version': 1, 'generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'decoder_library_sha256': hashlib.sha256((ROOT/'build/libeaf_host.so').read_bytes()).hexdigest(), 'python': platform.python_version(), 'numpy': np.__version__, 'pillow': pillow_version, 'zlib': zlib.ZLIB_RUNTIME_VERSION, 'libjpeg': Image.core.jpeglib_version, 'machine': platform.machine(), 'width': 160, 'height': 160, 'frames': 24, 'eaf4_period_ms': 80, 'key_interval': 12, 'clean_alpha': False, 'temporal_deadzone': 0, 'bounded_max_error': 4, 'jpeg_quality': 82, 'jpeg_mean_th': 0.8, 'jpeg_p95_th': 6.0, 'jpeg_denoise_median': False, 'jpeg_circle_roi': False, 'jpeg_auto_full_frame': True, 'jpeg_subsampling': 0, 'psnr_reference': 'RGB888 source versus C RGB565 output expanded with R*8/G*4/B*8; no alpha', 'note': 'Procedural data only; ratios use raw bytes of each declared output pixel format. Not a photo benchmark, encoding-speed benchmark or device-memory measurement.', 'results': rows}
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT/'docs/benchmarks/procedural.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2)+'\n')


if __name__ == '__main__':
    main()
