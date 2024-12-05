## 传感器控制 LED

本示例演示了如何在 `esp32-meshkit-sense` 开发板上使用 `sensor_hub` 组件快速开发一个温湿度传感器控制 LED 灯的应用程序，`sensor_hub` 可以帮助用户处理底层传感器的复杂实现，通过事件消息将数据包发送给应用程序，大大降低了传感器应用的开发难度。

## 如何使用该示例

### 硬件需求

1. 本示例默认使用 `common_components/boards` 组件中包含的 `esp32-meshkit-sense` 开发板，您也可根据 `examples/common_components/boards/esp32-meshkit-sense/iot_board.h` 中的开发板引脚定义使用任意 `ESP32` 或 `ESP32S2` 开发板实现; 
2. 本示例默认使用 `sensor_hub` 中已支持温湿度传感器 `SHT3X` 进行实验，您可以按照 `SHT3X` 注册 `sensor_hub` 的方式自行注册其他传感器。

3. 传感器接线图：

    ```
    esp32-meshkit-sense      SHT3X
                            --       
                            | || VCC
        [SCL 32]  --------> | || SCL
        [SDA 33]  --------> | || SDA
                            | || GND
                            --      
    ```

### 编译和烧写

1. 添加 `IOT_SOLUTION_PATH` 环境变量：

    ```
    export IOT_SOLUTION_PATH=PATH
    ```
    请将 `PATH` 替换为 `esp-iot-solution` 目录所在路径，例如 `~/esp-iot-solution`

2. 使用`idf.py`工具编译、下载程序，指令为：

    ```
    idf.py -p PORT build flash
    ```

    请将 `PORT` 替换为当前使用的端口号

3. 可使用 `monitor` 查看程序输出，指令为：

    ```
    idf.py -p PORT monitor
    ```

### 示例输出结果

以下是挂载了`SHT3X` 光照度传感器输出结果：

```
I (21) boot: ESP-IDF v4.4.8 2nd stage bootloader
I (21) boot: compile time 11:22:06
I (21) boot: chip revision: v0.0
I (24) boot.esp32s2: SPI Speed      : 80MHz
I (29) boot.esp32s2: SPI Mode       : DIO
I (34) boot.esp32s2: SPI Flash Size : 2MB
I (38) boot: Enabling RNG early entropy source...
I (44) boot: Partition Table:
I (47) boot: ## Label            Usage          Type ST Offset   Length
I (55) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (62) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (70) boot:  2 factory          factory app      00 00 00010000 00100000
I (77) boot: End of partition table
I (81) esp_image: segment 0: paddr=00010020 vaddr=3f000020 size=0b81ch ( 47132) map
I (99) esp_image: segment 1: paddr=0001b844 vaddr=3ffbf950 size=0162ch (  5676) load
I (101) esp_image: segment 2: paddr=0001ce78 vaddr=40022000 size=031a0h ( 12704) load
I (110) esp_image: segment 3: paddr=00020020 vaddr=40080020 size=1d108h (119048) map
I (139) esp_image: segment 4: paddr=0003d130 vaddr=400251a0 size=0a7a4h ( 42916) load
I (156) boot: Loaded app from partition at offset 0x10000
I (156) boot: Disabling RNG early entropy source...
I (167) cpu_start: Unicore app
I (168) cache: Instruction cache        : size 8KB, 4Ways, cache line size 32Byte
I (168) cpu_start: Pro cpu up.
I (192) cpu_start: Pro cpu start user code
I (192) cpu_start: cpu freq: 160000000
I (192) cpu_start: Application information:
I (195) cpu_start: Project name:     sensors_monitor
I (200) cpu_start: App version:      6ecfeec7-dirty
I (206) cpu_start: Compile time:     Dec 19 2024 11:22:02
I (212) cpu_start: ELF file SHA256:  4dc3344f3c816cae...
I (218) cpu_start: ESP-IDF:          v4.4.8
I (223) cpu_start: Min chip rev:     v0.0
I (228) cpu_start: Max chip rev:     v1.99 
I (232) cpu_start: Chip rev:         v0.0
I (237) heap_init: Initializing. RAM available for dynamic allocation:
I (244) heap_init: At 3FFC19E0 len 0003A620 (233 KiB): DRAM
I (251) heap_init: At 3FFFC000 len 00003A10 (14 KiB): DRAM
I (257) heap_init: At 3FF9E000 len 00002000 (8 KiB): RTCRAM
I (264) spi_flash: detected chip: generic
I (268) spi_flash: flash io: dio
W (272) spi_flash: Detected size(4096k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (285) cpu_start: Starting scheduler on PRO CPU.
I (291) Board_Common: Board Init ...
W (291) Board_Common: no gpio defined
I (291) Board_Common: BOARD_LED_NUM = 1
I (301) Board_Common: led(RGB) init succeed io=17
I (301) i2c_bus: i2c0 bus inited
I (311) i2c_bus: I2C Bus Config Succeed, Version: 1.1.0
I (311) Board_Common: i2c bus create succeed
I (321) Board_Common: Board Info: 

BOARD_NAME: ESP32-S2-Saola-1
BOARD_VENDOR: Espressif
BOARD_URL: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-saola-1-v1.2.html

I (341) Board_Common: Board Init Done !
I (341) SENSOR_HUB: Find sht3x driver, type: HUMITURE
I (351) SENSOR_HUB: Sensor created, Task name = SENSOR_HUB, Type = HUMITURE, Sensor Name = sht3x, Mode = MODE_POLLING, Min Delay = 200 ms
I (361) SENSOR_HUB: task: sensor_default_task created!
I (371) SENSOR_LOOP: event loop created succeed
I (371) SENSOR_LOOP: register a new handler to event loop succeed
I (381) SENSOR_CONTROL: Timestamp = 855805 - sht3x_0x44 STARTED
I (581) SENSOR_CONTROL: Timestamp = 1052607 - sht3x_0x44 TEMP_DATA_READY - temperature=25.00

I (581) SENSOR_CONTROL: LED turn on
I (581) SENSOR_CONTROL: Timestamp = 1052607 - event id = 14
I (781) SENSOR_CONTROL: Timestamp = 1251619 - sht3x_0x44 TEMP_DATA_READY - temperature=25.00

I (781) SENSOR_CONTROL: Timestamp = 1251619 - event id = 14
I (981) SENSOR_CONTROL: Timestamp = 1452601 - sht3x_0x44 TEMP_DATA_READY - temperature=25.00

I (981) SENSOR_CONTROL: Timestamp = 1452601 - event id = 14
I (1181) SENSOR_CONTROL: Timestamp = 1651617 - sht3x_0x44 TEMP_DATA_READY - temperature=25.00

I (1181) SENSOR_CONTROL: Timestamp = 1651617 - event id = 14
I (1381) SENSOR_CONTROL: Timestamp = 1852597 - sht3x_0x44 TEMP_DATA_READY - temperature=25.00

I (1381) SENSOR_CONTROL: Timestamp = 1852597 - event id = 14
I (1581) SENSOR_CONTROL: Timestamp = 2051617 - sht3x_0x44 TEMP_DATA_READY - temperature=25.00

I (1581) SENSOR_CONTROL: Timestamp = 2051617 - event id = 14
I (1781) SENSOR_CONTROL: Timestamp = 2252597 - sht3x_0x44 TEMP_DATA_READY - temperature=25.00

I (1781) SENSOR_CONTROL: Timestamp = 2252597 - event id = 14

```
