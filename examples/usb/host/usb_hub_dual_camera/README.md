# USB Hub Dual Camera Example

This example demonstrates how to use the [usb_host_uvc](https://components.espressif.com/components/espressif/usb_host_uvc) component to connect USB cameras and preview camera images via an HTTP server.

* Supports previewing MJPEG format images only.
* Supports USB Hub, allowing multiple cameras to be connected.
* Web interface allows saving the current frame.

> The number of cameras that can be opened simultaneously is limited by the hardware resources of the USB host.

## How to Use

### Hardware Requirements

* Development Board

    Any `ESP32-S2`, `ESP32-S3`, or `ESP32-P4` development board.

    By default, the ESP32-P4 uses the [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html#getting-started) development board.

* Connection

    |             | USB_DP | USB_DM |
    | ----------- | ------ | ------ |
    | ESP32-S2/S3 | GPIO20 | GPIO19 |
    | ESP32-P4    | GPIO50 | GPIO49 |

### Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

* Run `. ./export.sh` to set IDF environment
* Run `idf.py set-target esp32s3` to set target chip
* Run `pip install "idf-component-manager~=2.1.0"` to upgrade your component manager if any error happens during last step
* Run `idf.py -p PORT flash monitor` to build, flash and monitor the project

(To exit the serial monitor, type `Ctrl-]`.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

### Preview camera image

* Connect with ESP32S2 through Wi-Fi, SSID: `ESP-USB-UVC-Demo` with no password by default.

* Input `192.168.4.1` in your browser,  you can find a file list of the disk.

Note: Safari browser is not recommended.

![demo](https://dl.espressif.com/AE/esp-iot-solution/uvc_dual_hub_camera_1.gif)

<p align="center">
  <img src="https://dl.espressif.com/AE/esp-iot-solution/uvc_dual_hub_camera_2.gif" alt="demo2">
</p>

### Frontend Source Files

Please refer to the [frontend source files](./frontend_source).

## Example Output

```
I (4108) main_task: Returned from app_main()
W (31940) uvc: USB device with addr(1) is not UVC device
I (32025) uvc: Device connected
I (32026) uvc: Cam[0] uvc_stream_index = 0
I (32026) uvc: Pick Cam[0] FORMAT_MJPEG 1280*720@20.0fps
I (32026) uvc: Pick Cam[0] FORMAT_MJPEG 800*480@20.0fps
I (32031) uvc: Pick Cam[0] FORMAT_MJPEG 640*480@30.0fps
I (32036) uvc: Pick Cam[0] FORMAT_MJPEG 480*320@30.0fps
I (32041) uvc: Pick Cam[0] FORMAT_MJPEG 320*240@30.0fps
I (32109) uvc: Device connected
I (32110) uvc: Cam[1] uvc_stream_index = 0
I (32110) uvc: Pick Cam[1] FORMAT_MJPEG 1280*720@20.0fps
I (32110) uvc: Pick Cam[1] FORMAT_MJPEG 800*480@20.0fps
I (32115) uvc: Pick Cam[1] FORMAT_MJPEG 640*480@30.0fps
I (32120) uvc: Pick Cam[1] FORMAT_MJPEG 480*320@30.0fps
I (32125) uvc: Pick Cam[1] FORMAT_MJPEG 320*240@30.0fps
I (32246) uvc: Opening the UVC[0]...
I (32246) uvc: frame_size.advanced.frame_size: 40000
I (32330) uvc: Opening the UVC[1]...
I (32330) uvc: frame_size.advanced.frame_size: 40000
```