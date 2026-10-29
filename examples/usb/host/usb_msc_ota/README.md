| Supported Targets | ESP32-P4 | ESP32-S2 | ESP32-S3 | ESP32-S31 |
| ----------------- | -------- | -------- | -------- | --------- |

# USB Host MSC OTA Example

This example demonstrates how to use [esp_msc_ota](https://components.espressif.com/components/espressif/esp_msc_ota) to OTA with USB disk or other MSC devices.

## How to use the example

### Hardware Required

A development board with USB Host interface and a USB Disk with Fat32 format are required.

### Setup the Hardware

Connect the external USB MSC device to ESP32-S USB interface directly.

```
┌─────────────┐          ┌─────────────────┐
│             ┼──────────┼5V               │
│  USB Disk   ┼──────────┼GND              │
│             │          │    ESP32-xx     │
│             │          │                 │
│             ┼──────────┼USB D+           │
│             ┼──────────┼USB D-           │
│             │          │                 │
└─────────────┘          │                 │
                         └─────────────────┘
```

### Configure the project

1. Use the command below to set build target.

    ```
    idf.py set-target esp32s3
    ```

2. Build the project and flash it to the board, then run monitor tool to view serial output:

    ```
    idf.py -p PORT build flash monitor
    ```

    (To exit the serial monitor, type ``Ctrl-]``.)

    See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

3. Move the file located in the directory /build/usb_msc_ota.bin to the USB MSC drive (USB disk). Insert the USB flash drive into the USB interface. The upgrade process will start immediately. Once the upgrade is successful, press the reset button to initiate OTA partition 2.

Note: Please keep in mind, to continuous support for USB MSC OTA, the USB MSC OTA function must also be enabled in the OTA firmware.

![usb_msc_ota](https://dl.espressif.com/ae/esp-iot-solution/usb_msc_ota.gif)
