# USB Host CDC Basic Example

This example demonstrates how to use [iot_usbh_cdc](https://components.espressif.com/components/espressif/iot_usbh_cdc) to communicate with USB CDC device.

## How to use the example

### Hardware Required

The example can be run on ESP32-xx based development board with USB Host interface. 

### Setup the Hardware

Connect the external USB device to ESP32-xx USB interface directly.

| ESP32-xx GPIO | USB Device  |
| ------------- | ----------- |
| USB-D+        | D+ (green)  |
| USB-D-        | D- (white)  |
| GND           | GND (black) |
| +5V           | +5V (red)   |

### Configure the project

1. The example enables one bulk interface by default.
2. Users can modify the `EXAMPLE_BULK_ITF_NUM` to `2` in `usb_cdc_basic_main.c` to enable two bulk interfaces.
3. Use the command below to set build target

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

```
I (305) USBH_CDC: iot usbh cdc version: 3.0.0
I (335) main_task: Returned from app_main()
I (22555) USBH_CDC: New device connected, address: 1
*** Device descriptor ***
bLength 18
bDescriptorType 1
bcdUSB 1.10
bDeviceClass 0xff
bDeviceSubClass 0x0
bDeviceProtocol 0x0
bMaxPacketSize0 8
idVendor 0x1a86
idProduct 0x7523
bcdDevice 2.60
iManufacturer 0
iProduct 2
iSerialNumber 0
bNumConfigurations 1
*** Configuration descriptor ***
bLength 9
bDescriptorType 2
wTotalLength 39
bNumInterfaces 1
bConfigurationValue 1
iConfiguration 0
bmAttributes 0x80
bMaxPower 98mA
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 0
        bAlternateSetting 0
        bNumEndpoints 3
        bInterfaceClass 0xff
        bInterfaceSubClass 0x1
        bInterfaceProtocol 0x2
        iInterface 0
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x82   EP 2 IN
                bmAttributes 0x2        BULK
                wMaxPacketSize 32
                bInterval 0
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x2    EP 2 OUT
                bmAttributes 0x2        BULK
                wMaxPacketSize 32
                bInterval 0
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x81   EP 1 IN
                bmAttributes 0x3        INT
                wMaxPacketSize 8
                bInterval 1
I (22645) cdc_descriptor: Found NOTIF endpoint: 1
I (22645) cdc_descriptor: Found OUT endpoint: 2
I (22655) cdc_descriptor: Found IN endpoint: 2
I (22655) cdc_basic_demo: Device Connected!
I (22665) Received string descriptor: 0x4ff1bf34   1c 03 55 00 53 00 42 00  32 00 2e 00 30 00 2d 00  |..U.S.B.2...0.-.|
I (22675) Received string descriptor: 0x4ff1bf44   53 00 65 00 72 00 69 00  61 00 6c 00 bb e3 0b aa  |S.e.r.i.a.l.....|
I (22685) Received string descriptor: 0x4ff1bf54   27 9d 01 13 08 a8 d8 42  a5 9c f4 23 8d 8e 57 60  |'......B...#..W`|
I (22695) Received string descriptor: 0x4ff1bf64   b0 77 40 dd 5d dc 5e f2  23 ed 3d bb 55 2f 99 db  |.w@.].^.#.=.U/..|
I (23705) cdc_basic_demo: Send itf0 len=4: AT

I (24705) cdc_basic_demo: Send itf0 len=4: AT
```
