# ChangeLog

## v2.0.0 - 2025-10-29

### Changes:

* Compatible with ESP-IDF v6.0

## v1.0.0 - 2024-08-12

### Enhancements:

* Component version maintenance, code improvement, and documentation enhancement

## v0.4.1- 2024-05-09

### Enhancements:

* Upload the driver to the espressif repository
* Update test_apps
* use `xPortInIsrContext()` to check whether the function is called in the ISR

### bugfix

* Fix the non-reentrant of the `lcd_write_cmd` function

## v0.4.0- 2024-01-30

### Enhancements:

* Retain the RGB interface driver in the esp-iot-solution repository, and keep only the QSPI interface driver in this repository

## v0.3.1 - 2023-11-03

### bugfix

* Fix the missing check of flags `auto_del_panel_io` and `mirror_by_cmd` for RGB interface
* Remove LCD command  `29h` from the initialization sequence for QSPI interface

## v0.3.0 - 2023-09-08

### Enhancements:

* Implement the driver for the ST77903 LCD controller
* Support QSPI and RGB interface
