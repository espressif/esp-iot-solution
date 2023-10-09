[![Component Registry](https://components.espressif.com/components/espressif/usb_device_uvc/badge.svg)](https://components.espressif.com/components/espressif/usb_device_uvc)

## USB Device UVC Component

`usb_device_uvc` is a USB `UVC` device driver for ESP32-S2/ESP32-S3, which supports streaming JPEG frames to the USB Host. User can wrapper the Camera or any devices as a UVC standard device through the callback functions.

Features:

1. Support video stream through the UVC Stream interface
2. Support both isochronous and bulk mode
2. Support multiple resolutions and frame rates

### Add component to your project

Please use the component manager command `add-dependency` to add the `usb_device_uvc` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/usb_device_uvc=*"
```

### Examples

Please use the component manager command `create-project-from-example` to create the project from the example template

* USB WecCamera

```
idf.py create-project-from-example "espressif/usb_device_uvc=*:usb_webcam"
```

Then the example will be downloaded in the current folder, you can check into it for build and flash.

> Or you can download examples from esp-iot-solution repository: [USB WebCamera](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_webcam)
