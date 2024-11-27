[![Component Registry](https://components.espressif.com/components/espressif/usb_device_uac/badge.svg)](https://components.espressif.com/components/espressif/usb_device_uac)

## USB Device UAC Component

`usb_device_uac` is a USB `UAC` device driver for ESP32-S2/ESP32-S3/ESP32-P4. Support for transmitting/receiving audio from the host side, with a maximum of 8 speaker channels, 4 microphone channels, and configurable sampling rates.

Features:

1. Supports audio transmission/reception via the UAC interface.
2. Supports setting the interval for receiving/sending audio.
3. Supports adjusting volume and setting mute.
4. Support for synchronous transfer feedback endpoints.

Currently not supported:

1. Dynamic configuration of MIC/SPK sampling rates.
2. Cannot be compatible with both Windows and Linux simultaneously. If you need to use it on macOS, please enable the macro `UAC_SUPPORT_MACOS`.

### Add component to your project

Please use the component manager command `add-dependency` to add the `usb_device_uac` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/usb_device_uac=*"
```

### Docs

* [USB Device UAC](https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_device/usb_device_uac.html)

### Examples

* [USB UAC](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_uac)
