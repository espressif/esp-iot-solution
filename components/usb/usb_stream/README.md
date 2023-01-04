## USB Stream Component

`usb_stream` is an USB `UVC` + `UAC` host driver for ESP32-S2/ESP32-S3, which supports read/write/control multimedia streaming from usb device. For example, at most one UVC + one Microphone + one Speaker streaming can be supported at the same time.

Features:

1. Support video stream through UVC Stream interface, support both isochronous and bulk mode
2. Support microphone stream and speaker stream through the UAC Stream interface
3. Support volume, mute and other features control through the UAC Control interface
4. Support stream separately suspend and resume

### USB Stream User Guide

Please refer: https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/usb/usb_stream.html

### Examples

[USB Camera + Audio stream](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_camera_mic_spk)