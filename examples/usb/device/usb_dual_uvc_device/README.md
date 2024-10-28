## USB UVC Device Example

This example demonstrates how to use the usb_device_uvc component to simulate two virtual cameras. Camera one transmits an AVI video stream, while camera two transmits JPEG images.

|            |          CONFIG          |     CAM1      |    CAM2    |
| :--------: | :----------------------: | :-----------: | :--------: |
| ESP32-S2/3 | CONFIG_FORMAT_MJPEG_CAM1 | 640*360 MJPEG | 720P MJPEG |
| ESP32-S2/3 | CONFIG_FORMAT_H264_CAM1  | 640*360 H264  | 720P MJPEG |
|  ESP32-P4  | CONFIG_FORMAT_MJPEG_CAM1 |  720P MJPEG   | 720P MJPEG |
|  ESP32-P4  | CONFIG_FORMAT_H264_CAM1  |   720P H264   | 720P MJPEG |

Note: After modifying the camera transmission format, please also delete the SPIFFS folder accordingly.

```
  rm -rf build spiffs
```

## Hardware

* Development board

  1. Any `ESP32-S2`, `ESP32-S3`, `ESP32-P4` board with USB Host port can be used.
  2. Please note that the `esp32-sx-devkitC` board can not output 5V through USB port, If the OTG conversion cable is used directly, the device cannot be powered.
  3. For `esp32s3-usb-otg` board, please enable the USB Host power domain to power the device
  4. The flash size should be at least 8MB.

* Connection

    |             | USB_DP | USB_DM |
    | ----------- | ------ | ------ |
    | ESP32-S2/S3 | GPIO20 | GPIO19 |

## Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

* Run `. ./export.sh` to set IDF environment
* Run `idf.py set-target esp32s3` to set target chip
* Run `pip install "idf-component-manager~=1.1.4"` to upgrade your component manager if any error happens during last step
* Run `idf.py -p PORT flash monitor` to build, flash and monitor the project

(To exit the serial monitor, type `Ctrl-]`.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

>> Note: The example will fetch an AVI file online. Please make sure the first compilation is done with an internet connection.

## Example Output

```
I (770) avi player: AVI Player Version: 0.1.0
I (770) usb_cam1: Format List
I (770) usb_cam1:       Format(1) = MJPEG
I (772) usb_cam1: Frame List
I (775) usb_cam1:       Frame(1) = 480 * 270 @20fps
I (781) usb_cam2: Format List
I (784) usb_cam2:       Format(1) = MJPEG
I (788) usb_cam2: Frame List
I (792) usb_cam2:       Frame(1) = 1280 * 720 @15fps
I (798) usbd_uvc: UVC Device Start, Version: 0.2.0
I (803) main_task: Returned from app_main()
I (1136) usbd_uvc: Mount
I (3451) usb_cam1: Camera:1 Stop
I (3452) usb_cam2: Camera:2 Stop
I (3452) usbd_uvc: Suspend
I (6279) usbd_uvc: Resume
I (6690) usbd_uvc: bFrameIndex: 1
I (6690) usbd_uvc: dwFrameInterval: 666666
I (6690) usb_cam2: Camera:2 Start
I (6692) usb_cam2: Format: 0, width: 1280, height: 720, rate: 15
I (11180) usbd_uvc: bFrameIndex: 1
I (11180) usbd_uvc: dwFrameInterval: 666666
I (11180) usb_cam2: Camera:2 Start
I (11182) usb_cam2: Format: 0, width: 1280, height: 720, rate: 15
I (32942) usb_cam1: Camera:1 Stop
I (32943) usb_cam2: Camera:2 Stop

```
