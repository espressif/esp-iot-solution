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

### Functions

1.  USB MSC supported, you can read-write onboard flash or SDCard from host;
2.  Download-upload data through Wi-Fi, ESP32-SX can act as a Wi-Fi AP or STA;

### Build Example

1. Make sure `ESP-IDF` installed successfully, and checkout to specified commit [idf_usb_support_patch](../../../usb/idf_usb_support_patch/readme.md)

2. Clone `ESP-IOT-SOLUTION` repository, and checkout to branch `usb/add_usb_solutions`

    ```bash
    git clone -b usb/add_usb_solutions --recursive https://github.com/espressif/esp-iot-solution
    ```

3. Set up the `ESP-IDF` environment variables，you can refer [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables), Linux can using:

    ```bash
    $HOME/esp/esp-idf/export.sh
    ```

4. Set up the `ESP-IOT-SOLUTION` environment variables，Linux can using:

    ```bash
    export IOT_SOLUTION_PATH=$HOME/esp/esp-iot-solution
    ```

5. Set idf build target to `esp32s2` or `esp32s3`

    ```bash
    idf.py set-target esp32s2
    ```

6. Choose a develop board for quick start, default is `ESP32-S3-OTG`

    `menuconfig → Board Options → Choose Target Board`

7. Build, Flash, check log

    ```bash
    idf.py build flash monitor
    ```

### How to Use

1. Plug-in with a micro USB cable to host USB port directly;
2. By default, You can find a `1.5MB` disk in your `files resource manager`;
3. Connect with ESP32S2 through Wi-Fi, SSID:`ESP-Wireless-Disk` with no password by default;
4. Input `192.168.4.1` in your browser,  you can find a file list of the disk;
5. Everything you drag to the disk will be displayed in web page;
6. Everything you upload from web page will be displayed in the disk too (file size must < 20MB in this demo).

### Config Wi-Fi

* You can config wi-fi AP ssid and password in `menuconfig → Board Options → Board Wi-Fi Settings`, to change the esp32-sx hotspot name.
* You can set wi-fi STA ssid and password also, to make esp32-sx connect to a router at the same time.

### Known Issues

1. Files uploaded through web can not be aware by host , so Windows  `files resource manager` can not update the files list automatically. Please remount the disk to update the files list (弹出重新加载) . This bug will be fixed later.
2. Files added or removed through USB disk, sometimes can not be found by web refresh.
