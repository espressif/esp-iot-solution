## LED Indicator WS2812

* Support ON/OFF
* Support set brightness
* Support breathe with brightness
* Support set hsv color
* Support set rgb color
* Support color ring with hsv
* Support color ring with rgb

### Hardware Required

* RGB LED

### Configure the project

```
idf.py menuconfig
```

* Set `EXAMPLE_GPIO_RED_NUM` to set led red gpio.
* Set `EXAMPLE_GPIO_GREEN_NUM` to set led green gpio.
* Set `EXAMPLE_GPIO_BLUE_NUM` to set led blue gpio.
* Set `EXAMPLE_GPIO_ACTIVE_LEVEL` to set gpio level when led light

### How to USE

If the macro `EXAMPLE_ENABLE_CONSOLE_CONTROL` is enabled, please use the following method for control; otherwise, the indicator lights will flash sequentially in order.

* Help
    ```shell
    help
    ```

* Immediate display mode, without considering priority.
    ```shell
    led -p 0 # Start
    led -p 2 # Start
    led -x 2 # Stop
    ```

* Display mode based on priority.
    ```shell
    led -s 0 # Start 0
    led -s 2 # Start 2
    led -e 2 # Stop 2
    ```
