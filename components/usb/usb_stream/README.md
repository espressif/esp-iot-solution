[![Component Registry](https://components.espressif.com/components/espressif/usb_stream/badge.svg)](https://components.espressif.com/components/espressif/usb_stream)

## USB Stream Component

`usb_stream` is an USB `UVC` + `UAC` host driver for ESP32-S2/ESP32-S3, which supports read/write/control multimedia streaming from usb device. For example, at most one UVC + one Microphone + one Speaker streaming can be supported at the same time.

Features:

1. Support video stream through UVC Stream interface, support both isochronous and bulk mode
2. Support microphone stream and speaker stream through the UAC Stream interface
3. Support volume, mute and other features control through the UAC Control interface
4. Support stream separately suspend and resume

> Note: For ESP-IDF V5.3.3 and above, if you enable UVC, please also enable `Bias IN` in `Component config → USB-OTG → Hardware FIFO size biasing`.

### USB Stream User Guide

Please refer: https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_host/usb_stream.html

### Add component to your project

Please use the component manager command `add-dependency` to add the `usb_stream` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/usb_stream=*"
```

### Examples

Please use the component manager command `create-project-from-example` to create the project from example template.

* USB Camera WIFI picture transfer
```
idf.py create-project-from-example "espressif/usb_stream=*:usb_camera_mic_spk"
```

* USB Camera local display
```
idf.py create-project-from-example "espressif/usb_stream=*:usb_camera_lcd_display"
```

Then the example will be downloaded in current folder, you can check into it for build and flash.

> Or you can download examples from esp-iot-solution repository: [USB Camera + Audio stream](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_camera_mic_spk), [USB Camera LCD Display](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_camera_lcd_display).

### Q&A

Q1. I encountered the following problems when using the package manager

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username
```

A1. This is because an older version package manager was used, please run `pip install -U idf-component-manager` in ESP-IDF environment to update.