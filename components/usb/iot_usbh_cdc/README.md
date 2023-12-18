## iot_usbh_cdc Component

This component implements a simple version of the USB host CDC driver. The API is designed similarly to [ESP-IDF UART driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-reference/peripherals/uart.html), which can be used to replace the original UART driver to realize the update from UART to USB.

**Features:**

1. Similar API to ESP-IDF UART driver
2. Support USB CDC device
3. Support USB Vendor device
4. Support USB CDC multiple interface

### User Guide

Please refer: https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_host_iot_usbh_cdc.html

### Add component to your project

Please use the component manager command `add-dependency` to add the `iot_usbh_cdc` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/iot_usbh_cdc=*"
```

### Examples

Please use the component manager command `create-project-from-example` to create the project from the example template

* USB Host CDC Basic Example

```
idf.py create-project-from-example "espressif/iot_usbh_cdc=*:usb_cdc_basic"
```

Then the example will be downloaded in the current folder, you can check into it for build and flash.

> Or you can download examples from esp-iot-solution repository: [usb_cdc_basic](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_cdc_basic).
