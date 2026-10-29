# ChangeLog

## v1.0.0 - 2026-06-25

### Breaking Changes:

* Reworked the servo API to use `servo_handle_t` instances instead of global `speed_mode` and channel based APIs.
* Changed `servo_config_t` to describe a single servo instance, including GPIO, LEDC speed mode, timer, and channel.

### Enhancements:

* Added `SERVO_CONFIG_DEFAULT()` to simplify common 180-degree servo configuration.
* Added LEDC timer and channel resource tracking to prevent channel reuse and timer configuration conflicts.
* Improved angle conversion accuracy by using 11-bit LEDC duty resolution and rounded duty calculation.

## v0.1.0 - 2024-11-27

### Enhancements:

* Initial version
