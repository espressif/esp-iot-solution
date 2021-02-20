[[中文]](./lvgl_example_cn.md)

# ESP32 LittlevGL Widget

## What You Need

- Hardware:
	* 1 x [ESP32\_LCD\_EB\_V1](https://github.com/espressif/esp-dev-kits/blob/master/esp32-lcdkit/docs/ESP32_LCDKit_guide_en.md) HMI development board (for this example, it has to be used with the [ESP32_DevKitC](https://docs.espressif.com/projects/esp-idf/en/stable/hw-reference/modules-and-boards.html#esp32-devkitc-v4) development board)
	* 1 x display (2.8 inches, 240x320 pixels, ILI9341 LCD + XPT2046 Touchscreen)
- Software: 
	* [esp-iot-solution](https://github.com/espressif/esp-iot-solution)
	* [LittlevGL GUI](https://lvgl.io/)


See the connection image below:

<div align="center"><img src="../../../docs/_static/hmi_solution/lcd_connect.jpg" width = "700" alt="lcd_connect" align=center /></div>  

The pins to be connected:

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

## Run the Example

- Open Terminal and navigate to the directory `examples/hmi/lvgl_example`
- Run `make defconfig`(Make) or `idf.py defconfig`(CMake) to apply the default configuration
- Run `make menuconfig`(Make) or `idf.py menuconfig`(CMake) to set up the configuration, you can go to `Example Configuration → Choose LVGL Demo to Run` to choose a demo to run
- Run `make -j8 flash`(Make) or `idf.py flash`(CMake) to build the example and flash it to the device

## Example Demonstration

<div align="center"><img src="../../../docs/_static/hmi_solution/littlevgl/tft_zen.jpg" width = "700" alt="tft_zen" align=center /></div>  