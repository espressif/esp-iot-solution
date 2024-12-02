# USB Host Device mode manual switch

This example demonstrates how to dynamically switch the USB OTG peripheral between host and device modes.
In this example, the device is emulated as a USB CDC device, while the host functions as an MSC host capable of connecting to a USB flash drive.

## Dependences

USB Host:

* [esp_msc_ota](https://components.espressif.com/components/espressif/esp_msc_ota/versions/1.0.0)

The host application must support dynamic registration and unregistration.

USB Device:

* [tinyusb](https://components.espressif.com/components/espressif/tinyusb/versions/0.17.0~1)

The TinyUSB version must be greater than **0.17.0**, as versions after this support deinitializing the DWC2 driver.

## How to use the example

### Hardware Required

The example can be run on ESP32-S2 or ESP32-S3 or ESP32-P4 based development board with USB Disk (with Fat32 format).

### Setup the Hardware

Connect the external USB MSC device to ESP32-S USB interface directly.

| ESP32-Sx GPIO | ESP32-P4 GPIO | USB Device  |
| ------------- | ------------- | ----------- |
| 20            | 50            | D+ (green)  |
| 19            | 49            | D- (white)  |
| GND           | GND           | GND (black) |
| +5V           | +5V           | +5V (red)   |

### Configure the project

1. Use the command below to set build target to the chip.

    ```
    idf.py set-target esp32s3
    ```

2. Build the project and flash it to the board, then run monitor tool to view serial output:

    ```
    idf.py -p PORT build flash monitor
    ```

    (To exit the serial monitor, type ``Ctrl-]``.)

    See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

3. Connect a USB disk to the USB port, and you will see the USB disk being detected along with its descriptors printed.

4. Disconnect the USB disk, then press the boot button to set the device to device mode. At this point, connect it to a computer, and a serial device will appear.
