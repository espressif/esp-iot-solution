# Touch Button Sensor Example

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

Under "Touch Button Sensor Configuration" you can adjust numerous parameters:

- `TOUCH_BUTTON_SENSOR_DEBUG`: Enable debug mode for detailed logging
- `TOUCH_BUTTON_SENSOR_CALIBRATION_TIMES`: Number of readings for initial calibration
- `TOUCH_BUTTON_SENSOR_DEBOUNCE_INACTIVE`: Debounce count for inactive state detection
- `TOUCH_BUTTON_SENSOR_POLLING_INTERVAL`: Interval between polling touch readings (ESP32 only)
- `TOUCH_BUTTON_SENSOR_SMOOTH_COEF_X1000`: Smoothing coefficient for readings stability
- `TOUCH_BUTTON_SENSOR_BASELINE_COEF_X1000`: Baseline adaptation coefficient for drift compensation
- `TOUCH_BUTTON_SENSOR_MAX_P_X1000`: Maximum positive change ratio threshold
- `TOUCH_BUTTON_SENSOR_MIN_N_X1000`: Minimum negative change ratio threshold
- `TOUCH_BUTTON_SENSOR_NEGATIVE_LOGIC`: Enable negative logic for touch detection
- `TOUCH_BUTTON_SENSOR_NOISE_P_SNR`: Signal-to-noise ratio for positive noise filtering
- `TOUCH_BUTTON_SENSOR_NOISE_N_SNR`: Signal-to-noise ratio for negative noise filtering
- `TOUCH_BUTTON_SENSOR_RESET_COVER`: Reset count threshold for cover detection
- `TOUCH_BUTTON_SENSOR_RESET_CALIBRATION`: Reset count for calibration error recovery
- `TOUCH_BUTTON_SENSOR_RAW_BUF_SIZE`: Size of raw data buffer for processing
- `TOUCH_BUTTON_SENSOR_SCALE_FACTOR`: Scale factor for threshold calculations

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
I (320) TOUCH_SENSOR: IoT Touch Button Driver Version: 0.2.0
I (330) TOUCH_SENSOR: Starting touch sensor...
I (340) TOUCH_SENSOR: Touch channel detected: Active
...
```

## Implementation Details

The example demonstrates:

1. Touch button sensor initialization
2. Configuring touch sensor thresholds and filters
3. Setting up callback handlers for touch events
4. Creating a dedicated task for processing touch events
5. Processing touch state changes in the background

Key configuration parameters:

- `CONFIG_TOUCH_BUTTON_SENSOR_SMOOTH_COEF_X1000`: Smoothing coefficient for readings (x1000)
- `CONFIG_TOUCH_BUTTON_SENSOR_BASELINE_COEF_X1000`: Baseline adaptation coefficient (x1000)
- `CONFIG_TOUCH_BUTTON_SENSOR_MAX_P_X1000`: Maximum positive threshold (x1000), to avoid false positives
- `CONFIG_TOUCH_BUTTON_SENSOR_MIN_N_X1000`: Minimum negative threshold (x1000), to avoid false negatives
- `CONFIG_TOUCH_BUTTON_SENSOR_NOISE_P_SNR`: Signal-to-noise ratio for positive noise
- `CONFIG_TOUCH_BUTTON_SENSOR_NOISE_N_SNR`: Signal-to-noise ratio for negative noise
- `CONFIG_TOUCH_BUTTON_SENSOR_CALIBRATION_TIMES`: Initial calibration readings count

## Troubleshooting

- If touch detection is unreliable, try adjusting the threshold values
- For noisy environments, increase the SNR values and debounce count
- Ensure proper grounding of the touch sensor system
- Keep touch traces short and isolated from noise sources
- If experiencing calibration issues, adjust the calibration times or reset parameters

## Tuning Tools

* Enable the `Enable touch button sensor debug mode` option in menuconfig
* Install the tptool for threshold tuning, run `pip install tptool` in the terminal
* Run `python -m tptool` to start the tuning tool



## Additional Notes

- The touch sensor is capable of detecting both brief taps and continuous touches
- Multiple touch channels can be used simultaneously
- Sleep mode is Not supported for low power applications
- The dedicated task handling ensures responsive touch detection without blocking the main application

