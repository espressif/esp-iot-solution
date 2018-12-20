# ESP32 LittlevGL MP3 示例

## 示例环境

- 硬件：ESP32_LCD_EB_V1 开发板、屏幕（2.8 inch、240*320 pixel、 ILI9341 LCD + XPT2046 Touch）
- 软件：[esp-iot-solution](https://github.com/espressif/esp-iot-solution)、[LittlevGL GUI](https://littlevgl.com/)

- 环境搭建：[README.md](../../../README.md#preparation)

LittlevGL 介绍及相关配置见 [littlevgl_guide_cn](../../../documents/hmi_solution/littlevgl/littlevgl_guide_cn.md)

连接示意图：

<div align="center"><img src="../../../documents/_static/hmi_solution/littlevgl/lvgl_mp3_connect.jpg" width = "700" alt="lvgl_mp3_connect" align=center /></div>  

默认引脚连接：

Name | Pin
-------- | -----
CLK | 22
MOSI | 21
MISO | 27
CS(LCD) | 5
DC | 19
RESET | 18
LED | 23
CS(Touch) | 32
IRQ | 33

## 运行示例

- 进入到 `examples/hmi/lvgl_example` 目录下
- 运行 `make defconfig` 使用默认配置
- 运行 `make menuconfig` 进行烧录相关配置
- 运行 `make -j8 flash` 编译、烧录程序到设备

### 使用 ESP-ADF 播放 MP3 音频文件

- 根据 [Get Started](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html) 搭建 ESP-ADF 开发环境
- 运行 `make menuconfig` 选择使用 ADF 播放音频文件，目录：`IoT Example - LittlevGL MP3 Example->Use esp-adf to play song`
- 保存配置并运行 `make -j8 flash` 编译、烧录程序到设备

## 示例结果

<div align="center"><img src="../../../documents/_static/hmi_solution/littlevgl/lvgl_mp3.jpg" width = "700" alt="lvgl_mp3" align=center /></div>  