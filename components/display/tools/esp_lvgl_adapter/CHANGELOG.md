# ChangeLog

## v0.5.2 (2026-06-09)

* Add LVGL v9 PPA acceleration for RGB888 displays: ARGB8888 per-pixel alpha blend and opaque fill paths; opaque RGB565/RGB888 images fall back to CPU to preserve tiled image rendering
* Fix framebuffer cache sync to use the destination pixel size (2 for RGB565, 3 for RGB888) instead of `sizeof(lv_color_t)`
* Guard PPA source bounds on the custom-handler path to avoid reads outside the source image
* Add bypass mode for adapter-managed ESP-IDF LCD and touch callbacks with public `notify_from_isr` entry points
* Fix TE vsync wait deadlock by replacing `portMAX_DELAY` with a bounded timeout (6× TE period, clamped to [20ms, 1000ms])
* Fix stale DMA completion race by moving `ulTaskNotifyValueClear()` before `esp_lcd_panel_draw_bitmap()`
* Add `CONFIG_ESP_LVGL_ADAPTER_FREETYPE_MINIMAL_BUILD` to reduce FreeType flash footprint
* Fix IRAM coverage of `from_isr` display and touch notification call chain
* Add LVGL v9 multi-control touch mode for independent control of discrete widgets, plus parameter validation and cleanup handling updates

## v0.5.1 (2026-05-21)

* Add optional FreeType render pool reduction for lower stack usage
* Add optional LVGL FreeRTOS thread stack allocation in PSRAM
* Relax LVGL v9's conservative 32KB FreeType draw-stack diagnostic when the small render pool is enabled
* Simplify optional dependency linking with `idf_component_optional_requires`

## v0.5.0 (2026-05-09)

* Add draw bitmap and touch callback hooks for external display/input adaptation
* Improve cache synchronization for accelerated display paths
* Update the PPA hang workaround patch and documentation for ESP-IDF `tags/v6.0`

## v0.4.3 (2026-04-03)

* Fix `pause()` / `resume()` handling so the internal `LVGL tick` timer is stopped during pause and restarted during resume
* Update sleep management documentation for tickless auto Light Sleep usage

## v0.4.2 (2026-03-31)

* Add `ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_PARTIAL` for double-buffer partial refresh
* Add `ESP_LVGL_ADAPTER_PARTIAL_AUX_IMG_CACHE` to increase LVGL image cache in partial tear-avoid modes

## v0.4.1 (2026-03-12)

* Fix refresh issue when LV_DRAW_BUF_STRIDE_ALIGN > 1 in LVGL v9 bridge
* Fix resource leaks and use-after-free risks in deinit path (flush-wait, task stack, VSync callbacks, lv_deinit)

## v0.4.0 (2026-02-11)

* Add multi-touch adaptation support
* Add ESP-IDF v6.0 compatibility updates

## v0.3.2 (2026-01-22)

* Fix C++ designated initializer order in SPI-with-PSRAM display profile macros

## v0.3.1 (2026-01-07)

* Fix monochrome SPI default display macro to use the correct profile macro

## v0.3.0 (2026-01-07)

* Added monochrome display support for LVGL v8 and v9
* Add migration guide from esp_lvgl_port to esp_lvgl_adapter
* Reverted conditional dependency management

## v0.2.0 (2025-12-24)

* Optimized dependency management by adding conditional rules for optional dependencies

## v0.1.4 (2025-12-22)

* Fixed the LVGL naming compatibility dependency issue

## v0.1.3 (2025-12-16)

* Fixed the issue with 180-degree rotation

## v0.1.2 (2025-12-12)

* Prevent deadlock when pausing the adapter from the LVGL worker

## v0.1.1 (2025-12-10)

* Improved the screen update failure handling mechanism

## v0.1.0 (2025-11-17)

* Add a sleep mechanism compatible with Light Sleep
* Add a TE-based anti-tearing mechanism

## v0.1.0-beta.2 (2025-11-4)

* Fix a crash that occurred in some cases when PPA acceleration was enabled

## v0.1.0-beta.1 (2025-10-27)

* Support for LVGL v8.x and v9.x with unified API
* Unified display registration for MIPI DSI, RGB, QSPI, SPI, I2C, I80 interfaces
* Multiple buffering modes and screen rotation support (0°/90°/180°/270°)
* Tearing effect avoidance for RGB and MIPI DSI displays
* PPA hardware acceleration support for ESP32-P4
* Thread-safe LVGL access with lock/unlock APIs
* Input device support: touch screen, button navigation, rotary encoder
* Optional filesystem bridge integration (esp_lv_fs)
* Optional image decoder support (esp_lv_decoder)
* Optional FreeType font rendering support
* FPS statistics and Dummy Draw mode
* API naming aligned with esp_lv_decoder and esp_lv_fs for consistency
