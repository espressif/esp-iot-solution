# 球泡灯应用示例

本示例演示了如何在 ESP32 系列芯片上使用 `lightbulb_driver` 组件开发一个球泡灯应用程序。具体演示的功能如下：

- 可选择基于 WS2812 驱动、SM16825E 驱动、 PWM 驱动、BP5758D 驱动的演示
- 可配置不同的灯珠组合方案
- 所有光色切换启用了渐变效果
- 输出预置的光色方案
- 基于最底层驱动 API 的控制

## 如何使用该示例

### 硬件需求

1. 通过 `idf.py menuconfig` 选择需要演示的驱动，并进行驱动配置
2. 硬件连接:

   - 对于 WS2812 驱动，仅需要将信号端口连接到所配置的 GPIO 上。
   - 对于 SM16825E 驱动，需要将数据信号端口连接到所配置的 GPIO 上，支持5通道RGBCW控制。
   - 对于 PWM 驱动，需要将 R G B 三个信号端口正确连接到所配置的 GPIO 上，错误的连接将导致颜色错误。
   - 对于 BP5758D 驱动，需要将时钟信号与数据信号正确连接到所配置的 GPIO 上。

   > IIC 类调光驱动工作时需要接入市电，**连接时请断开市电，谨防触电**。
   >

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

以下为SM16825E 驱动测试 log:

```log
W (293) lightbulb demo: Lightbulb Example Start
I (303) lightbulb demo: SM16825E Driver Selected - LED Count: 4, GPIO: 18, Frequency: 800000 Hz
I (313) lightbulb: set [h:0 s:100 v:100]
I (313) lightbulb: 8 bit color conversion value [r:255 g:0 b:0]
I (323) lightbulb: hal write value [r:255 g:0 b:0], channel_mask:7 fade_ms:0
I (333) lightbulb: ---------------------------------------------------------------------
I (343) lightbulb: lightbulb driver component version: 1.0.0
I (343) lightbulb: driver name: SM16825E
I (353) lightbulb: low power control: disable
I (353) lightbulb: status storage: disable
I (363) lightbulb: status storage delay 0 ms
I (363) lightbulb: fade: enable
I (373) lightbulb: fade 800 ms
I (373) lightbulb: led_beads: 5
I (383) lightbulb: hardware cct: No
I (383) lightbulb: precise cct control: enable
I (393) lightbulb: sync change: disable
I (403) lightbulb: auto on: enable
I (403) lightbulb: color mode: enable
I (413) lightbulb: sync change: disable
I (413) lightbulb: power limit param: 
I (423) lightbulb:      white max brightness: 100
I (423) lightbulb:      white min brightness: 1
I (433) lightbulb:      white max power: 100
I (433) lightbulb:      color max brightness: 100
I (443) lightbulb:      color min brightness: 1
I (443) lightbulb:      color max power: 300
I (453) lightbulb: hue: 0, saturation: 100, value: 100
I (463) lightbulb: select works mode: color, power status: 1
I (473) lightbulb: ---------------------------------------------------------------------
W (483) output_test: rainbow
I (2483) lightbulb: set [h:0 s:100 v:100]
I (2483) lightbulb: 8 bit color conversion value [r:255 g:0 b:0]
I (2483) lightbulb: hal write value [r:255 g:0 b:0], channel_mask:7 fade_ms:800
I (4483) lightbulb: set [h:30 s:100 v:100]
I (4483) lightbulb: 8 bit color conversion value [r:255 g:127 b:0]
I (4483) lightbulb: hal write value [r:255 g:127 b:0], channel_mask:7 fade_ms:800
I (6483) lightbulb: set [h:60 s:100 v:100]
I (6483) lightbulb: 8 bit color conversion value [r:255 g:255 b:0]
I (6483) lightbulb: hal write value [r:255 g:255 b:0], channel_mask:7 fade_ms:800
...
...
...
W (80407) output_test: TEST DONE
I (80407) gpio: GPIO[18]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
W (80407) lightbulb demo: Test SM16825E underlying Driver
I (80407) lightbulb demo: SM16825E: Test - Custom pin mapping
I (80407) lightbulb demo: SM16825E: Configuring custom pin mapping...
I (80407) lightbulb demo: SM16825E: Custom mapping configured successfully
I (80407) lightbulb demo: SM16825E: Testing individual channels with custom mapping
I (80407) lightbulb demo: SM16825E: Setting RED channel to 255 (should control OUTW pin)
I (85407) lightbulb demo: SM16825E: Setting GREEN channel to 255 (should control OUTR pin)
I (90407) lightbulb demo: SM16825E: Setting BLUE channel to 255 (should control OUTY pin)
I (95407) lightbulb demo: SM16825E: Testing RGB function with custom mapping
I (95407) lightbulb demo: SM16825E: Setting RGB(0,0,0) - check if colors appear on correct pins
I (100407) lightbulb demo: SM16825E: Testing RGBWY function with custom mapping
I (100407) lightbulb demo: SM16825E: Setting White channel to 255 (should control OUTB pin)
I (105407) lightbulb demo: SM16825E: Setting Yellow channel to 255 (should control OUTG pin)
I (110407) lightbulb demo: SM16825E: Testing current gain control with custom mapping
I (110407) lightbulb demo: SM16825E: Setting channel current gains
I (110407) lightbulb demo: SM16825E: Applying new current settings
I (113407) lightbulb demo: SM16825E: Setting Blue color
I (115407) lightbulb demo: SM16825E: Testing RGBWY mode
I (115407) lightbulb demo: SM16825E: Setting RGBWY colors
I (117407) lightbulb demo: SM16825E: Setting RGBWY colors
I (119407) lightbulb demo: SM16825E: Setting RGBWY colors
I (121407) lightbulb demo: SM16825E: Testing standby mode
I (121407) lightbulb demo: SM16825E: Entering standby mode
I (123407) lightbulb demo: SM16825E: Exiting standby mode
I (125407) lightbulb demo: SM16825E: Final color display
I (127407) lightbulb demo: Lightbulb Example End
```
