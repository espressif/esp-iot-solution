# ESP SIMPLEFOC开环速度控制应用示例

本示例演示了如何在 ESP32 系列芯片上使用 `esp_simplefoc` 组件开发电机开环速度控制程序。具体演示的功能如下：

- 电机速度开环控制

## 如何使用该示例

### 硬件需求

1. 硬件连接:
    * 对于ESP32-S3芯片，串口连接至默认RXD0与TXD0，若需使用其他串口，需要在串口初始化部分修改对应的uart_num以及RXD与TXD。
    * 示例电机极对数为14,额定电压为12V。
    * 示例MOS预驱采用EG2133，并将LIN与HIN连接，采用3PWM驱动方式。
    * 示例硬件图纸: [单电机硬件](https://dl.espressif.com/AE/esp-iot-solution/docs/foc_hardware_single_bldc.pdf)
    > 不同类型电机与硬件需修改调试参数，**请确保在正确的参数下调试，谨防异常。**

### 编译和烧写

1. 进入 `foc_openloop_control` 目录：

    ```linux
    cd ./esp-iot-solution/examples/motor/foc_openloop_control
    ```

2. 使用 `idf.py` 工具设置编译芯片，随后编译下载，指令分别为：

    ```linux
    # 设置编译芯片
    idf.py idf.py set-target esp32s3

    # 编译并下载
    idf.py -p PORT build flash
    ```

    请将 `PORT` 替换为当前使用的端口号

### 示例输出结果

以下为开环控制log:

```log
I (27) boot: ESP-IDF v5.2-dev-1113-g28c643a56d 2nd stage bootloader
I (27) boot: compile time Jul 19 2023 11:12:44
I (28) boot: Multicore bootloader
I (32) boot: chip revision: v0.1
I (36) boot.esp32s3: Boot SPI Speed : 80MHz
I (40) boot.esp32s3: SPI Mode       : DIO
I (45) boot.esp32s3: SPI Flash Size : 2MB
I (50) boot: Enabling RNG early entropy source...
I (55) boot: Partition Table:
I (59) boot: ## Label            Usage          Type ST Offset   Length
I (66) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (74) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (81) boot:  2 factory          factory app      00 00 00010000 00100000
I (88) boot: End of partition table
I (93) esp_image: segment 0: paddr=00010020 vaddr=3c030020 size=0f220h ( 61984) map
I (112) esp_image: segment 1: paddr=0001f248 vaddr=3fc92700 size=00dd0h (  3536) load
I (113) esp_image: segment 2: paddr=00020020 vaddr=42000020 size=21428h (136232) map
I (142) esp_image: segment 3: paddr=00041450 vaddr=3fc934d0 size=01bdch (  7132) load
I (144) esp_image: segment 4: paddr=00043034 vaddr=40374000 size=0e690h ( 59024) load
I (167) boot: Loaded app from partition at offset 0x10000
I (167) boot: Disabling RNG early entropy source...
I (179) cpu_start: Multicore app
I (179) cpu_start: Pro cpu up.
I (179) cpu_start: Starting app cpu, entry point is 0x403752ac
0x403752ac: call_start_cpu1 at /home/yanke/esp/esp-idf/components/esp_system/port/cpu_start.c:154

I (0) cpu_start: App cpu up.
I (197) cpu_start: Pro cpu start user code
I (197) cpu_start: cpu freq: 240000000 Hz
I (197) cpu_start: Application information:
I (200) cpu_start: Project name:     foc_openloop_control
I (207) cpu_start: App version:      d580537b
I (212) cpu_start: Compile time:     Jul 19 2023 11:12:38
I (218) cpu_start: ELF file SHA256:  4515fda00...
I (223) cpu_start: ESP-IDF:          v5.2-dev-1113-g28c643a56d
I (230) cpu_start: Min chip rev:     v0.0
I (234) cpu_start: Max chip rev:     v0.99 
I (239) cpu_start: Chip rev:         v0.1
I (244) heap_init: Initializing. RAM available for dynamic allocation:
I (251) heap_init: At 3FC961C8 len 00053548 (333 KiB): DRAM
I (257) heap_init: At 3FCE9710 len 00005724 (21 KiB): STACK/DRAM
I (264) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (270) heap_init: At 600FE010 len 00001FD8 (7 KiB): RTCRAM
I (277) spi_flash: detected chip: gd
I (281) spi_flash: flash io: dio
W (284) spi_flash: Detected size(16384k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (298) sleep: Configure to isolate all GPIO pins in sleep state
I (305) sleep: Enable automatic switching of GPIO sleep configuration
I (312) app_start: Starting scheduler on CPU0
I (317) app_start: Start)�I (331) gpio: GPIO[17]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (332) gpio: GPIO[16]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (339) gpio: GPIO[15]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
MCPWM Group: 0 is idle
Auto. Current Driver uses Mcpwm GroupId:0
I (354) gpio: GPIO[17]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
I (363) gpio: GPIO[16]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
I (373) gpio: GPIO[15]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
MOT: Init
MOT: Enable driver.
```