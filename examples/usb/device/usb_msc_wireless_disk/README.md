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
  - When set to <kbd>Dynamic (all code pages supported)</kbd> (`FATFS_CODEPAGE_DYNAMIC`), FATFS will support all code pages, but it will increase the size of the compiled output by about 500 kB.
  - If you choose another code page, make sure that the selected code page matches the character set used in the filenames; otherwise it may cause garbled text, file not found errors, or other latent issues.
- (Top) → <kbd>Component config</kbd> → <kbd>FAT Filesystem support</kbd> → <kbd>API character encoding</kbd>
  - This example defaults to <kbd>API uses UTF-8 encoding</kbd> (`FATFS_API_ENCODING_UTF_8`).
  - This option controls the encoding method used by the filenames read by the FATFS API. For more details, please refer to the configuration help.
  - For web development, it is recommended to use UTF-8 encoding.

### Wireless Transfer Rate Optimization

**For optimal wireless access speeds, it is strongly recommended to use chips or modules with PSRAM, particularly ESP32-S3 equipped with 8MB or larger octal (OCT) PSRAM.**

#### Performance Test Results

The following chart shows the HTTP file upload speed analysis results with optimized configurations:

![ESP HTTP File Upload Speed Analysis](./tools/upload_test_20250826_150059_analysis_20250826_151202.png)

*Test results show an average upload speed of 4.24 MB/s with a maximum speed of 4.61 MB/s across 100 upload tests.*

You can further improve wireless file upload and download speeds using the following methods:

1. **Wi-Fi Configuration Optimization** (RAM impact: High)
   - The following parameters have been configured in the `sdkconfig.defaults.r8` file:
     - `CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=16`: Increases the number of static receive buffers (approx. 1.6KB each)
     - `CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=85`: Increases the number of dynamic receive buffers (approx. 1.6KB each)
     - `CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=64`: Increases the number of dynamic transmit buffers (approx. 1.6KB each)
     - `CONFIG_ESP_WIFI_TX_BA_WIN=32` and `CONFIG_ESP_WIFI_RX_BA_WIN=32`: Enlarges the Block ACK window size
     - `CONFIG_ESP_WIFI_EXTRA_IRAM_OPT=y`: Enables additional IRAM optimization (reduces RAM usage but increases IRAM usage)

2. **TCP/IP Stack Optimization** (RAM impact: Medium-High)
   - `CONFIG_LWIP_TCPIP_TASK_PRIO=23`: Increases TCP/IP task priority (no RAM impact)
   - `CONFIG_LWIP_IRAM_OPTIMIZATION=y` and `CONFIG_LWIP_EXTRA_IRAM_OPTIMIZATION=y`: Enables LWIP IRAM optimization (increases IRAM usage)
   - `CONFIG_LWIP_TCP_SND_BUF_DEFAULT=65535` and `CONFIG_LWIP_TCP_WND_DEFAULT=65535`: Enlarges TCP send and receive windows (significantly increases RAM usage per connection)
   - `CONFIG_LWIP_TCPIP_RECVMBOX_SIZE=64`: Increases TCP/IP receive mailbox size (moderate RAM impact)
   - `CONFIG_LWIP_TCP_RECVMBOX_SIZE=64` and `CONFIG_LWIP_UDP_RECVMBOX_SIZE=64`: Increases TCP and UDP receive mailbox sizes (moderate RAM impact)

3. **File Operation Buffer Optimization** (RAM impact: High)
   - `CONFIG_FILE_WRITE_BUFFER_COUNT=10`: Increases the number of write buffers
   - `CONFIG_FILE_WRITE_BUFFER_SIZE=128`: Increases the size of write buffers
   - `CONFIG_FILE_DMA_BUFFER_SIZE=32`: Sets DMA buffer size
   - Total RAM impact approx: 10 × 128 + 32 = 1312 KB

## Changelog

### 2025-08-25

- Support for 4-bit SD card interface
- The file system employs asynchronous writing to enhance performance.
- A section on optimizing wireless transmission rates has been added, along with detailed configuration guidelines.

