# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Render shared codec/size/memory tables in both READMEs; --check detects drift."""
import argparse
import ast
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
START = '<!-- BEGIN GENERATED CODEC GUIDE -->'
END = '<!-- END GENERATED CODEC GUIDE -->'


def validate(metadata):
    values = {}
    for node in ast.parse((ROOT/'encoder/names.py').read_text()).body:
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    try:
                        values[target.id] = ast.literal_eval(node.value)
                    except (ValueError, TypeError):
                        pass
    assert tuple(c['name'] for c in metadata['codecs']) == values['PUBLIC_CODEC_NAMES']
    for c in metadata['codecs']:
        name = 'CODEC_' + c['name'].upper().replace('-', '_')
        assert c['id'] == values[name], name
    for c in metadata['compatibility_codecs']:
        assert c['id'] == values['CODEC_' + c['name']], c['name']
    assert sorted(c['id'] for c in metadata['compatibility_codecs'] + metadata['codecs']) == list(range(len(metadata['compatibility_codecs']) + len(metadata['codecs'])))


def detailed_section(data, bench, language):
    zh = language == 'zh'
    lines = [START, '## 支持的编码模式与编码能力' if zh else '## Codec support and encoding capabilities', '',
             '“仅解码”表示组件可播放这类资源，但内置转换工具不生成这类文件。' if zh else '“Decode only” means the component can play these assets, but the bundled converter does not generate them.', '',
             '| ID | 编码模式 | CLI `--codec` | 文件格式 | 像素格式 | 适用场景 |' if zh else '| ID | Codec | CLI `--codec` | File format | Pixel formats | Recommended use |',
             '|---|---|---|---|---|---|']
    for c in data['compatibility_codecs'] + data['codecs']:
        encodable = 'alignment' in c
        name = c['name'].upper().replace('-', '_')
        cli = f"`{c['name']}`" if encodable else ('仅解码' if zh else 'Decode only')
        formats = ', '.join(fmt.upper() for fmt in c['formats'])
        lines.append(f"| `0x{c['id']:02x}` | {name} | {cli} | {c['container']} | {formats} | {c['use_case'][language]} |")
    lines += ['',
              '转换工具会在图片右侧和底部补像素，使 LOSSLESS／BOUNDED 的宽高为 4 的倍数，JPEG_DELTA 为 16 的倍数；不会缩放原图。' if zh else 'The converter pads the right and bottom edges so dimensions are multiples of 4 for LOSSLESS/BOUNDED or 16 for JPEG_DELTA. It does not resize the original image.', '',
              '支持 PNG 序列目录、GIF、APNG、动画 WebP 和视频输入；完整参数与输入要求见[编码指南](docs/ENCODING.md)。' if zh else 'Inputs include PNG sequence directories, GIF, APNG, animated WebP and video. See the [encoding guide](docs/ENCODING.md) for options and input requirements.', '',
              'RGB565 的“无损”以量化后的 16-bit 像素为准；RGB565A8 额外精确保留 A8。`jpeg-delta` 不保存 Alpha。BOUNDED 的 BGRA8888 对包括 A8 在内的通道施加误差上限，不保证 Alpha 逐字节无损。' if zh else 'RGB565 losslessness is relative to quantized 16-bit pixels; RGB565A8 also preserves A8 exactly. JPEG Delta does not preserve alpha. BOUNDED BGRA8888 bounds channel error including A8; alpha is not guaranteed bit-exact.', '',
              'JPEG 解码优先使用硬件，不支持或失败时默认回退至 `esp_new_jpeg`；关闭软件 JPEG 后无此回退。' if zh else 'JPEG decoding tries hardware first, falling back to `esp_new_jpeg` for unsupported input or failures. Disabling software JPEG removes this fallback.', '',
              '**EAF4 输出限制：**宽高均为 4 的倍数且不超过 1024；1..10000 帧；帧周期 10..10000 ms；文件不超过 32 MiB。' if zh else '**EAF4 output limits:** dimensions divisible by 4 and at most 1024; 1..10000 frames; 10..10000 ms periods; files at most 32 MiB.', '',
              '### 可复现的通用素材数据' if zh else '### Reproducible procedural examples', '',
              f"{bench['width']}×{bench['height']}，{bench['frames']} 帧；关键帧间隔 {bench['key_interval']}；无透明清零、无 temporal deadzone。每格为“文件 KiB / 原始像素字节÷文件字节”；原始字节按该列的输出像素格式计算，不能把跨格式的倍率当作纯压缩算法收益。" if zh else f"{bench['width']}×{bench['height']}, {bench['frames']} frames; key interval {bench['key_interval']}; no transparent cleanup or temporal deadzone. Cells show file KiB / raw pixel bytes divided by file bytes. The raw baseline follows each column's output format; ratios across formats include the pixel-format difference.", '',
              '| 程序生成素材 | BGRA8888 无损 | BGRA8888 Bounded ≤4 | RGB565／A8 无损 | JPEG Delta RGB565 |' if zh else '| Procedural input | Lossless BGRA8888 | Bounded BGRA8888 ≤4 | Lossless RGB565/A8 | JPEG Delta RGB565 |',
              '|---|---|---|---|---|']
    names = {'flat_ui': ('Flat UI', '扁平 UI'), 'alpha_sprite': ('Translucent sprite', '半透明图形'), 'textured_pan': ('Noisy texture pan', '带噪纹理平移')}
    for name in names:
        cells = [names[name][1 if zh else 0]]
        for codec in ('lossless_bgra', 'bounded_bgra', 'lossless_565', 'jpeg_delta'):
            row = next((r for r in bench['results'] if r['fixture'] == name and r['codec'] == codec), None)
            cells.append(f"{row['file_bytes']/1024:.1f} / {row['ratio']:.1f}×" if row else '— (Alpha)')
        lines.append('| ' + ' | '.join(cells) + ' |')
    lines += ['',
              '半透明行的 565 列使用 RGB565A8，其余使用 RGB565。JPEG quality=82、4:4:4。所有输出均经生产 C 解码；无损检查像素一致，Bounded 检查最大通道误差 ≤4，JPEG Delta 检查可解码性。' if zh else 'The translucent row uses RGB565A8; the other 565 rows use RGB565. JPEG quality=82, 4:4:4. Every output is decoded by production C: exact pixels for lossless, maximum channel error ≤4 for Bounded, and decodability for JPEG Delta.', '',
              ('JPEG Delta 的 RGB PSNR（按 R×8/G×4/B×8 展开 RGB565，与源 RGB888 比较）：' if zh else 'JPEG Delta RGB PSNR (RGB565 expanded with R×8/G×4/B×8 versus source RGB888): ') + ', '.join(f"{names[row['fixture']][1 if zh else 0]} {row['psnr_rgb_db']:.2f} dB" for row in bench['results'] if row['codec'] == 'jpeg_delta') + '.', '',
              '这些是合成输入，不是照片、客户素材或性能承诺。Bounded 可能大于无损；不要用这张表推导任意图片的压缩率。脚本、依赖版本、模式统计及文件哈希见[原始数据](docs/benchmarks/procedural.json)和[复现方法](docs/ENCODING.md#reproduce-the-tables)。' if zh else 'These are synthetic inputs, not photographs, customer assets or performance guarantees. Bounded can be larger than lossless. Do not generalize these ratios to arbitrary images. See the [raw data](docs/benchmarks/procedural.json) and [reproduction steps](docs/ENCODING.md#reproduce-the-tables) for versions, modes and hashes.', '',
              '### 解码内存预算' if zh else '### Decoder memory budget', '',
              '下表只计算一份帧缓冲，单位 KiB（1024 字节），不是播放器峰值或 PSRAM 实测：' if zh else 'This table is one frame buffer in KiB (1024 bytes), not player peak memory or measured PSRAM use:', '',
              '| 尺寸 | BGRA8888：4 B/px | RGB565：2 B/px | RGB565A8：3 B/px |' if zh else '| Dimensions | BGRA8888: 4 B/px | RGB565: 2 B/px | RGB565A8: 3 B/px |', '|---|---:|---:|---:|']
    for w, h in ((240, 240), (320, 240), (480, 480), (800, 480), (1024, 1024)):
        lines.append(f'| {w}×{h} | {w*h*4/1024:g} | {w*h*2/1024:g} | {w*h*3/1024:g} |')
    lines += ['', '```text', 'EAF4 base ≈ frame_bytes + max_compressed + max_delta_scratch', '            + 24 × frame_count + 8 KiB CRC table + small state', 'optional  = compressed payload cache + one seek snapshot frame', '```', '',
              '文件模式可能另缓存不超过 4 MiB 的压缩载荷；随机 seek 可能再分配一帧快照。还要单独计算调用方输入、LVGL/LCD 缓冲、DMA 对齐及分配器开销。传统 EAF 文件入口会保留整份文件，JPEG 另有后端工作区；不能直接套用 EAF4 公式。缓存可通过 `esp_eaf_player_release_cache()` 释放。' if zh else 'File mode may cache a compressed payload of at most 4 MiB; a random seek may allocate another frame snapshot. Account separately for caller-owned input, LVGL/LCD buffers, DMA alignment and allocator overhead. Classic EAF file input retains the whole file and JPEG has backend workspace; this EAF4 formula does not cover those paths. Use `esp_eaf_player_release_cache()` to release optional caches.', '',
              '能力表由 `docs/codec_capabilities.json` 生成；运行 `python3 tools/render_codec_docs.py --check` 检查中英文文档是否同步。' if zh else 'Capability tables come from `docs/codec_capabilities.json`; run `python3 tools/render_codec_docs.py --check` to check both README versions.', END]
    return '\n'.join(lines)


