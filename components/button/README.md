[![Component Registry](https://components.espressif.com/components/espressif/button/badge.svg)](https://components.espressif.com/components/espressif/button)

# Component: Button
[Online documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/button.html)

After creating a new button object by calling function `button_create()`, the button object can create press events, every press event can have its own callback.

List of supported events:
 * Button pressed
 * Button released
 * Button pressed repeat
 * Button press repeat done
 * Button single click
 * Button double click
 * Button multiple click
 * Button long press start
 * Button long press hold
 * Button long press up
 * Button Press end

![](https://dl.espressif.com/AE/esp-iot-solution/button_3.3.1.svg)

There are three ways this driver can handle buttons:
1. Buttons connected to standard digital GPIO
2. Multiple buttons connected to single ADC channel
3. Matrix keyboard employs multiple GPIOs for operation.
4. Custom button connect to any driver

The component supports the following functionalities:
1. Creation of an unlimited number of buttons, accommodating various types simultaneously.
2. Multiple callback functions for a single event.
3. Allowing customization of the consecutive key press count to any desired number.
4. Facilitating the setup of callbacks for any specified long-press duration.
5. Support power save mode (Only for gpio button)

## Add component to your project

Please use the component manager command `add-dependency` to add the `button` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/button=*"
```