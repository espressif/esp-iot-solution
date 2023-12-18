# USB Host CDC Basic Example

This example demonstrates how to use [iot_usbh_cdc](https://components.espressif.com/components/espressif/iot_usbh_cdc) to communicate with USB CDC device.

## How to use the example

### Hardware Required

The example can be run on ESP32-S2 or ESP32-S3 based development board with USB interface. 

### Setup the Hardware

Connect the external USB device to ESP32-S USB interface directly.

| ESP32-Sx GPIO | USB Device  |
| ------------- | ----------- |
| 20            | D+ (green)  |
| 19            | D- (white)  |
| GND           | GND (black) |
| +5V           | +5V (red)   |

### Configure the project

1. The example enables one bulk interface by default.
2. Users can modify the `EXAMPLE_BULK_ITF_NUM` to `2` in `usb_cdc_basic_main.c` to enable two bulk interfaces.
3. Users need to modify the endpoint address `EXAMPLE_BULK_OUTx_EP_ADDR` and `EXAMPLE_BULK_INx_EP_ADDR` to the actual endpoint address of the USB device.
4. Use the command below to set build target to `esp32s2` or `esp32s3`.

```
idf.py set-target esp32s3
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT build flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

If your USB device (eg. 4G Module) supports AT command, When host send `AT`, the device usually returns `OK` in most cases.

```
I (408) USB_HCDC: Waitting Device Connection
I (438) USB_HCDC: Port=1 init succeed
W (438) USB_HCDC: Waitting USB Connection
I (688) USB_HCDC: line 261 HCD_PORT_EVENT_CONNECTION
I (688) USB_HCDC: Resetting Port
I (748) USB_HCDC: Port speed = 1
I (748) USB_HCDC: Set Device Addr = 1
I (748) USB_HCDC: Set Device Addr Done
I (748) USB_HCDC: Set Device Configuration = 1
I (748) USB_HCDC: Set Device Configuration Done
I (758) USB_HCDC: Set Device Line State: dtr 1, rts 0
I (758) USB_HCDC: Set Device Line State Done
I (768) USB_HCDC: Creating bulk in pipe
I (768) USB_HCDC: Creating bulk out pipe
I (808) USB_HCDC: usb driver install succeed

I (6059) cdc_basic_demo: Send itf0 len=4: AT

I (6069) cdc_basic_demo: Send itf1 len=4: AT

I (6079) cdc_basic_demo: Itf 0, Receive len=6: 
OK

I (6079) cdc_basic_demo: Itf 1, Receive len=6: 
OK

I (7089) cdc_basic_demo: Send itf0 len=4: AT

I (7089) cdc_basic_demo: Send itf1 len=4: AT

I (7109) cdc_basic_demo: Itf 0, Receive len=6: 
OK

I (7109) cdc_basic_demo: Itf 1, Receive len=6: 
OK
```