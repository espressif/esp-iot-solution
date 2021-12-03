[中文版本](./README_cn.md)

## USB Camera + Wi-Fi Transfer Example

This example implements USB camera `MJPEG` stream reading + Wi-Fi picture transfer via the `ESP32-S2` or `ESP32-S3` series Soc, and supports the following functions:

* Support for reading and parsing USB Camera data streams
* Support setting Wi-Fi to AP or STA mode
* Support for HTTP graphical transfer, you can use mobile devices or PC browser to view the live pictures

### Hardware Requirement

You can use any `ESP32-S2` or `ESP32-S3` development board integrated with PSRAM

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

### Build Example

The example code requires an additional `2 MB` PSRAM and can be used with development boards such as `ESP32-S2-Saola-1` and `ESP32-S2-Kaluga-1` based on the `ESP32-S2-WROVER` module.

1. Set up the `ESP-IDF` environment variables，you can refer [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables), Linux can using:

    ```bash
    $HOME/esp/esp-idf/export.sh
    ```

2. Set up the `ESP-IOT-SOLUTION` environment variables，you can refer [readme](../../../../README.md), Linux can using:

    ```bash
    export IOT_SOLUTION_PATH=$HOME/esp/esp-iot-solution
    ```

3. According to the camera configuration descriptor，[Modify camera configuration items](../../../../components/usb/uvc_stream/README.md)
4.  Set build target to `esp32s2` or `esp32s3`

    ```bash
    idf.py set-target esp32s2
    ```

5. Build, Flash, check log

    ```bash
    idf.py build flash monitor
    ```

### How to Use

1. Use PC or mobile devices access to Wi-Fi hotspot of ESP32-S2/S3, SSID: `ESP32S2-UVC` No password by default
2. Enter `192.168.4.1` in your browser to open the operation window
3. Click `Start Stream` to start video streaming
4. Click `Get Still` to take a photo
5. Click on the preview window `Save` to save the current image

### Performance parameters

**Under the USB isochronous transfer bandwidth limitation**, different resolution images **compression rate** corresponds to **frame rate**:

  **320*240** image throughput up to **33 frames** per second at a compression ratio of **15:1**, with an image size of approximately 15 KB per frame:

  ![](./_static/320_240_fps.jpg)

  **640*480** image throughput rates up to **15 frames** per second at a compression ratio of **25:1**, with an image size of approximately 36 KB per frame:

  ![](./_static/640_480_fps.jpg)
