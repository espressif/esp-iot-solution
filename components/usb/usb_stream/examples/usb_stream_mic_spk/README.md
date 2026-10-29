## USB Stream Example

This example demonstrates how to use `usb_stream` component with an ESP device. Example does the following steps:

1. Config a UVC function with specified frame resolution and frame rate, register frame callback
2. Config a UAC function with one microphone and one speaker stream, register mic frame callback
3. Start the USB streaming
4. In image frame callback, if `ENABLE_UVC_WIFI_XFER` is set to `1`, the real-time image can be fetched through ESP32Sx's Wi-Fi softAP (ssid: ESP32S3-UVC, http: 192.168.4.1), else will just print the image message
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

* Run `. ./export.sh` to set IDF environment
* Run `idf.py set-target esp32s3` to set target chip
* Run `pip install "idf-component-manager~=1.1.4"` to upgrade your component manager if any error happens during last step
* Run `idf.py -p PORT flash monitor` to build, flash and monitor the project

(To exit the serial monitor, type `Ctrl-]`.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

## Example Output

```
I (439) UVC_STREAM: line 486 HCD_PORT_EVENT_CONNECTION
I (489) UVC_STREAM: Resetting Port
I (549) UVC_STREAM: Setting Port FIFO
I (549) UVC_STREAM: Getting Port Speed
I (549) UVC_STREAM: Port speed = 1
I (549) UVC_STREAM: USB Speed: full-speed
I (550) UVC_STREAM: Set Device Addr = 1
I (550) UVC_STREAM: Set Device Addr Done
I (551) UVC_STREAM: get device desc
I (551) UVC_STREAM: get device desc, actual_num_bytes:26
*** Device descriptor ***
bcdUSB 2.00
bDeviceClass 0xef
bDeviceSubClass 0x2
bDeviceProtocol 0x1
bMaxPacketSize0 64
idVendor 0x1871
idProduct 0xff50
bNumConfigurations 1
I (552) UVC_STREAM: get short config desc
I (553) UVC_STREAM: get config desc, actual_num_bytes:16
I (553) UVC_STREAM: get full config desc
I (561) UVC_STREAM: get full config desc, actual_num_bytes:397
*** Configuration descriptor ***
wTotalLength 389
bNumInterfaces 2
bConfigurationValue 1
*** Interface Association Descriptor: Video ***
        *** Interface descriptor ***
        bInterfaceNumber 0
        bAlternateSetting 0
        bNumEndpoints 1
        bInterfaceClass 0xe (Video)
        bInterfaceSubClass 0x1
                *** Endpoint descriptor ***
                bEndpointAddress 0x83   EP 3 IN
                bmAttributes 0x3        INT
                wMaxPacketSize 16
                bInterval 6
        *** Interface descriptor ***
        bInterfaceNumber 1
        bAlternateSetting 0
        bNumEndpoints 0
        bInterfaceClass 0xe (Video)
        bInterfaceSubClass 0x2
        *** Class-specific VS Interface Descriptor ***
        bNumFormats 2
        *** VS Format MJPEG Descriptor ***
        bFormatIndex 0x2
        bNumFrameDescriptors 4
        bDefaultFrameIndex 1
        *** VS MJPEG Frame Descriptor ***
        bFrameIndex 0x1
        wWidth 640
        wHeigh 480
        FrameInterval[0] 666666
        *** VS MJPEG Frame Descriptor ***
        bFrameIndex 0x2
        wWidth 480
        wHeigh 320
        FrameInterval[0] 666666
        FrameInterval[1] 1000000
        FrameInterval[2] 2000000
        *** VS MJPEG Frame Descriptor ***
        bFrameIndex 0x3
        wWidth 352
        wHeigh 288
        FrameInterval[0] 666666
        *** VS MJPEG Frame Descriptor ***
        bFrameIndex 0x4
        wWidth 320
        wHeigh 240
        FrameInterval[0] 666666
        *** Interface descriptor ***
        bInterfaceNumber 1
        bAlternateSetting 1
        bNumEndpoints 1
        bInterfaceClass 0xe (Video)
        bInterfaceSubClass 0x2
                *** Endpoint descriptor ***
                bEndpointAddress 0x81   EP 1 IN
                bmAttributes 0x5        ISOC
                wMaxPacketSize 956
                bInterval 1
W (568) UVC_STREAM: VS Interface(MPS < 600) NOT found
W (569) UVC_STREAM: Try with first alt-interface config
I (569) UVC_STREAM: Actual MJPEG format index = 2, contains 4 frames
I (569) UVC_STREAM: Actual MJPEG width*height: 320*240, frame index = 4
I (570) UVC_STREAM: UVC Streaming Config Succeed
W (570) UVC_STREAM: UAC 1.0 TYPE1 NOT found
I (570) UVC_STREAM: Set Device Configuration = 1
I (571) UVC_STREAM: Set Device Configuration Done
I (571) UVC_STREAM: SET_CUR Probe
I (575) UVC_STREAM: SET_CUR Probe Done
I (575) UVC_STREAM: GET_CUR Probe
I (576) UVC_STREAM: GET_CUR Probe Done, actual_num_bytes:34
I (576) UVC_STREAM: SET_CUR COMMIT
I (580) UVC_STREAM: SET_CUR COMMIT Done
I (580) UVC_STREAM: Set Device Interface = 1, Alt = 1
I (930) UVC_STREAM: Set Device Interface Done
I (930) UVC_STREAM: usb stream task start
I (930) UVC_STREAM: uvc stream urb ready
I (931) UVC_STREAM: Sample processing task started
I (931) UVC_STREAM: Creating uvc in(isoc) pipe itf = 1-1, ep = 0x81
I (931) UVC_STREAM: uvc streaming...
I (1492) uvc_mic_spk_demo: uvc callback! frame_format = 7, seq = 1, width = 320, height = 240, length = 8752, ptr = 0
I (1557) uvc_mic_spk_demo: uvc callback! frame_format = 7, seq = 2, width = 320, height = 240, length = 9496, ptr = 0
I (1624) uvc_mic_spk_demo: uvc callback! frame_format = 7, seq = 3, width = 320, height = 240, length = 7920, ptr = 0
I (1688) uvc_mic_spk_demo: uvc callback! frame_format = 7, seq = 4, width = 320, height = 240, length = 7880, ptr = 0
I (1752) uvc_mic_spk_demo: uvc callback! frame_format = 7, seq = 5, width = 320, height = 240, length = 8616, ptr = 0
I (1820) uvc_mic_spk_demo: uvc callback! frame_format = 7, seq = 6, width = 320, height = 240, length = 8808, ptr = 0
I (1885) uvc_mic_spk_demo: uvc callback! frame_format = 7, seq = 7, width = 320, height = 240, length = 8912, ptr = 0
I (1952) uvc_mic_spk_demo: uvc callback! frame_format = 7, seq = 8, width = 320, height = 240, length = 8832, ptr = 0
I (2017) uvc_mic_spk_demo: uvc callback! frame_format = 7, seq = 9, width = 320, height = 240, length = 8816, ptr = 0

```
