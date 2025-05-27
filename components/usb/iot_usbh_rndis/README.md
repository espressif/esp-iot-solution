[![Component Registry](https://components.espressif.com/components/espressif/iot_usbh_rndis/badge.svg)](https://components.espressif.com/components/espressif/iot_usbh_rndis)

## IOT USBH RNDIS Component

`iot_usbh_rndis` is a USB `RNDIS` device driver for ESP32-S2/ESP32-S3/ESP32-P4. It acts as a USB host to connect RNDIS devices.

Features:

1. Support hot-plug of RNDIS devices
2. Support auto-detection and connection of RNDIS devices

### Add component to your project

Please use the component manager command `add-dependency` to add the `iot_usbh_rndis` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/iot_usbh_rndis=*"
```

### Docs

* [IOT USBH RNDIS](https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_host/usb_rndis.html)

### Examples

* [USB RNDIS](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_rndis_4g_module)
