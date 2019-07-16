[[中文]](./mp3_example_cn.md)

# ESP32 LittlevGL MP3

## What You Need

- Hardware:
	* 1 x [ESP32\_LCD\_EB\_V1](../../../documents/evaluation_boards/ESP32_LCDKit_guide_en.md) HMI development board (for this example, it has to be used with the [ESP32_DevKitC](https://docs.espressif.com/projects/esp-idf/en/stable/hw-reference/modules-and-boards.html#esp32-devkitc-v4) development board)
	* 1 x display (2.8 inches, 240*320 pixels, ILI9341 LCD + XPT2046 Touchscreen)
- Software:
	* [esp-iot-solution](https://github.com/espressif/esp-iot-solution)
	* [LittlevGL GUI](https://littlevgl.com/)

- Setup: see [README.md](../../../README.md#preparation)

For detailed information on LittlevGL and related configuration, please refer to [LittlevGL Guide](../../../documents/hmi_solution/littlevgl/littlevgl_guide_en.md).

See the connection image below:

<div align="center"><img src="../../../documents/_static/hmi_solution/littlevgl/lvgl_mp3_connect.jpg" width = "700" alt="lvgl_mp3_connect" align=center /></div>  

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

## Example Framework
                               +---------------+
                               |               |
                               |      HMI      |
                               |               |
                               +----^-----+----+
                                    |     |
                                    |     |
                                    |     |
                                    |     |
                                    |     |
    +---------------+          +----+-----v----+           +--------------+
    |               |          |               |           |              |
    |    SD-Card    +---------->     ESP32     +----------->   DAC-Audio  |
    |               |          |               |           |              |
    +---------------+          +---------------+           +--------------+


## Run the Example

- Open Terminal and navigate to the directory `examples/hmi/mp3_example`
- Run `make defconfig`(Make) or `idf.py defconfig`(CMake) to apply the default configuration
- Run `make menuconfig`(Make) or `idf.py menuconfig`(CMake) to set up the flashing-related configuration
- Run `make -j8 flash`(Make) or `idf.py flash`(CMake) to build the example and flash it to the device

### Play MP3 files with ESP-ADF

- Build ESP-ADF development environment according to [Get Started](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html). The environment variable `ADF_PATH` is used in the example.
- Run `make menuconfig`(Make) or `idf.py menuconfig`(CMake) and select ADF for playing the audio files. To do this, please go to `IoT Example - LittlevGL MP3 Example->Use esp-adf to play song`.
- Save the configuration and run `make -j8 flash`(Make) or `idf.py flash`(CMake) to build the example and flash it to the device.

#### SD-Card and audio related notes

- Currently, only MP3 audio files are supported.
- MP3 files can be placed either into the root directory of SD-Card or its secondary directory.
- No more than 20 audio files are supported in this example.
- ESP32\_LCD\_EB\_V1 has two audio output ports associated with the left and right channel respectively. Speakers can be connected to either of them.

## Example Demonstration

<div align="center"><img src="../../../documents/_static/hmi_solution/littlevgl/lvgl_mp3.jpg" width = "700" alt="lvgl_mp3" align=center /></div>  