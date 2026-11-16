# Size measurements and memory

## Reproducible procedural examples

160×160, 24 frames; key interval 12; no transparent cleanup or temporal deadzone. Cells show file KiB / raw pixel bytes divided by file bytes. The raw baseline follows each column's output format; ratios across formats include the pixel-format difference.

| Procedural input | Lossless BGRA8888 | Bounded BGRA8888 ≤4 | Lossless RGB565/A8 | JPEG Delta RGB565 |
|---|---|---|---|---|
| Flat UI | 1.9 / 1278.7× | 2.0 / 1205.3× | 1.8 / 660.3× | 14.6 / 82.4× |
| Translucent sprite | 31.5 / 76.1× | 43.1 / 55.7× | 30.0 / 59.9× | — (Alpha) |
| Noisy texture pan | 1605.8 / 1.5× | 1998.8 / 1.2× | 903.6 / 1.3× | 196.2 / 6.1× |

The translucent row uses RGB565A8; the other 565 rows use RGB565. JPEG quality=82, 4:4:4. Every output is decoded by production C: exact pixels for lossless, maximum channel error ≤4 for Bounded, and decodability for JPEG Delta.

JPEG Delta RGB PSNR (RGB565 expanded with R×8/G×4/B×8 versus source RGB888): Flat UI 35.01 dB, Noisy texture pan 29.76 dB.

These are synthetic inputs, not photographs, customer assets or performance guarantees. Bounded can be larger than lossless. Do not generalize these ratios to arbitrary images. See the [raw data](benchmarks/procedural.json) and [reproduction steps](ENCODING.md#reproduce-the-tables) for versions, modes and hashes.

## Decoder memory budget

This table is one frame buffer in KiB (1024 bytes), not player peak memory or measured PSRAM use:

| Dimensions | BGRA8888: 4 B/px | RGB565: 2 B/px | RGB565A8: 3 B/px |
|---|---:|---:|---:|
| 240×240 | 225 | 112.5 | 168.75 |
| 320×240 | 300 | 150 | 225 |
| 480×480 | 900 | 450 | 675 |
| 800×480 | 1500 | 750 | 1125 |
| 1024×1024 | 4096 | 2048 | 3072 |

```text
EAF4 base ≈ frame_bytes + max_compressed + max_delta_scratch
            + 24 × frame_count + 8 KiB CRC table + small state
optional  = compressed payload cache + one seek snapshot frame
```

File mode may cache a compressed payload of at most 4 MiB; a random seek may allocate another frame snapshot. Account separately for caller-owned input, LVGL/LCD buffers, DMA alignment and allocator overhead. Classic EAF file input retains the whole file and JPEG has backend workspace; this EAF4 formula does not cover those paths. Use `esp_eaf_player_release_cache()` to release optional caches.

Capability tables come from `codec_capabilities.json`; run `python3 tools/render_codec_docs.py --check` to check both README versions.
