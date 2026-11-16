# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Create a device conformance pack from procedural source pixels, without Host C."""
import argparse
import io
import json
import struct
import sys
import tempfile
import zlib
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from encoder.eaf4 import encode, encode_565, decode, _pack, _encode_565a8_frame_candidates
from encoder.eaf565 import convert_to_eaf
from encoder.eaf565_lossless_rgba import encode_scene, Deflater
from host.jpeg_fixtures import split_jpeg_header_and_scan, rebuild_abbreviated_jpeg

RECORD = struct.Struct('<48sIIIIHHHBBBBBBBB2x')
NONE = 255


def to565(f):
    v = f.astype(np.uint16)
    return (((v[..., 2] >> 3) << 11) | ((v[..., 1] >> 2) << 5) | (v[..., 0] >> 3)).astype('<u2')


def classic_frame(blocks, depth=24, palette=b'', width=16, height=8, block_height=4):
    return (b'_S\x0000V1.0' + bytes([depth]) + struct.pack('<HHHH', width, height, len(blocks), block_height)
            + b''.join(struct.pack('<I', len(b)) for b in blocks) + palette + b''.join(blocks))


def classic_file(frames, magic=b'EAF'):
    blobs = [b'ZZ' + f for f in frames]
    table = bytearray()
    offset = 0
    for b in blobs:
        table += struct.pack('<II', len(b), offset)
        offset += len(b)
    raw = bytes(table) + b''.join(blobs)
    return b'\x89' + magic + struct.pack('<III', len(frames), sum(raw) & 0xffffffff, len(raw)) + raw


def rle(data):
    return b''.join(bytes([1, b]) for b in data)


def huff(data):
    dictionary = b'\0' + b''.join(bytes([v, 8, v]) for v in sorted(set(data)))
    return struct.pack('<H', len(dictionary)) + dictionary + data


def compress(mode, raw):
    return bytes([mode]) + (rle(raw) if mode == 0 else huff(rle(raw)) if mode == 1 else huff(raw))


def source_frames():
    first = np.random.default_rng(20261112).integers(0, 256, (8, 16, 4), dtype=np.uint8)
    first[0, :4, 3] = [0, 1, 128, 255]
    second = first.copy()
    second[4:8, 4:8] = (second[4:8, 4:8].astype(int) + 1) % 256
    key = ((second.astype(int) + [1, 127, 128, 255]) % 256).astype(np.uint8)
    small = key.copy()
    small[4:8, 4:8] = (small[4:8, 4:8].astype(int) + [8, 4, 8, 1]) % 256
    return [first, second, second.copy(), key, small, first.copy()]


