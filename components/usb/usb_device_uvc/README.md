[![Component Registry](https://components.espressif.com/components/espressif/usb_device_uvc/badge.svg)](https://components.espressif.com/components/espressif/usb_device_uvc)

## USB Device UVC Component

`usb_device_uvc` is a USB `UVC` device driver for ESP32-S2/ESP32-S3, which supports streaming JPEG frames to the USB Host. User can wrapper the Camera or any devices as a UVC standard device through the callback functions.

Features:

1. Support video stream through the UVC Stream interface
2. Support both isochronous and bulk mode
3. Support multiple resolutions and frame rates
4. Support trans two cam's picture

> Note: If `UVC_SUPPORT_TWO_CAM` is enabled in `Kconfig` and multiple camera switches are required, please set both `UVC_CAM1_XFER_MODE` and `UVC_CAM2_XFER_MODE` to Isochronous mode.

### Add component to your project

Please use the component manager command `add-dependency` to add the `usb_device_uvc` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/usb_device_uvc=*"
```

### Examples

* [USB WebCamera: Make ESP32-S3-EYE as a USB Camera Device](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_webcam)
