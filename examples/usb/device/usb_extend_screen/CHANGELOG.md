# ChangeLog

## v0.3.1 2026-8-5

### Enhancements:

* Decode JPEG frames to the RGB565 or RGB888 framebuffer format selected by the Board Manager display configuration

## v0.3.0 2026-7-29

### Enhancements:

* Migrate display, touch, backlight, and audio initialization to ESP Board Manager
* Use the common Board Manager device names so compatible ESP32-P4, ESP32-S31, and ESP32-S3 boards can share the application code
* Remove the example-local BSP copies
* Handle hardware JPEG MCU padding independently of the Board Manager display resolution
* Select the RGB565 byte order from the Board Manager display configuration

## v0.2.0 2026-5-25

### Enhancements:

* Support ESP32-S31 with ESP32-S31-Korvo board (800x480@60FPS)
* Refactor esp32_p4_function_ev_board BSP

## v0.1.0 

* Init version
