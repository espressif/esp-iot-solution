# ESP32 IoT Solution Overview

---

## Solutions

* [Deep-sleep low power solutions](./documents/low_power_solution/readme_en.md) [[中文]](./documents/low_power_solution/readme_cn.md)
    * [Current Consumption Test for ESP32 in Deep sleep]
    * [ESP32 Low-Power Management Overview]
    * [ESP32 ULP Co-processor and Assembly Environment Setup]
* [Light-sleep low power solutions overview](./documents/DFS_and_light_sleep/readme_en.md) [[中文]](./documents/DFS_and_light_sleep/readme_cn.md)
    * [ESP32 DFS test manual]
    * [ESP32 Light-sleep features]
    * [ESP32 Light-sleep test manua]
* [Touch Sensor Application Note](./documents/touch_pad_solution/touch_sensor_design_en.md) [[中文]](./documents/touch_pad_solution/touch_sensor_design_cn.md)
	* [ESP-Tuning Tool User Guide](./documents/touch_pad_solution/esp_tuning_tool_user_guide_en.md) [[中文]](./documents/touch_pad_solution/esp_tuning_tool_user_guide_cn.md)
* [Security and Factory Flow](./documents/security_solution/readme_en.md) [[中文]](./documents/security_solution/readme_cn.md)
    * [ESP32 secure and encrypt]
    * [Download Tool GUI Instruction]

---

## Evaluation Board Series

* [ESP32_ULP_EB_V1 Evaluation Board](./documents/evaluation_boards/readme_en.md) [[中文]](./documents/evaluation_boards/readme_cn.md)
* [ESP32-Sense kit Board](./documents/evaluation_boards/readme_en.md) [[中文]](./documents/evaluation_boards/readme_cn.md)
    * ESP32 Touch Sensor development kit.  
* [ESP-Prog board](./documents/evaluation_boards/readme_en.md) [[中文]](./documents/evaluation_boards/readme_cn.md)
    * Firmware download and JTAG debug tool.  

---

## ESP32 IoT Example List:

* [ESP32 AWS IOT Example](examples/aws_iot_demo)
    - The example demonstrates how to use AWS IOT on ESP32.
    - Keywords: __AWS IOT__

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
    - Keywords: __LittlevGL embedded GUI__

* [ESP32 uGFX GUI Example](examples/hmi/ugfx_example)
    - The example demonstrates how to use uGFX embedded GUI library on ESP32.
    - Keywords: __uGFX embedded GUI__

* [ESP32 Smart Device to Cloud Framework](examples/smart_device)
    - It provides a framework for a ESP32 smart device (smart plug, smart light, etc) to connect and communicate with a cloud service.
    - Keywords: __SmartLight SmartPlug Alink Joylink__

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

---

## Components

### Features

[DAC Audio](./components/features/dac_audio) - Driver and example of using DAC Audio.<br>
[ULP Monitor](./components/features/ulp_monitor) - Provides some APIs for running simple ulp program in deep sleep.<br>
[TouchPad](./components/features/touchpad) - Driver and example of using different types of touch pad.<br>
[Infrared](./components/features/infrared) - Driver and framework of using infrared remote control.<br>

### Framework

[Lowpower_framework](./components/framework/lowpower_framework) - Development framework for lowpower applications.<br>

### General fucntions

[Button](./components/general/button) - Driver and example of using buttons and keys.<br>
[Debugs](./components/general/debugs) - Provides different commands for debugging via UART.<br>
[LED](./components/general/led) - Driver and example of using LED, which provides such APIs as to blink with different frequency.<br>
[Light](./components/general/light) - Driver and example of using PWM to drive a light, which provides such APIs as to control several channels of LED.<br>
[OTA](./components/general/ota) - Driver and example of upgrading firmware from the given URL.<br>
[Param](./components/general/param) - Driver and example of saving and loading data via NVS flash.<br>
[Power Meter](./components/general/power_meter) - Driver and example of a single-phase energy meter such as BL0937 or HLW8012.<br>
[Relay](./components/general/relay) - Driver and example of a relay with different driving modes.<br>
[Weekly timer](./components/general/weekly_timer) - Driver and example of a weekly timer to trigger events at some certain moment in each week.<br>

### HMI

