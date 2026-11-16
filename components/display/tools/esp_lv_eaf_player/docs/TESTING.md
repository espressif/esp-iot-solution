# Component verification

Use a source checkout: registry packages exclude `test_apps/` and the Host port.
For device build, flash, menu and acceptance instructions, see
[test_apps/README.md](../test_apps/README.md).

## Host

Run from the component root. Install Python dependencies with
`python3 -m pip install -r requirements.txt`. Host tests also require a C11
compiler, pthreads, zlib and libjpeg development headers (for example,
`build-essential`, `zlib1g-dev` and `libjpeg-dev` on Debian/Ubuntu).
Video input extraction requires FFmpeg; the Host test fixtures do not.

```sh
make -C test_apps/host test
make -C test_apps/host sanitize
python3 tools/render_codec_docs.py --check
```

The Makefile builds production C sources and runs Python/C roundtrips, all EAF4
frame modes 0..11, classic EAF block layouts, exact pixels, bounded errors,
random seeks, independent streams, concurrent first decoding and CLI contracts.
The sanitizer target then runs lifecycle/malformed-input fixtures under ASan/UBSan
with leak detection. No external images or datasets are required.

## Device coverage

`core` runs assertions at boot. Its five generated files cover classic EAF,
two-frame BGRA8888 EAF4 with a tile delta, JPEG keyframes, JPEG Delta updates,
unsupported-width fallback/rejection, and direct RGB565 hardware JPEG output.
It verifies EAF4 pixel values in both frames, dirty rectangles, seek back to the
keyframe, and concurrent JPEG players. The two worker tasks use separate cores
where available and share the only core on single-core targets.

`lvgl8` and `lvgl9` run Unity menus with simulated 240×240 displays. They check
loading, flush, timing/configuration, repeat/restart, pause/resume, invalid source
replacement and leak thresholds. Device fixture generation needs only the
standard library; the first build may download component dependencies.

Build success alone does not establish JPEG DMA behavior, device runtime success
or physical display output. Run core tests on P4/S31 with software JPEG both on
and off before claiming hardware validation. Use the separate build/sdkconfig
commands in [the app guide](../test_apps/README.md#hardware-jpeg-on-esp32-p4).

## Recorded device verification

The current device regression used ESP-IDF 5.5.5 on physical ESP32-P4 with PSRAM.
Core JPEG/JPEG Delta assertions passed with software both enabled and disabled,
including hardware RGB565 output, hardware RGB888 converted to RGB565, partial
updates, unchanged pixels, unsupported-width fallback/rejection and concurrent
open/decode/seek/close. The two-frame EAF4 pixel/dirty-rectangle and seek
assertions also passed in both configurations.

Physical ESP32-S3 runs of LVGL 8 and LVGL 9 each passed all six current cases,
including the two-frame fixture, pause/resume, repeat restoration after restart,
and failed source replacement. Both tracked heap deltas were zero in these runs.
The simulated flush tests do not establish physical LCD appearance.

## Complete mode coverage on devices

Use the core application's [all-mode configuration](../test_apps/README.md#continuous-all-mode-conformance)
to verify all codec IDs and EAF4 frame modes on the device. Its procedural pack
contains 49 cases with source pixel references, error bounds, interleaved players,
seeking, cache release and malformed-input rejection. It loops continuously and
checks heap stability; it does not initialize an LCD.

Physical ESP32-S3 and ESP32-P4 (ESP-IDF 5.5.5), and ESP32-S31 RGB
(ESP-IDF 6.2-dev), each passed all 49 cases with software JPEG enabled and
again with it disabled. With software disabled, JPEG cases requiring software
are checked for rejection; they are not counted as successful JPEG decodes.
The enabled configurations also completed more than 100 consecutive sweeps
on each target with unchanged free heap and no panic or comparison failure.
This covers serial decoding and pixel validation, not LCD rendering.

The ESP raw-Deflate port allocates its ROM decompressor workspace on the heap.
The ROM mem-to-mem convenience wrapper uses a large automatic workspace that
can overflow an 8 KiB task during classic EAF Lossless decoding. The conformance
configuration retains the ordinary 8 KiB main-task stack to cover that path.

Software JPEG also stages unaligned output in a 16-byte-aligned buffer, covering
S3 SIMD decoding of strip atlases and block offsets without imposing a caller
alignment requirement.

## Documentation data

The README codec table is generated from `docs/codec_capabilities.json`.
Size comparisons and frame-buffer budgets are generated in
[MEASUREMENTS.md](MEASUREMENTS.md), using `docs/benchmarks/procedural.json`.
See [ENCODING.md](ENCODING.md#reproduce-the-tables) for the deterministic fixture
recipe and production C roundtrip/error checks. Run the renderer's `--check`
after documentation changes.
