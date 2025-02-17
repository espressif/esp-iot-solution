# Touch Button Sensor

**Note:** This component is for developers testing only. It is not intended for production use.

A component providing enhanced touch button detection functionality for ESP32 series chips. It uses multiple frequency sampling (for ESP32-P4) and FSM-based processing to provide reliable touch detection even in noisy environments.

## Features

- Multi-frequency touch sampling for improved noise immunity
- FSM-based touch detection with configurable thresholds
- Debounce support for reliable state changes
- Support for multiple touch channels
- Callback-based event notification

## Dependencies

- [touch_sensor_fsm](https://components.espressif.com/components/espressif/touch_sensor_fsm)
- [touch_sensor_lowlevel](https://components.espressif.com/components/espressif/touch_sensor_lowlevel)
