## v0.2.2 - 2026-11-11

### Bug Fixes:

* Fix BL0937 accumulated energy calculation to apply the same `kp` calibration factor as active power.
* Fix missing overcurrent, overvoltage, and undervoltage detection on voltage/current reads, with warning logs and optional event callbacks registered through `power_measure_set_event_callback()` for BL0937, BL0942, and INA236, preserving the original common configuration layout.

## v0.2.1 - 2025-10-16

### Enhancement:

* Add BL0942 chip support for power_measure component

## v0.2.0 - 2025-9-5

### Improve:

* Integrate power measurement equipment into a unified management component.

## v0.1.1 - 2024-12-10

### Enhancements:

* Modified the formatting of the document
* Added more detailed documentation
* Optimised some code

## v0.1.0 - 2024-11-5

### Enhancements:

* Initial version
* Support BL0937 chip to measure voltage, current, energy
