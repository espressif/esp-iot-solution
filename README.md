[![Documentation Status](https://dl.espressif.com/AE/docs/docs_latest.svg)](https://docs.espressif.com/projects/esp-iot-solution/en)

## Espressif IoT Solution Overview

* [中文版](./README_CN.md)

ESP-IoT-Solution contains device drivers and code frameworks for the development of IoT systems, providing extra components that enhance the capabilities of ESP-IDF and greatly simplify the development process.

ESP-IoT-Solution contains the following contents:

* Device drivers for sensors, display, audio, input, actuators, etc.
* Framework and documentation for Low power, security, storage, etc.
* Guide for espressif open source solutions from practical application point.

## Documentation Center

- 中文：https://docs.espressif.com/projects/esp-iot-solution/zh_CN
- English:https://docs.espressif.com/projects/esp-iot-solution/en

## Quick Reference

### Development Board

You can choose any of the ESP series development boards to use ESP-IoT-Solution or choose one of the boards supported in [boards component](./examples/common_components/boards) for a quick start.

Powered by 40nm technology, ESP series SoC provides a robust, highly integrated platform, which helps meet the continuous demands for efficient power usage, compact design, security, high performance, and reliability.

### Setup Environment

#### Setup ESP-IDF Environment

ESP-IoT-Solution is developed based on ESP-IDF functions and tools, so ESP-IDF development environment must be set up first, you can refer [Setting up Development Environment](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#setting-up-development-environment) for the detailed steps.

Please note that different versions of ESP-IoT-Solution may depend on different versions of ESP-IDF, please refer to the below table to select the correct version:

| ESP-IoT-Solution | Dependent ESP-IDF |                    Major Change                     |                                                  User Guide                                                  |        Support State        |
| :--------------: | :---------------: | :-------------------------------------------------: | :----------------------------------------------------------------------------------------------------------: | --------------------------- |
|      master      |      >= v4.4      |       support component manager and new chips       |             [Docs online](https://docs.espressif.com/projects/esp-iot-solution/zh_CN)              | active, new feature develop |
|   release/v1.1   |      v4.0.1       | IDF version update, remove no longer supported code | [v1.1 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.1#esp32-iot-solution-overview) | archived             |
|   release/v1.0   |      v3.2.2       |                   legacy version                    | [v1.0 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.0#esp32-iot-solution-overview) | archived                    |

> Since the `master` branch uses the `ESP Component Manager` to manager components, each of them is a separate package, and each package may support a different version of the ESP-IDF, which will be declared in the component's `idf_component.yml` file

#### Get Components from ESP Component Registry

If you just want to use the components in ESP-IoT-Solution, we recommend you use it from the [ESP Component Registry](https://components.espressif.com/).

The registered components in ESP-IoT-Solution are listed below:

<center>

|                                              Component Name                                              |                                                                                             Version                                                                                             |
| :------------------------------------------------------------------------------------------------------: | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------: |
|                 [ble_ota](https://components.espressif.com/components/espressif/ble_ota)                 |                 [![Component Registry](https://components.espressif.com/components/espressif/ble_ota/badge.svg)](https://components.espressif.com/components/espressif/ble_ota)                 |
| [bootloader_support_plus](https://components.espressif.com/components/espressif/bootloader_support_plus) | [![Component Registry](https://components.espressif.com/components/espressif/bootloader_support_plus/badge.svg)](https://components.espressif.com/components/espressif/bootloader_support_plus) |
|                  [button](https://components.espressif.com/components/espressif/button)                  |                  [![Component Registry](https://components.espressif.com/components/espressif/button/badge.svg)](https://components.espressif.com/components/espressif/button)                  |
|         [cmake_utilities](https://components.espressif.com/components/espressif/cmake_utilities)         |         [![Component Registry](https://components.espressif.com/components/espressif/cmake_utilities/badge.svg)](https://components.espressif.com/components/espressif/cmake_utilities)         |
|            [esp_lcd_panel_io_additions](https://components.espressif.com/components/espressif/esp_lcd_panel_io_additions)            |            [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_panel_io_additions/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_panel_io_additions)            |
|              [esp_tinyuf2](https://components.espressif.com/components/espressif/esp_tinyuf2)              |              [![Component Registry](https://components.espressif.com/components/espressif/esp_tinyuf2/badge.svg)](https://components.espressif.com/components/espressif/esp_tinyuf2)              |
|            [extended_vfs](https://components.espressif.com/components/espressif/extended_vfs)            |            [![Component Registry](https://components.espressif.com/components/espressif/extended_vfs/badge.svg)](https://components.espressif.com/components/espressif/extended_vfs)            |
|                   [gprof](https://components.espressif.com/components/espressif/gprof)                   |                   [![Component Registry](https://components.espressif.com/components/espressif/gprof/badge.svg)](https://components.espressif.com/components/espressif/gprof)                   |
|                [iot_usbh](https://components.espressif.com/components/espressif/iot_usbh)                |                [![Component Registry](https://components.espressif.com/components/espressif/iot_usbh/badge.svg)](https://components.espressif.com/components/espressif/iot_usbh)                |
|            [iot_usbh_cdc](https://components.espressif.com/components/espressif/iot_usbh_cdc)            |            [![Component Registry](https://components.espressif.com/components/espressif/iot_usbh_cdc/badge.svg)](https://components.espressif.com/components/espressif/iot_usbh_cdc)            |
|          [iot_usbh_modem](https://components.espressif.com/components/espressif/iot_usbh_modem)          |          [![Component Registry](https://components.espressif.com/components/espressif/iot_usbh_modem/badge.svg)](https://components.espressif.com/components/espressif/iot_usbh_modem)          |
|                    [knob](https://components.espressif.com/components/espressif/knob)                    |                    [![Component Registry](https://components.espressif.com/components/espressif/knob/badge.svg)](https://components.espressif.com/components/espressif/knob)                    |
|           [led_indicator](https://components.espressif.com/components/espressif/led_indicator)           |           [![Component Registry](https://components.espressif.com/components/espressif/led_indicator/badge.svg)](https://components.espressif.com/components/espressif/led_indicator)           |
|        [lightbulb_driver](https://components.espressif.com/components/espressif/lightbulb_driver)        |        [![Component Registry](https://components.espressif.com/components/espressif/lightbulb_driver/badge.svg)](https://components.espressif.com/components/espressif/lightbulb_driver)        |
|        [openai](https://components.espressif.com/components/espressif/openai)                            |        [![Component Registry](https://components.espressif.com/components/espressif/openai/badge.svg)](https://components.espressif.com/components/espressif/openai)
|               [pwm_audio](https://components.espressif.com/components/espressif/pwm_audio)               |               [![Component Registry](https://components.espressif.com/components/espressif/pwm_audio/badge.svg)](https://components.espressif.com/components/espressif/pwm_audio)               |
|              [usb_stream](https://components.espressif.com/components/espressif/usb_stream)              |              [![Component Registry](https://components.espressif.com/components/espressif/usb_stream/badge.svg)](https://components.espressif.com/components/espressif/usb_stream)              |
|                      [xz](https://components.espressif.com/components/espressif/xz)                      |                      [![Component Registry](https://components.espressif.com/components/espressif/xz/badge.svg)](https://components.espressif.com/components/espressif/xz)                      |

</center>

You can directly add the components from the Component Registry to your project by using the `idf.py add-dependency` command under your project's root directory. eg run `idf.py add-dependency "espressif/usb_stream"` to add the `usb_stream`, the component will be downloaded automatically during the `CMake` step.

> Please refer to [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html) for details.

#### Get ESP-IoT-Solution Repository

If you want to [Contribute](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/contribute/index.html) to the components in ESP-IoT-Solution or want to start from the examples in ESP-IoT-Solution, you can get the ESP-IoT-Solution repository by following the steps:

* If select the `master` version, open the terminal, and run the following command:

    ```
    git clone --recursive https://github.com/espressif/esp-iot-solution
    ```

* If select the `release/v1.1` version, open the terminal, and run the following command:

    ```
    git clone -b release/v1.1 --recursive https://github.com/espressif/esp-iot-solution
    ```

#### Build and Flash Examples

**We highly recommend** you [Build Your First Project](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#build-your-first-project) to get familiar with ESP-IDF and make sure the environment is set up correctly.

There is no difference between building and flashing the examples in ESP-IoT-Solution and in ESP-IDF. In most cases, you can build and flash the examples in ESP-IoT-Solution by following the steps:

1. Change the current directory to the example directory, eg `cd examples/usb/host/usb_audio_player`.
2. Run `idf.py set-target TARGET` to set the target chip. eg `idf.py set-target esp32s3` to set the target chip to ESP32-S3.
3. Run `idf.py build` to build the example.
4. Run `idf.py -p PORT flash monitor` to flash the example, and view the serial output. eg `idf.py -p /dev/ttyUSB0 flash monitor` to flash the example and view the serial output on `/dev/ttyUSB0`.

> Some examples may need extra steps to setup the ESP-IoT-Solution environment, you can run `export IOT_SOLUTION_PATH=~/esp/esp-iot-solution` in Linux/MacOS or `set IOT_SOLUTION_PATH=C:\esp\esp-iot-solution` in Windows to setup the environment.

### Resources

- Documentation for the latest version: https://docs.espressif.com/projects/esp-iot-solution/ . This documentation is built from the [docs directory](./docs) of this repository.
- ESP-IDF Programming Guide: https://docs.espressif.com/projects/esp-idf/zh_CN . Please refer to the version ESP-IoT-Solution depends on.
- The [IDF Component Registry](https://components.espressif.com/) is where you can find the components in ESP-IoT-Solution and other registered components.
- The [esp32.com forum](https://www.esp32.com/) is a place to ask questions and find community resources.
- [Check the Issues section on GitHub]((https://github.com/espressif/esp-iot-solution/issues)) if you find a bug or have a feature request. Please check existing Issues before opening a new one.
- If you're interested in contributing to ESP-IDF, please check the [Contributions Guide](./CONTRIBUTING.rst).
