# Lottie Player for LVGL

[![Component Registry](https://components.espressif.com/components/espressif/esp_lv_lottie_player/badge.svg)](https://components.espressif.com/components/espressif/esp_lv_lottie_player)

## Overview

`esp_lv_lottie_player` is a lightweight Lottie animation player for LVGL v8 and v9. It uses ThorVG to parse and rasterize JSON animations, then renders into an LVGL canvas buffer for display.

## Features

- LVGL v8 and v9 support
- File and memory-based Lottie JSON sources
- Playback controls: play, pause, stop, restart
- Loop control and segment playback
- Custom frame delay
- Internal buffer allocation or user-provided buffers

## Dependencies

- **LVGL**: v8 or v9 (`lvgl/lvgl` >=8, <10)
- **ThorVG**: used as the Lottie renderer (`espressif/thorvg` 0.15.*)
- **ESP-IDF**: >=4.4

## Installation

```bash
idf.py add-dependency "espressif/esp_lv_lottie_player"
```

## Usage

```c
#include "esp_lv_lottie.h"

lv_obj_t *lottie = lv_lottie_create(lv_scr_act());

/* Allocate internal buffer (ARGB8888 premultiplied) */
lv_lottie_set_size(lottie, 200, 200);

/* Load from LVGL filesystem */
lv_lottie_set_src(lottie, "A:animation.json");

/* Or load from raw JSON in memory */
lv_lottie_src_dsc_t src_dsc = {
    .data = json_data,
    .data_size = json_size,
};
lv_lottie_set_src(lottie, &src_dsc);

lv_lottie_set_loop_enabled(lottie, true);
lv_lottie_play(lottie);
```

**Note:** When using raw JSON data, the buffer must remain valid until a new source is set or the object is deleted.

## Buffer Management

- **Internal buffer**: `lv_lottie_set_size()` allocates an ARGB8888 premultiplied buffer (PSRAM preferred).
- **External buffer**: `lv_lottie_set_buffer()` accepts a `width x height x 4` byte buffer.
- **Draw buffer (LVGL v9)**: `lv_lottie_set_draw_buf()` requires ARGB8888 or ARGB8888_PREMULTIPLIED.
- **LVGL v8 + 16bpp**: ThorVG renders ARGB8888; the component converts to RGB565 in place and ignores alpha.

## API Reference

| Function | Description |
|----------|-------------|
| `lv_lottie_create()` | Create a Lottie animation object |
| `lv_lottie_set_src()` | Set source (JSON file path or `lv_lottie_src_dsc_t`) |
| `lv_lottie_set_size()` | Allocate internal buffer and set size |
| `lv_lottie_set_buffer()` | Use a user-provided raw buffer |
| `lv_lottie_set_draw_buf()` | Use a user-provided LVGL draw buffer (v9) |
| `lv_lottie_is_loaded()` | Check if the animation loaded successfully |
| `lv_lottie_get_total_frames()` | Get total frame count |
| `lv_lottie_get_current_frame()` | Get current frame index |
| `lv_lottie_set_frame_delay()` | Set custom frame delay |
| `lv_lottie_set_segment()` | Set playback frame range |
| `lv_lottie_set_loop_count()` | Set loop count (-1 for infinite) |
| `lv_lottie_set_loop_enabled()` | Enable or disable looping |
| `lv_lottie_play()` | Start or resume playback |
| `lv_lottie_pause()` | Pause playback |
| `lv_lottie_stop()` | Stop and reset to segment start |
| `lv_lottie_restart()` | Restart from segment start |
| `lv_lottie_get_anim()` | Get the underlying LVGL animation handle |

## Notes and Limitations

- **LVGL task stack**: Increase the LVGL task stack (for example, 32 KB) to avoid stack overflow when loading and rendering Lottie assets. The loader itself uses a 32 KB worker task stack.
- **Asset compatibility**: Not all Lottie files are guaranteed to render correctly. Assets with elements outside the canvas or with uncommon features may be clipped or display artifacts.
- **LVGL v8 transparency**: LVGL v8 uses TRUE_COLOR for the canvas buffer in this component, so alpha blending is not supported and transparency is ignored.
- **Memory usage**: Lottie buffers are ARGB8888; PSRAM is strongly recommended for medium and large resolutions.
- **Avoid embedded image sequences**: Lottie files with embedded PNG/JPEG frames are **not recommended**. ThorVG preloads all frames into memory simultaneously (e.g., 78 frames at 500×417 ≈ 62 MB), exceeding typical PSRAM capacity. Use pure vector animations instead.

## License

This component is provided under the Apache 2.0 License. See [LICENSE](LICENSE) for details.
