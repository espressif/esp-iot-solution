# Encoding, validation and memory

This guide describes the bundled tools and production decoder. It does not
announce new capabilities for the hosted converter. Machine-readable codec
metadata is in [codec_capabilities.json](codec_capabilities.json); generated
README tables and [procedural results](benchmarks/procedural.json) share it.

## Input and output contracts

The CLI accepts a list of images, a naturally sorted directory of lowercase
`*.png`, GIF, `.apng`, animated WebP, or MP4/AVI/MOV/MKV through FFmpeg. A single
image is also accepted. Animated input iteration is selected by the `.gif`,
`.apng` or `.webp` suffix: an APNG named `.png` currently takes the single-image
path. Use `.apng` to retain all frames.

Decoded input is normalized to straight-alpha BGRA8888. `--resize WIDTHxHEIGHT`
resamples before encoding; alignment adds transparent pixels at the right and
bottom (opaque codecs cannot retain that transparent padding). Encoded frames
must have equal dimensions. For EAF4, `--period` sets a uniform period (default
67 ms); source animation frame durations are not preserved individually. Classic
EAF playback timing is application-controlled; do not infer a frame period from
that container. For JPEG Delta, alpha cannot be stored, so use opaque inputs.

The default Lossless and Bounded outputs use EAF4. JPEG Delta uses classic
EAF. The CLI pads dimensions to 4 pixels, or 16 for JPEG Delta. Classic EAF
`--legacy-container` is intended for
Lossless RGB565A8 and Bounded RGB565; prefer the default container for other
combinations. EAF4 limits must not be presented as universal classic EAF limits.

EAF4 requires dimensions divisible by 4 and at most 1024, 1..10000 frames,
10..10000 ms periods, and a file no larger than 32 MiB. Encoder checks are not
uniform: dimensions/periods are checked in the EAF4 encoder, while reader-side
limits such as final file size must also be checked after encoding. These are
output limits; they are not a complete service upload or job resource policy.
Large input decoding can itself consume substantial memory before encoding.

JPEG Delta's 16x16 alignment belongs to its tile/mask encoding layout, not to
hardware JPEG allocation. The CLI pads input dimensions to this layout even
when playback uses software JPEG. Hardware has additional width, subsampling
and DMA/cache alignment constraints. The device tries hardware first and, on
unsupported input or decode failure, falls back to `esp_new_jpeg` by default.
With `CONFIG_ESP_LV_EAF_ENABLE_SW_JPEG=n`, only hardware remains and unsupported
assets fail instead of falling back. Keep software enabled unless the deployed
assets have been validated on the target.

## What lossless and bounded mean

- Lossless BGRA8888 preserves the encoded BGRA bytes. The reference is the
  normalized frame after resize/padding and transparent RGB cleanup.
- RGB565 is already a quantization of RGB888. Lossless RGB565 preserves those
  16-bit values; RGB565A8 additionally preserves the separate A8 plane.
- EAF4 BGRA8888 normalizes hidden RGB at A=0 to zero by default; this preserves
  straight-alpha rendering. Use `--no-clean-alpha` to preserve source bytes.
  `--temporal-deadzone` changes visible pixel values across frames and is off
  by default. Lossless encoding preserves the resulting normalized pixels.
- Bounded BGRA8888 checks maximum absolute error in 8-bit B/G/R/A, including
  alpha. Threshold 4 means each reconstructed channel differs by at most 4
  from the frame supplied to the bounded encoder. It can affect transparency.
- Bounded RGB565 checks error relative to the quantized RGB565 reference,
  expanded to RGB888 using R*8/G*4/B*8. It does not promise the same bound
  relative to the pre-quantization source image. Raw blocks escape approximation
  when a candidate exceeds the configured bound.
- JPEG Delta is lossy and opaque. It requires decodability and quality
  checks; a file checksum cannot establish a pixel-error bound or useful PSNR.

The public codec ID (`0x00..0x06`) and EAF4 frame sub-mode (`0..11`) are different
fields; see [the frozen names](NAMING.md) and [the EAF4 format](FORMAT.md).

## Checksums and roundtrip validation

EAF4 stores CRCs for the header, index, compressed payloads and reconstructed
frames. The production reader verifies the header/index/payload CRCs. Its Host
build additionally checks the reconstructed frame CRC; the ESP build skips that
last full-frame pass. A reconstructed-frame CRC refers to the encoder's
reconstruction, which can already be lossy.

Lossless validation should compare decoded bytes to the reference in the declared
pixel format. Bounded validation additionally compares source and reconstruction
against the configured error bound. JPEG quality comparisons should specify
color conversion, pixel format, alpha handling and the decoder used. Host libjpeg
and a chip's hardware JPEG decoder need not round every pixel identically.

The inspector is a diagnostic tool, not a substitute for decoding all frames and
checking the appropriate reference. A future wrapper around the same C code must
state which optional codecs and output formats were actually compiled in.

## Memory is a path-dependent budget

For EAF4, let F be declared frame bytes, C the largest compressed frame, D the
largest inflated non-FULL/non-HSUB payload (at least 64 bytes), and N frame count:

