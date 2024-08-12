# ChangeLog

## v1.0.2 - 2024-08-12

### Enhancements:

* Component version maintenance, code improvement, and documentation enhancement

## v0.0.1 - 2023-08-22

### Enhancements:

* Implement the driver for the SPD2010 LCD controller
* Support SPI and QSPI interface

## v0.0.2 - 2023-10-07

### Enhancements:

* Support to use ESP-IDF release/v5.0

## v1.0.0 - 2023-11-03

### bugfix

* Fix the incompatible dependent version of ESP-IDF
* Remove LCD command `29h` from the initialization sequence
* Add parameter `max_trans_sz` in default bus configuration macro
* Check conflicting commands between initialization sequence and driver

## v1.0.1 - 2023-12-04

### bugfix

* Remove unused header `hal/spi_ll.h`
