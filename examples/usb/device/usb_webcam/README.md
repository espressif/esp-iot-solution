## USB WebCam Example

This example demonstrates how to use ESP32-Sx USB function as the USB Web Camera (UVC device). 

* Support both UVC Isochronous and Bulk transfer mode
* Support MJPEG format only
* Support LCD animation in `esp32-s3-eye` board (**IDF v5.0 or later**)

![esp32_s3_eye_webcam](https://dl.espressif.com/AE/esp-dev-kits/webcam.gif)

### Try the example through the esp-launchpad

<a href="https://espressif.github.io/esp-launchpad/?flashConfigURL=https://dl.espressif.com/AE/esp-iot-solution/config.toml">
    <img alt="Try it with ESP Launchpad" src="https://espressif.github.io/esp-launchpad/assets/try_with_launchpad.png" width="200" height="56">
</a>

This example has been built for `ESP32-S3-EYE`, you can use `esp-launchpad` to flash the binary to your board directly.

Please choose `ESP32-S3-EYE-USB_WebCam_v1_0`, then click flash to quickly start

## How to build the example

### Hardware Required

- Any ESP32-S2 or ESP32-S3 development board with both camera and USB interface, you can also use `idf.py menuconfig`,  select board through `USB WebCam config → Camera Pin Configuration → Select Camera Pinout`, this example use `ESP32-S3-EYE` by default
 
- USB Connection：
  - GPIO19 to D
  - GPIO20 to D+

- Camera Connection：
  - For boards not in the list, please choose `Custom Camera Pinout` to configure each Pin

### Camera Configuration

1. Please check [esp32-camera](https://github.com/espressif/esp32-camera) to find the supported cameras, for this example camera JPEG compression should be supported.
2. Using `idf.py menuconfig`, through `USB WebCam config` users can configure the frame resolution, frame rate and image quality.
3. Through ` USB WebCam config → UVC transfer mode`, users can change to `Bulk` mode to get twice the throughput than `Isochronous`.

|Transfer Mode|Max Throughput|Compatibility|
|--|--|--|
|Isochronous|~512KB/s|Windows/Linux/MacOS|
|Bulk|~1216KB/s|Windows/MacOS|

* `Bulk` mode may encounter compatibility issues on some Linux platform

### Build and Flash

1. Make sure `ESP-IDF` is setup successfully

2. Set up the `ESP-IDF` environment variables, please refer [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables), Linux can use:

    ```bash
    . $HOME/esp/esp-idf/export.sh
    ```

3. Set ESP-IDF build target to `esp32s2` or `esp32s3`

    ```bash
    idf.py set-target esp32s3
    ```

4. Build, Flash, output log

    ```bash
    idf.py build flash monitor
    ```

## Example Output

```
I (0) cpu_start: Starting scheduler on APP CPU.
I (533) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (534) usb_webcam: Selected Camera Board ESP-S3-EYE
I (534) usb_webcam: Format List
I (534) usb_webcam:     Format(1) = MJPEG
I (534) usb_webcam: Frame List
I (535) usb_webcam:     Frame(1) = 1280 * 720 @15fps, Quality = 14
I (535) usb_webcam:     Frame(2) = 640 * 480 @15fps, Quality = 14
I (535) usb_webcam:     Frame(3) = 480 * 320 @30fps, Quality = 10
I (869) usb_webcam: Mount
I (2099) usb_webcam: Suspend
I (9343) usb_webcam: bFrameIndex: 1
I (9343) usb_webcam: dwFrameInterval: 666666
I (9343) s3 ll_cam: DMA Channel=4
I (9344) cam_hal: cam init ok
I (9344) sccb: pin_sda 4 pin_scl 5
I (9344) sccb: sccb_i2c_port=1
I (9354) camera: Detected camera at address=0x30
I (9357) camera: Detected OV2640 camera
I (9357) camera: Camera PID=0x26 VER=0x42 MIDL=0x7f MIDH=0xa2
I (9434) cam_hal: buffer_size: 16384, half_buffer_size: 1024, node_buffer_size: 1024, node_cnt: 16, total_cnt: 180
I (9435) cam_hal: Allocating 184320 Byte frame buffer in PSRAM
I (9435) cam_hal: Allocating 184320 Byte frame buffer in PSRAM
I (9436) cam_hal: cam config ok
I (9436) ov2640: Set PLL: clk_2x: 0, clk_div: 0, pclk_auto: 0, pclk_div: 12
```