| Supported Targets | ESP32-C6 |
| ----------------- | -------- |
## LP Environment Sensor example

This example demonstrates how to use the LP CPU to read data from the AHT21B temperature and humidity sensor, enabling real-time monitoring of environmental temperature and humidity data.

* Supports real-time display of temperature and humidity data on the e-paper display
* Supports reporting temperature and humidity data to RainMaker.
* Supports HTTP fetching of weather information

## How to use the example

### Hardware Required

The example hardware has been open-sourced on [OSHW-Hub](https://oshwhub.com/esp-college/esp-ths).

 * An ESP32-C6 development board
 * An AHT21B temperature and humidity sensor
 * A 2.9-inch e-paper display(based on SSD1680 driver)

- Hardware Connection：

| ESP32-C6 | AHT21B | 2.9-inch e-paper display | LDO CHIP |
| :------: | :----: | :----------------------: | :------: |
|  GPIO2   |        |                          |    EN    |
|  GPIO6   |  SDA   |                          |          |
|  GPIO7   |  SCL   |                          |          |
|  GPIO15  |        |           SCL            |          |
|  GPIO18  |        |           SDA            |          |
|  GPIO19  |        |           RES            |          |
|  GPIO20  |        |            DC            |          |
|  GPIO21  |        |            CS            |          |
|  GPIO22  |        |           BUSY           |          |

### Configure the project

By default, this example configured to disable Rainmaker data reporting, HTTP weather information retrieval, and ULP UART printing functions. You can use `idf.py menuconfig` to enable these functions in the `LP Environment Sensor Configuration` directory.

 * E-paper display effect

<div style="text-align:center;">
  <img src="https://dl.espressif.com/ae/esp-box/esp_ths_physical_drawing.png/esp_ths_physical_drawing.png" width="400" height="315">
</div>

### Build and Flash

1. Make sure `ESP-IDF` is installed successfully

2. Set up the `ESP-IDF` environment variables， you can refer [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables).

3. Set ESP-IDF build target to `esp32c6`

    ```bash
    idf.py set-target esp32c6
    ```

4. Build, Flash, output log

    ```bash
    idf.py build flash monitor
    ```

## Example Output

```
I (139) main_task: Started on CPU0
I (142) main_task: Calling app_main()
I (147) gpio: GPIO[8]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (156) gpio: GPIO[19]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (165) gpio: GPIO[20]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (174) gpio: GPIO[22]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:2
I (3035) LP_Environment_Sensor: e-paper refresh done.
I (3036) LP_Environment_Sensor: Enter deep sleep.
...
...
I (139) main_task: Started on CPU0
I (142) main_task: Calling app_main()
wake up by LP CPU.
I (148) LP_Environment_Sensor: temperature: 25.918961, humidity: 40.086460
I (156) gpio: GPIO[8]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (166) gpio: GPIO[19]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (174) gpio: GPIO[20]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (184) gpio: GPIO[22]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:2
I (3044) LP_Environment_Sensor: e-paper refresh done.
I (3045) LP_Environment_Sensor: Enter deep sleep.
...
```
