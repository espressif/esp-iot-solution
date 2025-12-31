# ChangeLog

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
