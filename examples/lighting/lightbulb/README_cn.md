# 球泡灯应用示例

本示例演示了如何在 ESP32 系列芯片上使用 `lightbulb_driver` 组件开发一个球泡灯应用程序。具体演示的功能如下：

- 可选择基于 WS2812 驱动、 PWM 驱动、BP5758D 驱动的演示
- 可配置不同的灯珠组合方案
- 所有光色切换启用了渐变效果
- 输出预置的光色方案
- 基于最底层驱动 API 的控制

## 如何使用该示例

### 硬件需求

1. 通过 `idf.py menuconfig` 选择需要演示的驱动，并进行驱动配置

2. 硬件连接:
    - 对于 WS2812 驱动，仅需要将信号端口连接到所配置的 GPIO 上。
    - 对于 PWM 驱动，需要将 R G B 三个信号端口正确连接到所配置的 GPIO 上，错误的连接将导致颜色错误。
    - 对于 BP5758D 驱动，需要将时钟信号与数据信号正确连接到所配置的 GPIO 上。

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
W (293) lightbulb demo: Lightbulb Example Start
I (303) lightbulb: set [h:0 s:100 v:100]
I (303) lightbulb: 8 bit color conversion value [r:255 g:0 b:0]
I (313) lightbulb: hal write value [r:255 g:0 b:0], channel_mask:7 fade_ms:0
I (323) lightbulb: ---------------------------------------------------------------------
I (333) lightbulb: lightbulb driver component version: 1.0.0
I (333) lightbulb: driver name: WS2812
I (343) lightbulb: low power control: disable
I (343) lightbulb: status storage: disable
I (353) lightbulb: status storage delay 0 ms
I (353) lightbulb: fade: enable
I (363) lightbulb: fade 800 ms
I (363) lightbulb: led_beads: 4
I (363) lightbulb: hardware cct: No
I (373) lightbulb: precise cct control: enable
I (373) lightbulb: sync change: disable
I (383) lightbulb: auto on: enable
I (383) lightbulb:      color mode: enable
I (393) lightbulb: sync change: disable
I (393) lightbulb: power limit param: 
I (393) lightbulb:      white max brightness: 100
I (403) lightbulb:      white min brightness: 1
I (413) lightbulb:      white max power: 100
I (413) lightbulb:      color max brightness: 100
I (423) lightbulb:      color min brightness: 1
I (423) lightbulb:      color max power: 300
I (433) lightbulb: hue: 0, saturation: 100, value: 100
I (433) lightbulb: select works mode: color, power status: 1
I (443) lightbulb: ---------------------------------------------------------------------
W (453) output_test: rainbow
I (2453) lightbulb: set [h:0 s:100 v:100]
I (2453) lightbulb: 8 bit color conversion value [r:255 g:0 b:0]
I (2453) lightbulb: hal write value [r:255 g:0 b:0], channel_mask:7 fade_ms:800
I (4453) lightbulb: set [h:30 s:100 v:100]
I (4453) lightbulb: 8 bit color conversion value [r:255 g:127 b:0]
I (4453) lightbulb: hal write value [r:255 g:127 b:0], channel_mask:7 fade_ms:800
I (6453) lightbulb: set [h:60 s:100 v:100]
I (6453) lightbulb: 8 bit color conversion value [r:255 g:255 b:0]
I (6453) lightbulb: hal write value [r:255 g:255 b:0], channel_mask:7 fade_ms:800
...
...
...
W (80407) output_test: TEST DONE
I (80407) gpio: GPIO[18]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
W (80407) lightbulb demo: Test underlying Driver
I (86407) gpio: GPIO[18]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
W (86407) lightbulb demo: Lightbulb Example End
```