def section(data, bench, language):
    # Keep the overview focused; reproducibility and memory details live in docs.
    full = detailed_section(data, bench, language)
    heading = '### 可复现的通用素材数据' if language == 'zh' else '### Reproducible procedural examples'
    overview = full[:full.index(heading)].rstrip()
    link = '压缩对比、内存预算与复现方法见[评测说明](docs/MEASUREMENTS.md)。' if language == 'zh' else 'See [measurements](docs/MEASUREMENTS.md) for size comparisons, memory budgets and reproduction steps.'
    return overview + '\n\n' + link + '\n' + END


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    data = json.loads((ROOT/'docs/codec_capabilities.json').read_text())
    bench = json.loads((ROOT/'docs/benchmarks/procedural.json').read_text())
    validate(data)
    stale = []
    full = detailed_section(data, bench, 'en')
    start = full.index('### Reproducible procedural examples')
    measurements = '# Size measurements and memory\n\n' + full[start:full.index(END)].replace('docs/', '').replace('### ', '## ')
    measurements_path = ROOT/'docs/MEASUREMENTS.md'
    if not measurements_path.exists() or measurements_path.read_text() != measurements:
        stale.append('docs/MEASUREMENTS.md')
        if not args.check:
            measurements_path.write_text(measurements)
    for name, lang, anchor in [('README.md', 'en', '## Tests'), ('README_CN.md', 'zh', '## 许可证')]:
        p = ROOT/name
        text = p.read_text()
        generated = section(data, bench, lang)
        if START in text:
            begin = text.index(START)
            end = text.index(END, begin)+len(END)
            desired = text[:begin]+generated+text[end:]
        else:
            if anchor not in text:
                parser.error('missing insertion anchor in '+name)
            desired = text.replace(anchor, generated+'\n\n'+anchor, 1)
        if desired != text:
            stale.append(name)
            if not args.check:
                p.write_text(desired)
    if args.check and stale:
        parser.error('stale generated tables: '+', '.join(stale))


if __name__ == '__main__':
    main()
