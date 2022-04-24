# Video Recorder Example

This example will show how to recorder video to sdcard with avi format.  
See the [README.md](../README.md) file in the upper level [camera](../) directory for more information about examples.  

## How to Use Example

### Hardware Required

* A development board with a camera module and an SD card slot  (e.g., ESP32-CAM, ESP32-S3-EYE, etc.)
* A USB cable for power supply and programming
* A SDCard

You need to plug the SDCard into the socket on the board.

- Note 1:
  By default, The [SDMMC Host driver](https://docs.espressif.com/projects/esp-idf/zh_CN/v4.4.1/esp32/api-reference/peripherals/sdmmc_host.html)  is used to initialize the SD card. SD cards can be used either over SPI interface (on all ESP chips) or over SDMMC interface (on ESP32 and ESP32-S3). To use SDMMC interface, enable "Use SDMMC host" (`CONFIG_EXAMPLE_USE_SDMMC_HOST`) option. To use SPI interface, disable this option.
  
  The example will be able to mount only cards formatted using FAT32 filesystem. If the card is formatted as exFAT or some other filesystem, you have an option to format it in the example code — "Format the card if mount failed" (`CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED`).

  For more information on pin configuration for SDMMC and SDSPI, check related examples: [sd card examples](https://github.com/espressif/esp-idf/tree/release/v4.4/examples/storage/sd_card)
  
- Note 2:

  It is recommended to use 4 line SD mode to speed up the storage rate. Besides, Please format the card with an appropriate allocation unit size. Using larger allocation unit size will result in higher read/write performance and higher overhead when storing small files.

- Note 3:
  Currently, only JPEG image encoding formats are supported, so using a camera that supports JPEG encoding will help a lot to achieve smooth video.

### Configure the project

step1: Chose your taget chip.

````
idf.py menuconfig -> Camera Pin Configuration
````
step2: Configure the camera.
```
idf.py menuconfig ->component config -> Camera Configuration
```
step3: Enable the HTTP file server if you want to download the video from the web page.

```
idf.py menuconfig ->Example Configuration
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Working with the example
1. Here is an example console output. In this case a 16GB SDHC card was connected, and `EXAMPLE_FILE_SERVER_ENABLED` menuconfig option enabled. 
```
I (855) ov2640: Set PLL: clk_2x: 0, clk_div: 0, pclk_auto: 0, pclk_div: 8
I (935) video_recorder: satrt to test fps
I (1693) video_recorder: fps=25.010317, image_average_size=22142
I (1693) file manager: Initializing SD card
I (1693) file manager: Using SDMMC peripheral
I (1693) gpio: GPIO[39]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
I (1693) gpio: GPIO[38]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
I (1695) gpio: GPIO[40]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
Name: SC16G
Type: SDHC/SDXC
Speed: 20 MHz
Size: 15193MB
I (1755) avi recorder: Starting an avi [/sdcard/recorde.avi]
I (21775) avi recorder: video info: width=640 | height=480 | fps=25
I (21785) avi recorder: frame number=502, size=9482KB
I (21849) avi recorder: avi recording completed
```
2. Note down the IP assigned to your ESP module. The IP address is logged by the example as follows:
```
I (5424) example_connect: - IPv4 address: 192.168.1.100
I (5424) example_connect: - IPv6 address:    fe80:0000:0000:0000:86f7:03ff:fec0:1620, type: ESP_IP6_ADDR_IS
The following steps assume that IP address 192.168.1.100 was assigned.
```
3. Test the example interactively in a web browser. The default port is 80.

Open path `http://192.168.1.100/` or `http://192.168.1.100/index.html` to see an HTML page with list of files on the server. 
