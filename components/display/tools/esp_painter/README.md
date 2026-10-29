[![Component Registry](https://components.espressif.com/components/espressif/esp_painter/badge.svg)](https://components.espressif.com/components/espressif/esp_painter)

## Instructions and Details

`esp_painter` is a lightweight utility for rendering ASCII text and drawing basic shapes directly into a framebuffer. It supports RGB565 and RGB888 color formats and provides built-in bitmap fonts in multiple sizes.

The component writes pixel data directly into a caller-owned framebuffer. A cache writeback is performed automatically after each draw call so that downstream DMA or display hardware sees the updated data immediately.

### Features

- Renders ASCII text (printable characters 0x20–0x7E) into a flat framebuffer
- Supports RGB565 and RGB888 color formats
- Optional per-byte swap for RGB565 (big-endian display controllers)
- Optional text background fill; `ESP_PAINTER_COLOR_TRANSPARENT` to skip
- Draws hollow rectangles with configurable border thickness
- Draws filled rectangles
- Draws line segments between two points with configurable width (`esp_painter_draw_line`)
- Draws filled square point markers centered on a coordinate (`esp_painter_draw_point`)
- Clears the entire canvas to a solid color
- Draws object detection bounding boxes (border + label in one call)
- Measures text pixel dimensions without drawing (`esp_painter_get_text_size`)
- Built-in bitmap fonts: 12, 16, 20, 24, 28, 32, 36, 40, 44, 48 px height (enabled individually via Kconfig)
- printf-style formatted string drawing
- Custom font generation from any TTF/OTF file via `tools/font_gen.py`
- Automatic cache writeback for DMA-accessible buffers (PSRAM); skipped automatically for non-cached buffers

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependency`, e.g.

```
idf.py add-dependency esp_painter
```

## Usage

### Initialization

```c
#include "esp_painter.h"
#include "esp_painter_font.h"

esp_painter_handle_t painter = NULL;

const esp_painter_config_t config = {
    .canvas = {
        .width  = 1280,
        .height = 720,
    },
    .color_format = ESP_PAINTER_COLOR_FORMAT_RGB565,
    .default_font = &esp_painter_basic_font_24,  // requires CONFIG_ESP_PAINTER_BASIC_FONT_24=y
    .flags = {
        .swap_color_bytes = 0,
    },
};

ESP_ERROR_CHECK(esp_painter_init(&config, &painter));
```

### Color encoding

All color parameters use 24-bit RGB888 encoding: `0x00RRGGBB`. Use predefined constants,
the `ESP_PAINTER_RGB888(r, g, b)` macro, or plain hex literals. The painter converts to the
target framebuffer format (RGB565 or RGB888) internally.

```c
// Predefined constant
uint32_t c1 = ESP_PAINTER_COLOR_ORANGE;

// Custom color from components
uint32_t c2 = ESP_PAINTER_RGB888(255, 128, 0);

// Plain hex literal
uint32_t c3 = 0xFF8000U;  // same color as above
```

### Clearing the canvas

```c
// Fill entire framebuffer with black before drawing a new frame
esp_painter_clear(painter, fb, fb_size, ESP_PAINTER_COLOR_BLACK);
```

### Drawing text

The `bg_color` parameter controls text background fill. Pass `ESP_PAINTER_COLOR_TRANSPARENT`
to draw only the foreground pixels (no background), or any color to fill each character cell
before drawing the glyph. The background fill is useful when updating text on top of image
data (e.g. camera preview) to ensure the previous text is erased.

```c
// Plain string, transparent background (only foreground pixels written)
esp_painter_draw_string(painter, fb, fb_size, 10, 10, NULL,
                        ESP_PAINTER_COLOR_WHITE, ESP_PAINTER_COLOR_TRANSPARENT,
                        "Hello, World!");

// Text with black background — previous content erased automatically
esp_painter_draw_string(painter, fb, fb_size, 10, 40, NULL,
                        ESP_PAINTER_COLOR_WHITE, ESP_PAINTER_COLOR_BLACK,
                        "FPS: 30");

// Formatted string with an explicit font
esp_painter_draw_string_format(painter, fb, fb_size, 10, 70,
                               &esp_painter_basic_font_16,
                               ESP_PAINTER_RGB888(255, 200, 0),
                               ESP_PAINTER_COLOR_TRANSPARENT,
                               "Frame: %d  FPS: %.1f", frame, fps);
```

### Measuring text size

Use `esp_painter_get_text_size` to query the pixel bounding box a string would occupy
without actually drawing it. This function does not require a painter handle.

```c
uint16_t tw, th;
esp_painter_get_text_size(&esp_painter_basic_font_24, "Hello!", &tw, &th);

// Center the string horizontally
uint16_t x = (canvas_width - tw) / 2;
esp_painter_draw_string(painter, fb, fb_size, x, 10, &esp_painter_basic_font_24,
                        ESP_PAINTER_COLOR_WHITE, ESP_PAINTER_COLOR_TRANSPARENT, "Hello!");
```

### Drawing rectangles

```c
// Hollow rectangle — e.g. a UI panel border (2 px thick)
esp_painter_draw_rect(painter, fb, fb_size, 20, 20, 200, 100, 2,
                      ESP_PAINTER_COLOR_WHITE);

// Filled rectangle — e.g. a solid background block
esp_painter_draw_fill_rect(painter, fb, fb_size, 20, 20, 200, 100,
                           ESP_PAINTER_COLOR_NAVY);
```

### Drawing lines and points

```c
// Line segment — e.g. skeleton bone between two pose keypoints
esp_painter_draw_line(painter, fb, fb_size,
                      shoulder_x, shoulder_y, elbow_x, elbow_y,
                      2, ESP_PAINTER_COLOR_YELLOW);

// Point marker — e.g. a keypoint or landmark (radius=4 → 9×9 px square)
esp_painter_draw_point(painter, fb, fb_size,
                       kpt_x, kpt_y, 4, ESP_PAINTER_COLOR_CYAN);

// Thin grid line (line_width=1)
esp_painter_draw_line(painter, fb, fb_size, 0, y, canvas_w, y,
                      1, ESP_PAINTER_RGB888(50, 50, 90));
```

`line_width` controls the stroke thickness for lines. `radius` controls the half-side
of the square marker: a `radius` of 0 draws a single pixel, 1 draws 3×3, 4 draws 9×9, etc.

### Drawing AI detection boxes

```c
// Define a reusable style (create once, use for every detection result)
static const esp_painter_box_style_t box_style = {
    .line_width  = 2,
    .box_color   = ESP_PAINTER_COLOR_GREEN,
    .font        = NULL,                       // NULL = use painter's default font
    .text_color  = ESP_PAINTER_COLOR_GREEN,
};

// Draw bounding box + label in one call
esp_painter_draw_detection_box(painter, fb, fb_size,
                               det.x, det.y, det.w, det.h,
                               &box_style, det.label);           // e.g. "cat 0.92"

// Different style per class
static const esp_painter_box_style_t styles[] = {
    [CLASS_PERSON] = { .line_width = 2, .box_color = 0xFF0000U, .font = NULL, .text_color = 0xFF0000U },
    [CLASS_CAR]    = { .line_width = 2, .box_color = 0x00FF00U, .font = NULL, .text_color = 0x00FF00U },
};
esp_painter_draw_detection_box(painter, fb, fb_size,
                               det.x, det.y, det.w, det.h,
                               &styles[det.class_id], det.label);
```

### Cleanup

```c
esp_painter_deinit(painter);
```

## Custom fonts

Use `tools/font_gen.py` to generate a C bitmap font from any TTF or OTF file.

**Prerequisites**

```
pip install pillow
```

**Generate a font**

```
python tools/font_gen.py --font DejaVuSans.ttf --size 24 --name my_font_24 --output components/my_component/font/
```

The script creates two files in the output directory:

- `my_font_24.c` — bitmap data and `esp_painter_basic_font_t` definition
- `my_font_24.h` — header with `ESP_PAINTER_FONT_DECLARE(my_font_24)`; `#include` this in your source

**Preview the font before generating**

Use `--preview` to print an ASCII-art rendering of characters. Optionally list specific
characters; defaults to `A a 0 !` if none are given.

```
python tools/font_gen.py --font DejaVuSans.ttf --size 24 --name my_font_24 --preview
python tools/font_gen.py --font DejaVuSans.ttf --size 24 --name my_font_24 --preview A b 3 @
```

**Use the generated font**

Add `my_font_24.c` to your component's `CMakeLists.txt` source list, then include the header:

```cmake
idf_component_register(
    SRCS "main.c" "my_font_24.c"
    INCLUDE_DIRS ".")
```

```c
#include "my_font_24.h"

esp_painter_draw_string(painter, fb, fb_size, 0, 0, &my_font_24,
                        ESP_PAINTER_COLOR_WHITE, ESP_PAINTER_COLOR_TRANSPARENT, "Custom font");
```

## Kconfig options

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_ESP_PAINTER_FORMAT_SIZE_MAX` | 128 | Stack buffer size for `esp_painter_draw_string_format`; reduce if stack is constrained |
| `CONFIG_ESP_PAINTER_BASIC_FONT_12` … `_48` | n (24: y) | Enable individual built-in font sizes |

## Notes

- Only printable ASCII characters (space `0x20` to `~` `0x7E`) are supported; other bytes are silently skipped.
- Text and shapes that extend beyond the canvas boundary are clipped silently.
- `swap_color_bytes` applies only to RGB565 and is ignored for RGB888; a warning is logged at init if set for RGB888.
- For DMA-accessible buffers (e.g. PSRAM), each draw call performs an `esp_cache_msync` writeback automatically. For non-cached buffers (e.g. IRAM) the sync is skipped automatically.
- `esp_painter_draw_detection_box` performs a single cache sync covering both the border and the label, making it more efficient than calling `draw_rect` + `draw_string` separately.
- `esp_painter_get_text_size` does not require a painter handle and can be called before `esp_painter_init`.