[LittlevGL](./components/hmi/lvgl_gui) - Driver of using LittlevGL embedded GUI library.<br>
[uGFX](./components/hmi/ugfx_gui) - Driver and example of using uGFX embedded GUI library.<br>

### I2C Sensors

[APDS9960](./components/i2c_devices/sensor/apds9960) - Driver and example of reading APDS9960, which is an ambient light photo Sensor.<br>
[BH1750](./components/i2c_devices/sensor/bh1750) - Driver and example of reading BH1750 light sensor (GY-30 module).<br>
[BME280](./components/i2c_devices/sensor/bme280) - Driver and example of reading BME280, which is an pressure and temperature Sensor.<br>
[FT5X06](./components/i2c_devices/sensor/ft5x06) - Driver and example of reading FT5X06, which is a touch Sensor.<br>
[HDC2010](./components/i2c_devices/sensor/hdc2010) - Driver and example of reading HDC2010, which is a low power temperature and humidity sensor.<br>
[HTS221](./components/i2c_devices/sensor/hts221) - Driver and example of reading HTS221 temperature and humidity sensor.<br>
[LIS2DH12](./components/i2c_devices/sensor/lis2dh12) - Driver and example of reading LIS2DH12, which is a 3-axis accelerometer.<br>
[MVH3004D](./components/i2c_devices/sensor/mvh3004d) - Driver and example of reading MVH3004D temperature and humidity sensor.<br>
[VEML6040](./components/i2c_devices/sensor/veml6040) - Driver and example of reading VEML6040, which is a light UV sensor.<br>

### I2C Devices

[AT24C02](./components/i2c_devices/others/at24c02) - Driver and example of driving AT24C02, which is an eeprom storage.<br>
[CH450](./components/i2c_devices/others/ch450) - Driver and example of driving CH450, which is a 7-segment LED driver.<br>
[HT16C21](./components/i2c_devices/others/ht16c21) - Driver and example of driving HT16C21, which is a LED driver.<br>
[IS31FL3XXX](./components/i2c_devices/others/is31fl3xxx) - Driver and example of driving is31fl3xxx series chips, which are light effect LED driver chips.<br>
[MCP23017](./components/i2c_devices/others/mcp23017) - Driver and example of using mcp23017, which is a 16-bit I/O expander.<br>
[SSD1306](./components/i2c_devices/others/ssd1306) - Driver and example of using ssd1306, which is a 132 x 64 dot matrix OLED/PLED segment driver chip.<br>

### I2S Devices

[ILI9806](./components/i2s_devices/ili9806) - Driver and example of driving ILI9806 LCD.<br>
[NT35510](./components/i2s_devices/nt35510) - Driver and example of driving NT35510 LCD.<br>

### Motor

[Servo](./components/motor/servo) - Driver and example of driving servo motors.<br>
[A4988](./components/motor/stepper/a4988) - Driver and example of driving A4988, which is a stepper motor driver.<br>

### Network Abstract

[MQTT](./components/network/mqtt) - Driver and example of using MQTT client, which is a light-weight IoT protocol.<br>
[TCP](./components/network/tcp) - API and example of using TCP server and client in C++.<br>
[UDP](./components/network/udp) - API and example of using UDP in C++.<br>
[Alink](./components/platforms/alink) - API and example of connecting and communicating with Alink cloud service.<br>

### SPI Devices

[E-ink display](./components/spi_devices/epaper) - API and example of driving and controlling SPI E-ink screen.<br>
[LCD screen](./components/spi_devices/lcd) - API and example of driving and controlling SPI LCD.<br>
[XPT2046 Touch screen](./components/spi_devices/xpt2046) - API and example of driving and controlling SPI Touch Screen.<br>

### WiFi Abstract

[ESP-TOUCH for smart-config](./components/wifi/smart_config) - Abstract APIs and example of configuring devices via esp-touch.<br>
[Blufi abstract APIs](./components/wifi/iot_blufi) - Abstract APIs and example of configuring devices via blufi.<br>
[WiFi connection abstract APIs](./components/wifi/wifi_conn) - Abstract APIs and example of WiFi station connecting to router.<br>

---

## <h2 id="preparation">Preparation(Make)</h2>

