# ChangeLog

## v2.1.0 - 2026-09-29

### Bugfix:

* Fix SPD2010 I2C transactions with no command/control phase by using the `-1` panel-IO command sentinel.
* Validate HDP packet lengths and avoid accessing a missing first touch point.
* Keep coordinate transformation in the common `esp_lcd_touch` API instead of duplicating it in the raw driver callback.

## v2.0.1 - 2026-04-21

### Changes:

* Add default I2C frequency to configuration macro

## v2.0.0 - 2025-12-09

### Changes:

* Migrate examples, docs and tests to esp_lcd_touch_get_data instead of the deprecated esp_lcd_touch_get_coordinates
* Raise minimum ESP-IDF requirement to v5.3

## v1.0.0 - 2024-08-12

### Enhancements:

* Component version maintenance, code improvement, and documentation enhancement

## v0.0.1 - 2023-08-21

### Enhancements:

* Implement the driver for the SPD2010 Touch controller
* Support read point on polling
* Support read point on interrupt callback
