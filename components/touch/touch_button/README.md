[![Component Registry](https://components.espressif.com/components/espressif/touch_button/badge.svg)](https://components.espressif.com/components/espressif/touch_button)

# Touch Button

Supports a series of operations on touch buttons, such as press, release, long press, and short press.

**Note:** This component is for developers testing only. It is not intended for production use.

Touch-related components are intended for testing or demo purposes only. Due to
the sensitivity of touch hardware to board and environmental conditions, verify
the behavior on the target board before using it in a product.

ESP-IDF 5.5 and 6.0 support ESP32-S2, ESP32-S3, and ESP32-P4 for this
component. ESP-IDF 6.1 also supports ESP32-H4 and ESP32-S31.

Version 1.0 has a breaking threshold migration: `button_touch_config_t.channel_threshold`
is now a `uint32_t` absolute, unscaled raw-count threshold. Replace old values
such as `0.05f` with a measured raw-count value before creating the button;
old ratio values convert to zero and are rejected by the new argument check.
As an initial value, use about 1.5% of the startup reference (approximately
70 counts when the reference is 5000), then tune with the amplitude reported by
`touch_button_sensor`. On hardware, lowlevel `start()` automatically aligns the
first configured channel to approximately 5000 before the button begins
processing samples. Keep that electrode untouched and the board stable during
startup; a startup failure is returned by lowlevel `start()`.

## Dependencies

- [touch_sensor_fsm](https://components.espressif.com/components/espressif/touch_sensor_fsm)
- [touch_sensor_lowlevel](https://components.espressif.com/components/espressif/touch_sensor_lowlevel)
- [touch_button_sensor](https://components.espressif.com/components/espressif/touch_button_sensor)

## Add component to your project

Please use the component manager command `add-dependency` to add the `touch_button` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/touch_button=*"
```
