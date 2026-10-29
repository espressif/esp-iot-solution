| Supported Targets | ESP32-P4 | ESP32-S2 | ESP32-S3 | ESP32-S31 |
| ----------------- | -------- | -------- | -------- | --------- |

# USB Hub Dual Camera Example

This example demonstrates how to use the [usb_host_uvc](https://components.espressif.com/components/espressif/usb_host_uvc) component to connect USB cameras and preview camera images via an HTTP server.

* Supports previewing MJPEG format images only.
* Supports USB Hub, allowing multiple cameras to be connected.
* Web interface allows saving the current frame.

> The number of cameras that can be opened simultaneously is limited by the hardware resources of the USB host.

## How to Use

### Hardware Requirements

- An ESP32 board with USB OTG host capability.
- A proper USB connection and 5V power for the USB device when required.

Connection example:

    ```
    ┌─────────────┐          ┌─────────────────┐
    │             ┼──────────┼5V               │
    │ USB Hub/Cam ┼──────────┼GND              │
    │             │          │    ESP32-xx     │
    │             │          │                 │
    │             ┼──────────┼USB D+           │
    │             ┼──────────┼USB D-           │
    │             │          │                 │
    └─────────────┘          │                 │
                             └─────────────────┘
    ```

### Build and Flash

```bash
idf.py set-target TARGET
idf.py -p PORT flash monitor
```

Replace `PORT` and `TARGET` with the serial port of your board.

(To exit the serial monitor, type `Ctrl-]`.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

### Preview camera image

* Connect with ESP32S2 through Wi-Fi, SSID: `ESP-USB-UVC-Demo` with no password by default.

* Input `192.168.4.1` in your browser,  you can see the list of cameras connected to the USB, and you can preview the camera image.

Note: Safari browser is not recommended.

![demo](https://dl.espressif.com/AE/esp-iot-solution/uvc_dual_hub_camera_1.gif)

<p align="center">
  <img src="https://dl.espressif.com/AE/esp-iot-solution/uvc_dual_hub_camera_2.gif" alt="demo2">
</p>

### Frontend Source Files

Please refer to the [frontend source files](./frontend_source).
