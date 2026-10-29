# ChangeLog

## Unreleased

* **Breaking:** migrate to `touch_sensor_fsm` v1.0 and the v1.0 low-level
  complete-frame API. Slider configuration now uses absolute normalized
  `thresholds`, explicit debounce fields, and one FSM that fuses frequencies.
  The frequency argument was removed from `touch_slider_sensor_get_data()`.
* Remove the legacy per-frequency FSM, benchmark lock, and float raw-ratio
  threshold path. Position calculations now consume the positive fused
  filtered amplitude.

## v0.1.0 - 2025-04-27

* Add Initial version
