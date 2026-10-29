| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-S31 | ESP32-C3 |
| ----------------- | -------- | -------- | --------- | -------- |

# LVGL Dummy Draw Example

This example demonstrates how to use the **dummy draw mode** of the `esp_lvgl_adapter` component. Dummy draw mode allows you to take direct control of the LCD framebuffer, bypassing LVGL's rendering pipeline entirely.

## Overview

The example showcases:
- **Two update paths** – automatic selection based on the configured display interface:
  - **Path A – Pipeline / Tear-free** (RGB, MIPI-DSI with multi-buffer anti-tearing)
  - **Path B – Direct Blit** (SPI, I80, QSPI, or single-buffer modes)
- **Color format awareness** – handles both RGB565 (2 B/px) and RGB888 (3 B/px) natively
- **Touch / encoder interaction** – tap or button to toggle between LVGL and dummy draw mode
- **Automatic throttling** – Path A is naturally paced by the display refresh rate

## What is Dummy Draw Mode?

Dummy draw mode lets you bypass LVGL and write directly to the LCD hardware:

1. Call `esp_lv_adapter_set_dummy_draw(disp, true)` – LVGL rendering stops
2. Write pixel data to the display using one of the two paths
3. Call `esp_lv_adapter_set_dummy_draw(disp, false)` – LVGL rendering resumes

**Typical use cases:**
- Video playback (camera preview, MJPEG, H.264 decode output)
- High-throughput sensor visualisation (oscilloscopes, spectrum analysers)
- Boot splash screens before LVGL initialisation
- Mixed rendering – LVGL UI overlaid with a custom graphics layer

## Two Update Paths

### Path A – Pipeline / Tear-free (RGB, MIPI-DSI)

Available when the adapter is configured with a multi-buffer anti-tearing mode:
`TRIPLE_FULL`, `TRIPLE_PARTIAL`, `DOUBLE_FULL`, `DOUBLE_DIRECT`, or `DOUBLE_PARTIAL`.

```
get_free_buf() ──► fill buffer ──► flush_buf()
                                       │
                                       └── blocks until DPI/RGB hardware
                                           switches to the new buffer
                                           (tear-free, ~1 VSYNC latency)
```

**Key properties:**
- **Zero extra heap allocation** – reuses the adapter's existing frame buffers
- **Tear-free** – `flush_buf()` blocks until the hardware completes the buffer switch
- **Self-throttled** – naturally paced at the display refresh rate (e.g., 60 fps)
- **Color-format transparent** – the buffer is in the display's native format (RGB565 or RGB888)

```c
void *fb = esp_lv_adapter_dummy_draw_get_free_buf(disp);   // non-blocking
if (fb) {
    fill_my_content(fb);
    esp_lv_adapter_dummy_draw_flush_buf(disp, fb);          // blocks ~1 frame
}
```

### Path B – Direct Blit (SPI, I80, QSPI, or fallback)

Used when the pipeline path is unavailable (e.g., tear avoidance mode `NONE` or `TE_SYNC`,
or SPI/I80/QSPI interfaces that do not use frame-buffer switching).

```
alloc buffer ──► fill buffer ──► dummy_draw_blit()
                                       │
                                       └── DMA transfer to LCD controller
```

**Key properties:**
- Requires a separately allocated framebuffer (PSRAM if available)
- No inherent tear prevention
- Requires explicit pacing (`vTaskDelay`)

```c
void *fb = heap_caps_malloc(w * h * bpp, MALLOC_CAP_SPIRAM);
fill_my_content(fb);
esp_lv_adapter_dummy_draw_blit(disp, 0, 0, w, h, fb, true /* wait */);
vTaskDelay(pdMS_TO_TICKS(16));   // manual pacing
```

### Automatic Path Selection

The example calls `esp_lv_adapter_dummy_draw_get_free_buf()` at task start to detect
which path is available:

```c
bool use_pipeline = (esp_lv_adapter_dummy_draw_get_free_buf(disp) != NULL);
// → true  : RGB / MIPI-DSI with multi-buffer mode  → Path A
// → false : SPI / I80 / QSPI or NONE / TE_SYNC     → Path B
```

## Hardware Required

* An ESP32-P4, ESP32-S3, ESP32-S31, or ESP32-C3 development board
* A LCD panel with one of the supported interfaces:
  - **MIPI DSI**: For high-resolution displays (e.g., 1024×600)
  - **RGB**: For parallel RGB interface displays (e.g., 800×480)
  - **QSPI**: For quad-SPI displays (e.g., 360×360, 400×400)
  - **SPI**: For standard SPI displays (e.g., 240×240, 320×240)
* (Optional) Touch panel or rotary encoder for input

**Recommended Hardware Combinations:**

