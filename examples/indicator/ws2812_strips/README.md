| Supported Targets | ESP32 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- |

## LED Indicator WS2812

* Support ON/OFF
* Support set brightness
* Support breathe with brightness
* Support set hsv color
* Support set rgb color
* Support color ring with hsv
* Support color ring with rgb
* Support set by index

### Hardware Required

* WS2812 Strips
* ESP32 SOC Support RMT

### Configure the project

```
idf.py menuconfig
```

* Set `EXAMPLE_WS2812_GPIO_NUM` to set ws2812 gpio.
* Set `EXAMPLE_WS2812_STRIPS_NUM` to set ws2812 number.

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

![flowing leds](https://dl.espressif.com/ae/esp-iot-solution/led_indicator_ws2812.gif)