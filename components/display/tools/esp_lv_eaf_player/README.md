[![Component Registry](https://components.espressif.com/components/espressif/esp_lv_eaf_player/badge.svg)](https://components.espressif.com/components/espressif/esp_lv_eaf_player)

# EAF/EAF4 Player Core and LVGL Adapter

**[中文](README_CN.md)**

`esp_lv_eaf_player` is an ESP-IDF component that decodes legacy `EAF/AAF` and `EAF4`
animations through a GUI-independent player API (`esp_eaf_player.h`). An optional
adapter exposes the same player as an LVGL 8 or LVGL 9 image widget (`lv_eaf.h`).

## Architecture

The component has four independent layers:

1. `player.c` owns stream state, frame buffers, seeking and container
   selection. Its public API is `esp_eaf_player.h` and contains no LVGL types.
2. Container readers under `container/eaf` and `container/eaf4` validate
   headers and indexes, then submit a normalized request. They contain no pixel
   reconstruction code.
3. Supported EAF/AAF modes `0x00..0x06` and all EAF4 frame sub-modes are selected by
   `codec_registry.c`. Bounded uses one callback and one tile loop for
   both containers; only its two wire-payload profiles differ. Other genuinely
   different payload implementations remain under `codec/eaf` or `codec/eaf4`.
4. `lv_eaf.c` adapts decoded frames to an LVGL image descriptor and handles only
   timers, events, and dirty-area invalidation.

The implementation mirrors those boundaries on disk: `src/core`,
`src/container/{eaf,eaf4}`, `src/codec/{common,eaf,eaf4}`, and
`src/adapter/lvgl`. Public headers remain flat under `include/` so existing
include statements and the published API stay compatible.

This boundary lets another GUI consume `esp_eaf_frame_t.pixels`, optional A8
alpha, stride, and dirty rectangle without including or linking LVGL.

## Requirements

- ESP-IDF 5.5 or newer;
- LVGL 8.x or 9.x only when using `lv_eaf.h`;
- PSRAM is recommended for large animations, but small assets can fall back to
  internal RAM;
- software JPEG decoding is enabled by default. ESP32-P4 and ESP32-S31 use the
  hardware JPEG decoder when available.

<!-- BEGIN GENERATED CODEC GUIDE -->
## Codec support and encoding capabilities

“Decode only” means the component can play these assets, but the bundled converter does not generate them.

| ID | Codec | CLI `--codec` | File format | Pixel formats | Recommended use |
|---|---|---|---|---|---|
| `0x00` | RLE | Decode only | EAF / AAF | RGB565, BGRA8888 | Existing assets and icon animation |
| `0x01` | HUFFMAN_RLE | Decode only | EAF / AAF | RGB565, BGRA8888 | Existing assets and icon animation |
| `0x02` | JPEG | Decode only | EAF / AAF | RGB565 | Existing JPEG assets |
| `0x03` | HUFFMAN | Decode only | EAF / AAF | RGB565, BGRA8888 | Existing assets and icon animation |
| `0x04` | LOSSLESS | `lossless` | EAF4 | BGRA8888, RGB565, RGB565A8 | Exact UI, shadows, translucent animation |
| `0x05` | BOUNDED | `bounded` | EAF4 | BGRA8888, RGB565 | UI/cartoon animation with bounded channel error |
| `0x06` | JPEG_DELTA | `jpeg-delta` | EAF | RGB565 | Photos, gradients, opaque animation |

The converter pads the right and bottom edges so dimensions are multiples of 4 for LOSSLESS/BOUNDED or 16 for JPEG_DELTA. It does not resize the original image.

Inputs include PNG sequence directories, GIF, APNG, animated WebP and video. See the [encoding guide](docs/ENCODING.md) for options and input requirements.

RGB565 losslessness is relative to quantized 16-bit pixels; RGB565A8 also preserves A8 exactly. JPEG Delta does not preserve alpha. BOUNDED BGRA8888 bounds channel error including A8; alpha is not guaranteed bit-exact.

JPEG decoding tries hardware first, falling back to `esp_new_jpeg` for unsupported input or failures. Disabling software JPEG removes this fallback.

**EAF4 output limits:** dimensions divisible by 4 and at most 1024; 1..10000 frames; 10..10000 ms periods; files at most 32 MiB.

See [measurements](docs/MEASUREMENTS.md) for size comparisons, memory budgets and reproduction steps.
<!-- END GENERATED CODEC GUIDE -->

Experimental bytecodes are rejected.

## Add to a project

```bash
idf.py add-dependency "espressif/esp_lv_eaf_player:*"
```

Or add it to the application's `idf_component.yml`:

```yaml
dependencies:
  espressif/esp_lv_eaf_player: "*"
```

LVGL is intentionally not a transitive dependency. An LVGL application also
adds `lvgl/lvgl: ">=8,<10"`; a non-LVGL application does not.

## Use from another GUI

```c
#include "esp_eaf_player.h"

esp_eaf_player_t *player = NULL;
esp_eaf_player_config_t config = ESP_EAF_PLAYER_CONFIG_DEFAULT();

/* Optional for classic EAF: only codecs with a direct BGRA8888 path accept it. */
config.output_format = ESP_EAF_OUT_BGRA8888;

ESP_ERROR_CHECK(esp_eaf_player_open_memory(eaf_data, eaf_size, &config, &player));
const esp_eaf_stream_info_t *info = esp_eaf_player_get_info(player);
const esp_eaf_frame_t *frame = esp_eaf_player_get_frame(player);

/* Submit frame->pixels, frame->stride and frame->alpha to your GUI. */
ESP_ERROR_CHECK(esp_eaf_player_next(player));
esp_eaf_player_close(player);
```

The input buffer is borrowed until `esp_eaf_player_close()`. A decoded frame is
owned by the player and remains valid until the next seek/next call or close.

`output_format` is a per-player runtime choice, not a Kconfig option. Classic
EAF defaults to RGB565 for compatibility. Its 8-bit palette and 24-bit BGR
codecs can write BGRA8888 directly; codecs backed by RGB565 payloads or
RGB565-only JPEG return `ESP_ERR_NOT_SUPPORTED`. The player never expands a
decoded RGB565 frame into BGRA8888. EAF4 exposes the pixel format declared in its header: BGRA8888, RGB565,
or RGB565 with a separate A8 plane. `ESP_EAF_OUT_RGB888` remains reserved and is rejected.

## Load an embedded asset

The data descriptor must contain the complete EAF/EAF4 file, not decoded pixels.

```c
#include "lv_eaf.h"

extern const lv_image_dsc_t my_animation_eaf;

#if LVGL_VERSION_MAJOR >= 9
lv_obj_t *screen = lv_screen_active();
#else
lv_obj_t *screen = lv_scr_act();
#endif

lv_obj_t *animation = lv_eaf_create(screen);
lv_eaf_set_frame_delay(animation, 50);  // milliseconds; set before or after load
lv_eaf_set_loop_count(animation, -1);   // infinite
lv_eaf_set_src(animation, &my_animation_eaf);
lv_obj_center(animation);

if (!lv_eaf_is_loaded(animation)) {
    // Reject the asset or show a fallback image.
}
```

To load an existing memory buffer without wrapping it in an LVGL descriptor:

```c
lv_eaf_set_src_data(animation, eaf_bytes, eaf_size);
```

The buffer passed to `lv_eaf_set_src_data()` is borrowed and must remain valid
until another source is selected or the widget is deleted.

## Load from LVGL FS

```c
lv_eaf_set_src(animation, "E:/animations/intro.eaf");
```

Mount the LVGL drive before loading the source. Data loaded from a file is owned
by the widget and released when the source changes or the widget is deleted.

## Playback controls

```c
lv_eaf_pause(animation);
lv_eaf_resume(animation);
lv_eaf_restart(animation);
lv_eaf_set_loop_enabled(animation, true);
lv_eaf_set_loop_count(animation, 2);
```

Unless explicitly configured, EAF4 uses its embedded frame period; classic EAF
defaults to unpaced playback. An explicit delay of `0` requests unpaced playback.

For RGB565 assets without continuous alpha, select a one-bit transparent color
with `lv_eaf_set_color_key()` before loading the source. Pass `-1` to disable it.

## Configuration

`Component config > ESP LV EAF Player > Enable Software JPEG Decoding` controls
`CONFIG_ESP_LV_EAF_ENABLE_SW_JPEG`. Disable it when the hardware backend handles all deployed JPEG blocks, or when
assets contain no JPEG blocks. Disabling it saves decoder working memory,
and JPEG assets require a compatible hardware decoder when the fallback is off.
Hardware JPEG is selected automatically on supported SoCs (`ESP32-P4`, `ESP32-S31`)
independently of the software fallback; a project can override
`ESP_LV_EAF_ENABLE_HW_JPEG` at compile time.

## Public headers

- `lv_eaf.h`: supported LVGL widget API;
- `esp_eaf_player.h`: GUI-independent player, stream metadata, and frame views;
- `esp_eaf_codec.h`: container probing, codec IDs, and output format types;
- `eaf4.h`: direct EAF4 container access. New GUI
  integrations should prefer `esp_eaf_player.h`.

GUI adapters should normally use `esp_eaf_player.h`; LVGL applications may use
only `lv_eaf.h`. Structures and codec hooks under `src/` are private and may
evolve without an API compatibility guarantee.

Set `CONFIG_ESP_LV_EAF_ENABLE_LVGL_ADAPTER=n` to keep a core-only build even
when another component uses LVGL. The adapter supports LVGL major versions 8 and 9.
Independent players may decode concurrently; hardware JPEG calls share a serialized
engine which is released after the final classic decoder closes.

## Asset conversion and CLI tools

Install dependencies with `python3 -m pip install -r requirements.txt`.
Video inputs require FFmpeg. For byte-exact BGRA preservation, use `--no-clean-alpha` and leave
`--temporal-deadzone` at 0.
See [format](docs/FORMAT.md), [naming](docs/NAMING.md) and [testing](docs/TESTING.md).


- **Online EAF Converter**: [https://esp32-gif.espressif.com/](https://esp32-gif.espressif.com/) generates legacy `0x00..0x03` EAF files.
- **Bundled Python CLI (`tools/eaf_cli.py`)**: encodes PNG sequences, folders, GIF, APNG, or MP4 into `EAF4` or `EAF` (`lossless`, `bounded`, `jpeg-delta`), batch-packs 64-byte-aligned partitions, emits C headers, and inspects `.eaf`/`.eaf4` files:

Common conversion and packing commands:

```bash
# Lossless EAF4; zero hidden RGB at A=0 by default, preserving straight-alpha rendering
python3 tools/eaf_cli.py encode frames/ -o anim.eaf4 --codec lossless

# BTC4 / BTC8: one codec, error thresholds 4 / 8, both with 4×4 blocks
python3 tools/eaf_cli.py encode frames/ -o btc4.eaf4 --codec bounded --max-error 4
python3 tools/eaf_cli.py encode frames/ -o btc8.eaf4 --codec bounded --max-error 8

# Opaque RGB565 JPEG Delta
python3 tools/eaf_cli.py encode frames/ -o anim.eaf --format rgb565 --codec jpeg-delta

# Assets, a 64-byte-aligned partition image, manifest and index header
python3 tools/eaf_cli.py batch animations/ -o build/eaf_assets --codec lossless
python3 tools/eaf_cli.py inspect anim.eaf4
```

| Option | Default | Purpose |
|---|---|---|
| `--format` | `bgra8888` | Also `rgb565`, `rgb565a8`; see support below |
| `--period` | `67` ms | EAF4 frame period |
| `--key-interval` | `30` frames | Keyframe/full refresh interval |
| `--max-error` | `8` | Bounded maximum channel error, 0..255 |
| `--no-clean-alpha` | Not set | Disable default cleanup to preserve hidden RGB bytes at A=0 |
| `--temporal-deadzone` | `0` | Temporal approximation threshold; BGRA8888 Lossless only; 0 disables it |
| `--resize WIDTHxHEIGHT` | No resize | Resize before encoding |

Lossless automatically selects its sub-modes; no strategy version is needed.
Transparent cleanup applies to EAF4 BGRA8888 Lossless/Bounded. See the
[encoding guide](docs/ENCODING.md#cli-options) for all options, C arrays and classic
containers, or run `encode --help` / `batch --help`.



## Tests

`test_apps/core`, `test_apps/lvgl8`, and `test_apps/lvgl9` are self-contained
ESP-IDF test projects. Their tiny legacy EAF and EAF4 fixtures are generated
locally during CMake configuration; no customer data or network download is used.

See [test_apps/README.md](test_apps/README.md) for commands and acceptance criteria.

## License

Apache-2.0. See [LICENSE](LICENSE).
