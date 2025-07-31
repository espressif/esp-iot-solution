## USB MSC Wireless Disk

Using ESP32-Sx as a USB Disk with Wireless accessibility. HTTP file server be used with both upload and download capability.

**The demo is only used for function preview ,don't be surprised if you find bug**

### Hardware

- Board：ESP32-S3-USB-OTG, or any ESP32-Sx board
- MCU：ESP32-S2, ESP32-S3
- Flash：4MB NOR Flash
- Hardware Connection：
  - GPIO19 to D-
  - GPIO20 to D+
  - SDCard IO varies from boards, you can defined your own in code.

Note: If you are a self-powered device, please check [Self-Powered Device](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_device.html#self-powered-device)

### Functions

1.  USB MSC supported, you can read-write onboard flash or SDCard from host;
2.  Download-upload data through Wi-Fi, ESP32-SX can act as a Wi-Fi AP or STA;

### How to Use

1. Plug-in with a micro USB cable to host USB port directly;
2. By default, You can find a `1.5MB` disk in your `files resource manager`;
3. Connect with ESP32S2 through Wi-Fi, SSID:`ESP-Wireless-Disk` with no password by default;
4. Input `192.168.4.1` in your browser,  you can find a file list of the disk;
5. Everything you drag to the disk will be displayed in web page;
6. Everything you upload from web page will be displayed in the disk too (file size must < 20MB in this demo).

### Config Wi-Fi

* You can config wi-fi AP ssid and password in `menuconfig → USB MSC Device Demo → Wi-Fi Settings`, to change the esp32-sx hotspot name.
* You can set wi-fi STA ssid and password also, to make esp32-sx connect to a router at the same time.
* You can also configure Wi-Fi settings through the `settings` page on the web interface.

### Multi-language Support

This demo has enabled FATFS OEM multi-code page support. To manually adjust multi-language support for filenames, please adjust the following options in menuconfig:

- (Top) → <kbd>Component config</kbd> → <kbd>FAT Filesystem support</kbd> → <kbd>OEM Code Page</kbd>
  - This example defaults to <kbd>Dynamic (all code pages supported)</kbd>.
  - When set to <kbd>Dynamic (all code pages supported)</kbd> (`FATFS_CODEPAGE_DYNAMIC`), FATFS will support all code pages, but will increase the compiled binary size by ~500kB.
  - When set to other code pages, you need to select a code page that matches the character set of the filenames, otherwise it may cause garbled characters, inability to find files, or other hidden issues.
- (Top) → <kbd>Component config</kbd> → <kbd>FAT Filesystem support</kbd> → <kbd>API character encoding</kbd>
  - This example defaults to <kbd>API uses UTF-8 encoding</kbd> (`FATFS_API_ENCODING_UTF_8`).
  - This option controls the encoding used by FATFS API for reading filenames. Please refer to the description of this configuration item for details.
  - For web development, it is recommended to use UTF-8 encoding.
