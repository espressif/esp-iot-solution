## USB Stream Example

This example demonstrates how to use `usb_stream` component with an ESP device. Example does the following steps:

1. Config a UVC function with specified frame resolution and frame rate, register frame callback
2. Config a UAC function with one microphone and one speaker stream, register mic frame callback
3. Start the USB streaming
4. In image frame callback, if `ENABLE_UVC_WIFI_XFER` is set to `1`, the real-time image can be fetched through ESP32Sx's Wi-Fi softAP (ssid: ESP32S2-UVC, http: 192.168.4.1), else will just print the image message
5. In mic callback, if `ENABLE_UAC_MIC_SPK_LOOPBACK` is set to `1`, the mic data will be write back to usb speaker, else will just print mic data message
6. For speaker, if `ENABLE_UAC_MIC_SPK_LOOPBACK` is set to `0`, the default sound will be played back

## Hardware

* Development board

  1. Any `ESP32-S2`,` ESP32-S3` board with USB Host port can be used.
  2. Please note that the `esp32-sx-devkitC` board can not output 5V through USB port, If the OTG conversion cable is used directly, the device cannot be powered.
  3. For `esp32s3-usb-otg` board, please enable the USB Host power domain to power the device

* Connection

    ||USB_DP|USB_DM|
    |--|--|--|
    |ESP32-S2/S3|GPIO20|GPIO19|

* Camera module
  * please refer the README of `usb_stream` for the Camera requirement


## Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

Run `idf.py set-target esp32s3` to set target chip.

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type `Ctrl-]`.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

## Example Output

```
I (304) wifi_init: rx ba win: 6
I (304) wifi_init: tcpip mbox: 32
I (304) wifi_init: udp mbox: 6
I (304) wifi_init: tcp mbox: 6
I (304) wifi_init: tcp tx win: 5744
I (305) wifi_init: tcp rx win: 5744
I (305) wifi_init: tcp mss: 1440
I (305) wifi_init: WiFi IRAM OP enabled
I (305) wifi_init: WiFi RX IRAM OP enabled
I (309) camera wifi: wifi_init_softap finished.SSID:ESP32S2-UVC password:
I (310) phy_init: phy_version 503,13653eb,Jun  1 2022,17:47:08
I (350) wifi:mode : softAP (f4:12:fa:d8:db:91)
I (355) wifi:Total power save buffer number: 16
I (355) wifi:Init max length of beacon: 752/752
I (355) wifi:Init max length of beacon: 752/752
I (356) camera_httpd: Starting web server on port: '80'
I (359) camera_httpd: Starting stream server on port: '81'
I (362) UVC_STREAM: Isoc transfer mode
I (362) UVC_STREAM: UVC Streaming Config Succeed
I (362) UVC_STREAM: UAC Streaming Config Succeed
I (393) UVC_STREAM: Port=1 init succeed
I (393) UVC_STREAM: Waiting USB Connection
I (413) UVC_STREAM: USB Streaming Starting
I (413) uvc_mic_spk_demo: usb streaming start succeed
W (413) UVC_STREAM: Device Not Connected
I (413) UVC_STREAM: Will take effect after device connected
I (4961) UVC_STREAM: line 477 HCD_PORT_EVENT_CONNECTION
I (5011) UVC_STREAM: Resetting Port
I (5071) UVC_STREAM: Setting Port FIFO
I (5071) UVC_STREAM: Getting Port Speed
I (5071) UVC_STREAM: Port speed = 1
I (5071) UVC_STREAM: USB Speed: full-speed
I (5072) UVC_STREAM: Set Device Addr = 1
I (5072) UVC_STREAM: Set Device Addr Done
I (5072) UVC_STREAM: get device desc
I (5074) UVC_STREAM: get device desc, actual_num_bytes:26
*** Device descriptor ***
bLength 18
bDescriptorType 1
bcdUSB 1.10
bDeviceClass 0xef
bDeviceSubClass 0x2
bDeviceProtocol 0x1
bMaxPacketSize0 64
idVendor 0x4c4a
idProduct 0x4c55
bcdDevice 1.00
iManufacturer 1
iProduct 2
iSerialNumber 0
bNumConfigurations 1
I (5075) UVC_STREAM: get short config desc
I (5084) UVC_STREAM: get config desc, actual_num_bytes:16
I (5084) UVC_STREAM: get full config desc
I (5094) UVC_STREAM: get full config desc, actual_num_bytes:608
*** Configuration descriptor ***
bLength 9
bDescriptorType 2
wTotalLength 600
bNumInterfaces 5
bConfigurationValue 1
iConfiguration 0
bmAttributes 0x80
bMaxPower 400mA
*** Interface Association Descriptor ***

I (5113) UVC_STREAM: Set Device Configuration = 1
I (5113) UVC_STREAM: Set Device Configuration Done
I (5113) UVC_STREAM: SET_CUR Probe
I (5114) UVC_STREAM: SET_CUR Probe Done
I (5114) UVC_STREAM: GET_CUR Probe
bmHint: 0001
bFormatIndex: 1
bFrameIndex: 2
dwFrameInterval: 400000
wKeyFrameRate: 0
wPFrameRate: 0
wCompQuality: 0
wCompWindowSize: 0
wDelay: 0
dwMaxVideoFrameSize: 4177920
dwMaxPayloadTransferSize: 512
dwClockFrequency: 0
bmFramingInfo: 0
bPreferredVersion: 0
bMinVersion: 0
bMaxVersion: 0
bInterfaceNumber: 0
I (5116) UVC_STREAM: GET_CUR Probe Done, actual_num_bytes:34
I (5116) UVC_STREAM: SET_CUR COMMIT
I (5116) UVC_STREAM: SET_CUR COMMIT Done
I (5117) UVC_STREAM: Set Device Interface = 1, Alt = 1
I (5119) UVC_STREAM: Set Device Interface Done
I (5119) UVC_STREAM: Set Device Interface = 4, Alt = 1
I (5120) UVC_STREAM: Set Device Interface Done
I (5121) UVC_STREAM: SET_CUR frequence 16000
I (5124) UVC_STREAM: SET_CUR Done
I (5124) UVC_STREAM: Set Device Interface = 3, Alt = 1
I (5127) UVC_STREAM: Set Device Interface Done
I (5127) UVC_STREAM: SET_CUR mute, 0
I (5128) UVC_STREAM: SET_CUR mute Done
I (5128) UVC_STREAM: SET_CUR volume e670 (10)
I (5128) UVC_STREAM: SET_CUR volume Done
I (5129) UVC_STREAM: SET_CUR frequence 16000
I (5129) UVC_STREAM: SET_CUR Done
I (5129) UVC_STREAM: usb stream task start
I (5129) UVC_STREAM: mic stream urb ready
I (5129) UVC_STREAM: spk stream urb ready
I (5130) UVC_STREAM: uvc stream urb ready
I (5130) UVC_STREAM: Sample processing task started
I (5130) UVC_STREAM: Creating mic in pipe itf = 4, ep = 0x82, 
I (5131) UVC_STREAM: mic streaming...
I (5131) UVC_STREAM: Creating spk out pipe itf = 3, ep = 0x02
I (5131) UVC_STREAM: spk streaming...
I (5131) UVC_STREAM: Creating uvc in(isoc) pipe itf = 1-1, ep = 0x84
I (5132) UVC_STREAM: uvc streaming...
I (5541) uvc_mic_spk_demo: uvc callback! frame_format = 7, seq = 1, width = 640, height = 480, length = 8905, ptr = 0

```
