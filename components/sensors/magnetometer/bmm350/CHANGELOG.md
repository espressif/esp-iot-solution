# ChangeLog

> For changes to BMM350_SensorAPI itself,
> please see https://github.com/boschsensortec/BMM350_SensorAPI/commits/main/

## v1.1.0 - 2026-09-10

* Add `bmm350_set_i2c_master_bus_handle()` for native I2C master buses.
* Remove the previous device on bus switch; block transfers until cleanup succeeds.

## v1.0.0 - 2026-09-02

* Initial version, based on BMM350_SensorAPI v1.4.0.
* ESP platform I2C support via `common/*`.
* Support ESP-IDF v5.3 and later.
