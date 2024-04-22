## USB Host Audio Player Example

This example demonstrates how to use `usb_host_uac` component to handle an USB Headset device.

* If you want to test your own MP3 file, put it directly in the `spiffs` folder of the project and modify the macro definition `MP3_FILE_NAME`

> The default MP3 file is `spiffs/new_epic.mp3`, which is a 48 KHz 16 bit dual channel MP3 file From Royalty Free Music By 500Audio from https://500audio.com/track/new-epic_22682

## Hardware

* Development board

  1. Any `ESP32-S2`, `ESP32-S3`， `ESP32-P4` board with USB Host port can be used.
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
* Run `pip install "idf-component-manager~=1.1.4"` to upgrade your component manager if any error happens during last step
* Run `idf.py -p PORT flash monitor` to build, flash and monitor the project

(To exit the serial monitor, type `Ctrl-]`.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

## Example Output

```
I (689) usb_audio_player: USB Host installed
I (689) usb_audio_player: UAC Class Driver installed
*** Configuration descriptor ***
bLength 9
bDescriptorType 2
wTotalLength 280
bNumInterfaces 4
bConfigurationValue 1
iConfiguration 0
bmAttributes 0xa0
bMaxPower 100mA
        *** Interface descriptor ***

...
...

I (1544) usb_audio_player: UAC Device connected: MIC
I (1550) usb_audio_player: UAC Device connected: SPK
find UAC 1.0 Speaker device
interface number: 2
total alt interfaces number: 2
--------alt interface[1]--------- 
channels = 2 
bit_resolution = 16 
sample_freq: 
        44100
        48000
        96000
--------alt interface[2]--------- 
channels = 2 
bit_resolution = 24 
sample_freq: 
        44100
        48000
        96000
I (1583) uac-host: Set Interface 2-1
I (1586) uac-host: Set EP 03 frequency 48000
I (1714) usb_audio_player: mute setting: mute
I (1716) usb_audio_player: Playing '/new_epic.mp3'
I (1716) usb_audio_player: ctx->audio_event = 2
I (1720) usb_audio_player: AUDIO_PLAYER_REQUEST_PLAY
I (1725) usb_audio_player: mute setting: unmute
I (1740) usb_audio_player: Re-config: speaker rate 48000, bits 16, mode STEREO
I (1742) uac-host: Set Interface 2-1
I (1742) uac-host: Set EP 03 frequency 48000
I (35701) usb_audio_player: mute setting: mute
I (35703) usb_audio_player: ctx->audio_event = 0
I (35703) usb_audio_player: AUDIO_PLAYER_REQUEST_IDLE
I (35707) usb_audio_player: Play in loop
I (35712) usb_audio_player: Playing '/new_epic.mp3'
I (35717) usb_audio_player: ctx->audio_event = 2
I (35723) usb_audio_player: AUDIO_PLAYER_REQUEST_PLAY
I (35728) usb_audio_player: mute setting: unmute
I (35744) usb_audio_player: Re-config: speaker rate 48000, bits 16, mode STEREO
I (35746) uac-host: Set Interface 2-1
I (35746) uac-host: Set EP 03 frequency 48000
I (69705) usb_audio_player: mute setting: mute
I (69707) usb_audio_player: ctx->audio_event = 0
I (69707) usb_audio_player: AUDIO_PLAYER_REQUEST_IDLE
I (69711) usb_audio_player: Play in loop
I (69716) usb_audio_player: Playing '/new_epic.mp3'
I (69721) usb_audio_player: ctx->audio_event = 2
I (69727) usb_audio_player: AUDIO_PLAYER_REQUEST_PLAY
I (69732) usb_audio_player: mute setting: unmute
I (69747) usb_audio_player: Re-config: speaker rate 48000, bits 16, mode STEREO
I (69749) uac-host: Set Interface 2-1
I (69750) uac-host: Set EP 03 frequency 48000
```
