# ESP Painter Demo

Demonstrates the `esp_painter` component on an ESP32-P4 board with a 1024×600 MIPI DSI display (EK79007 panel).

## Hardware

| Item         | Detail                              |
|-------------|-------------------------------------|
| SoC          | ESP32-P4                            |
| Display      | EK79007, 1024×600, MIPI DSI 2-lane  |
| Interface    | MIPI DPI (RGB565)                   |
| Framebuffer  | PSRAM (external, 8-bit capable)     |

## Demo Screens

The example cycles through four screens every 3 seconds:

1. **Text** — Centered title rendered with the 48-pixel font, multi-color text strings
   with transparent and solid backgrounds, and a formatted string showing canvas size.
2. **Shapes** — Filled color-palette rectangles and hollow bordered rectangles with
   varying line widths.
3. **Detection + Pose** — Two simulated person detections, each with a bounding box
   and a full 13-keypoint skeleton drawn with `draw_line` and `draw_point`.
4. **Lines & Points** — Detection-confidence line chart for two classes over 10 frames,
   demonstrating `draw_line` (data series, axes, grid) and `draw_point` (sample markers)
   with a custom background color via `ESP_PAINTER_RGB888`.

## Build and Flash

```bash
idf.py set-target esp32p4
idf.py build
idf.py flash monitor
```

`sdkconfig.defaults` enables PSRAM at 200 MHz, 48- and 24-pixel fonts,
and sets the flash to 16 MB with a custom partition table.

## Notes

- The framebuffer (`1024 × 600 × 2 = 1.2 MB`) is allocated from PSRAM.
- `esp_painter` automatically calls `esp_cache_msync` after each draw operation,
  ensuring DMA sees the latest pixel data before `esp_lcd_panel_draw_bitmap`.
- The demo waits on a semaphore posted by the `on_color_trans_done` ISR before
  advancing to the next frame, preventing buffer modification during DMA transfer.
