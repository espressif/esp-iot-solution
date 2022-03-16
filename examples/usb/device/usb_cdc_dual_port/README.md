## USB 双虚拟串口例程

使用 ESP32-S2/S3 的 USB 功能实现两个虚拟串口，主机可以同时使用两个端口与 ESP32-S2/S3 进行通信

![](./cdc_dual_port.png)

### 编译示例代码

1. 确认 `ESP-IDF` 环境成功搭建，并按照说明文件切换到指定 commit [idf_usb_support_patch](../../../usb/idf_usb_support_patch/readme.md)

2. 确认已经完整下载 `ESP-IOT-SOLUTION` 仓库，并切换到 `usb/add_usb_solutions` 分支

    ```bash
    git clone -b usb/add_usb_solutions --recursive https://github.com/espressif/esp-iot-solution
    ```

3. 添加 `ESP-IDF` 环境变量，Linux 方法如下，其它平台请查阅 [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables)

    ```bash
    . $HOME/esp/esp-idf/export.sh
    ```

4. 添加 `ESP-IOT-SOLUTION` 环境变量，Linux 方法如下:

    ```bash
    export IOT_SOLUTION_PATH=$HOME/esp/esp-iot-solution
    ```

5. 设置编译目标为 `esp32s2` 或 `esp32s3`

    ```bash
    idf.py set-target esp32s2
    ```

6. 编译、下载、查看输出

    ```bash
    idf.py build flash monitor
    ```

### Log 输出

```
I (0) cpu_start: App cpu up.
I (180) cpu_start: Pro cpu start user code
I (180) cpu_start: cpu freq: 160000000
I (180) cpu_start: Application information:
I (183) cpu_start: Project name:     usb_dual_cdc
I (188) cpu_start: App version:      3505a91f
I (193) cpu_start: Compile time:     Mar 16 2022 11:59:11
I (199) cpu_start: ELF file SHA256:  0eb35b6feadc80ae...
I (205) cpu_start: ESP-IDF:          v4.4-316-g89f57f3402
I (211) heap_init: Initializing. RAM available for dynamic allocation:
I (219) heap_init: At 3FC97B58 len 000484A8 (289 KiB): D/IRAM
I (225) heap_init: At 3FCE0000 len 0000EE34 (59 KiB): STACK/DRAM
I (232) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (238) heap_init: At 600FE000 len 00002000 (8 KiB): RTCRAM
I (245) spi_flash: detected chip: gd
I (248) spi_flash: flash io: dio
W (252) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (266) sleep: Configure to isolate all GPIO pins in sleep state
I (272) sleep: Enable automatic switching of GPIO sleep configuration
I (280) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (300) USB_CDC_MAIN: USB initialization
I (300) tusb_desc: using custom config desc
I (300) tusb_desc: config desc size=141
I (310) tusb_desc: 
┌─────────────────────────────────┐
│  USB Device Descriptor Summary  │
├───────────────────┬─────────────┤
│bDeviceClass       │ 239         │
├───────────────────┼─────────────┤
│bDeviceSubClass    │ 2           │
├───────────────────┼─────────────┤
│bDeviceProtocol    │ 1           │
├───────────────────┼─────────────┤
│bMaxPacketSize0    │ 64          │
├───────────────────┼─────────────┤
│idVendor           │ 0x303a      │
├───────────────────┼─────────────┤
│idProduct          │ 0x4001      │
├───────────────────┼─────────────┤
│bcdDevice          │ 0x100       │
├───────────────────┼─────────────┤
│iManufacturer      │ 0x1         │
├───────────────────┼─────────────┤
│iProduct           │ 0x2         │
├───────────────────┼─────────────┤
│iSerialNumber      │ 0x3         │
├───────────────────┼─────────────┤
│bNumConfigurations │ 0x1         │
└───────────────────┴─────────────┘
I (480) TinyUSB: TinyUSB Driver installed
I (480) USB_CDC_MAIN: USB initialization DONE
I (10530) USB_CDC_MAIN: Line state changed! itf:0 dtr:0, rst:0
I (11360) USB_CDC_MAIN: Line state changed! itf:1 dtr:0, rst:0
```