```text
base ≈ F + C + D + 24*N + 8192 + small reader/player state
optional cache = compressed file payload, only if <= 4 MiB and allocation succeeds
optional seek  = another F-byte snapshot for a non-sequential delta seek
```

`D` is not necessarily F: masks, symbols and literal escapes can make it larger.
Even memory-backed EAF4 currently allocates the compressed streaming buffer.
Memory input borrows the caller's source bytes, whose storage is outside this
budget. File input may add the optional compressed cache. Calling
`esp_eaf_player_release_cache()` releases both that cache and a seek snapshot;
streaming buffers and the active frame remain allocated.

Classic EAF file input retains a complete owned file buffer in addition to its
output frame. Its codec-specific scratch, JPEG atlas/decoder workspaces, optional
hardware RGB888 conversion buffer need separate accounting.
The EAF4 formula is not the classic EAF formula.

GUI draw buffers, LCD framebuffers, DMA alignment, allocator metadata, task stacks
and application-owned copies are additional. Browser use would also need to count
input ArrayBuffers, WASM memory and canvas/upload buffers. A single 480x480 frame
is 900 KiB in BGRA8888, 450 KiB in RGB565 or 675 KiB in RGB565A8; none of these is
a complete player-memory measurement.

## Reproduce the tables

From the component checkout, install `requirements.txt` and the Host prerequisites
listed in [TESTING.md](TESTING.md), then run:

```sh
make -C test_apps/host test
python3 test_apps/host/benchmark_docs.py
python3 tools/render_codec_docs.py
python3 tools/render_codec_docs.py --check
```

The benchmark uses 24 deterministic 160x160 frames for flat UI, a translucent
circle, and a synthetic noisy-texture pan. It imports the production encoder,
checks every frame through the production C Host decoder, and records file sizes,
mode counts, dependency versions, generator/library hashes and validation results.
It does not require customer images or network data. The same encoding settings
are used for every input; the translucent case skips opaque JPEG Delta rather
than silently dropping alpha.

Each ratio uses raw bytes of the declared output format, not PNG/GIF size and not
a universal BGRA baseline. The RGB565A8 row therefore has a different raw byte
count from opaque RGB565 rows. File bytes include the container and index.
JPEG Delta uses quality 82 and 4:4:4; its RGB PSNR uses source RGB888 versus C
RGB565 output expanded by R*8/G*4/B*8. It includes quantization error and cannot be
compared with metrics using another expansion/color conversion without adjustment.
There are no encoding-FPS or device-PSRAM measurements
in this dataset. Do not substitute results from historical reports with different
sources, frame modes or preprocessing settings.

`codec_capabilities.json` is descriptive metadata for these tables, not a new
HTTP API schema. Update it together with the encoder contract when support
changes. The renderer verifies public names/IDs against `encoder/names.py` and
`--check` detects drift between the shared data and both README versions.

## CLI options

Lossless uses one adaptive mode-selection strategy. Format sub-modes remain
internal; users select a codec, pixel format, period and keyframe interval.

| Option | Default | Meaning and scope |
|---|---|---|
| `--codec` | `lossless` | `lossless`, `bounded`, `jpeg-delta` |
| `--format` | `bgra8888` | `bgra8888`, `rgb565`, `rgb565a8`; Bounded excludes RGB565A8, JPEG Delta requires RGB565 |
| `--period` | `67` ms | Uniform EAF4 period, 10..10000; individual input frame durations are not retained; classic EAF does not store it |
| `--key-interval` | `30` frames | Periodic keyframe/full refresh interval; at least 1 for EAF4; shorter intervals help seeking but usually increase size |
| `--max-error` | `8` | Bounded only, 0..255; applies to B/G/R/A channels for BGRA8888, or expanded quantized RGB565 channels; does not set Lossless/JPEG quality |
| `--no-clean-alpha` | Cleanup enabled | EAF4 BGRA8888 Lossless/Bounded normalizes RGB at A=0 by default, preserving straight-alpha rendering. Disable cleanup to retain hidden RGB bytes |
| `--temporal-deadzone` | `0` | EAF4 BGRA8888 Lossless only, 0..255; on delta frames, reuse a previous reconstructed pixel when its maximum channel difference is within the threshold; 0 disables it |
| `--resize WIDTHxHEIGHT` | No resize | Resize before padding to encoding dimensions, e.g. `--resize 320x240` |
| `--legacy-container` | Off | Classic EAF: Lossless RGB565A8 or Bounded RGB565 only; prefer default EAF4 for new assets |
| `--c-header` / `--c-symbol` | No header / automatic name | `encode` only: emit an additional C array header and set its symbol |

`batch` shares these encoding options; `-o` is an output directory and the
single-asset C array options are unavailable. JPEG Delta's lower-level quality
and subsampling controls are not exposed by `eaf_cli.py`; it currently uses
quality=82, 4:4:4. Run `python3 tools/eaf_cli.py encode --help` or `batch --help`
for syntax; see the [encoding guide](ENCODING.md) for input contracts,
error definitions and decoder fallback.
