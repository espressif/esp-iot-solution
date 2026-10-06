# ChangeLog

> For changes to BME690_SensorAPI itself,
> please see https://github.com/boschsensortec/BME690_SensorAPI/commits/master/

## v1.1.0 - 2026-09-03

* Fix `bme69x_delay_us` so the wait is never shorter than requested at any FreeRTOS tick rate.
* `REQUIRES` now lists `i2c_bus`, `esp_driver_spi`, and `esp_timer`.
* Adapt `test_apps` for ESP-IDF v6.0.
* Recreate the I2C device handle if `bme69x_interface_init()` is called again.
* Add `bme69x_set_i2c_address()` for 0x76 / 0x77. I2C device creation inherits the bus clock.

## v1.0.3 - 2026-01-05

* Initial version, based on BME690_SensorAPI v1.0.3 [`eed112e4`](https://github.com/boschsensortec/BME690_SensorAPI/tree/eed112e49c5a4a9bd104fd952aa15c00f1fb265a).
