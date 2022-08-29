# 球泡灯应用示例

本示例演示了如何在 ESP32 系列芯片上使用 `lightbulb_driver` 组件开发一个球泡灯应用程序。具体演示的功能如下：

- 可选择基于 WS2812 驱动、 PWM 驱动、SM2135E 驱动的演示
- 所有光色切换启用了渐变效果
- 输出 3 组预置的光色方案
- 基于最底层驱动 API 的控制

## 如何使用该示例

### 硬件需求

1. 通过 `idf.py menuconfig` 选择需要演示的驱动，并进行驱动配置

2. 硬件连接:
    - 对于 WS2812 驱动，仅需要将信号端口连接到所配置的 GPIO 上
    - 对于 PWM 驱动，需要将 R G B 三个信号端口正确连接到所配置的 GPIO 上，错误的连接将导致颜色错误。
    - 对于 SM2135E 驱动，需要将时钟信号与数据信号正确连接到所配置的 GPIO 上。

    > IIC 类调光驱动工作时需要接入市电，**连接时请断开市电，谨防触电**。

### 编译和烧写

1. 进入 `lightbulb` 目录：

    ```linux
    cd ./esp-iot-solution/examples/lighting/lightbulb
    ```

2. 使用 `idf.py` 工具设置编译芯片，随后编译下载，指令分别为：

    ```linux
    # 设置编译芯片
    idf.py idf.py set-target esp32

    # 编译并下载
    idf.py -p PORT build flash
    ```

    请将 `PORT` 替换为当前使用的端口号

### 示例输出结果

以下为 WS2812 驱动测试 log:

