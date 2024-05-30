# ESP SIMPLEFOC速度控制应用示例

本示例演示了如何在 ESP32 系列芯片上使用 `esp_simplefoc` 组件开发电机速度控制程序。具体演示的功能如下：

- 电机速度闭环
- 串口指令控制电机速度

## 如何使用该示例

### 硬件需求

1. 硬件连接:
    * 对于ESP32-S3芯片，串口连接至默认RXD0与TXD0，若需使用其他串口，需要在串口初始化部分修改对应的uart_num以及RXD与TXD。
    * 示例电机极对数为14,额定电压为12V。
    * 示例MOS预驱采用EG2133，并将LIN与HIN连接，采用3PWM驱动方式。
    * 示例角度传感器采用AS5600，若使用其他角度传感器，需要手动将初始化与角度读取函数填入至GenericSensor。
    * 示例硬件图纸：[单电机硬件](https://dl.espressif.com/AE/esp-iot-solution/docs/foc_hardware_single_bldc.pdf)
    > 不同类型电机与硬件需修改调试参数，**请确保在正确的参数下调试，谨防异常。**

### 编译和烧写

1. 进入 `foc_velocity_control` 目录：

    ```linux
    cd ./esp-iot-solution/examples/motor/foc_velocity_control
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

以下为速度控制log:

```log
 Usage          Type ST Offset   Length
I (66) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (74) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (81) boot:  2 factory          factory app      00 00 00010000 00100000
I (88) boot: End of partition table
I (93) esp_image: segment 0: paddr=00010020 vaddr=3c030020 size=0fc00h ( 64512) map
I (113) esp_image: segment 1: paddr=0001fc28 vaddr=3fc92f00 size=003f0h (  1008) load
I (113) esp_image: segment 2: paddr=00020020 vaddr=42000020 size=24d48h (150856) map
I (145) esp_image: segment 3: paddr=00044d70 vaddr=3fc932f0 size=025dch (  9692) load
I (148) esp_image: segment 4: paddr=00047354 vaddr=40374000 size=0ee18h ( 60952) load
I (171) boot: Loaded app from partition at offset 0x10000
I (171) boot: Disabling RNG early entropy source...
I (182) cpu_start: Multicore app
I (183) cpu_start: Pro cpu up.
I (183) cpu_start: Starting app cpu, entry point is 0x403752d4
I (0) cpu_start: App cpu up.
I (201) cpu_start: Pro cpu start user code
I (201) cpu_start: cpu freq: 240000000 Hz
I (201) cpu_start: Application information:
I (204) cpu_start: Project name:     foc_example
I (209) cpu_start: App version:      d580537b
I (214) cpu_start: Compile time:     Jul 19 2023 10:20:23
I (221) cpu_start: ELF file SHA256:  686b47738...
I (226) cpu_start: ESP-IDF:          v5.2-dev-1113-g28c643a56d
I (232) cpu_start: Min chip rev:     v0.0
I (237) cpu_start: Max chip rev:     v0.99
I (242) cpu_start: Chip rev:         v0.1
I (247) heap_init: Initializing. RAM available for dynamic allocation:
I (254) heap_init: At 3FC96AF8 len 00052C18 (331 KiB): DRAM
I (260) heap_init: At 3FCE9710 len 00005724 (21 KiB): STACK/DRAM
I (267) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (273) heap_init: At 600FE010 len 00001FD8 (7 KiB): RTCRAM
I (280) spi_flash: detected chip: gd
I (283) spi_flash: flash io: dio
W (287) spi_flash: Detected size(16384k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (301) sleep: Configure to isolate all GPIO pins in sleep state
I (307) sleep: Enable automatic switching of GPIO sleep configuration
I (315) app_start: Starting scheduler on CPU0
I (319) app_start: StartY�I (345) gpio: GPIO[17]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (345) gpio: GPIO[16]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (352) gpio: GPIO[15]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
MCPWM Group: 0 is idle
Auto. Current Driver uses Mcpwm GroupId:0
I (368) gpio: GPIO[17]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (377) gpio: GPIO[16]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (386) gpio: GPIO[15]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
MOT: Monitor enabled!
MOT: Init
MOT: Enable driver.
MOT: Align sensor.
MOT: sensor_direction==CW
MOT: PP check: OK!
MOT: Zero elec. angle: 4.49
MOT: No current sense.
MOT: Ready.

T2.0
2.000
```

![FOC example](https://dl.espressif.com/ae/esp-iot-solution/foc_close_loop.gif)
