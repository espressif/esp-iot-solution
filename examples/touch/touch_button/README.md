# Touch Button Example

**Note:** This example is for developers testing only. It is not intended for production use.

This article demonstrates how to use `iot_button` to detect touch interactions.
The example showcases how to differentiate between light and firm presses on
the same touch channel.

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

The light and firm thresholds in `main/touch_button_main.c` are absolute raw
counts and start near 1.5% of the 5000 reference (about 70). On hardware,
lowlevel `start()` automatically aligns the first configured channel to
approximately 5000 before button processing. Keep the startup electrodes
untouched and the board stable, then tune the constants for the board and
electrode after observing the measured amplitude.

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
I (358) touch_lowlevel: Touch sensor lowlevel started
I (5028) touch_button: Light Button: BUTTON_PRESS_DOWN
I (5068) touch_button: Heavy Button: BUTTON_PRESS_DOWN
I (5358) touch_button: Heavy Button: BUTTON_PRESS_UP
I (5388) touch_button: Light Button: BUTTON_PRESS_UP
...
```
