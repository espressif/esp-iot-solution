# Touch Button Example

**Note:** This example is for developers testing only. It is not intended for production use.

This example demonstrates how to use the touch_button_sensor component to implement touch button functionality on ESP32 devices.

touch_button_sensor is an pure software FSM based touch sensor driver aims to provide an more flexible and configurable touch sensor solution. And it has more strong noise immunity.

## Overview

The touch button sensor component provides a high-level abstraction for implementing capacitive touch buttons. It handles:

- Button state detection (active/inactive)
- Debounce filtering
- Multiple frequency scanning for noise immunity
- Event-based notifications
- Configurable sensitivity and thresholds

## Hardware Required

* An ESP development board with touch sensor capability
* Touch pads or electrodes connected to touch-enabled GPIO pins
* A USB cable for power supply and programming

## How to Use Example

### Hardware Connection

Connect touch pads/electrodes to the touch-enabled GPIO pins on your ESP board. Check your board's documentation for which pins support touch sensing.

### Configuration

The example can be configured through menuconfig:

```
idf.py menuconfig
```

Under "Touch Button Sensor Configuration" you can adjust:
- Smoothing coefficient
- Baseline coefficient  
- Threshold values
- Noise filtering parameters
- Debounce settings

### Build and Flash

Build the project and flash it to the board:

```bash
idf.py build
idf.py -p PORT flash
```

Replace PORT with your serial port name.

### Example Output

The example will output touch sensor readings to the serial monitor:

```
I (320) TOUCH_SENSOR: IoT Touch Button Driver Version: 1.0.0
I (330) TOUCH_SENSOR: Starting touch sensor...
I (340) TOUCH_SENSOR: Touch channel detected: Active
...
```

## Implementation Details 

The example demonstrates:

1. Touch button sensor initialization
2. Setting up callback handlers for touch events
3. Processing touch state changes 
4. Reading touch sensor data values
5. Configuring touch sensor thresholds and filters

Key configuration parameters:

- `CONFIG_TOUCH_BUTTON_SENSOR_SMOOTH_COEF_X1000`: Smoothing coefficient for readings
- `CONFIG_TOUCH_BUTTON_SENSOR_BASELINE_COEF_X1000`: Baseline adaptation coefficient 
- `CONFIG_TOUCH_BUTTON_SENSOR_MAX_P_X1000`: Maximum positive threshold
- `CONFIG_TOUCH_BUTTON_SENSOR_MIN_N_X1000`: Minimum negative threshold

## Troubleshooting

- If touch detection is unreliable, try adjusting the threshold values
- For noisy environments, increase the noise filter parameters
- Ensure proper grounding of the touch sensor system
- Keep touch traces short and isolated from noise sources

## Additional Notes

- The touch sensor is capable of detecting both brief taps and continuous touches
- Multiple touch channels can be used simultaneously
- Sleep mode is supported for low power applications
- Hardware filtering helps reject EMI/noise

