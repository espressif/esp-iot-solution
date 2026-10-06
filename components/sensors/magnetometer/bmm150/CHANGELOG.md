# ChangeLog

> For changes to BMM150_SensorAPI itself,
> please see https://github.com/boschsensortec/BMM150_SensorAPI/commits/master/

## v1.1.0 - 2026-09-29

* Add `bmm150_interface_init_from_master_bus()` (caller selects the device clock) and `bmm150_interface_init_from_i2c_bus()`; several sensors can share one bus.
* Add `bmm150_interface_deinit_device()`; on a removal error the context is kept for a retry and transfers stay blocked.
* Keep the file-level setters and `bmm150_interface_init()` for single-sensor use.

## v1.0.1 - 2026-09-02

* Fix `bmm150_delay` so the wait is never shorter than requested at any FreeRTOS tick rate.
* AUX adapter uses `bmm150_delay` instead of forwarding to `bmi2_delay_us`.
* `REQUIRES` now lists `i2c_bus` and `esp_timer`.

## v1.0.0 - 2026-09-01

* Initial version, based on BMM150_SensorAPI v2.0.0.
* ESP platform I2C support via `common/*`.
* BMI270 AUX support via `bmm150_aux_adapter.*` when the project already depends on `espressif/bmi270_sensor` (>= 0.2.1). Direct I2C does not pull BMI270.
* Support ESP-IDF v5.3 and later.
