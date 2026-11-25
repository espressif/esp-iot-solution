# ChangeLog

## v1.0.0 (2026-11-11)

* Keep ROM raw-Deflate workspace off task stacks; added continuous device checks
  for every codec ID and EAF4 frame mode with procedural pixel references.
* Stage unaligned software JPEG output in a 16-byte-aligned buffer for S3 SIMD decoding.

* Default to transparent RGB cleanup for EAF4 BGRA8888 encoding, with
  `--no-clean-alpha` for byte preservation; use one adaptive lossless strategy.

* Decoupled hardware JPEG from software fallback, serialized the shared engine,
  and released it with the last decoder; synchronized shared lookup initialization.
  Corrected hardware DMA block widths and cache alignment; added RGB888 hardware
  output conversion for supported JPEG Delta atlas widths.
* Preserved configured LVGL repeat counts across restart/load, stopped timers on
  unload and honored EAF4 timing unless an explicit delay overrides it.
* Added LVGL adapter opt-out, self-contained Host tests,
  and corrected inspector sizing, empty delta blocks, codec labels and CLI errors.

* Removed unused reference-only JPEG Delta tooling and obsolete private JPEG/strip
  encoder modes; all encoding entrypoints now accept the same three public codecs.

* Added the GUI-independent `esp_eaf_player.h` player API and made LVGL an
  optional adapter (`lv_eaf.c` is compiled automatically when `lvgl` is in the
  project).
* Unified legacy `EAF/AAF` and `EAF4` containers behind a single codec registry
  supporting `BGRA8888`, `RGB565`, and `RGB565A8` pixel formats and dirty
  rectangle tracking.
* Added `0x04` EAF Lossless (including BGRA8888 `HSUB` keyframes and packed
  temporal `SUB`/`XOR` tiles, plus bit-exact `RGB565`/`RGB565A8` 4x4 change-mask
  delta coding), `0x05` EAF Bounded (error-bounded 4x4 BTC with raw escape),
  `0x06` EAF JPEG Delta.
* Bundled the host Python encoder (`encoder/`) and CLI tools
  (`tools/eaf_cli.py`, `tools/eaf_inspect.py`) for single-asset conversion,
  64-byte-aligned batch packaging, C array generation, and container inspection.
* Made LVGL 8, LVGL 9, and GUI-independent `core` test applications
  self-contained with locally generated fixtures.

## v0.3.0 (2026-05-06)

* Added ESP32-S31 support.

## v0.2.0

* Add EAF transparency support.

## v0.1.0

* Implement the EAF LVGL decoder.
