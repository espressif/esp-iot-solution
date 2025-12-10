# ChangeLog

## v2.0.2 - 2025-12-10

### Bugfix:

* Fix draw_bitmap not propagating tx_color errors, preventing system deadlock on SPI transmission failures

## v2.0.1 - 2025-12-10

### Changes:

* Update docs and tests to use esp_lcd_touch_get_data instead of the deprecated esp_lcd_touch_get_coordinates

## v2.0.0 - 2025-10-29

### Enhancements:

- Compatible with ESP-IDF v6.0

## v1.0.1 - 2025-03-09

### bugfix:

- Fixed an issue where using i2c_master could not set scl_speed_hz resulting in an invalid scl frequency error

## v1.0.0 - 2024-02-29

### Enhancements:

- Implement the driver for the AXS15231B LCD controller
