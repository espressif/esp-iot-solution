## USB Host Audio Player Example

This example demonstrates how to use `usb_stream` component to handle an USB Headset device.

* The `usb_stream` driver currently support 48 KHz 16 bit dual channel at most due to the limitation of the USB FIFO strategy, if you want to play other format MP3 file, please convert it to 44.1KHz/48 KHz 16 bit dual channel first.
* If you want to test your own MP3 file, put it directly in the `spiffs` folder of the project and modify the macro definition `MP3_FILE_NAME`

> The default MP3 file is `spiffs/new_epic.mp3`, which is a 48 KHz 16 bit dual channel MP3 file From Royalty Free Music By 500Audio from https://500audio.com/track/new-epic_22682

## Hardware

* Development board

  1. Any `ESP32-S2`,` ESP32-S3` board with USB Host port can be used.
  2. Please note that the `esp32-sx-devkitC` board can not output 5V through USB port, If the OTG conversion cable is used directly, the device cannot be powered.
  3. For `esp32s3-usb-otg` board, please enable the USB Host power domain to power the device

* Connection

    ||USB_DP|USB_DM|
    |--|--|--|
    |ESP32-S2/S3|GPIO20|GPIO19|

## Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

* Run `. ./export.sh` to set IDF environment
* Run `idf.py set-target esp32s3` to set target chip
* Run `pip install "idf-component-manager~=1.1.4"` to upgrade your component manager if any error happends during last step
* Run `idf.py -p PORT flash monitor` to build, flash and monitor the project

(To exit the serial monitor, type `Ctrl-]`.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

## Example Output

```
I (364) USB_STREAM: UAC Streaming Config Succeed, Version: 1.0.1
I (370) USB_STREAM: USB streaming callback register succeed
I (377) USB_STREAM: Pre-alloc ctrl urb succeed, size = 1024
I (383) USB_STREAM: USB task start
I (387) USB_STREAM: USB stream task start
I (417) USB_STREAM: Port=1 init succeed, context = 0x3fcb6af8
I (417) USB_STREAM: Waiting USB Connection
I (417) USB_STREAM: USB Streaming Start Succeed
I (422) USB_STREAM: Waiting USB Device Connection
I (667) USB_STREAM: line 152 HCD_PORT_EVENT_CONNECTION
I (667) USB_STREAM: Action: ACTION_DEVICE_CONNECT
I (717) USB_STREAM: Resetting Port
I (777) USB_STREAM: Setting Port FIFO, 0
I (777) USB_STREAM: USB Speed: full-speed
I (777) USB_STREAM: ENUM Stage START, Succeed
I (781) USB_STREAM: ENUM Stage GET_SHORT_DEV_DESC, Succeed
I (788) USB_STREAM: Default pipe endpoint MPS update to 64
I (793) USB_STREAM: ENUM Stage CHECK_SHORT_DEV_DESC, Succeed
I (800) USB_STREAM: ENUM Stage SET_ADDR, Succeed
I (815) USB_STREAM: ENUM Stage CHECK_ADDR, Succeed
I (816) USB_STREAM: ENUM Stage GET_FULL_DEV_DESC, Succeed
*** Device descriptor ***
bcdUSB 2.00
bDeviceClass 0x0
bDeviceSubClass 0x0
bDeviceProtocol 0x0
bMaxPacketSize0 64
idVendor 0x339b
idProduct 0x3a07
bNumConfigurations 1
I (831) USB_STREAM: ENUM Stage CHECK_FULL_DEV_DESC, Succeed
I (838) USB_STREAM: ENUM Stage GET_SHORT_CONFIG_DESC, Succeed
I (845) USB_STREAM: ENUM Stage CHECK_SHORT_CONFIG_DESC, Succeed
I (851) USB_STREAM: ENUM Stage GET_FULL_CONFIG_DESC, Succeed
*** Configuration descriptor ***
...

I (1131) USB_STREAM: Audio control interface = 0
I (1136) USB_STREAM: Speaker feature unit = 2
I (1141) USB_STREAM:    Support volume control, ch = 1
I (1147) USB_STREAM:    Support mute control, ch = 1
I (1152) USB_STREAM: Mic feature unit = 5
I (1157) USB_STREAM:    Support volume control, ch = 1
I (1163) USB_STREAM:    Support mute control, ch = 1
I (1168) USB_STREAM: Speaker Interface found, interface = 2
I (1174) USB_STREAM:    Endpoint(ISOC) Addr = 0x3, MPS = 384
I (1181) USB_STREAM:    Speaker frequency control  Support
I (1187) USB_STREAM: ENUM Stage CHECK_FULL_CONFIG_DESC, Succeed
I (1194) USB_STREAM: ENUM Stage SET_CONFIG, Succeed
I (1200) usb_audio_player: UAC SPK: get frame list size = 3, current = 0
I (1206) usb_audio_player:       [0] ch_num = 2, bit_resolution = 16, samples_frequence = 44100, samples_frequence_min = 0, samples_frequence_max = 0
I (1220) usb_audio_player:       [1] ch_num = 2, bit_resolution = 16, samples_frequence = 48000, samples_frequence_min = 0, samples_frequence_max = 0
I (1233) usb_audio_player:       [2] ch_num = 2, bit_resolution = 16, samples_frequence = 96000, samples_frequence_min = 0, samples_frequence_max = 0
I (1247) usb_audio_player: UAC SPK: use frame[0] ch_num = 2, bit_resolution = 16, samples_frequence = 44100
I (1258) USB_STREAM: Set volume CH0: 0xf048db (45)
I (1266) USB_STREAM: SPK Feature Control: type = 4 value = 0x2d, Done
I (1320) usb_audio_player: Device connected
I (1320) USB_STREAM: Creating SPK pipe: ifc=2-1, ep=0x03, mps=192
I (1321) USB_STREAM: Suspend SPK stream. Reason: user suspend
I (1327) USB_STREAM: USB Device Connected
I (1450) usb_audio_player: mute setting: mute
I (1451) USB_STREAM: Mute CH0
I (1453) USB_STREAM: SPK Feature Control: type = 3 value = 0x1, Done
I (1505) usb_audio_player: Playing '/new_epic.mp3'
I (1505) usb_audio_player: ctx->audio_event = 2
I (1505) USB_STREAM: Resume SPK streaming
I (1509) USB_STREAM: Set Device Interface = 2, Alt = 1
I (1519) USB_STREAM: Set Device Interface Done
I (1520) USB_STREAM: Set frequence endpoint 0x03: (44100) Hz
I (1581) usb_audio_player: AUDIO_PLAYER_REQUEST_PLAY
I (1581) usb_audio_player: mute setting: unmute
I (1581) USB_STREAM: UnMute CH0
I (1585) USB_STREAM: SPK Feature Control: type = 3 value = 0x0, Done
I (1649) usb_audio_player: Re-config: speaker rate 48000, bits 16, mode STEREO
I (1650) USB_STREAM: Suspend SPK streaming
I (1651) USB_STREAM: Set Device Interface = 2, Alt = 0
I (1662) USB_STREAM: Set Device Interface Done
I (1812) USB_STREAM: SPK frame size change, ch_num = 2, bit_resolution = 16, samples_frequence = 48000
I (1812) USB_STREAM: Resume SPK streaming
I (1815) USB_STREAM: Set Device Interface = 2, Alt = 1
I (1827) USB_STREAM: Set Device Interface Done
I (1827) USB_STREAM: Set frequence endpoint 0x03: (48000) Hz

```
