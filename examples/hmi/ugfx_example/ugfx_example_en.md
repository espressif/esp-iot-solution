[[中文]](./ugfx_example_cn.md)

# ESP32 μGFX Widget

## What You Need

- Hardware:
	* 1 x [ESP32\_LCD\_EB\_V1](../../../documents/evaluation_boards/ESP32_LCDKit_guide_en.md) development board (for this example, it has to be used with the [ESP32_DevKitC](https://docs.espressif.com/projects/esp-idf/en/stable/hw-reference/modules-and-boards.html#esp32-devkitc-v4) development board)
	* 1 x display (4.3 inches, 480x800 pixels, NT35510 LCD + FT5X06 Touchscreen)
- Software:
	* [esp-iot-solution](https://github.com/espressif/esp-iot-solution)
	* [μGFX GUI](https://ugfx.io/)

- Setup: see [README.md](../../../README.md#preparation)

For detailed introduction on μGFX and related configuration, please refer to [μGFX Guide](../../../documents/hmi_solution/ugfx/ugfx_guide_en.md).

See the connection image below:

<div align="center"><img src="../../../documents/_static/hmi_solution/lcd480x800_connect.jpg" width = "700" alt="lcd_connect" align=center /></div>  

The pins to be connected:

Name | Pin | Name | Pin
-------- | -------- | -------- | --------
WR | 18 | SCL | 3
RS | 5 | SDA | 1
D0 | 19 | D8 | 25
D1 | 21 | D9 | 26
D2 | 0 | D10 | 12
D3 | 22 | D11 | 13
D4 | 23 | D12 | 14
D5 | 33 | D13 | 15
D6 | 32 | D14 | 2
D7 | 27 | D15 | 4

## Run the Example

- Open Terminal and navigate to the directory `examples/hmi/ugfx_example`
- Run `make defconfig`(Make) or `idf.py defconfig`(CMake) to apply the default configuration
- Run `make menuconfig`(Make) or `idf.py menuconfig`(CMake) to set up the flashing-related configuration
- Run `make -j8 flash`(Make) or `idf.py flash`(CMake) to build the example and flash it to the device

## Example Demonstration

<div align="center"><img src="../../../documents/_static/hmi_solution/ugfx/ugfx_example.jpg" width = "700" alt="μgfx_example" align=center /></div>  