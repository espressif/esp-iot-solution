# ChangeLog

## v2.0.1 - 2025-12-10

### Bugfix:

* Fix draw_bitmap not propagating tx_color errors, preventing system deadlock on SPI transmission failures

## v2.0.0 - 2025-10-29

### Changes:

* Compatible with ESP-IDF v6.0

## v1.0.0 - 2024-08-12

### Enhancements:

* Component version maintenance, code improvement, and documentation enhancement

## v0.0.1 - 2023-08-22

### Enhancements:

* Implement the driver for the SH8601 LCD controller
* Support SPI and QSPI interface

## v0.0.2 - 2023-10-07

### Enhancements:

* Support to use ESP-IDF release/v5.0

## v1.0.0 - 2023-11-03

### bugfix

* Fix the incompatible dependent version of ESP-IDF
* Remove LCD command `29h` from the initialization sequence
* Increase `pclk_hz` to 40MHz in default panel IO configuration
* Add parameter `max_trans_sz` in default bus configuration macro
* Add check for conflicting commands between initialization sequence and driver
