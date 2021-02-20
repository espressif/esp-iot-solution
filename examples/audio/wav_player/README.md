# WAV Palyer Example

This example will show how to configure and operate the LEDC peripheral to play WAV files that store in flash or sdcard.

## How to Use Example

### Hardware Required

* A development board with ESP32 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* A USB cable for power supply and programming

The following is the hardware connection:

|hardware|GPIO|
|:---:|:---:|
|speaker(R)|GPIO25|
|speaker(L)|GPIO26|

If you have ESP32-LCDKit board, just plug the speakers into the socket on the board.

If you only have ESP32-DevKitC board, connect two speakers to gpio of ESP32 directly, as shown in the figure below. The volume of this connection will sound small.

```
             47R
GPIO25 +----/\/\/\----+
                      |    
                      | /|
                     +-+ |   Speaker
                     | | |     or
                     +-+ |  earphone
                      | \|
                      |
                      +--------------+ GND

             47R
GPIO26 +----/\/\/\----+
                      |    
                      | /|
                     +-+ |   Speaker
                     | | |     or
                     +-+ |  earphone
                      | \|
                      |
                      +--------------+ GND

```

### Configure the Project

```
idf.py menuconfig
```

* Select where to play WAV files in menu "WAV Player Configuration", it will play the file in flash by default.
* Set the flash size to 4 MB under Serial Flasher Options.
* Select "Custom partition table CSV" and rename "Custom partition CSV file" to "partitions.csv".

(Note that you can use `sdkconfig.defaults`)

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

1. When choosing to play the file in flash, the application it will initialize the spiffs and then play audio file `sample.wav` with pwm.
The following is the output log:

```
I (335) wav palyer: [APP] Startup..
I (339) wav palyer: [APP] Free memory: 299824 bytes
I (344) wav palyer: [APP] IDF version: v4.2-rc-5-g511965b26
I (351) pwm_audio: timer: 0:0 | left io: 25 | right io: 26 | resolution: 10BIT
I (359) file manager: Initializing SPIFFS
I (635) file manager: Partition size: total: 2439971, used: 1935712
I (636) wav palyer: file stat info: /audio/sample.wav (1920044 bytes)...
I (640) wav palyer: frame_rate=32000, ch=1, width=16
I (30426) wav palyer: File reading complete, total: 1920000 bytes
```

2. When choosing to play the file in sdcard, the application it will initialize the sdcard and then play all files with `.wav` suffix.
The following is the output log:

```
I (354) wav palyer: [APP] Startup..
I (358) wav palyer: [APP] Free memory: 299384 bytes
I (363) wav palyer: [APP] IDF version: v4.2-rc-5-g511965b26
I (370) pwm_audio: timer: 0:0 | left io: 25 | right io: 26 | resolution: 10BIT
I (379) file manager: Initializing SD card
I (382) file manager: Using SDMMC peripheral
I (388) gpio: GPIO[13]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
Name: APPSD
Type: SDSC
Speed: 20 MHz
Size: 961MB
Traverse directory /audio
|--- System Volume Information
     |--- WPSettings.dat[12B]
     |--- IndexerVolumeGuid[76B]
|--- Michael Jackson - We Are The World (Demo).wav[56443632B]
|--- Main Titles.wav[18522044B]
|--- See you again.wav[40490720B]

I (466) wav palyer: have file [0:Michael Jackson - We Are The World (Demo).wav]
I (471) wav palyer: have file [1:Main Titles.wav]
I (477) wav palyer: have file [2:See you again.wav]
I (482) wav palyer: Start to play [0:Michael Jackson - We Are The World (Demo).wav]
I (492) wav palyer: file stat info: /audio/Michael Jackson - We Are The World (Demo).wav (56443632 bytes)...
I (503) wav palyer: frame_rate=44100, ch=2, width=16
```

## Troubleshooting

### Program upload failure

* Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
* The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.

### Card fails to initialize with `sdmmc_init_sd_scr: send_scr (1) returned 0x107` error

Check connections between the card and the ESP32. For example, if you have disconnected GPIO2 to work around the flashing issue, connect it back and reset the ESP32 (using a button on the development board, or by pressing Ctrl-T Ctrl-R in IDF Monitor).

### Card fails to initialize with `sdmmc_check_scr: send_scr returned 0xffffffff` error

Connections between the card and the ESP32 are too long for the frequency used. Try using shorter connections, or try reducing the clock speed of SD interface.

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