| Chip | LCD Interface | Anti-tearing Path | Development Board |
|------|---------------|-------------------|-------------------|
| ESP32-P4 | MIPI DSI | **Path A** (pipeline) | [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html) |
| ESP32-S3 | RGB | **Path A** (pipeline) | [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) |
| ESP32-S3 | QSPI | Path B (blit) | [ESP-VoCat](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp-vocat/index.html) |
| ESP32-S3 | SPI | Path B (blit) | [ESP32-S3-BOX-3](https://github.com/espressif/esp-box/blob/master/docs/hardware_overview/esp32_s3_box_3/hardware_overview_for_box_3.md) |
| ESP32-S31 | RGB | **Path A** (pipeline) | Refer to your board documentation |
| ESP32-C3 | SPI | Path B (blit) | [ESP32-C3-LCDkit](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c3/esp32-c3-lcdkit/index.html) |

## Configure the Project

Run `idf.py menuconfig` and configure:

**Example Configuration:**
- LCD Interface – Select MIPI DSI / RGB / QSPI / SPI
- Anti-tearing Mode – For Path A, choose any multi-buffer mode
  (`Triple Buffer Full`, `Triple Buffer Partial`, `Double Buffer`, etc.)
- Display Rotation – Choose 0° / 90° / 180° / 270°
- Input Device – Enable touch panel or encoder if available

**Performance (Optional):**
- Enable FPS statistics to measure the refresh rate in dummy draw mode

## Build and Flash

1. Set the target chip:
```bash
idf.py set-target esp32p4
# or
idf.py set-target esp32s3
# or
idf.py set-target esp32s31
# or
idf.py set-target esp32c3
```

2. Build, flash and monitor:
```bash
idf.py -p PORT build flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

## Expected Output

**Initial Screen (LVGL Mode):**
```
┌─────────────────────────┐
│                         │
│     Tap to start        │
│       800x480           │  ← Display resolution
│                         │
└─────────────────────────┘
```
Tap anywhere on the screen to enter Dummy Draw mode.

**Dummy Draw Mode:**
```
┌─────────────────────────┐
│                         │
│    [Solid Red fill]     │  ← Full-screen colour
│                         │
└─────────────────────────┘
```
The screen cycles Red → Green → Blue. Tap to exit back to LVGL mode.

**Serial Console (Path A – pipeline):**
```
I (xxx) main: UI created for 800x480
I (xxx) main: [Dummy] Enabled – path: Pipeline (tear-free)
I (xxx) main: [Demo] Color cycle started – pipeline / tear-free
I (xxx) main: [Demo] Red (0xF800)
I (xxx) main: [Demo] Green (0x07E0)
I (xxx) main: [Demo] Blue (0x001F)
I (xxx) main: [Demo] Stopped
I (xxx) main: [Dummy] Disabled
```

**Serial Console (Path B – direct blit):**
```
I (xxx) main: UI created for 240x240
I (xxx) main: [Dummy] Enabled – path: Direct Blit
I (xxx) main: [Blit] Buffer allocated: 240x240 x2B = 115200 B
I (xxx) main: [Demo] Color cycle started – direct blit
I (xxx) main: [Demo] Red (0xF800)
...
```

## Code Structure

```
main.c
├── Color helpers
│   └── fill_native_framebuf()        Fill buffer in display-native format (RGB565 / RGB888)
│
├── Path A – Pipeline / Tear-free
│   └── pipeline_update()             get_free_buf → fill → flush_buf (blocks ~1 frame)
│
├── Path B – Direct Blit
│   ├── blit_ensure_buffer()          Lazy heap allocation (PSRAM preferred)
│   └── blit_update()                 fill → dummy_draw_blit
│
├── Dummy draw enable / disable
│   ├── dummy_draw_enable()           Set mode + log active path
│   └── dummy_draw_disable()          Restore LVGL control
│
├── Demo task
│   └── dummy_draw_cycle_colors_task  Auto-detect path, cycle Red/Green/Blue
│
├── Mode switching
│   ├── enter_dummy_mode()
│   └── exit_dummy_mode()
│
└── UI & Events
    ├── create_control_ui()
    └── screen_touch_event_cb()
```

**Key APIs used:**

| API | Description |
|-----|-------------|
| `esp_lv_adapter_set_dummy_draw()` | Enable / disable dummy draw mode |
| `esp_lv_adapter_get_dummy_draw_enabled()` | Query current mode |
| `esp_lv_adapter_dummy_draw_get_free_buf()` | Get next writable pipeline buffer (Path A) |
| `esp_lv_adapter_dummy_draw_flush_buf()` | Submit buffer; block until hardware switch (Path A) |
| `esp_lv_adapter_dummy_draw_blit()` | Copy user buffer to LCD (Path B) |
| `lv_display_get_color_format()` | Determine display colour format |
| `lv_color_format_get_size()` | Get bytes-per-pixel for the format |

## Troubleshooting

**Screen stays black after entering dummy mode:**
- Check framebuffer allocation succeeded (serial logs)
- Verify LCD interface is properly initialised
- For Path A: confirm a multi-buffer anti-tearing mode is selected in menuconfig

**Path B is selected on a MIPI-DSI / RGB display:**
- Ensure anti-tearing mode is set to a multi-buffer mode (not `NONE`)
- Confirm the display adapter was initialised before entering dummy draw mode

**Colors appear incorrect on MIPI-DSI (RGB888):**
- The example auto-detects bytes-per-pixel via `lv_display_get_color_format()`
- Verify the color format is correctly configured in `example_lvgl_init`

**Touch not responding:**
- Verify touch panel is functioning (check serial logs during touch init)
- Try tapping center of screen
- For encoder: use the toggle button at the bottom of the screen

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
