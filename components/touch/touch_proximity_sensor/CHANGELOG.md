# ChangeLog

## Unreleased

* **Breaking:** migrate to `touch_sensor_fsm` v1.0 complete-frame input.
  Proximity configuration now uses absolute normalized `thresholds`, explicit
  debounce fields, and optional sampling settings.
* Preserve the independent proximity low-level data type and accumulated
  measurement period; ordinary touch 5000-count alignment and frequency
  estimation are not used for proximity channels.

## v0.2.0 - 2025-1-20

### Break changes:

* Simplify the touch proximity sensor configs

### Enhancements:

* Refactor using esp_touch_fsm component

## v0.1.2 - 2024-6-7

* Add user debugging guide documentation for touch proximity sensor
* Change the parameter `response_ms` to `meas_count`

## v0.1.1 - 2024-5-14

### Enhancements:
* Update the touch proximity sensor API:
  * Add touch proximity sensor create/delete APIs
  * Update touch proximity sensor start/stop APIs
* Add test_apps and doc

## v0.1.0 - 2022-9-8

### Enhancements:
* Add touch proximity driver & example
