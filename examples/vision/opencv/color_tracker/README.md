| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

# OpenCV Color Tracker

This example is based on the [esp_video](https://components.espressif.com/components/espressif/esp_video/) and [opencv](https://components.espressif.com/components/espressif/opencv/) components, and demonstrates how to display the detect results on an LCD screen.

## How to use the example

## ESP-IDF Required

- This example supports ESP-IDF release/v5.4 and later branches. By default, it runs on ESP-IDF release/v5.4.
- Please follow the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html) to set up the development environment. **We highly recommend** you [Build Your First Project](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html#build-your-first-project) to get familiar with ESP-IDF and make sure the environment is set up correctly.


### Prerequisites

* An ESP32-P4-Function-EV-Board.
* A 7-inch 1024 x 600 LCD screen powered by the [EK79007](https://dl.espressif.com/dl/schematics/display_driver_chip_EK79007AD_datasheet.pdf) IC, accompanied by a 32-pin FPC connection [adapter board](https://dl.espressif.com/dl/schematics/esp32-p4-function-ev-board-lcd-subboard-schematics.pdf) ([LCD Specifications](https://dl.espressif.com/dl/schematics/display_datasheet.pdf)).
* A MIPI-CSI camera powered by the SC2336 IC, accompanied by a 32-pin FPC connection [adapter board](https://dl.espressif.com/dl/schematics/esp32-p4-function-ev-board-camera-subboard-schematics.pdf) ([Camera Specifications](https://dl.espressif.com/dl/schematics/camera_datasheet.pdf)).
* A USB-C cable for power supply and programming.
* Please refer to the following steps for the connection:
    * **Step 1**. According to the table below, connect the pins on the back of the screen adapter board to the corresponding pins on the development board.

        | Screen Adapter Board | ESP32-P4-Function-EV-Board |
        | -------------------- | -------------------------- |
        | 5V (any one)         | 5V (any one)               |
        | GND (any one)        | GND (any one)              |
        | PWM                  | GPIO26                     |
        | LCD_RST              | GPIO27                     |

    * **Step 2**. Connect the FPC of LCD through the `MIPI_DSI` interface.
    * **Step 3**. Connect the FPC of Camera through the `MIPI_CSI` interface.
    * **Step 4**. Use a USB-C cable to connect the `USB-UART` port to a PC (Used for power supply and viewing serial output).
    * **Step 5**. Turn on the power switch of the board.

### Configure the Project

Run `idf.py menuconfig` and navigate to the `Example Configuration` menu, where you can configure the relevant pins and example-related options.

This example supports configuring two HSV ranges, and you can adjust the Primary Color Range and the Secondary Color Range separately to identify the colors you want. By default, the intervals are set for recognizing red.

```
Example Configuration  --->
    Primary Color Range  --->
        (0) Primary Lower H (Hue)
        (120) Primary Lower S (Saturation)
        (70) Primary Lower V (Value)
        (10) Primary Upper H (Hue)
        (255) Primary Upper S (Saturation)
        (255) Primary Upper V (Value)
    Secondary Color Range  --->
        (170) Secondary Lower H (Hue)
        (120) Secondary Lower S (Saturation)
        (70) Secondary Lower V (Value)
        (180) Secondary Upper H (Hue)
        (255) Secondary Upper S (Saturation)
        (255) Secondary Upper V (Value)
```

As well, in the `Espressif Camera Sensors` Configurations, the camera sensor can be selected.

```
Component config  --->
    Espressif Camera Sensors Configurations  --->
        [*] SC2336  ---->
            Default format select for MIPI (RAW8 1280x720 30fps, MIPI 2lane 24M input)  --->
                (X) RAW8 1280x720 30fps, MIPI 2lane 24M input
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```c
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

![color_tracker](https://dl.espressif.com/AE/esp-iot-solution/cv_color_tracker.gif)
