[![Component Registry](https://components.espressif.com/components/espressif/esp_lvgl_adapter/badge.svg)](https://components.espressif.com/components/espressif/esp_lvgl_adapter)

# ESP LVGL Adapter

English | [中文](README_CN.md)

## Table of Contents

- [Overview](#overview)
- [Add to Project](#add-to-project)
- [Quick Start](#quick-start)
- [Usage](#usage)
  - [Decision Flow](#decision-flow)
  - [Display Registration and Tearing](#display-registration-and-tearing)
  - [Buffer Tuning](#buffer-tuning-defaults-and-tips)
  - [Hardware Initialization](#hardware-initialization-pass-num_fbs)
  - [Common Recipes](#common-recipes)
  - [Task Stack & PSRAM](#task-stack--psram)
  - [Thread-Safe LVGL Access](#thread-safe-lvgl-access)
  - [Input Devices](#input-devices)
  - [Filesystem Bridge](#filesystem-bridge-optional)
  - [Image Decoding](#image-decoding-optional)
  - [FreeType Fonts](#freetype-fonts-optional)
  - [FPS Statistics and Dummy Draw](#fps-statistics-and-dummy-draw)
  - [Immediate Refresh](#immediate-refresh)
  - [Area Rounding](#area-rounding)
  - [Sleep Management](#sleep-management)
- [Configuration (Kconfig)](#configuration-kconfig)
- [Limitations & Notes](#limitations--notes)
- [Deinitialization](#deinitialization)
- [Compatibility](#compatibility)
- [Examples](#examples)
- [Troubleshooting](#troubleshooting)
- [Glossary](#glossary)
- [See Also](#see-also)
- [License](#license)

---

## Overview

`esp_lvgl_adapter` provides a runtime-managed LVGL integration for ESP-IDF projects. It unifies display registration, tearing control, thread-safe LVGL access, and optional integration with filesystem, image decoding, and FreeType fonts. It supports LVGL v8 and v9.

### Features

- **Unified display registration**: Supports MIPI DSI / RGB / QSPI / SPI / I2C / I80 interfaces
- **Tearing control**: Multiple tearing modes
- **Thread safety**: Lock/unlock APIs for safe multi-threaded LVGL access
- **Optional modules** (via Kconfig):
  - Filesystem bridge (`esp_lv_fs`)
  - Image decoder (`esp_lv_decoder`)
  - FreeType vector font loader
  - FPS statistics, Dummy Draw (headless rendering)
- **Multi-display support**: Manage multiple displays simultaneously

---

## Add to Project

### Via Component Service

This component is available from Espressif's component service. Add it to your project with:

```sh
idf.py add-dependency "espressif/esp_lvgl_adapter"
```

### In `idf_component.yml`

Alternatively, declare it in your component manifest:

```yaml
dependencies:
  espressif/esp_lvgl_adapter: "*"
```

### Requirements

- **ESP-IDF**: >= 5.5
- **LVGL**: 8.x or 9.x (auto-detected)
- **Optional dependencies**: Required only if corresponding Kconfig options are enabled

### Build and Run

```sh
idf.py set-target esp32s3   # Set target chip (or esp32, esp32c3, esp32p4, etc.)
idf.py build                # Build the project
idf.py flash monitor        # Flash and monitor
```

---

## Quick Start

Here's a minimal example showing how to initialize the adapter and register a display:

```c
#include "esp_lv_adapter.h"  // Includes display & input adapters

void app_main(void)
{
    // Step 0: Create your esp_lcd panel and (optionally) panel_io with esp_lcd APIs
    esp_lcd_panel_handle_t panel = /* ... */;
    esp_lcd_panel_io_handle_t panel_io = /* ... or NULL */;

    // Step 1: Initialize the adapter
    esp_lv_adapter_config_t cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&cfg));

    // Step 2: Register a display (choose macro by interface)
    esp_lv_adapter_display_config_t disp_cfg = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
        panel,           // LCD panel handle
        panel_io,        // LCD panel IO handle (can be NULL for some interfaces)
        800,             // Horizontal resolution
        480,             // Vertical resolution
        ESP_LV_ADAPTER_ROTATE_0 // Rotation
    );
    lv_display_t *disp = esp_lv_adapter_register_display(&disp_cfg);
    assert(disp != NULL);

    // Step 3: (Optional) Register input device(s)
    // Create touch handle using esp_lcd_touch API (implementation omitted here)
    // esp_lcd_touch_handle_t touch_handle = /* ... */;
    // esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
    // lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_cfg);
    // assert(touch != NULL);

    // Step 4: Start the adapter task
    ESP_ERROR_CHECK(esp_lv_adapter_start());

    // Step 5: Draw with LVGL (guarded by adapter lock for thread safety)
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        lv_obj_t *label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "Hello LVGL!");
        lv_obj_center(label);
        esp_lv_adapter_unlock();
    }
}
```

---

## Usage

This section guides you through integrating the LVGL adapter with ESP-IDF projects.

### Recommended SDKCONFIG Settings

Add these configurations to your project's `sdkconfig.defaults` for optimal LVGL performance:

#### Core LVGL Configurations

```ini
# FreeRTOS tick rate (improves timer precision)
CONFIG_FREERTOS_HZ=1000

# LVGL OS integration
CONFIG_LV_OS_FREERTOS=y

# Use standard C library functions
CONFIG_LV_USE_CLIB_MALLOC=y
CONFIG_LV_USE_CLIB_STRING=y
CONFIG_LV_USE_CLIB_SPRINTF=y

# Display refresh rate (in milliseconds, 15ms ≈ 66 FPS)
CONFIG_LV_DEF_REFR_PERIOD=15

# Enable style caching for better performance
CONFIG_LV_OBJ_STYLE_CACHE=y
```

#### Multi-core Optimization (ESP32-S3/P4)

For chips with multiple cores, enable parallel rendering:

```ini
# Use 2 drawing threads for better performance
CONFIG_LV_DRAW_SW_DRAW_UNIT_CNT=2
```

#### PSRAM Optimization (When Available)

For devices with PSRAM, optimize cache and XIP settings:

**ESP32-S3:**
```ini
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_ESP32S3_INSTRUCTION_CACHE_32KB=y
CONFIG_ESP32S3_DATA_CACHE_64KB=y
CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y
```

**ESP32-P4:**
```ini
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_CACHE_L2_CACHE_256KB=y
CONFIG_CACHE_L2_CACHE_LINE_128B=y
```

#### Performance Optimization

```ini
# Enable compiler optimizations
CONFIG_COMPILER_OPTIMIZATION_PERF=y
```

> **Note**: These are recommended baseline settings. Adjust based on your specific requirements and available resources.

### Decision Flow

Before starting, make these decisions:

1. **Pick interface type**: Determine your display interface (MIPI DSI / RGB / QSPI / SPI / I2C / I80)
2. **Pick default macro**: Select corresponding `ESP_LV_ADAPTER_DISPLAY_XXX_DEFAULT_CONFIG` macro
3. **Pick appropriate tearing mode** (RGB/MIPI DSI only):
   - Compute required frame buffer count (`num_fbs`)
   - Pass `num_fbs` to esp_lcd panel config
4. **Tune buffer size**: Adjust `buffer_height` based on memory vs throughput needs

### Display Registration and Tearing

#### Interface Types and Default Macros

Choose the configuration macro based on your display interface:

| Interface Type | Default Config Macro | Default Tearing Mode |
|---------------|---------------------|---------------------|
| MIPI DSI | `ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(...)` | `ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI` |
| RGB | `ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(...)` | `ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB` |
| SPI/I2C/I80/QSPI (with PSRAM) | `ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(...)` | `ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT` (i.e., `NONE`) |
| SPI/I2C/I80/QSPI (without PSRAM) | `ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(...)` | `ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT` (i.e., `NONE`) |
| MONO (Monochrome) | `ESP_LV_ADAPTER_DISPLAY_PROFILE_MONO_DEFAULT_CONFIG(...)` | `ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT` (i.e., `NONE`) |

**Notes**:
- Only MIPI DSI and RGB support tearing modes
- SPI/I2C/I80/QSPI are collectively called "OTHER" interfaces in the adapter and support only `NONE` mode
- MONO (Monochrome) interface supports I1 horizontal tiled (HTILED) and vertical tiled (VTILED) layouts, and **supports rotation**

#### Computing Frame Buffer Count

For RGB/MIPI DSI, compute required frame buffers early before hardware initialization:

```c
uint8_t num_fbs = esp_lv_adapter_get_required_frame_buffer_count(
    ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB,  // Tearing mode
    ESP_LV_ADAPTER_ROTATE_0                // Rotation
);
// Pass num_fbs to your esp_lcd panel config
```

**Frame Buffer Count Rules**:
- 90°/270° rotation or triple-buffer modes: **3** frame buffers
- Double-buffer modes: **2** frame buffers
- Single-buffer mode: **1** frame buffer

#### Tearing Modes: Selector

Choose the appropriate tearing mode based on your use case:

| Tearing Mode | Use Case | Frame Buffers | Memory | Supported Interfaces |
|-------------|----------|---------------|--------|---------------------|
| `TRIPLE_PARTIAL` | 90°/270° rotation<br>High-res smooth UI | 3 | High | RGB / MIPI DSI |
| `TRIPLE_FULL` | Full-screen/large-area updates<br>Plenty of RAM | 3 | High | RGB / MIPI DSI |
| `DOUBLE_FULL` | Large-area updates<br>Tighter RAM | 2 | Medium | RGB / MIPI DSI |
| `DOUBLE_DIRECT` | Small-area updates<br>Widget/UI deltas | 2 | Medium | RGB / MIPI DSI |
| `TE_SYNC` | SPI/I2C/I80/QSPI interfaces<br>Panel provides TE signal, syncs with vertical blanking to eliminate tearing | 1 | Low | SPI / I2C / I80 / QSPI |
| `NONE` | Static UI<br>Ultra-low RAM | 1 | Low | All interfaces |

**Important Limitations**:
- RGB/MIPI DSI with `TEAR_AVOID_MODE_NONE` **forbids rotation** (any non-zero rotation is rejected)
- OTHER (SPI/I2C/I80/QSPI) interfaces support `NONE` and `TE_SYNC` modes; for rotation, configure panel orientation (swap XY/mirror) during LCD initialization and adjust touch mapping accordingly
- MONO (Monochrome) interface **supports software rotation** (0°/90°/180°/270°) with pixel-level rotation handled by LVGL
- `TE_SYNC` mode requires panel to provide TE output signal and connect TE pin to ESP GPIO; use `ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_TE_DEFAULT_CONFIG` macro for configuration. The `examples/display/gui/lvgl_common_demo` automatically detects and uses TE synchronization if available

#### Memory Estimation

Full-frame buffer size ≈ `horizontal_res × vertical_res × bytes_per_pixel`

**Example** (RGB565 format, 2 bytes/pixel):
- 800×480 resolution: ≈ 750 KB
- Triple buffering: ≈ 2.2 MB
- **Conclusion**: PSRAM is typically required

### Buffer Tuning (Defaults and Tips)

#### Default `buffer_height` Values

Default `buffer_height` for each interface (see `esp_lv_adapter_display.h`):

| Interface Type | Default `buffer_height` | Description |
|---------------|------------------------|-------------|
| MIPI DSI | 50 | Medium stripe height |
| RGB | 50 | Medium stripe height |
| OTHER with PSRAM | `ver_res` (full height) | Full-height partial refresh, better throughput |
| OTHER without PSRAM | 10 | Small stripe to save RAM |

#### Tuning Tips

- **Increase `buffer_height`**:
  - ✅ Improves throughput (fewer flushes, larger stripes)
  - ❌ Increases RAM usage
  
- **Decrease `buffer_height`**:
  - ✅ Saves RAM
  - ❌ More flushes, may reduce performance
  
### Hardware Initialization: Pass `num_fbs`

For RGB/MIPI DSI, compute panel frame buffer count early and pass into esp_lcd configs:

```c
// 1. Choose tearing mode and rotation
esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;
esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;

// 2. Compute required frame buffer count
uint8_t num_fbs = esp_lv_adapter_get_required_frame_buffer_count(tear_avoid_mode, rotation);

// 3. Pass num_fbs to esp_lcd panel config
#if SOC_MIPI_DSI_SUPPORTED
esp_lcd_dpi_panel_config_t dpi_cfg = {
    .num_fbs = num_fbs,
    // ... other MIPI DSI DPI config fields
};
#endif

#if SOC_LCDCAM_RGB_LCD_SUPPORTED
esp_lcd_rgb_panel_config_t rgb_cfg = {
    .num_fbs = num_fbs,
    // ... other RGB config fields
};
#endif
```

See examples and test apps for real configurations.

### Task Stack & PSRAM

#### Adapter LVGL Task Stack

The adapter's LVGL task stack defaults to 8KB and supports placement in PSRAM:

```c
esp_lv_adapter_config_t cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
cfg.stack_in_psram = true;   // Try placing adapter task stack in PSRAM
ESP_ERROR_CHECK(esp_lv_adapter_init(&cfg));
```

**Notes**:
- Default stack size: 8KB (suitable for most applications)
- Falls back to internal RAM if PSRAM is unavailable
- PSRAM has slightly higher latency than internal RAM, but minimal impact on UI tasks

### Thread-Safe LVGL Access

LVGL APIs are **not thread-safe**. Guard all LVGL calls with the adapter's lock:

```c
if (esp_lv_adapter_lock(-1) == ESP_OK) {
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 100, 50);
    esp_lv_adapter_unlock();
}
```

### Input Devices

#### Touch

The adapter provides a default configuration macro `ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG` to simplify touch device registration:

```c
// Register touch device using default configuration macro
esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_cfg);
assert(touch != NULL);
```

**Parameters**:
- `disp`: Associated LVGL display device
- `touch_handle`: Touch handle created via `esp_lcd_touch` API
- Default scale factors: x = 1.0, y = 1.0

#### Encoder/Knob

Requires Kconfig option `ESP_LV_ADAPTER_ENABLE_KNOB`. See `esp_lv_adapter_input.h` for `esp_lv_adapter_encoder_config_t`.

#### Navigation Buttons

Requires Kconfig option `ESP_LV_ADAPTER_ENABLE_BUTTON`. Use `esp_lv_adapter_register_navigation_buttons()`.

### Filesystem Bridge (Optional)

If `ESP_LV_ADAPTER_ENABLE_FS` is enabled, mount a memory-mapped asset partition as an LVGL drive. Indexing comes from `esp_mmap_assets`.

```c
#include "esp_lv_fs.h"
#include "esp_mmap_assets.h"

// 1. Create mmap assets handle
mmap_assets_handle_t assets;
const mmap_assets_config_t mmap_cfg = {
    .partition_label = "assets_A",
    .max_files = MMAP_DRIVE_A_FILES,
    .checksum  = MMAP_DRIVE_A_CHECKSUM,
    .flags = { .mmap_enable = true },
};
ESP_ERROR_CHECK(mmap_assets_new(&mmap_cfg, &assets));

// 2. Mount as LVGL filesystem
esp_lv_fs_handle_t fs_handle;
const fs_cfg_t fs_cfg = {
    .fs_letter = 'A',           // Drive letter (A to Z)
    .fs_assets = assets,
    .fs_nums = MMAP_DRIVE_A_FILES,
};
ESP_ERROR_CHECK(esp_lv_adapter_fs_mount(&fs_cfg, &fs_handle));

// 3. Use file paths in LVGL, e.g., "A:my_image.png"
lv_obj_t *img = lv_img_create(lv_scr_act());
lv_img_set_src(img, "A:my_image.png");
```

### Image Decoding (Optional)

Enable `ESP_LV_ADAPTER_ENABLE_DECODER` to integrate `esp_lv_decoder` with LVGL. Use LVGL image widgets as usual; load from the mounted drive:

```c
// Use LVGL image widget to load images
lv_obj_t *img = lv_img_create(lv_scr_act());
lv_img_set_src(img, "I:red_png.png");  // I: is the decoder drive letter
```

**Supported Formats**:
- Standard: JPG, PNG, QOI
- Split formats: SJPG, SPNG, SQOI (optimized for embedded systems)
- Hardware acceleration: JPEG, PJPG (if chip supports)

**Details**: Refer to `components/display/tools/esp_lv_decoder/README.md`

### FreeType Fonts (Optional)

Enable `ESP_LV_ADAPTER_ENABLE_FREETYPE` and ensure LVGL FreeType is enabled. Then initialize fonts from file or memory:

```c
#include "esp_lv_adapter.h" // FreeType APIs under this header

// Load font from file
esp_lv_adapter_ft_font_handle_t fh;
esp_lv_adapter_ft_font_config_t cfg = {
    .name = "F:my_font.ttf",         // Font file path (Note: no slash after colon for mmap assets)
    .size = 30,                      // Font size
    .style = ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL,
};
ESP_ERROR_CHECK(esp_lv_adapter_ft_font_init(&cfg, &fh));
const lv_font_t *font30 = esp_lv_adapter_ft_font_get(fh);

// Use the font
lv_obj_set_style_text_font(label, font30, 0);
```

**Stack Configuration**:

| LVGL Version | Required Setting | Explanation |
|--------------|------------------|-------------|
| v8 | `CONFIG_ESP_MAIN_TASK_STACK_SIZE=32768` | Font initialization runs on calling thread |
| v9 | `CONFIG_LV_DRAW_THREAD_STACK_SIZE=32768` | Font rendering runs on draw threads |

**Limitations**:
- LVGL v8: Does not support LVGL virtual filesystem (`lv_fs`), use direct file paths or memory buffers
- LVGL v9: Supports virtual filesystem paths (e.g., `"F:font.ttf"`)

### FPS Statistics and Dummy Draw

#### FPS Statistics

Requires `ESP_LV_ADAPTER_ENABLE_FPS_STATS`.

```c
// Enable FPS statistics
ESP_ERROR_CHECK(esp_lv_adapter_fps_stats_enable(disp, true));

// Get current FPS
uint32_t fps;
if (esp_lv_adapter_get_fps(disp, &fps) == ESP_OK) {
    printf("Current FPS: %lu\n", fps);
}
```

#### Dummy Draw

Dummy Draw mode bypasses LVGL rendering for direct display control:

```c
ESP_ERROR_CHECK(esp_lv_adapter_set_dummy_draw(disp, true));
ESP_ERROR_CHECK(esp_lv_adapter_dummy_draw_blit(disp, 0, 0, 800, 480, framebuffer, true));
ESP_ERROR_CHECK(esp_lv_adapter_set_dummy_draw(disp, false));
```

See `examples/display/gui/lvgl_dummy_draw` for details.

### Immediate Refresh

Force a synchronous refresh:

```c
esp_lv_adapter_refresh_now(disp);
```

### Area Rounding

Some display panels require that drawing areas must be aligned to specific boundaries (e.g., width/height must be multiples of 8). The adapter provides an area rounding callback feature that allows rounding the drawing area before flushing to hardware.

**Use Cases:**
- Panel hardware requires drawing areas to be aligned to specific boundaries (e.g., 8-pixel alignment)
- Some DMA transfers require aligned buffer sizes
- Optimize hardware transfer efficiency

**Example Code:**

```c
// Define rounding callback function (align area to multiples of 8)
void area_rounder_cb(lv_area_t *area, void *user_data)
{
    // Get alignment value (e.g., 8)
    uint32_t align = *(uint32_t *)user_data;
    
    // Round down x1 and y1
    area->x1 = (area->x1 / align) * align;
    area->y1 = (area->y1 / align) * align;
    
    // Round up x2 and y2
    area->x2 = ((area->x2 + align - 1) / align) * align;
    area->y2 = ((area->y2 + align - 1) / align) * align;
}

// Register rounding callback
uint32_t align_value = 8;  // Align to 8 pixels
ESP_ERROR_CHECK(esp_lv_adapter_set_area_rounder_cb(disp, area_rounder_cb, &align_value));

// Disable rounding callback (pass NULL)
ESP_ERROR_CHECK(esp_lv_adapter_set_area_rounder_cb(disp, NULL, NULL));
```

**Notes:**
- The callback function is called before each area flush
- The callback function should modify the passed `lv_area_t` structure to adjust area boundaries
- Pass `NULL` as the callback function to disable area rounding
- The `user_data` parameter can be used to pass alignment values or other configuration information

### Sleep Management

Safely power down displays while preserving UI state to reduce power consumption. The adapter provides a sleep mechanism that works seamlessly with ESP-IDF Light Sleep.

**Basic Flow:**

```c
// Enter sleep
esp_lv_adapter_sleep_prepare();      // Pauses worker, waits for flush
esp_lcd_panel_del(panel);             // Delete hardware
esp_light_sleep_start();              // Enter Light Sleep (CPU paused, peripherals maintained)

// Recover from sleep (executed automatically after Light Sleep wake-up)
panel = /* reinit LCD hardware */;
esp_lv_adapter_sleep_recover(disp, panel, panel_io);  // Rebinds panel, resumes worker
```

**Using with Light Sleep:**

1. **Before entering sleep**: Call `esp_lv_adapter_sleep_prepare()` to pause the adapter task and wait for current flush to complete
2. **Delete hardware**: Call `esp_lcd_panel_del()` to release display hardware resources
3. **Enter Light Sleep**: Call `esp_light_sleep_start()`, which pauses the CPU while maintaining peripheral state
4. **Recover after wake-up**: Reinitialize LCD hardware, then call `esp_lv_adapter_sleep_recover()` to resume adapter operation

**Key Features:**
- UI state preserved (no need to recreate widgets)
- Touch inputs remain registered (power down separately if needed)
- Multi-display: call `sleep_recover()` for each display
- Seamless integration with Light Sleep for low-power standby

**⚠️ Advanced:** Use `esp_lv_adapter_pause()`/`resume()` for custom flows, but do NOT mix with `sleep_prepare()`

---

## Configuration (Kconfig)

Configure via `idf.py menuconfig`:

| Option | Description |
|--------|-------------|
| `ESP_LV_ADAPTER_ENABLE_FS` | Enable filesystem bridge (`esp_lv_fs`) |
| `ESP_LV_ADAPTER_ENABLE_DECODER` | Enable image decoder (`esp_lv_decoder`) |
| `ESP_LV_ADAPTER_ENABLE_FREETYPE` | Enable FreeType vector font support |
| `ESP_LV_ADAPTER_ENABLE_FPS_STATS` | Enable FPS monitoring APIs |
| `ESP_LV_ADAPTER_ENABLE_BUTTON` | Enable navigation button input support |
| `ESP_LV_ADAPTER_ENABLE_KNOB` | Enable rotary encoder input support |

**Default Task Stack Size**:
- LVGL adapter task stack: 8KB

---

## Limitations & Notes

### Interface and Tearing Mode Support

| Interface Type | Supported Tearing Modes |
|---------------|------------------------|
| RGB / MIPI DSI | `NONE`, `DOUBLE_FULL`, `TRIPLE_FULL`, `DOUBLE_DIRECT`, `TRIPLE_PARTIAL` |
| OTHER (SPI/I2C/I80/QSPI) | `NONE`, `TE_SYNC` |

### Rotation Limitations

- **RGB/MIPI DSI**:
  - `TEAR_AVOID_MODE_NONE` **forbids rotation** (any non-zero rotation is rejected)

- **OTHER (SPI/I2C/I80/QSPI)**:
  - Adapter does not apply 90°/270° rotation
  - For rotation, configure panel orientation (swap XY/mirror) during LCD initialization
  - Must also adjust touch coordinate mapping

- **MONO (Monochrome)**:
  - **Fully supports rotation** (0°/90°/180°/270°)
  - Rotation is handled at the software level by LVGL
  - Supports both I1 horizontal tiled (HTILED) and vertical tiled (VTILED) layouts
  - No additional configuration needed, just set via the `rotation` parameter

### Buffering and Render Modes

- **Mode Mapping**:
  - `DOUBLE_FULL` / `TRIPLE_FULL` → FULL render mode
  - `DOUBLE_DIRECT` → DIRECT render mode
  - Others (incl. `TRIPLE_PARTIAL`) → PARTIAL render mode
  
- **LVGL Draw Buffer Count**:
  - `TRIPLE_PARTIAL`: 1 buffer
  - `TRIPLE_FULL` / `DOUBLE_*`: 2 buffers
  - FULL/DIRECT default: 2 buffers
  - Otherwise: 1 buffer
  
- **OTHER + NONE**:
  - No panel frame buffers requested
  - Adapter sends partial stripes from LVGL draw buffer

### PPA/DMA2D Acceleration

- With PPA acceleration, set `LV_DRAW_SW_DRAW_UNIT_CNT == 1` for best behavior
- Rotation uses PPA when possible; otherwise CPU copy with cache-friendly blocks
- **Effect**:
  - PPA reduces CPU load
  - Usually does not deliver noticeable FPS gains

---

## Deinitialization

Recommended cleanup order when shutting down UI:

```c
// 1. Unregister input devices
esp_lv_adapter_unregister_touch(touch);
// esp_lv_adapter_unregister_encoder(encoder);
// esp_lv_adapter_unregister_navigation_buttons(buttons);

// 2. Unregister display(s)
esp_lv_adapter_unregister_display(disp);

// 3. Unmount filesystem(s) (if used)
esp_lv_adapter_fs_unmount(fs_handle);
// Release mmap assets
mmap_assets_del(assets);

// 4. Deinitialize adapter
// Note: FreeType fonts are auto-cleaned if enabled
esp_lv_adapter_deinit();
```

---

## Compatibility

- **ESP-IDF**: >= 5.5
- **LVGL**: >= 8, < 10 (v8/v9 supported)

### ESP-IDF Patches

#### PPA Freeze Fix (ESP32-P4)

If you encounter display freeze when using `ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL` mode with screen rotation enabled on ESP32-P4, you need to apply the following patch:

**Target Version**: ESP-IDF release/v5.5 (commit `62beeae461bd3692c2028f96a93c84f11291e155`)

**Patch File**: `0001-bugfix-lcd-Fixed-PPA-freeze.patch`

**How to Apply**:

In your ESP-IDF repository root directory:

```bash
cd $IDF_PATH
git apply /path/to/esp_lvgl_adapter/0001-bugfix-lcd-Fixed-PPA-freeze.patch
```

**Issue Description**:
- This patch fixes a display freeze issue on ESP32-P4 when using `ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL` mode with rotation enabled, caused by PPA hardware acceleration
- Only affects scenarios that meet ALL of the following conditions:
  - Using ESP32-P4 chip
  - Using `TRIPLE_PARTIAL` tear avoidance mode
  - Screen rotation enabled (90°/270°)
- Not required for other configuration combinations or chips

**Verify Patch Applied**:

Check if `components/esp_driver_ppa/src/ppa_srm.c` contains the following code:

```c
PPA.sr_byte_order.sr_macro_bk_ro_bypass = 1;
```

---

## Examples

| Example | Path | Description |
|---------|------|-------------|
| Common demo | `examples/display/gui/lvgl_common_demo` | Unified LVGL demo supporting multiple LCD interfaces |
| Multiple displays | `examples/display/gui/lvgl_multi_screen` | Multi-display management |
| Image decoding | `examples/display/gui/lvgl_decode_image` | Image decoder showcase |
| FreeType fonts | `examples/display/gui/lvgl_freetype_font` | FreeType font loading and usage |
| Dummy Draw | `examples/display/gui/lvgl_dummy_draw` | Dummy Draw switching |

---

## Troubleshooting

### FreeType Crashes or Renders Incorrectly

**Solutions**:
- Ensure LVGL FreeType is enabled (`CONFIG_LV_USE_FREETYPE=y`)
- LVGL v8: Set `CONFIG_ESP_MAIN_TASK_STACK_SIZE=32768`
- LVGL v9: Set `CONFIG_LV_DRAW_THREAD_STACK_SIZE=32768`

### Screen Tearing or Flicker

**Possible Causes**:
- Inappropriate tearing mode
- Incorrect frame buffer count
- Improper rotation config

**Solutions**:
1. Choose appropriate tearing mode based on use case
2. Use `esp_lv_adapter_get_required_frame_buffer_count()` to compute correct frame buffer count

### Decoder or FS Not Working

**Possible Causes**:
- Kconfig options not enabled
- Component dependencies missing
- Asset partition not flashed

**Solutions**:
1. Verify corresponding Kconfig options
2. Ensure dependencies like `esp_mmap_assets` are added
3. Verify asset partition is properly packed and flashed

### LVGL Object API Crashes

**Possible Cause**:
- Accessing LVGL APIs without lock in multi-threaded environment

**Solution**:
```c
// All LVGL API calls must be protected by lock
if (esp_lv_adapter_lock(-1) == ESP_OK) {
    // LVGL API calls
    esp_lv_adapter_unlock();
}
```

---

## Glossary

| Term | Description |
|------|-------------|
| **Panel frame buffers** | Hardware frame buffers owned by the panel driver |
| **LVGL draw buffer** | RAM used by LVGL to render stripes/frames before flush |
| **Partial refresh** | Updating a sub-region (dirty area) of the screen instead of full screen |
| **Direct mode** | LVGL renders directly to the frame buffer (reduced copies; suited to small-area updates) |
| **PPA** | Pixel Processing Accelerator used for hardware-accelerated rotate/scale/memcpy-like operations |
| **Tearing** | Visual artifact when screen updates while being scanned; mitigated by buffering strategies |

---

## See Also

- [ESP LV FS Component](../../esp_lv_fs/README.md)
- [ESP LV Decoder Component](../../esp_lv_decoder/README.md)
- [ESP Mmap Assets Component](../../esp_mmap_assets/README.md)

---

## License

Apache-2.0

---

**Related Links**:
- [Component Registry](https://components.espressif.com/components/espressif/esp_lvgl_adapter)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
- [LVGL Documentation](https://docs.lvgl.io/)
