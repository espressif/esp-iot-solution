# ChangeLog

## v0.4.0 - 2026-04-30

* Add full support for ESP-IDF 5.3+ new driver architecture
  - GPIO: Compatible with `esp_driver_gpio` component
  - I2C: Compatible with `esp_driver_i2c` component
  - LEDC: Compatible with `esp_driver_ledc` component
  - SPI: Compatible with `esp_driver_spi` component
  - Maintains backward compatibility with ESP-IDF 4.4.3+

## v0.3.2 - 2023-11-23

* Fix possible cmake_utilities dependency issue

## v0.3.1 - 2023-04-23

* SPI: fix slave mode transmission error when TX/RX buffer in invalid
* LED: fix open -> close -> open error

## v0.3.0 - 2023-04-12

* Add SPI VFS driver

## v0.2.0 - 2023-04-04

* Add LEDC VFS driver

## v0.1.0 - 2023-02-27

* Add basic component including GPIO and I2C VFS driver
