# ChangeLog

## v0.3.0 - 2026-9-1

### Enhancements:

* Add standby state detection, capacity stays at 100% when the battery is full and still charging
* Add `adc_battery_estimation_get_voltage` to get the filtered battery voltage

### Bug Fix:

* Fix resource leaks on the create and destroy error paths

## v0.2.2 - 2026-6-9

### Bug Fix:

* Fix ADC bitwidth validation for SoCs using SOC_ADC_DIGI_* macros (e.g. ESP32-H4).

## v0.2.1 - 2025-10-9

### Bug Fix:

* Fix ADC internal/external judge via unit & bitwidth (avoid union issue)

## v0.2.0 - 2025-6-18

* Add software estimation for charging state

## v0.1.0 - 2025-5-16

### Enhancements:

* Initial version.
