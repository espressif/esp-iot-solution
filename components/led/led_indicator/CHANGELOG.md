# ChangeLog

## v2.0.1 - 2025-8-12

### Bugfix

* Fix: For LEDs driven by the LEDC driver with active-low enable, use the LEDC inversion function to avoid the LED turning on immediately after creation due to an initial low-level output.

## v2.0.0 - 2025-7-30

### Improve:

* Refactor with Factory Pattern
* Update the version of `led_strip` to v3.0
* Add Xiaomi Blink support

## v1.1.1 - 2025-03-11

### Bugfix

* Fix: Scale RGB values (0-255) to PWM duty range (0-8191)

## v1.1.0 - 2025-02-24

### Improve:

* Update the version of dependent cmake_utilities to *

## v1.0.0 - 2024-8-14

* Publish the official version

## v0.9.4 - 2024-10-28

* make set brightness can set index. [!414](https://github.com/espressif/esp-iot-solution/issues/414)

## v0.9.3 - 2024-6-20

* Setting light brightness perform gamma correction.
* Resolved the issue of incorrect brightness save. [!376](https://github.com/espressif/esp-iot-solution/issues/376)

## v0.9.2 - 2024-2-4

* Fixed test_led_indicator.c build in C++ error
* Upgraded the display effect of color gradient.

## v0.9.1 - 2024-1-17

* Resolve the issue of calling preempt_stop with a specified blink_type that is not running, causing the current preemptive blink to stop.
* Resolve the issue of the RGB ring not functioning.
* Ensure that preempt_start stops the previous preemptive blink.
* Add examples

## v0.9.0 - 2023-11-28

### BUG FIX

* Resolve the error with 'is_active_level_high' in LEDC RGB type.

### Break Change

* Remove `led_indicator_strips_config_t:is_active_level_high`

* Remove `led_indicator_get_handle`, the query of 'led_indicator handle' through GPIO number and LEDC channel is no longer supported.

## v0.8.0 -2023-11-21

### Enhancements:

* Supports RGB lights with three beads

## v0.7.2 - 2023-11-23

* Fix possible cmake_utilities dependency issue

## v0.7.1 - 2023-10-15

### BUG FIX

* Fix set HSV hue color.

### Enhancements:

* Added API for set color by temperature.

## v0.7.0 - 2023-11-09

### Enhancements:

* Renamed function `get_current_fade_value` to `get_brightness`.

* Added get functions for color (in RGB and HSV color format).

## v0.6.1 - 2023-10-31

### BUG FIX

* Fix preempt blink not stop as expected.

### Enhancements:

* Support LED RGB type by driver [led_strip](https://components.espressif.com/components/espressif/led_strip)

* Add api to control led indicator's brightness, HSV color, RGB color in real time

* Support change led indicator's color by type HSV and RGB.

* Support color gradient modification based on the color ring.

## v0.5.1 - 2023-10-23

### BUG FIX

* Fix the issue that led_indicator is destroyed but timer is still running.

## v0.5.0 - 2023-10-7

### Enhancements:

* Simplified the initialization process for LEDC.

### BUG FIX

* Resolved the blink issue in the breathing mode.

## v0.4.0 - 2023-4-12

### Enhancements:

* LED INDICATOR:
  * Add gamma adjustment
  * Update test_apps

## v0.3.0 - 2023-1-31

### Enhancements:

* LED INDICATOR:
  * Support customer mode: can customize multiple drivers
  * Support create LED by LEDC driver
  * Support breathe blink
  * Support set brightness
  * Support jump in line

## v0.2.0 - 2022-12-28

### Enhancements:

* LED INDICATOR:
  * Support customer-defined blink step
  * Support custom blink type