[中文版本](./README.md)

## USB Camera + LCD Display Example

This example implements USB camera `MJPEG` stream reading, JPEG decoding, and screen refresh via the `ESP32-S2` or `ESP32-S3` series USB host function, and supports the following functions.

* Support USB Camera data stream reading
* Support JPEG decoding
* Support LCD screen display

### Hardware Requirement

You can use any `ESP32-S2` or `ESP32-S3` development board with [esp-iot-solution supported screen](https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/display/screen.html)

**Parameters to be met for USB camera:**

1. Camera compatible with USB1.1 full-speed mode
2. Camera comes with MJPEG compression
3. The camera supports setting the interface Max Packet Size to `512`
4. MJPEG support **320*240** resolution
5. MJPEG supports setting frame rates up to 15 fps

**USB camera hardware wiring:**

 1. Please use 5V power supply for USB camera VBUS, or use IO to control VBUS ON/OFF.
 2. USB camera D+ D- data line please follow the regular differential signal standard alignment
 3. USB Camera D+ (green wire) to ESP32-S2/S3 GPIO20
 4. USB Camera D- (white wire) to ESP32-S2/S3 GPIO19

### Build Example

The example code can be used with development boards such as `ESP32-S2-Kaluga-1` based on the `ESP32-S2-WROVER` module.

1. Set up the `ESP-IDF` environment variables,you can refer [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables), Linux can using:

    ```bash
    $HOME/esp/esp-idf/export.sh
    ```
2. Set up the `ESP-IOT-SOLUTION` environment variables,you can refer [readme](../../../../README.md), Linux can using:

    ```bash
    export IOT_SOLUTION_PATH=$HOME/esp/esp-iot-solution
    ```
3. According to the camera configuration descriptor, [Modify camera configuration items](../../../../components/usb/uvc_stream/README.md)
4.  Set build target to `esp32s2` or `esp32s3`

    ```bash
    idf.py set-target esp32s2
    ```

5. Build, Flash, check log

    ```bash
    idf.py build flash monitor
    ```

### How to Use

1. Just pay attention to the LCD screen and camera interface wiring
2. Screen will refresh camera outputs by default
3. You can verify that the LCD function by enabling `boot animation` in menuconfig

### Performance Parameters

**ESP32-S2 240Mhz**:

| Typical resolution  | USB Typical Transfer Frame Rate | JPEG maximum decoding + display frame rate* |
| :-----: | :--------------: | :----------------------: |
| 320*240 |        15        |           ~12            |
| 160*120 |        30        |           ~28            |

1. JPEG decoding + display frame rate fluctuates with CPU load
2. Other resolutions are subject to actual testing or estimated according to the positive correlation between decoding time and resolution
