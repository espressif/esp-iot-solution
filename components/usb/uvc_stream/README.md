## UVC Stream Component

`UVC Stream` is a USB camera driver based on `UVC` Protocol. Developers can use ESP32-S2 / ESP32-S3 as a USB host send request then continuously receive USB camera `MJPEG` frames. With the help of `ESP-IOT-SOLUTION` decoding or network transmission components, real-time camera display or IPC can be implemented.

Users can control USB video stream to start, pause, restart, or stop via a simple API interface. By registering a frame callback, the user's method can be called when a new frame is received.

The component support both isochronous and bulk mode camera

### Setup Environment

1. Setup ESP-IDF `master` Environment, please refer [installation-step-by-step](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html#installation-step-by-step)
2. Setup `ESP-IOT-Solution` Environment, please refer [Setup ESP-IOT-Solution Environment](../../../README.md)

### Hardware Requirement

* Development board

  1. Any `ESP32-S2`,` ESP32-S3` board with USB Host port can be used.
  2. Please refer example readme for special hardware requirements

* USB camera

  1. Camera must be compatible with `USB1.1` full-speed mode
  2. Camera must support `MJPEG` compression
  3. For isochronous mode camera, interface `max packet size` should be less than `512` bytes
  4. For isochronous mode camera, frame stream bandwidth should be less than `4 Mbps` (500 KB/s)
  5. For bulk mode camera, frame stream bandwidth should be less than `8.8 Mbps` (1100 KB/s)
  6. Please refer example readme for special camera requirements

### UVC Stream API Guide

1. Users need to know USB camera detailed descriptors in advance, the` uvc_config_t` should be filled based on the information, the parameters correspondence is as follows:

```
uvc_config_t uvc_config = {
        .xfer_type = UVC_XFER_ISOC,  // camera transfer mode
        .dev_speed = usb_speed_full, // ​​usb speed mode, must fixed to usb_speed_full
        .configuration = 1, // configure descriptor index, generally 1
        .format_index = 1, // mjpeg format index, for example 1
        .frame_width = 320, // mjpeg width pixel, for example 320
        .frame_height = 240, // mjpeg height pixel, for example 240
        .frame_index = 1, // frame resolution index, for example 1
        .frame_interval = 666666, / frame interval (100µs units), such as 15fps
        .interface = 1, // video stream interface number, generally 1
        .interface_alt = 1, // alt interface number, `max packet size` should be less than `512` bytes
        .isoc_ep_addr = 0x81, // alt interface endpoint address, for example 0x81
        .isoc_ep_mps = 512, // alt interface endpoint MPS, for example 512
        .xfer_buffer_size = 32 * 1024, // single frame image size, need to be determined according to actual testing, 320 * 240 generally less than 35KB
        .xfer_buffer_a = pointer_buffer_a, // the internal transfer buffer
        .xfer_buffer_b = pointer_buffer_b, // the internal transfer buffer
        .frame_buffer_size = 32 * 1024, // single frame image size, need to determine according to actual test
        .frame_buffer = pointer_frame_buffer, // the image frame buffer
    }
```

1. Use the `uvc_streaming_config` and above uvc_config_t parameters to config the driver;
2. Use the `uvc_streaming_start` to turn on the video stream, after USB connection and UVC parameter negotiation, the camera will continue to output the data stream. This driver will call the user's callback function to decode, display, transmission, etc.
3. If you perform an error, reduce the resolution or frame rate, return to step 1 to modify the parameters;
4. Use the `uvc_streaming_suspend` to temporarily suspend the camera stream;
5. Use the `uvc_streaming_resume` to resume the camera stream from suspend;
6. Use the `uvc_streaming_stop` to stop the video stream, USB resource will be completely released.

### USB Camera Examples

1. [USB Camera + Wi-Fi](../../../examples/usb/host/usb_camera_wifi_transfer)
2. [USB Camera + LCD](../../../examples/usb/host/usb_camera_lcd_display)
3. [USB Camera + SD Card](../../../examples/usb/host/usb_camera_sd_card)