* To clone this repository by

    ```
    git clone --recursive https://github.com/espressif/esp-iot-solution
    ```

* To initialize the submodules

    ```
    git submodule update --init --recursive
    ```

* Set IOT_SOLUTION_PATH, for example:

    ```
    export IOT_SOLUTION_PATH=~/esp/esp-iot-solution
    ```

* esp-iot-solution project will over `write` the `IDF_PATH` to the submodule in makefile by default
* You can refer to the Makefile in example if you want to build your own application.
* To run the example project, please refer to:

    * [example](#example-make)
    * [unit-test](#unit-test-make)

---

## <h2 id="preparation">Preparation(CMake)</h2>

* Add to PATH the path to the tools directory inside the IDF_PATH directory.[Add IDF_PATH & idf.py PATH to User Profile(CMake)](https://docs.espressif.com/projects/esp-idf/en/stable/get-started-cmake/add-idf_path-to-profile.html)
* To clone this repository by

    ```
    git clone --recursive https://github.com/espressif/esp-iot-solution
    ```

* To initialize the submodules

    ```
    git submodule update --init --recursive
    ```

* Set IOT_SOLUTION_PATH, for example:

    ```
    export IOT_SOLUTION_PATH=~/esp/esp-iot-solution
    ```

* esp-iot-solution project will over `write` the `IDF_PATH` to the submodule in makefile by default
* You can refer to the Makefile in example if you want to build your own application.
* To run the example project, please refer to:

    * [example](#example-cmake)
    * [unit-test](#unit-test-cmake)

---

## Build system and dependency (Make)

* We can regard IoT solution project as a platform that contains different device drivers and features
* `Add-on project`: If you want to use those drivers and build your project base on the framework, you need to include the IoT components into your project.

    ```
    PROJECT_NAME := empty_project
    #If IOT_SOLUTION_PATH is not defined, use relative path as default value
    IOT_SOLUTION_PATH ?= $(abspath $(shell pwd)/../../)
    include $(IOT_SOLUTION_PATH)/Makefile
    include $(IDF_PATH)/make/project.mk
    ```

    As we can see, in the Makefile of your project, you need to include the Makefile under $(IOT_SOLUTION_PATH) directory so that the build system can involve all the components and drivers you need.

    `Note: In this way, the build system will replace the IDF_PATH with the directory of idf repository in submodule during the build process.`

    If you don't want the replacement to happen(which means to use the ESP_IDF value from your system environment), you can modify as the following Makefile does:

    ```
    PROJECT_NAME := empty_project
    #If IOT_SOLUTION_PATH is not defined, use relative path as default value
    IOT_SOLUTION_PATH ?= $(abspath $(shell pwd)/../../)
    include $(IOT_SOLUTION_PATH)/components/component_conf.mk
    include $(IDF_PATH)/make/project.mk
    ```

* `Stand-alone component`: if you just want to pick one of the component and put it into your existing project, you just need to copy the single component to your project components directory. But you can also append the component list in your project Makefile like this:

    ```
    EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/the_one_you_want_to_use
    ```

* `Components control`: Usually we don't need all the device drivers to be compiled into our project, we can choose the necessary ones in menuconfig:

    ```
    make menuconfig --> IoT Solution settings --> IoT Components Management
    ```

    Those components that are not enabled, will not be compiled into the project, which alos means, you need to enable the components you would like to use.

---

## Build system and dependency (CMake)

* We can regard IoT solution project as a platform that contains different device drivers and features
* `Add-on project`: If you want to use those drivers and build your project base on the framework, you need to include the IoT components into your project.

    ```
    # The following lines of boilerplate have to be in your project's
    # CMakeLists in this exact order for cmake to work correctly
    cmake_minimum_required(VERSION 3.5)

    #If IOT_SOLUTION_PATH is not defined, use relative path as default value
    set(IOT_SOLUTION_PATH "$ENV{IOT_SOLUTION_PATH}")
    if(NOT IOT_SOLUTION_PATH)
        # Documentation says you should set IOT_SOLUTION_PATH in your environment, but we
        # can infer it relative to PROJECT_PATH directory if it's not set.
        get_filename_component(IOT_SOLUTION_PATH "${PROJECT_PATH}/../.." ABSOLUTE)
    endif()
    file(TO_CMAKE_PATH "${IOT_SOLUTION_PATH}" IOT_SOLUTION_PATH)
    set(ENV{IOT_SOLUTION_PATH} ${IOT_SOLUTION_PATH})

    include($ENV{IOT_SOLUTION_PATH}/CMakeLists.txt)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)

    project(empty-project)
    ```

    As we can see, in the Makefile of your project, you need to include the Makefile under $(IOT_SOLUTION_PATH) directory so that the build system can involve all the components and drivers you need.

    `Note: In this way, the build system will replace the IDF_PATH with the directory of idf repository in submodule during the build process.`

    If you don't want the replacement to happen(which means to use the ESP_IDF value from your system environment), you can modify as the following Makefile does:

    ```
    # The following lines of boilerplate have to be in your project's
    # CMakeLists in this exact order for cmake to work correctly
    cmake_minimum_required(VERSION 3.5)

    #If IOT_SOLUTION_PATH is not defined, use relative path as default value
    set(IOT_SOLUTION_PATH "$ENV{IOT_SOLUTION_PATH}")
    if(NOT IOT_SOLUTION_PATH)
        # Documentation says you should set IOT_SOLUTION_PATH in your environment, but we
        # can infer it relative to PROJECT_PATH directory if it's not set.
        get_filename_component(IOT_SOLUTION_PATH "${PROJECT_PATH}/../.." ABSOLUTE)
    endif()
    file(TO_CMAKE_PATH "${IOT_SOLUTION_PATH}" IOT_SOLUTION_PATH)
    set(ENV{IOT_SOLUTION_PATH} ${IOT_SOLUTION_PATH})

    include($ENV{IOT_SOLUTION_PATH}/CMakeLists.txt)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)

    project(empty-project)
    ```

* `Stand-alone component`: if you just want to pick one of the component and put it into your existing project, you just need to copy the single component to your project components directory. But you can also append the component list in your project Makefile like this:

    ```
    set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS} ${IOT_SOLUTION_PATH}/components/the_one_you_want_to_use")
    ```

* `Components control`: Usually we don't need all the device drivers to be compiled into our project, we can choose the necessary ones in menuconfig:

    ```
    idf.py menuconfig --> IoT Solution settings --> IoT Components Management
    ```

    Those components that are not enabled, will not be compiled into the project, which alos means, you need to enable the components you would like to use.

---

## Framework

* `components`
    * small drivers of different divices like button and LED
    * drivers of sensors
    * drivers of different I2C devices
    * friendly APIs of WiFi and OTA
* `Documents`:
    * Documentations of some important features
    * Instruction of some different solutions
* `Examples`:
    * Example project using this framework
* `Submodule`:
    * esp-idf works as submodule here
* `tools`:
    * different tools and scripts
    * unit-test project

    ```
    ├── Makefile
    ├── README.md
    ├── components
    ├── documents
    ├── examples
    │   └── aws_iot_demo
    │   └── check_pedestrian_flow
    │   └── empty_project
    │   └── esp32_azure_iot_kit 
    │   └── eth2wifi
    │   └── hmi
    │   └── lowpower_evb
    │   └── oled_screen_module
    │   └── smart_device
    │   └── touch_pad_evb
    │   └── ulp_examples
    ├── submodule
    │   └── esp-idf
    └── tools
        └── unit-test-app
    ```

---

## Usage (Make)

* This code is based on esp-idf project.
* This repository contains esp-idf code as submodule.
* To clone this repository by **git clone --recurisve https://github.com/espressif/esp-iot-solution.git**
* Change to the directory of examples (such as examples/smart_device) in esp-iot-solution, run `make menuconfig` to configure the project.
* Compiling the Project by `make all`

    > ... will compile app, bootloader and generate a partition table based on the config.

*  Flashing the Project
* Flash the binaries by `make flash`

    > This will flash the entire project (app, bootloader and partition table) to a new chip. The settings for serial port flashing can be configured with `make menuconfig`.    
    > You don't need to run `make all` before running `make flash`, `make flash` will automatically rebuild anything which needs it.

* Viewing Serial Output by `make monitor` target will use the already-installed [miniterm](http://pyserial.readthedocs.io/en/latest/tools.html#module-serial.tools.miniterm) (a part of pyserial) to display serial output from the ESP32 on the terminal console.
Exit miniterm by typing Ctrl-].
* To flash and monitor output in one pass, you can run: `make flash monitor`
* You can use unit-test in esp-iot-solution to test all the components.

---

## Usage (CMake)

* This code is based on esp-idf project.
* This repository contains esp-idf code as submodule.
* To clone this repository by **git clone --recurisve https://github.com/espressif/esp-iot-solution.git**
* Change to the directory of examples (such as examples/smart_device) in esp-iot-solution, run `idf.py menuconfig` to configure the project.
* Compiling the Project by `idf.py build`

    > ... will compile app, bootloader and generate a partition table based on the config.

*  Flashing the Project
* Flash the binaries by `idf.py flash`

    > This will flash the entire project (app, bootloader and partition table) to a new chip. The settings for serial port flashing can be configured with `idf.py menuconfig`.    
    > You don't need to run `idf.py all` before running `idf.py flash`, `idf.py flash` will automatically rebuild anything which needs it.

* Viewing Serial Output by `idf.py monitor` target will use the already-installed [miniterm](http://pyserial.readthedocs.io/en/latest/tools.html#module-serial.tools.miniterm) (a part of pyserial) to display serial output from the ESP32 on the terminal console.
Exit miniterm by typing Ctrl-].
* To flash and monitor output in one pass, you can run: `idf.py flash monitor`
* You can use unit-test in esp-iot-solution to test all the components.

---

## <h2 id="unit-test">Unit-test (Make)</h2>

[back to top](#preparation)

### To use uint-test, follow these steps:

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

---

## <h2 id="unit-test">Unit-test (CMake)</h2>

[back to top](#preparation)

### To use uint-test, follow these steps:

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

---

## <h2 id="example">Example (Make)</h2>

[back to top](#preparation)

### To run the Examples projects, follow the steps below:

* Change the directory to example
* choose one example project you want to run, we take smart_device here.
* Change the directory to the example project under example directory, take smart_device example as example here:

    ```
    cd YOUR_IOT_SOLUTION_PATH/example/smart_device
    ```

* Run `make menuconfig` to set the project parameters in

    ```
    make menuconfig --> IoT Example - smart_device
    ```

* Run `make` to compile the project, or `make flash` to compile and flash the module.

---

## <h2 id="example">Example (CMake)</h2>

[back to top](#preparation)

### To run the Examples projects, follow the steps below:

* Change the directory to example
* choose one example project you want to run, we take smart_device here.
* Change the directory to the example project under example directory, take smart_device example as example here:

    ```
    cd YOUR_IOT_SOLUTION_PATH/example/smart_device/alink_smart_device_demo
    ```

* Run `idf.py menuconfig` to set the project parameters in

    ```
    idf.py menuconfig --> IoT Example - smart_device
    ```

* Run `idf.py` to compile the project, or `idf.py flash` to compile and flash the module.

---

## <h2 id="empty_project">Empty project (Make)</h2>

[back to top](#preparation)

* You can start your own appliction code base on the empty_project.
* By default, you just need to run `make` under the example/empty_project directory. The makefile will set all the path by default.
* Meanwhile, you can copy the example project to anywhere you want, in this case, you will have to set the `IOT_SOLUTION_PATH` so that the build system shall know where to find the code and link the files.
* Set iot path:(replace 'YOUR_PATH' below), you can also add `IOT_SOLUTION_PATH ` to your environment PATH

    ```
        export IOT_SOLUTION_PATH = "YOUR_PATH/esp-iot-solution"
    ```

---

## <h2 id="empty_project">Empty project (CMake)</h2>

[back to top](#preparation)

* You can start your own appliction code base on the empty_project.
* By default, you just need to run `idf.py build` under the example/empty_project directory. The makefile will set all the path by default.
* Meanwhile, you can copy the example project to anywhere you want, in this case, you will have to set the `IOT_SOLUTION_PATH` so that the build system shall know where to find the code and link the files.
* Set iot path:(replace 'YOUR_PATH' below), you can also add `IOT_SOLUTION_PATH ` to your environment PATH

    ```
        export IOT_SOLUTION_PATH = "YOUR_PATH/esp-iot-solution"
    ```

---
