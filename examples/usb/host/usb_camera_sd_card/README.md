[中文版本](./README_cn.md)

## USB Camera + SD Card Example

This example demonstrates USB camera `MJPEG` stream reading + SD card storage via the `ESP32-S2` or `ESP32-S3` USB host function, which supports the following functions.

* Support for reading and parsing USB Camera data streams
* Support saving images in SD Card

### Hardware Requirements

You can use any `ESP32-S2` or `ESP32-S3` development board with SD Card socket

* Cameras that meet the following necessary parameters are supported:
  1. Camera compatible with USB1.1 full speed mode
  2. Camera comes with MJPEG compression
  3. The camera supports setting the interface Max Packet Size to `512`
  4. Image **transfer bandwidth should be less than 4 Mbps**, because we use 512 Bytes Packet size with isochronous transfer, only one packet can be transmitted per millisecond in this mode.
  5. Due to the USB isochronous transfer bandwidth limitation, the image frame rate and single image size are mutually constrained. If the image size is 25 KB per frame, the frame rate can not above 20 FPS
  6. This example supports **any resolution** that satisfies the above conditions, as no local decoding is required

* USB camera hardware wiring.
  1. Please use 5V power supply for USB camera VBUS, or use IO to control VBUS ON/OFF.
  2. USB camera D+ D- data line please follow the regular differential signal standard alignment
  3. USB Camera D+ (green wire) to ESP32-S2/S3 GPIO20
  4. USB Camera D- (white wire) to ESP32-S2/S3 GPIO19

* SD card hardware wiring:
    1. MOSI_PIN GPIO38
    2. MISO_PIN GPIO40
    3. SCLK_PIN GPIO39
    4. CS_PIN   GPIO37

### Build Example

You can use any `ESP32-S2` or `ESP32-S3` development board with SD Card socket in this example

1. Set up the `ESP-IDF` environment variables,you can refer [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables), Linux can using:

    ```bash
    $HOME/esp/esp-idf/export.sh
    ```
2. Set up the `ESP-IOT-SOLUTION` environment variables,you can refer [readme](../../../../README.md), Linux can using:

    ```bash
    export IOT_SOLUTION_PATH=$HOME/esp/esp-iot-solution
    ```

3. According to the camera configuration descriptor,[Modify camera configuration items](../../../../components/usb/uvc_stream/README.md)
4.  Set build target to `esp32s2` or `esp32s3`

    ```bash
    idf.py set-target esp32s2
    ```

5. Build, Flash, check log

    ```bash
    idf.py build flash monitor
    ```

### How to Use

1. Just pay attention to the SD card and camera interface wiring
2. MJPEG stream captured by the camera are saved to the SD card as JPEG images