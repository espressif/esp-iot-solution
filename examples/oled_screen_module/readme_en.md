[[中文]](./readme_cn.md)

# ESP32 OLED Demo Walkthrough
## Solution Features

The ESP32 OLED demo has the following features:

- Collects temperature and humidity data
- Connects to the internet and gets local time
- Displays temperature/humidity data and local time on separate OLED screens
- Switches screens through touch sensors or using touchless hand gestures
- Enters low-power mode through touch sensor

The image belows shows an ESP32 development board with an OLED display.
    <br>
    <img src="../../documents/_static/example/oled_screen_module/screen demo.jpg" width = "500" alt="screen demo" align=center />

-------

## Hardware Preparation
This demo uses the ESP32\_Button\_Module\_V2 as a development board that contains:

- Proximity/ambient light sensor (APDS9960)
- OLED display with 132 x 64 dot matrix panel(SSD1306)
- Temperature/humidity sensor (HTS221)
- Two touch sensor buttons

For the complete schematics of ESP32\_Button\_Module\_V2, please click [here](../../documents/_static/example/oled_screen_module/ESP32_BUTTON_MODULE_V2_20170720A.pdf).

The image below shows the section for power supply and power control of the screen and sensor.
    <br>
    <img src="../../documents/_static/example/oled_screen_module/peripher switch.png" width = "500" alt="apds9960" align=center />

VDD33 is the output terminal of the LDO, with 3.3V power supply for ESP32, peripherals and flash. VDD33\_PeriP is the power supply for the screen, temperature/humidity sensor and hand-gesture sensing module. The SI2301 transistor is used as a power switch that controls the voltage on VDD33\_PeriP. By default, the transistor's gate terminal maintains a high level, that is, the power switch is turned off. You can turn it on by putting Power_ON to low level.

## Software Design

The OLED display demo:

- Uses esp-iot-solution development kit
- Is based on FreeRTOS that supports multi-tasking
- Gets local time with SNTP protocol
- Enters low-power mode through touch sensor
- Wakes up the device through touch sensor

-------

## Introduction to Low-power Mode
###  Hardware Design
To achieve a minimal power consumption in low-power mode, the solution implements:

- Power control of screen, temperature/humidity sensor and hand-gesture sensing module
- A low-power LDO with quiescent current of about 1 μA
- Power management for the touch sensor

### Touch Sensor Working Cycles
The touch sensor has two statuses: sleep and measurement. The touch sensor will sleep after each measurement. In normal working mode, the sleep period can be short, while in low-power mode, it can be long to reduce power consumption.

Call `touch_pad_set_meas_time(uint16_t sleep_cycle, uint16_t meas_cycle)` to set the sleep and measurement time before the touch sensor enters low-power mode.

Parameters:

- `sleep_cycle`: `sleep_cycle` determines the interval between two measurements. `t_sleep = sleep_cycle / (RTC_SLOW_CLK frequency)`。

   Get the frequency value of `RTC_SLOW_CLK` by using `rtc_clk_slow_freq_get_hz()` function.

- `meas_cycle`: `meas_cycle` determines the measurement time. `t_meas = meas_cycle / 8M`. The maximum value is 0xffff / 8M = 8.19 ms.

### Low-power Mode
Long tap any touch sensor button to enable low-power mode. The sampling frequency is the lowest in this mode. Long tap the sensor button to release it from low-power mode. The images below show the current sampling waveforms.

- Current sampling on LDO VOUT 3.3V (including the current consumption of ESP32, screen and touch sensor):
    <br>
    <img src="../../documents/_static/example/oled_screen_module/figure_3.3V.png" width = "500" alt="figure_3.3V" align=center />

> Note: In low-power mode, the average current on LDO VOUT 3.3V is about 30 μA, while the maximum current is 1.6 mA, when the touch sensor is taking measurement.

- Current sampling on LDO VIN 5V (including the current consumption of ESP32, screen, touch sensor and LDO):
    <br>
    <img src="../../documents/_static/example/oled_screen_module/figure_5V.png" width = "500" alt="figure_5V" align=center />

> Note: In low-power mode, the average current on LDO VIN 5V is about 45 μA, while the maximum current is 2.1 mA.

## Compile and Run the Demo

### Preparation

Make sure you have installed the ESP32 toolchain on your PC. For the toolchain installation, you can refer to [README.md](https://github.com/espressif/esp-idf/blob/master/README.md).

### Get IoT Solution Program Code

Run the following commands to download the iot-solution repository.

* You can use the `recursive` option to automatically initialize all the submodules.

    ```
    git clone -b release/v1.1 --recursive https://github.com/espressif/esp-iot-solution.git
    ```

* You can also initialize the submodules manually, by first running:

    ```
    git clone -b release/v1.1 https://github.com/espressif/esp-iot-solution.git
    ```

  - Then, switch to the root directory of the program and run the following command to get all the other dependent submodules.

    ```
    git submodule update --init --recursive
    ```

### Compilation and Running

After downloading all the submodules, you can compile and run the oled_screen_module program. Go to the esp-iot-solution/examples/oled_screen_module directory, and follow the steps below:

* Configure the settings for serial port.

Run the following command to configure the serial port settings. Set the port number and download speed by configuring `Serial flasher config` option.

Make:
```
    cd YOUR_IOT_SOLUTION_PATH/examples/oled_screen_module
    make menuconfig
```

CMake:
```
    cd YOUR_IOT_SOLUTION_PATH/examples/oled_screen_module
    idf.py menuconfig
```

* Compile, download and run the program.

Run the following command to compile `oled_screen_module`. `flash` in the command enables downloading, while the optional `monitor` enables system printing.

Make:
```
    make flash monitor
```

CMake:
```
    idf.py flash monitor
```

> Note: If the program cannot be downloaded automatically, you can download it mannually. After downloading, press the reset button on the board to run the program and check the logs on the serial port.