```log
I (272) cpu_start: Starting scheduler on PRO CPU.
W (277) lightbulb demo: Lightbulb Example Start
W (1277) hal_manage: Generate table with default parameters
I (1277) lightbulb: ---------------------------------------------------------------------
I (1277) lightbulb: driver name: WS2812
I (1277) lightbulb: low power control: disable
I (1287) lightbulb: status storage: disable
I (1287) lightbulb: status storage delay 0 ms
I (1297) lightbulb: fade: enable
I (1297) lightbulb: fade 800 ms
I (1307) lightbulb: mode: 2
I (1307) lightbulb:      color mode: enable
I (1317) lightbulb: mix cct: disable
I (1317) lightbulb: sync change: disable
I (1317) lightbulb: power limit param: 
I (1327) lightbulb:      white max brightness: 100
I (1327) lightbulb:      white min brightness: 10
I (1337) lightbulb:      white max power: 100
I (1337) lightbulb:      color max brightness: 100
I (1347) lightbulb:      color min brightness: 10
I (1357) lightbulb:      color max power: 100
I (1357) lightbulb: hue: 0, saturation: 100, value: 100
I (1367) lightbulb: cct: 0, brightness: 0
I (1367) lightbulb: select works mode: color, power status: 1
I (1377) lightbulb: ---------------------------------------------------------------------
I (1387) lightbulb: set [h:0 s:100 v:100]
I (1387) lightbulb: 8 bit color conversion value [r:255 g:0 b:0]
I (1397) lightbulb: hal write value [r:255 g:0 b:0], channel_mask:7 fade_ms:0
W (2407) output_test: rainbow
I (4407) lightbulb: set [h:0 s:100 v:100]
I (4407) lightbulb: 8 bit color conversion value [r:255 g:0 b:0]
I (4407) lightbulb: hal write value [r:255 g:0 b:0], channel_mask:7 fade_ms:800
I (6407) lightbulb: set [h:30 s:100 v:100]
I (6407) lightbulb: 8 bit color conversion value [r:255 g:127 b:0]
I (6407) lightbulb: hal write value [r:170 g:84 b:0], channel_mask:7 fade_ms:800
I (8407) lightbulb: set [h:60 s:100 v:100]
I (8407) lightbulb: 8 bit color conversion value [r:255 g:255 b:0]
I (8407) lightbulb: hal write value [r:127 g:127 b:0], channel_mask:7 fade_ms:800
I (10407) lightbulb: set [h:120 s:100 v:100]
I (10407) lightbulb: 8 bit color conversion value [r:0 g:255 b:0]
I (10407) lightbulb: hal write value [r:0 g:255 b:0], channel_mask:7 fade_ms:800
I (12407) lightbulb: set [h:210 s:100 v:100]
I (12407) lightbulb: 8 bit color conversion value [r:0 g:127 b:255]
I (12407) lightbulb: hal write value [r:0 g:84 b:170], channel_mask:7 fade_ms:800
I (14407) lightbulb: set [h:240 s:100 v:100]
I (14407) lightbulb: 8 bit color conversion value [r:0 g:0 b:255]
I (14407) lightbulb: hal write value [r:0 g:0 b:255], channel_mask:7 fade_ms:800
I (16407) lightbulb: set [h:300 s:100 v:100]
I (16407) lightbulb: 8 bit color conversion value [r:255 g:0 b:255]
I (16407) lightbulb: hal write value [r:127 g:0 b:127], channel_mask:7 fade_ms:800
W (18407) output_test: color effect
W (48407) output_test: Alexa
I (50407) lightbulb: set [h:0 s:100 v:100]
I (50407) lightbulb: 8 bit color conversion value [r:255 g:0 b:0]
I (50407) lightbulb: hal write value [r:255 g:0 b:0], channel_mask:7 fade_ms:800
I (52407) lightbulb: set [h:348 s:90 v:86]
I (52407) lightbulb: 8 bit color conversion value [r:221 g:20 b:61]
I (52407) lightbulb: hal write value [r:161 g:14 b:44], channel_mask:7 fade_ms:800
I (54407) lightbulb: set [h:17 s:51 v:100]
I (54407) lightbulb: 8 bit color conversion value [r:255 g:160 b:124]
I (54407) lightbulb: hal write value [r:120 g:75 b:58], channel_mask:7 fade_ms:800
I (56407) lightbulb: set [h:39 s:100 v:100]
I (56407) lightbulb: 8 bit color conversion value [r:255 g:165 b:0]
I (56407) lightbulb: hal write value [r:154 g:100 b:0], channel_mask:7 fade_ms:800
I (58407) lightbulb: set [h:50 s:100 v:100]
I (58407) lightbulb: 8 bit color conversion value [r:255 g:211 b:0]
I (58407) lightbulb: hal write value [r:139 g:115 b:0], channel_mask:7 fade_ms:800
I (60407) lightbulb: set [h:60 s:100 v:100]
I (60407) lightbulb: 8 bit color conversion value [r:255 g:255 b:0]
I (60407) lightbulb: hal write value [r:127 g:127 b:0], channel_mask:7 fade_ms:800
I (62407) lightbulb: set [h:120 s:100 v:100]
I (62407) lightbulb: 8 bit color conversion value [r:0 g:255 b:0]
I (62407) lightbulb: hal write value [r:0 g:255 b:0], channel_mask:7 fade_ms:800
I (64407) lightbulb: set [h:174 s:71 v:88]
I (64407) lightbulb: 8 bit color conversion value [r:63 g:226 b:209]
I (64407) lightbulb: hal write value [r:28 g:102 b:94], channel_mask:7 fade_ms:800
I (66407) lightbulb: set [h:180 s:100 v:100]
I (66407) lightbulb: 8 bit color conversion value [r:0 g:255 b:255]
I (66407) lightbulb: hal write value [r:0 g:127 b:127], channel_mask:7 fade_ms:800
I (68407) lightbulb: set [h:197 s:42 v:91]
I (68407) lightbulb: 8 bit color conversion value [r:132 g:204 b:232]
I (68407) lightbulb: hal write value [r:53 g:83 b:94], channel_mask:7 fade_ms:800
I (70407) lightbulb: set [h:240 s:100 v:100]
I (70407) lightbulb: 8 bit color conversion value [r:0 g:0 b:255]
I (70407) lightbulb: hal write value [r:0 g:0 b:255], channel_mask:7 fade_ms:800
I (72407) lightbulb: set [h:277 s:86 v:93]
I (72407) lightbulb: 8 bit color conversion value [r:155 g:33 b:237]
I (72407) lightbulb: hal write value [r:86 g:18 b:132], channel_mask:7 fade_ms:800
I (74407) lightbulb: set [h:300 s:100 v:100]
I (74407) lightbulb: 8 bit color conversion value [r:255 g:0 b:255]
I (74407) lightbulb: hal write value [r:127 g:0 b:127], channel_mask:7 fade_ms:800
I (76407) lightbulb: set [h:348 s:25 v:100]
I (76407) lightbulb: 8 bit color conversion value [r:255 g:191 b:204]
I (76407) lightbulb: hal write value [r:100 g:74 b:80], channel_mask:7 fade_ms:800
I (78407) lightbulb: set [h:255 s:50 v:100]
I (78407) lightbulb: 8 bit color conversion value [r:158 g:127 b:255]
I (78407) lightbulb: hal write value [r:74 g:59 b:120], channel_mask:7 fade_ms:800
W (80407) output_test: TEST DONE
I (80407) gpio: GPIO[18]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
W (80407) lightbulb demo: Test underlying Driver
I (86407) gpio: GPIO[18]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
W (86407) lightbulb demo: Lightbulb Example End
```
