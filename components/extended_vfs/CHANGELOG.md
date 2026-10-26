# ChangeLog

## v0.4.0 - 2026-04-30

* Add ESP-IDF 5.3+ new driver architecture for GPIO / I2C master / LEDC / SPI
  - GPIO: Compatible with `esp_driver_gpio` component
  - I2C master: Compatible with `esp_driver_i2c` component (see I2C slave notes below)
  - LEDC: Compatible with `esp_driver_ledc` component
  - SPI: Compatible with `esp_driver_spi` component
  - ESP-IDF < 5.3 keeps the legacy `driver` path; 5.3+ I2C slave has additional requirements (not full drop-in for every prior slave use)
* I2C (5.3+): fix transfer timeout unit (ms), ACK-check mapping, 10-bit address rejection
* I2C (5.3+): map `I2C_SDA_PULLUP` / `I2C_SCL_PULLUP` for master and slave v2 (driver single pull-up flag, then per-pin GPIO restore)
* I2C (5.3+): slave send/receive buffer depth; sync slave RX via callback; support slave driver v2 / IDF 6.x
* I2C (5.3+): only clear bus/device handles after successful delete; retry busy deletes and allow next `open()` to finish a failed `close()` cleanup (avoids permanent `EBUSY` after VFS drops the FD); never delete the master bus while a device handle remains (IDF 5.3/5.4); per-port mutex serializes open/close/ioctl cleanup
* I2C (5.3+): restore `I2C_MSG_NO_START` / `I2C_MSG_NO_END` via `i2c_master_execute_defined_operations()`
* I2C: custom transaction boundaries / zero-length write probe require IDF >= 5.3.4 (5.3.x), >= 5.4.1 (5.4.x), or >= 5.5.0; earlier patches in those lines return `ENOTSUP`
* I2C (5.3+): custom-ops path emits correct 10-bit address framing; `NO_START` with 10-bit returns `ENOTSUP`
* I2C (5.3+): slave reads capped to `I2C_SLAVE_BUF_DEPTH` so timeout can restore all consumed bytes
* I2C (5.3+): slave requires driver v2 (`CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2` or IDF >= 6.0); without it slave config (`I2CIOCSCFG`) and transfer return `ENOTSUP` (v1 has no RX length; no legacy `driver/i2c` mix with 5.3+ master API). v2 uses ring hold with length from `on_receive`. IDF 5.3.x has no workable slave migration: upgrade to IDF >= 5.4 with v2 enabled, or IDF >= 6.0 (pinning `extended_vfs` < 0.4.0 does not help — older releases also failed slave install with zero RX/TX buffers)
* I2C (5.3+): v2 slave RX hold is a ring buffer so ISR append and ioctl consume do not memmove over each other; pending overflow is reported as `EOVERFLOW` even when the ring still has readable bytes
* I2C (IDF < 5.3): legacy slave `i2c_driver_install` now passes non-zero RX/TX buffer depths so slave config can succeed under IDF's `> 100` check

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
