[![Component Registry](https://components.espressif.com/components/espressif/zero_detection/badge.svg)](https://components.espressif.com/components/espressif/zero_detection)

# Component: Zero_Detection
[Online documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/others/zero_detection.html)

The zero cross detection driver is a component designed to analyze zero cross signals. By examining the period and triggering edges of zero cross signals, it can determine the signal's validity, invalidity, whether it exceeds the expected frequency range, and if there are signal losses.

The program returns results in the form of events, meeting the user's need for timely signal processing. Additionally, it supports the analysis of two types of zero cross signals, including square waveforms and pulse types.

After creating a new zero detection object by calling function `zero_detect_create()`, the zero detection object can create many events.

Afterward, users can register the interrupt by calling function `zero_detect_register_cb()`, 

Note: The prefix `IRAM_ATTR` should be added before registering the user-written interrupt function.

List of supported events:
 * SIGNAL_FREQ_OUT_OF_RANGE
 * SIGNAL_VALID
 * SIGNAL_INVALID
 * SIGNAL_LOST
 * SIGNAL_RISING_EDGE
 * SIGNAL_FALLING_EDGE

Users have the flexibility to configure the program's drive modes, including MCPWM capture and GPIO interrupt. Furthermore, users can adjust parameters such as the effective frequency range and the number of valid signal judgments, providing a high level of flexibility.

There are two ways this driver can handle signal:
1. Analyzing and collecting signals using GPIO interrupts
2. Using GPIO for signal collection and analysis

## Add component to your project

Please use the component manager command `add-dependency` to add the `zero_detection` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/zero_detection=*"
```
