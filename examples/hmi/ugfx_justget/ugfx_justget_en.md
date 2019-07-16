[[中文]](./ugfx_justget_cn.md)

# ESP32 μGFX "Just Get 10"

## What You Need

- Hardware:
	* 1 x [ESP32\_LCD\_EB\_V1](../../../documents/evaluation_boards/ESP32_LCDKit_guide_en.md) development board (for this example, it has to be used with the [ESP32_DevKitC](https://docs.espressif.com/projects/esp-idf/en/stable/hw-reference/modules-and-boards.html#esp32-devkitc-v4) development board with wrover modules)
	* 1 x display (2.8 inches, 240x320 pixels, ILI9341 LCD + XPT2046 Touchscreen)
- Software: 
	* [esp-iot-solution](https://github.com/espressif/esp-iot-solution) 
	* [μGFX GUI](https://ugfx.io/)

- Setup: see [README.md](../../../README.md#preparation)

For detailed information on μGFX and related configuration, please refer to [μGFX Guide](../../../documents/hmi_solution/ugfx/ugfx_guide_en.md).

See the connection image below:

<div align="center"><img src="../../../documents/_static/hmi_solution/lcd_connect.jpg" width = "700" alt="lcd_connect" align=center /></div>  

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

- Open Terminal and navigate to the directory `examples/hmi/ugfx_justget`
- Run `make defconfig`(Make) or `idf.py defconfig`(CMake) to apply the default configuration
- Run `make menuconfig`(Make) or `idf.py menuconfig`(CMake) to set up the flashing-related configuration
- Run `make -j8 flash`(Make) or `idf.py flash`(CMake) to build the example and flash it to the device

## Example Demonstration

Click on the [demo video](http://demo.iot.espressif.cn:8887/cmp/demo/ugfx_justget.mp4) for detailed demonstration.

<div align="center"><img src="../../../documents/_static/hmi_solution/ugfx/ugfx_justget0.jpg" width = "400" alt="ugfx_justget0" align=center /></div>  

<div align="center"><img src="../../../documents/_static/hmi_solution/ugfx/ugfx_justget1.jpg" width = "400" alt="ugfx_justget1" align=center /></div>  