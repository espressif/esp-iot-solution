## iot_usbh_cdc Component

This component implements a simple version of the USB host CDC driver. 

**Features:**

1. Support USB CDC device
2. Support USB Vendor device
3. Support USB CDC multiple interface
4. Support USB HUB to connect multiple USB devices

### User Guide

Please refer: https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_host/usb_host_iot_usbh_cdc.html

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
