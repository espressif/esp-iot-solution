[[EN]](./lvgl_thermostat_en.md)

# ESP32 LittlevGL 恒温计面板示例

## 示例环境

- 硬件：
	* [ESP32\_LCD\_EB\_V1](https://github.com/espressif/esp-dev-kits/blob/master/esp32-lcdkit/docs/ESP32_LCDKit_guide_cn.md) 开发板（该示例需要搭配使用 [ESP32 DevKitC](https://docs.espressif.com/projects/esp-idf/en/stable/hw-reference/modules-and-boards.html#esp32-devkitc-v4) 开发板）
	* 屏幕（2.8 inch、240*320 pixel、ILI9341 LCD + XPT2046 Touch）
- 软件：
	* [esp-iot-solution](https://github.com/espressif/esp-iot-solution)
	* [LittlevGL GUI](https://lvgl.io/)

连接示意图：

<div align="center"><img src="../../../docs/_static/hmi_solution/lcd_connect.jpg" width = "700" alt="lcd_connect" align=center /></div>  

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

- 进入到 `examples/hmi/lvgl_thermostat` 目录下
- 运行 `make defconfig`(Make) 或者 `idf.py defconfig`(CMake) 使用默认配置
- 运行 `make menuconfig`(Make) 或者 `idf.py menuconfig`(CMake) 进行烧录相关配置
- 运行 `make -j8 flash`(Make) 或者 `idf.py flash`(CMake) 编译、烧录程序到设备

## 示例结果

<div align="center"><img src="../../../docs/_static/hmi_solution/littlevgl/thermostat.jpg" width = "400" alt="thermostat" align=center /></div>  