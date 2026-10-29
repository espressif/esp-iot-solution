# Changelog

## v1.1.0 - 2026-6-10

### Enhancements

* **IEC 62386-209 (Part 209)**: DT8 color control support
  * RGB, CCT (Tc), XY chromaticity, and primary color modes

* **IEC 62386-103 (Part 103)**: Input device commissioning and control

* **IEC 62386-303 (Part 303)**: Occupancy sensor support

* **IEC 62386-304 (Part 304)**: Light sensor support

* **dali_basic example**: Dynamic DT6/DT8 detection

* **dali_basic example**: Simultaneous blink demo
  * DT6 lamps: all blink together (bright/dark) when occupancy detected
  * DT8 lamp: alternate Red/Blue when occupancy detected

## v1.0.1 - 2026-6-3

### Bug Fixes:

* Fixed DALI RMT receive channel configuration issue.

## v1.0.0 - 2026-5-9

### Enhancements:

* Initial release of the DALI (IEC 62386) bus master driver based on the ESP32 RMT peripheral.
