[![Component Registry](https://components.espressif.com/components/espressif/keyboard_button/badge.svg)](https://components.espressif.com/components/espressif/keyboard_button)

# Component: Keyboard Button
[Online documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/keyboard_button.html)

`keyboard_button` is a library for scanning keyboard matrix, supporting the following features:

List of supported events:
 * KBD_EVENT_PRESSED
 * KBD_EVENT_COMBINATION

* Supports full-key anti-ghosting scanning method.
* Supports efficient key scanning with a scan rate of no less than 1K.
* Supports low-power keyboard scanning.

## Add component to your project

Please use the component manager command `add-dependency` to add the `keyboard_button` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/keyboard_button=*"
```