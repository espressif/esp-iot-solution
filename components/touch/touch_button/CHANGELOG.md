# ChangeLog

## v1.0.0 - 2026-09-11

* **Breaking:** `button_touch_config_t.channel_threshold` changed from a
  floating-point ratio to an absolute `uint32_t` raw-count threshold. Migrate
  values such as `0.05f` to a measured raw-count value; an initial value near
  1.5% of the 5000 startup reference is approximately 70 counts. Ratio values
  are rejected instead of being used as a threshold.
* Update the wrapper to `touch_button_sensor` v0.3.0 and ESP-IDF 5.5 or newer.

## v0.1.0 - 2025-03-18

* Add Initial version
