# ESP32 IoT Solution Overview

---

Espressif IoT Device Drivers, Documentations And Solutions.

>Current branch based on [ESP-IDF `v4.0.1`](https://github.com/espressif/esp-idf/tree/v4.0.1)

## Contents

* [components:](./components)
    * Small drivers of different devices like button and LED
    * Drivers of sensors
    * Drivers of different I2C devices
    * Friendly APIs of WiFi and OTA

* [documents:](./documents)
    * Documentations of some important features
    * Instruction of some different solutions

* [examples:](./examples)
    * Example project using this framework

* [tools:](./tools)
    * Different tools and scripts
    * Unit-test project

## Examples

* ~~ESP32 AWS IoT Example~~
    - **Note**：This example has been abandoned, please refer [esp-aws-iot](https://github.com/espressif/esp-aws-iot).

* [ESP32 Pedestrian Flow Monitoring introduction](examples/check_pedestrian_flow)
    - The example demonstrates how to use ESP32 to calculate pedestrian flow in the Wi-Fi sniffer mode.
    - Keywords: __WiFi sniffer OneNet MQTT__

* [ESP32 Empty Project](examples/empty_project)
    - It provides a framework for users to develop any projects.

* [Ethernet-WiFi data transmission](examples/eth2wifi)
    - An example that enables Ethernet-to-WiFi data forwarding function.
    - Keywords: __Ethernet WiFi__

* [ESP32 OLED screen panel](examples/oled_screen_module)
    - The example demonstrates how to use ESP32 to drive a OLED screen and to read sensor in low power mode.
    - Keywords: __SSD1306 Deep-sleep BH1750__

* [ESP32 LittlevGL GUI Example](examples/hmi/lvgl_example)
    - The example demonstrates how to use LittlevGL embedded GUI library on ESP32.
    - Keywords: __LittlevGL LVGL embedded GUI__
    - **Note**：This example will be abandoned in future, please refer [lvgl/lv_port_esp32](https://github.com/lvgl/lv_port_esp32).

* [ESP32 uGFX GUI Example](examples/hmi/ugfx_example)
    - The example demonstrates how to use uGFX embedded GUI library on ESP32.
    - Keywords: __uGFX embedded GUI__

* ~~ESP32 Smart Device to Cloud Framework~~
    - **Note**：This example has been abandoned,please refer [esp32-alink-demo](https://github.com/espressif/esp32-alink-demo).

* [ESP32 Lowpower EVB Example](examples/lowpower_evb)
    - An example to show how to use ESP32 lowpower framework to build applications with ESP32 lowpower evaluating board.
    - Keywords: __ESP32_Lowpower_EVB ULP Deepsleep Lowpower__

* [ESP32 Touch Sensor Example](examples/touch_pad_evb)
    - An example for the ESP32 touch sensor development kit, ESP32-Sense, which is used to evaluate and develop ESP32 touch sensor system.
    - Keywords: __TouchSensor ESP32-Sense__

* [ESP32 ULP Co-processor Detect Brownout Example](examples/ulp_examples/ulp_detect_brownout)
    - An example of using SAR_ADC to read voltage of VDD33 pin with the ESP32 ULP co-processor and determine if a brownout happened.
    - Keywords: __ULP Deep-sleep  Brownout  Assembly__

* [ESP32 ULP Co-processor Reads Hall Sensor Example](examples/ulp_examples/ulp_hall_sensor)
    - It provides an example of the ESP32 ULP co-processor reading the on-chip Hall sensor in low-power mode.
    - Keywords: __ULP Deep-sleep  Built-in-Sensor HallSensor Assembly__

* [ESP32 ULP Co-processor Operates RTC GPIO Example](examples/ulp_examples/ulp_rtc_gpio)
    - An example of operating RTC GPIO with the ESP32 ULP co-processor.
    - Keywords: __ULP Deep-sleep  RTC-GPIO Assembly__

* [ESP32 ULP Co-processor Send RTC Interrupt](examples/ulp_examples/ulp_send_interrupt)
    - An example of ESP32 ULP co-processor sending RTC interrupt.
    - Keywords: __ULP RTC-Interrupt Assembly__

* [ESP32 ULP Co-processor Reads Temperature Sensor Example](examples/ulp_examples/ulp_tsens)
    - An example of the ESP32 ULP co-processor reading the on-chip temperature sensor in low-power mode.
    - Keywords: __ULP Deep-sleep  Built-in-Sensor TemperatureSensor Assembly__

* [ESP32 ULP Co-processor Watering Example](examples/ulp_examples/ulp_watering_device)
    - An example of implementing the ESP32 ULP co-processor in a watering device.
    - Keywords: __ULP Deep-sleep  SAR-ADC RTC-GPIO Assembly__

* [ESP32 ULP Co-processor SAR-ADC Example](examples/ulp_examples/ulp_adc)
    - An example of using SAR_ADC to read NTC thermistor voltage and calculate temperature in ULP mode.
    - Keywords: __ULP Deep-sleep  SAR-ADC  Assembly__

* [ESP32 ULP Co-processor BitBang I2C Example](examples/ulp_examples/ulp_i2c_bitbang)
    - An example of using RTC-GPIO bitbanged I2C to read BH1750 light sensor in ULP mode.
    - Keywords: __ULP Deep-sleep  RTC-GPIO  BITBANG  I2C  Assembly__

* [ESP32 ULP Co-processor BitBang SPI Example](examples/ulp_examples/ulp_spi)
    - An example of using RTC-GPIO bitbanged SPI to read MS5611 sensor in ULP mode.
    - Keywords: __ULP Deep-sleep  RTC-GPIO  BITBANG  SPI  Assembly__

## Components 

### Features

* ~~DAC Audio- Driver and example of using DAC Audio.~~
    - **Note**：This example has been removed to [esp-idf/examples/peripherals/i2s_adc_dac](https://github.com/espressif/esp-idf/tree/v4.0.1/examples/peripherals/i2s_adc_dac)
* [ULP Monitor](./components/features/ulp_monitor)- Provides some APIs for running simple ulp program in deep sleep.
* [TouchPad](./components/features/touchpad)- Driver and example of using different types of touch pad.
* ~~Infrared- Driver and framework of using infrared remote control.~~
    - **Note**：This example has been removed to [esp-idf/examples/peripherals/rmt/ir_protocols](https://github.com/espressif/esp-idf/tree/v4.0.1/examples/peripherals/rmt/ir_protocols)

### Framework

* [Lowpower_framework](./components/framework/lowpower_framework) - Development framework for lowpower applications.

### General fucntions

* ~~ADC - Driver and example of ADC~~
    - **Note**：This example has been abandoned,please refer [esp-idf/examples/peripherals/adc](https://github.com/espressif/esp-idf/tree/v4.0.1/examples/peripherals/adc)
* [Button](./components/general/button) - Driver and example of using buttons and keys.
* [Debugs](./components/general/debugs) - Provides different commands for debugging via UART.
* [LED](./components/general/led) - Driver and example of using LED, which provides such APIs as to blink with different frequency.
* [Light](./components/general/light) - Driver and example of using PWM to drive a light, which provides such APIs as to control several channels of LED.
* [OTA](./components/general/ota) - Driver and example of upgrading firmware from the given URL.
  	- **Note**: This example will be abandoned in future, please refer [esp-idf/examples/system/ota](https://github.com/espressif/esp-idf/tree/v4.0.1/examples/system/ota)
* [Param](./components/general/param) - Driver and example of saving and loading data via NVS flash.
* [Power Meter](./components/general/power_meter) - Driver and example of a single-phase energy meter such as BL0937 or HLW8012.
* [Relay](./components/general/relay) - Driver and example of a relay with different driving modes.
* [Weekly timer](./components/general/weekly_timer) - Driver and example of a weekly timer to trigger events at some certain moment in each week.

### HMI

* [LittlevGL](./components/hmi/lvgl_gui) - Driver of using LittlevGL embedded GUI library.
* [uGFX](./components/hmi/ugfx_gui) - Driver and example of using uGFX embedded GUI library.

### I2C Sensors

* [APDS9960](./components/i2c_devices/sensor/apds9960) - Driver and example of reading APDS9960, which is an ambient light photo Sensor.
* [BH1750](./components/i2c_devices/sensor/bh1750) - Driver and example of reading BH1750 light sensor (GY-30 module).
* [BME280](./components/i2c_devices/sensor/bme280) - Driver and example of reading BME280, which is an pressure and temperature Sensor.
* [FT5X06](./components/i2c_devices/sensor/ft5x06) - Driver and example of reading FT5X06, which is a touch Sensor.
* [HDC2010](./components/i2c_devices/sensor/hdc2010) - Driver and example of reading HDC2010, which is a low power temperature and humidity sensor.
* [HTS221](./components/i2c_devices/sensor/hts221) - Driver and example of reading HTS221 temperature and humidity sensor.
* [LIS2DH12](./components/i2c_devices/sensor/lis2dh12) - Driver and example of reading LIS2DH12, which is a 3-axis accelerometer.
* [MVH3004D](./components/i2c_devices/sensor/mvh3004d) - Driver and example of reading MVH3004D temperature and humidity sensor.
* [VEML6040](./components/i2c_devices/sensor/veml6040) - Driver and example of reading VEML6040, which is a RGBW color sensor.

### I2C Devices

* [AT24C02](./components/i2c_devices/others/at24c02) - Driver and example of driving AT24C02, which is an eeprom storage.
* [CH450](./components/i2c_devices/others/ch450) - Driver and example of driving CH450, which is a 7-segment LED driver.
* [HT16C21](./components/i2c_devices/others/ht16c21) - Driver and example of driving HT16C21, which is a LED driver.
* [IS31FL3XXX](./components/i2c_devices/others/is31fl3xxx) - Driver and example of driving is31fl3xxx series chips, which are light effect LED driver chips.
* [MCP23017](./components/i2c_devices/others/mcp23017) - Driver and example of using mcp23017, which is a 16-bit I/O expander.
* [SSD1306](./components/i2c_devices/others/ssd1306) - Driver and example of using ssd1306, which is a 132 x 64 dot matrix OLED/PLED segment driver chip.

### I2S Devices

* [ILI9806](./components/i2s_devices/ili9806) - Driver and example of driving ILI9806 LCD.
* [NT35510](./components/i2s_devices/nt35510) - Driver and example of driving NT35510 LCD.

### Motor

* [Servo](./components/motor/servo) - Driver and example of driving servo motors.
* [A4988](./components/motor/stepper/a4988) - Driver and example of driving A4988, which is a stepper motor driver.

### Network Abstract

* ~~MQTT - Driver and example of using MQTT client, which is a light-weight IoT protocol.~~
    - **Note**: This example has been abandoned, please refer: [esp-idf/examples/protocols/mqtt](https://github.com/espressif/esp-idf/tree/v4.0.1/examples/protocols/mqtt)
* [TCP](./components/network/tcp) - API and example of using TCP server and client in C++.
* [UDP](./components/network/udp) - API and example of using UDP in C++.
* ~~Alink - API and example of connecting and communicating with Alink cloud service.~~
    - **Note**: This example has been abandoned, please refer: [esp32-alink](https://github.com/espressif/esp32-alink)

### SPI Devices

* [E-ink display](./components/spi_devices/epaper) - API and example of driving and controlling SPI E-ink screen.
* [LCD screen](./components/spi_devices/lcd) - API and example of driving and controlling SPI LCD.
* [XPT2046 Touch screen](./components/spi_devices/xpt2046) - API and example of driving and controlling SPI Touch Screen.

### WiFi Abstract

* [ESP-TOUCH for smart-config](./components/wifi/smart_config) - Abstract APIs and example of configuring devices via esp-touch.
* [Blufi abstract APIs](./components/wifi/iot_blufi) - Abstract APIs and example of configuring devices via blufi.
* [WiFi connection abstract APIs](./components/wifi/wifi_conn) - Abstract APIs and example of WiFi station connecting to router.



## Documents

* [Deep-sleep low power solutions](./documents/low_power_solution/readme_en.md) [[中文]](./documents/low_power_solution/readme_cn.md)
    * [Current Consumption Test for ESP32 in Deep sleep](./documents/low_power_solution/deep-sleep_current_test_en.md)[[中文]](./documents/low_power_solution/deep-sleep_current_test_cn.md)
    * [ESP32 Low-Power Management Overview](./documents/low_power_solution/esp32_lowpower_solution_en.md)[[中文]](./documents/low_power_solution/esp32_lowpower_solution_cn.md)
    * [ESP32 ULP Co-processor and Assembly Environment Setup](./documents/low_power_solution/esp32_ulp_co-processor_and_assembly_environment_setup_en.md)[[中文]](./documents/low_power_solution/esp32_ulp_co-processor_and_assembly_environment_setup_cn.md)
* [Light-sleep low power solutions overview](./documents/DFS_and_light_sleep/readme_en.md) [[中文]](./documents/DFS_and_light_sleep/readme_cn.md)
    * [ESP32 DFS test manual](./documents/DFS_and_light_sleep/DFS_test_manual_en.md)
    * [ESP32 Light-sleep features](./documents/DFS_and_light_sleep/light_sleep_performance_en.md)
    * ESP32 Light-sleep test manua[[中文]](./documents/DFS_and_light_sleep/light_sleep_test_manual_cn.md)
* [Touch Sensor Application Note](./documents/touch_pad_solution/touch_sensor_design_en.md) [[中文]](./documents/touch_pad_solution/touch_sensor_design_cn.md)
	* [ESP-Tuning Tool User Guide](./documents/touch_pad_solution/esp_tuning_tool_user_guide_en.md) [[中文]](./documents/touch_pad_solution/esp_tuning_tool_user_guide_cn.md)
* [Security and Factory Flow](./documents/security_solution/readme_en.md) [[中文]](./documents/security_solution/readme_cn.md)
    * ESP32 secure and encrypt[[中文]](./documents/security_solution/esp32_secure_and_encrypt_cn.md)
    * [Download Tool GUI Instruction](./documents/security_solution/download_tool_en.md)

## Tools

### unit-test-app (Make)

To use uint-test, follow these steps:

* Change to the directory of unit-test-app

    ```
    cd YOUR_IOT_SOLUTION_PATH/tools/unit-test-app
    ```

* Use the default sdkconfig and compile unit-test-app by `make IOT_TEST_ALL=1 -j8`

    ```
    make defconfig
    make IOT_TEST_ALL=1
    ```

* Flash the images by `make flash`

    ```
    make IOT_TEST_ALL=1 flash
    ```

* Reset the chip and see the uart log using an uart tool such as minicom
* All kinds of components will be shown by uart

    ```
    Here's the test menu, pick your combo:
    (1)     "Sensor BH1750 test" [bh1750][iot][sensor]
    (2)     "Button test" [button][iot]
    (3)     "Button cpp test" [button_cpp][iot]
    (4)     "Dac audio test" [dac_audio][iot][audio]
    (5)     "Debug cmd test" [debug][iot]
    (6)     "Deep sleep gpio test" [deep_sleep][rtc_gpio][current][iot]
    (7)     "Deep sleep touch test" [deep_sleep][touch][current][iot]
     ......
    ```

* You need to send the index of the unit you want to test by uart. Test code of the unit you select will be run

## Quick Start

### Hardware

* [ESP32_ULP_EB_V1 Evaluation Board](./documents/evaluation_boards/readme_en.md) [[中文]](./documents/evaluation_boards/readme_cn.md)
* [ESP32-Sense kit Board](./documents/evaluation_boards/readme_en.md) [[中文]](./documents/evaluation_boards/readme_cn.md)
    * [ESP32 Touch Sensor development kit](./documents/evaluation_boards/ESP32-MeshKit-Sense_guide_en.md)[[中文]](./documents/evaluation_boards/ESP32-MeshKit-Sense_guide_cn.md)
* [ESP-Prog board](./documents/evaluation_boards/readme_en.md) [[中文]](./documents/evaluation_boards/readme_cn.md)
    * [Firmware download and JTAG debug tool](./documents/evaluation_boards/ESP-Prog_guide_en.md)[[中文]](./documents/evaluation_boards/ESP-Prog_guide_cn.md)

### Setting up Development Environment

1. [Install ESP-IDF v4.0.1 Step By Step](https://docs.espressif.com/projects/esp-idf/en/v4.0.1/get-started/index.html#installation-step-by-step)

>Current branch based on ESP-IDF `v4.0.1`

2. Get ESP-IoT-Solution

    ```shell
    git clone --recursive https://github.com/espressif/esp-iot-solution
    ```

3. Initialize the Submodules

    ```shell
    git submodule update --init --recursive
    ```

4. Set up the Environment Variables

    ```shell
    export IOT_SOLUTION_PATH=~/esp/esp-iot-solution
    ```

### Creating Your Project (CMake)

* We can regard IoT solution project as a platform that contains different device drivers and features
* `Add-on project`: If you want to use those drivers and build your project base on the framework, you need to include the IoT components into your project's `CMakeLists.txt`.

    ```
    cmake_minimum_required(VERSION 3.5)

    include($ENV{IOT_SOLUTION_PATH}/component.cmake)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)

    project(empty-project)
    ```


* `Stand-alone component`: if you just want to pick one of the component and put it into your existing project, you just need to copy the single component to your project components directory. But you can also append the component list in your project's `CMakeLists.txt` like this:

    ```
    set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS} ${IOT_SOLUTION_PATH}/components/{component_you_choose}")
    ```

### Creating Your Project (Legacy GNU Make)

* We can regard IoT solution project as a platform that contains different device drivers and features
* `Add-on project`: If you want to use those drivers and build your project base on the framework, you need to include the IoT components into your project's `Makefile`.

    ```
    include $(IOT_SOLUTION_PATH)/Makefile
    include $(IDF_PATH)/make/project.mk
    ```

* `Stand-alone component`: if you just want to pick one of the component and put it into your existing project, you just need to copy the single component to your project components directory. But you can also append the component list in your project Makefile like this:

    ```
    EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/{component_you_choose}
    ```
