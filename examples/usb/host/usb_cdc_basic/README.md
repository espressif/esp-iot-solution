# USB Host CDC Basic Example

This example demonstrates how to use USB Host CDC interface to send and receive data to connected USB device.

## How to use example

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

Use the command below to set build target to `esp32s2` or `esp32s3`.

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

I (808) cdc_demo: Send len=4: AT
I (828) cdc_demo: Receive len=6: 
OK

I (1308) cdc_demo: Send len=4: AT
I (1328) cdc_demo: Receive len=6: 
OK

I (1808) cdc_demo: Send len=4: AT
I (1828) cdc_demo: Receive len=6: 
OK
```