def generate():
    cases = []

    def add(name, blob, refs, layout=0, output=0, error=0, sw_from=NONE,
            reject_from=NONE, codec=4, frame_mode=NONE, width=16, height=8):
        size = width * height * (4, 2, 3)[layout]
        assert refs and all(len(ref) == size for ref in refs), name
        assert len(name.encode()) < 48, name
        cases.append(dict(name=name, blob=blob, refs=refs, layout=layout, output=output,
                          error=error, sw_from=sw_from, reject_from=reject_from,
                          codec=codec, mode=frame_mode, width=width, height=height))

    source = source_frames()
    for mode in (0, 1, 2, 3, 4, 5, 6, 10, 11):
        blob, info = encode(source, forced_mode=mode, key_interval=3, max_error=8, clean_alpha=False)
        assert info['mode_counts_all'][mode], (mode, info)
        add(f'EAF4_BGRA_mode{mode}', blob, [f.tobytes() for f in source], error=8 if mode == 6 else 0, frame_mode=mode, codec=5 if mode == 6 else 4)
    clean = [f.copy() for f in source]
    for f in clean:
        f[f[..., 3] == 0, :3] = 0
    blob, _ = encode(source)
    add('EAF4_BGRA_default_cleanup', blob, [f.tobytes() for f in clean])
    colors, alphas = [to565(f) for f in source], [f[..., 3] for f in source]
    for alpha in (False, True):
        for mode in (7, 8):
            records, payloads, refs = [], [], []
            for i, (color, a) in enumerate(zip(colors, alphas)):
                raw = color.tobytes() + (a.tobytes() if alpha else b'')
                refs.append(raw)
                if i % 3 == 0:
                    m, blob, size = 0, zlib.compress(raw), len(raw)
                else:
                    candidates = _encode_565a8_frame_candidates(color, a, colors[i-1], alphas[i-1], with_alpha=alpha)
                    m, blob, size = next((c for c in candidates if c[0] == mode), candidates[0])
                records.append((len(blob), size, zlib.crc32(raw), m, zlib.crc32(blob)))
                payloads.append(blob)
            assert mode in [record[3] for record in records]
            blob = _pack(records, payloads, 16, 8, 67, pixel_format=2 if alpha else 1, codec_id=4)
            add(f'EAF4_{"RGB565A8" if alpha else "RGB565"}_mode{mode}', blob, refs, layout=2 if alpha else 1, frame_mode=mode)
    for error in (0, 4, 8):
        blob, _ = encode(source, codec='bounded', max_error=error, clean_alpha=False, key_interval=3)
        add(f'EAF4_BGRA_bounded{error}', blob, [f.tobytes() for f in source], error=error, codec=5, frame_mode=6)
        blob, info = encode_565(source, codec='bounded', max_error=error, key_interval=3)
        assert info['mode_counts_all'][9]
        add(f'EAF4_RGB565_bounded{error}', blob, [c.tobytes() for c in colors], layout=1, error=error, codec=5, frame_mode=9)

    for depth in (4, 8):
        palette = np.array([[i*7 % 256, i*13 % 256, i*23 % 256, i*17 % 256] for i in range(1 << depth)], dtype=np.uint8)
        indices = [(np.arange(16*7) + i).reshape(7, 16) % (1 << depth) for i in (0, 1)]
        for mode in (0, 1, 3):
            frames = []
            for idx in indices:
                blocks = []
                for stripe in (idx[:4].ravel(), idx[4:].ravel()):
                    raw = stripe.astype(np.uint8).tobytes() if depth == 8 else bytes((int(stripe[i]) << 4) | int(stripe[i+1]) for i in range(0, len(stripe), 2))
                    blocks.append(compress(mode, raw))
                frames.append(classic_frame(blocks, depth, palette.tobytes(), height=7))
            blob = classic_file(frames)
            rgba = [palette[idx] for idx in indices]
            refs = [to565(f).tobytes() + f[..., 3].tobytes() for f in rgba]
            add(f'EAF_palette{depth}_codec{mode}_565A8', blob, refs, layout=2, codec=mode, height=7)
            if depth == 8:
                add(f'EAF_palette8_codec{mode}_BGRA', blob, [f.tobytes() for f in rgba], output=2, codec=mode, height=7)
    for mode in (0, 1, 3):
        rgb = [np.full((8, 16, 4), (20+i*30, 40, 80, 255), np.uint8) for i in (0, 1)]
        blobs = [classic_frame([compress(mode, f[:4, :, :3].tobytes()), compress(mode, f[4:, :, :3].tobytes())]) for f in rgb]
        for output in (0, 2):
            add(f'EAF_BGR24_codec{mode}_{output}', classic_file(blobs), [to565(f).tobytes() if output == 0 else f.tobytes() for f in rgb], layout=1 if output == 0 else 0, output=output, codec=mode)
        if mode == 0:
            add('AAF_BGR24_RLE', classic_file(blobs, b'AAF'), [to565(f).tobytes() for f in rgb], layout=1, codec=mode)
    blob = encode_scene(colors, alphas, 4, Deflater(True, 1))
    add('EAF_RGB565A8_lossless', blob, [c.tobytes()+a.tobytes() for c, a in zip(colors, alphas)], layout=2)
    with tempfile.TemporaryDirectory() as temp:
        paths = []
        for i, f in enumerate(source):
            path = Path(temp)/f'{i}.png';Image.fromarray(f[..., [2, 1, 0, 3]]).save(path);paths.append(str(path))
        for error in (4, 8):
            path = Path(temp)/'bounded.eaf'
            convert_to_eaf(paths, str(path), mode='bounded', max_error=error, keyframe_interval=3)
            add(f'EAF_RGB565_bounded{error}', path.read_bytes(), [c.tobytes() for c in colors], layout=1, error=error, codec=5)

    def jpeg(w, color, subsampling):
        buf = io.BytesIO();Image.new('RGB', (w, 16), color).save(buf, format='JPEG', quality=90, subsampling=subsampling)
        return buf.getvalue()

    def jpeg_ref(data):
        bgra = np.array(Image.open(io.BytesIO(data)).convert('RGBA'))[..., [2, 1, 0, 3]]
        return to565(bgra).tobytes()

    for subsampling, width in ((0, 64), (1, 64), (2, 48), (2, 32), (2, 16)):
        jpgs = [jpeg(width, c, subsampling) for c in ((200, 30, 40), (20, 180, 60))]
        blob = classic_file([classic_frame([b'\x02'+j], width=width, height=16, block_height=16) for j in jpgs])
        add(f'EAF_JPEG_sub{subsampling}_w{width}', blob, [jpeg_ref(j) for j in jpgs], layout=1, error=16, codec=2, width=width, height=16, sw_from=0 if width == 16 else NONE)
    for subsampling in (0, 2):
        base = jpeg(32, (200, 30, 40), subsampling)
        header, sof, _ = split_jpeg_header_and_scan(base)
        atlas_width = 48 if subsampling == 0 else 32
        atlas = jpeg(atlas_width, (20, 180, 60), subsampling)
        _, _, scan = split_jpeg_header_and_scan(atlas)
        decoded_atlas = np.frombuffer(jpeg_ref(rebuild_abbreviated_jpeg(header, sof, atlas_width, 16, scan)), '<u2').reshape(16, atlas_width)
        first = np.frombuffer(jpeg_ref(base), '<u2').reshape(16, 32)
        second = first.copy();second[:, :16] = decoded_atlas[:, :16];second[:8, 16:24] = decoded_atlas[:8, :8]
        frame0 = classic_frame([b'\x02'+base], width=32, height=16, block_height=16)
        frame1 = classic_frame([b'\x06\x03\x1f'+scan], width=32, height=16, block_height=16)
        add(f'EAF_JPEG_DELTA_sub{subsampling}', classic_file([frame0, frame1, frame1]), [first.tobytes(), second.tobytes(), second.tobytes()], layout=1, error=16, codec=6, width=32, height=16, sw_from=1 if subsampling == 0 else NONE)
    # General malformed codec and CRC checks, with no reserved aliases.
    blob = classic_file([classic_frame([b'\xff'+bytes(8)]*2)])
    add('reject_unknown_codec', blob, [bytes(16*8*2)], layout=1, reject_from=0, codec=255)
    valid, _ = encode(source, forced_mode=4, clean_alpha=False)
    broken = bytearray(valid);broken[40] ^= 1
    add('reject_EAF4_header_crc', bytes(broken), [f.tobytes() for f in source], reject_from=0)
    broken = bytearray(valid);offset = struct.unpack_from('<I', valid, 64+24)[0];broken[offset] ^= 1
    add('reject_EAF4_payload_crc', bytes(broken), [f.tobytes() for f in source], reject_from=1)
    assert set(range(12)) <= {c['mode'] for c in cases}
    assert set(range(7)) <= {c['codec'] for c in cases}
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    cases = generate()
    out = bytearray(16 + len(cases)*RECORD.size)
    for i, c in enumerate(cases):
        while len(out) % 4:out.append(0)
        blob_offset = len(out);out += c['blob']
        while len(out) % 4:out.append(0)
        ref_offset = len(out);out += b''.join(c['refs'])
        RECORD.pack_into(out, 16+i*RECORD.size, c['name'].encode(), blob_offset, len(c['blob']), ref_offset,
                         len(c['refs'][0]), len(c['refs']), c['width'], c['height'], c['layout'], c['output'],
                         c['error'], c['sw_from'], c['reject_from'], c['codec'], c['mode'], 0)
    struct.pack_into('<4sIII', out, 0, b'ETST', 1, len(cases), len(out))
    args.output.parent.mkdir(parents=True, exist_ok=True);args.output.write_bytes(out)
    report = [{k: v for k, v in c.items() if k not in ('blob', 'refs')} | {'frames':len(c['refs'])} for c in cases]
    args.output.with_suffix('.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f'All-mode fixture pack: {len(cases)} cases, {len(out)} bytes')


if __name__ == '__main__':
    main()
