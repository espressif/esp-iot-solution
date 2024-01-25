## LED Indicator WS2812

* Support ON/OFF
* Support set brightness
* Support breathe with brightness

### Hardware Required

* LED

### Configure the project

```
idf.py menuconfig
```

* Set `EXAMPLE_GPIO_NUM` to set led gpio.
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

Note:
> Support replacing the LED with a passive buzzer to accomplish the functionality of a buzzer indicator light.