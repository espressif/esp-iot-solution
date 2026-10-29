# BMM350 Sensor Component for ESP-IDF

Vendored Bosch Sensortec BMM350 magnetometer SensorAPI, with a thin ESP-IDF
I2C platform glue layer built on this repository's `i2c_bus` component.

## Provenance

- `bmm350.c`, `include/bmm350.h`, `include/bmm350_defs.h`: unmodified core
  driver from https://github.com/boschsensortec/BMM350_SensorAPI (v1.10.0),
  licensed under BSD-3-Clause (see `LICENSE`).
- `bmm350_api.c`, `include/bmm350_api.h`: ESP-IDF I2C platform glue
  (`bmm350_i2c_read` / `bmm350_i2c_write` / `bmm350_delay` /
  `bmm350_interface_init`), adapted from the SensorAPI's own
  `examples/common` HAL and from Espressif's esp-dev-kits
  `esp-sensairshuttle/examples/factory_demo` Compass app, retargeted onto
  `i2c_bus_device_create/read_bytes/write_bytes` instead of COINES.

## Usage

This component only provides the low-level driver and I2C glue. The
airmouse-specific handle/API (address probing at 0x14/0x15, ODR/axis/power
configuration, coordinate-frame conversion) lives in
`demo/airmouse/main/airmouse_bmm350.cpp` and is gated by
`CONFIG_AIRMOUSE_ENABLE_BMM350`.
