# IoT USB Modem

This component using ESP32-S2, ESP32-S3, ESP32-P4 series SoC as a USB host to dial-up 4G module through PPP to access the Internet.

**Features Supported:**

* Compatible with mainstream 4G module AT commands
* USB PPP dial-up
* USB Dual Interface

> Secondary AT Port can be used for AT command when main modem port is in ppp network mode

## Docs

Please refer: https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_host/usb_ppp.html

## Examples

* [USB CDC 4G Module](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_cdc_4g_module)
