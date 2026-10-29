| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

# Video LCD Display

This example is based on the [esp_video](https://github.com/espressif/esp-video-components/tree/master/esp_video) component and demonstrates how to display images from the camera on an LCD screen.

## ESP-IDF Required

- This example requires ESP-IDF v5.5.6 or later with the native DPI framebuffer-completion callback; no upper version limit is imposed.
- Please follow the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/index.html) to set up the development environment. **We highly recommend** you [Build Your First Project](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/index.html#build-your-first-project) to get familiar with ESP-IDF and make sure the environment is set up correctly.

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

Run `idf.py menuconfig` and navigate to the `Example Configuration` menu, where you can configure the SCCB pins, LCD color format, capture buffer count, and buffer allocation mode. The example defaults to SCCB SCL GPIO8 and SDA GPIO7.

Under `Component config → Hardware Settings → Chip revision`, select the chip revision that matches your board.

In `Espressif Camera Sensors Configurations`, enable the SC2336 RAW8 1024x600 mode and select it as the default MIPI output format. Enabling a supported format alone does not select it as the default.

```text
Component config  --->
    Espressif Camera Sensors Configurations  --->
        [*] SC2336  ---->
            Choose supported formats for MIPI CSI interface  --->
                [*] MIPI 2-lane, 24M input, 1024x600  30fps, RAW8
            Select default output format for MIPI CSI interface  --->
                (X) MIPI 2-lane, 24M input, 1024x600  30fps, RAW8
```

The ISP converts the sensor's RAW8 output to the capture format selected for the LCD: little-endian RGB565 (`V4L2_PIX_FMT_RGB565`, default) or RGB888 (`V4L2_PIX_FMT_RGB24`). Matching-size USERPTR capture uses three shared LCD buffers with no frame copy: two are available for capture and one is held for scanout. The initial scanout buffer is withheld from CSI, and each later buffer is requeued only after LCD retirement.

For other sizes, USERPTR uses separate capture buffers sized by `VIDIOC_QUERYBUF`. Enable `Use Memory Mapping for Buffer Allocation` for driver-allocated `V4L2_MEMORY_MMAP` buffers. These copy paths use the configured capture buffer count and two LCD buffers; PPA center-crops larger frames and places smaller frames at the top-left without scaling.

### Build and Flash

CMake verifies the pinned `esp_video` 2.5.0 release and applies one [CSI API patch](patches/esp_video/) for IDF 5.5.6 and newer driver APIs. ESP-IDF is used without local patches. Subsequent configurations reuse the build-directory cache. Download, checksum, or patch failures stop configuration.

Downloads honor `IDF_COMPONENT_LOCAL_STORAGE_URL` first, then `IDF_COMPONENT_STORAGE_URL` (semicolon-separated storage roots, including `file://` mirrors). Without an explicit storage URL, public storage is used. A fresh offline build needs a local mirror or a cached archive in the build directory.

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```sh
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``. When switching ESP-IDF versions, use separate build directories and sdkconfig files, or run `idf.py fullclean` before rebuilding.

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.
