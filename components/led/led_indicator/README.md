## LED Indicator

As one of the simplest output peripherals, LED indicators can indicate the current operating state of the system by blinking in different types. ESP-IoT-Solution provides an LED indicator component with the following features:

* Can define multiple groups of different blink types.
* Can define the priority of blink types.
* Can set up multiple indicators.
* LEDC and other drivers support adjustable brightness, gradient.
* Support adjustment light with gamma.
* Supports LED strips types like WS2812 and SK6812, and offers gradual color transitions based on the color wheel, brightness. transitions, and the ability to specify LED index.

### Support LED Types

| LED TYPES  | ON_OFF | Brightness | Breath | Color | Color ring | Addressable |
| :--------: | :----: | :--------: | :----: | :---: | :--------: | :---------: |
|  GPIO LED  |   √    |     ×      |   ×    |   ×   |     ×      |      ×      |
|  PWM LED   |   √    |     √      |   √    |   ×   |     ×      |      ×      |
|  RGB LED   |   √    |     √      |   √    |   √   |     √      |      ×      |
| LED strips |   √    |     √      |   √    |   √   |     √      |      √      |

### LED Indicator User Guide

Please refer: https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/led_indicator.html
