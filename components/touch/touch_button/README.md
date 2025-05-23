[![Component Registry](https://components.espressif.com/components/espressif/touch_button/badge.svg)](https://components.espressif.com/components/espressif/touch_button)

# Touch Button

Supports a series of operations on touch buttons, such as press, release, long press, and short press.

**Note:** This component is for developers testing only. It is not intended for production use.

ESP32/ESP32-S2/ESP32-S3 touch-related components are intended for testing or demo purposes only. Due to the poor anti-interference capability of the touch functionality, it may not pass EMS testing, and therefore, it is not recommended for mass production products.

This component is currently applicable to ESP32, ESP32-S2, and ESP32-S3, and ESP32-P4 and requires an IDF version greater than or equal to v4.4 and less than or equal to v5.4.

## Dependencies

- [touch_sensor_fsm](https://components.espressif.com/components/espressif/touch_sensor_fsm)
- [touch_sensor_lowlevel](https://components.espressif.com/components/espressif/touch_sensor_lowlevel)
- [touch_button_sensor](https://components.espressif.com/components/espressif/touch_button_sensor)

## Add component to your project

Please use the component manager command `add-dependency` to add the `touch_button` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/touch_button=*"
```
