# ChangeLog

## v1.1.5 - 2024-05-10

### Improvement:

* Add the configuration option to enable print very little log with ESP_LOGW.

## v1.1.4 - 2024-05-07

### Bug Fixes:

* Reconfigure KP18052 parameters
* When the IIC dimming gray scale data size is 0, skip sending the data and only send the address.

## v1.1.3 - 2024-04-24

### Bug Fixes:

* Fix KP18058 chip current data range error

## v1.1.2 - 2024-02-05

### Bug Fixes:

* fix(lightbulb): incorrect type.

## v1.1.1 - 2024-02-04

### Bug Fixes:

* Add CCT data range check.
* Fixed the issue of setting interrupt_forbidden to true but the effect is still interrupted.

## v1.1.0 - 2024-01-30

### Enhancements:

* Added the conversion function of IIC current values to the enumeration values required by the driver.
* Added driver layer parameter checking macro and allowed to configure the detail level of the output log through menuconfig.

## v1.0.0 - 2024-01-16

### Enhancements:

* The driver provides LED bead combination options and supports different types of lightbulbs.
* Added feature to count IIC driver transmission failures, aiding in hardware health monitoring.
* Discontinued support for SM2135E driver, removing related files and code.
* Updated BP5758D driver files to BP57X8D for consistency with SM2X35EGH.
* The driver now supports setting white balance parameters for all channels. This means that you can adjust the white balance individually for each channel, allowing for precise control and customization of the lighting output.
* Increased the power limit range from 100-300 to 100-500, facilitating power setting for RGBWW beads.
* Introduced CCT output modes: standard mode (consistent with original scheme, mapping percentage to Kelvin values proportionally) and precise mode (setting individual percentages for each Kelvin value and predetermining current coefficients for each channel, scaling these coefficients proportionally during writing).
* Some configuration changes: 
  * Removed mode_mask from capability, now using led_beads to set the LED panel bead combination
  * Renamed enable_mix_cct to enable_hardware_cct, fades_ms to fade_time_ms, and enable_fades to enable_fade
  * Removed hardware_monitor_cb

## v0.5.5 - 2024-01-15

### Bug Fixes:

* Update conditions for CI tests

## v0.5.4 - 2024-01-12

### Bug Fixes:

* fix: stack overflow in task fade_tick_task on xtensa architecture

## v0.5.3 - 2024-01-10

### Bug Fixes:

* fix: function declaration isn't a prototype [-Werror=strict-prototypes]

## v0.5.2 - 2023-10-28

### Enhancements:

* Update LEDC low power code logic.
* Modify the test cases and use the pytest tool for real hardware testing.

## v0.5.1 - 2023-10-26

### Bug Fixes:

* Fix phase delay for pwm cw channel.

## v0.5.0 - 2023-10-09

### Enhancements:

* Add a phase delay function to the PWM drive, and after enabling this function, all channel outputs will be in a complementary state.

## v0.4.1 - 2023-8-30
### Bug Fixes:

* Fixed a thread safety issue.
* Fixed fade direction being reversed.

## v0.4.0 - 2023-7-10

### Enhancements:

* The PWM driver will update the timer resolution based on the incoming PWM frequency to accommodate a wider range of frequencies.
* Remove the blocking check of the timer status in `lightbulb_basic_effect_start` to allow for subsequent calls within the callback.

## v0.3.3 - 2023-7-04

### Bug Fixes:

* Fixed a compilation problem caused by a macro naming error.

## v0.3.2 - 2023-6-02

### Bug Fixes:

* Fixed a thread safety issue

## v0.3.1 - 2023-5-15

### Enhancements:

* Add an active flag to gptimer to reduce invalid error print in the IDF gptimer driver

### Bug Fixes:

* Fixed a wrong error on kelvin range

## v0.3.0 - 2023-5-8

### Enhancements:

* Provide an option to use gptimer instead of esptimer to generate ticks for the fade process

### Bug Fixes:

* Fixed a wrong conversion on `percentage_convert_to_kelvin`

## v0.2.2 - 2023-3-10

### Bug Fixes:

* Fixed some wrong data types
* Provides configuration options for the iic task instead of hardcoding
* Added null pointer judgment to effect timer

## v0.2.1 - 2023-2-21

### Bug Fixes:

* Provide a workaround solution to temporarily solve the issue that LEDC hardware fade API calls are blocked
  Please read some description of the macro PWM_ENABLE_HW_FADE in Kconfig
* Effect interrupt_forbidden flag will allow to set when total_ms == 0

## v0.2.0 - 2023-2-8

### Enhancements:

* Add interrupt_forbidden flag in effect
* Provide Yxy color model conversion function

### Bug Fixes:

* Fix nvs structure naming error

## v0.1.0 - 2023-1-11

### Enhancements:

* Support running on ESP32-C6

### Bug Fixes:

* Fix kelvin 2 percentage
* Fix effect timer time
* Modify the logic of low power mode entry and exit

## v0.0.9 - 2022-12-28

### Enhancements:

* Provide an effect timer

## v0.0.8 - 2022-12-22

### Enhancements:

* Provide new application layer capability: enable/disable auto-open

## v0.0.7 - 2022-12-15

### Enhancements:

* Support running on ESP32-H2

## v0.0.6 - 2022-12-05

### Enhancements:

* Support running on ESP32-C2
* Provide PWM signal invert

### Bug Fixes:

* Fix LEDC output is disabled after the software reset

## v0.0.5 - 2022-12-02

### Enhancements:

* Support compiling with IDF 5.0

### Bug Fixes:

* Fix some drivers being power halved in white light mode
* Fix some configuration errors in the demo

## v0.0.4 - 2022-11-10

### Enhancements:

* Support BP1658CJ IIC dimming chip

## v0.0.3 - 2022-10-26

### Enhancements:

* Support KP18058 IIC dimming chip

## v0.0.2 - 2022-10-26

### Enhancements:

* Support running on ESP32-S3

### Bug Fixes:

* Fix API `lightbulb_rgb2hsv` conversion error
* Update some typo
  * `basis` -> `basic`
  * `mix` -> `min`

## v0.0.1 - 2022-8-29

### Enhancements:

* Initial version

* The following dimming solutions are supported
  * PWM
    * RGB + CW
    * RGB + CCT/Brightness
  * IIC dimming IC
    * SM2135E
    * SM2135EH
    * SM2235EGH
    * SM2335EGH
    * BP5758/BP5758D/BP5768D
  * Single Line
    * WS2812

* Support for power limit
* Support for color calibration
* Support for effect
  * Blink
  * Breathe
* Support for application layer capability configuration
  * Status memory
  * Fade
  * Mix CCT
  * Low-power
  * Light mode select
  * Sync change
