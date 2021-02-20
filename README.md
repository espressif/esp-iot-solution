[![Documentation Status](https://readthedocs.com/projects/espressif-esp-iot-solution/badge/?version=latest)](https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/?badge=latest)

## Espressif IoT Solution Overview

* [中文版](./README_CN.md)

ESP-IoT-Solution contains device drivers and code frameworks for the development of IoT system, which works as extra components of ESP-IDF and much easier to start.

ESP-IoT-Solution contains following contents:

* Device drivers for sensors, display, audio, input, actuators, etc.
* Framework and documentation for Low power, security, storage, etc.
* Guide for espressif open source solutions from practical application point.

## Documentation Center

- 中文：https://docs.espressif.com/projects/espressif-esp-iot-solution/zh_CN
- English:https://docs.espressif.com/projects/espressif-esp-iot-solution/en

## Versions

| ESP-IoT-Solution | Dependent ESP-IDF |  Major Change                                          |    User Guide                                                             | Support State  |
| :--------------: | :---------------: | :----------------------------------------------------: | :-----------------------------------------------------------------------: | -------------- |
|      master      |   release/v4.3    |  code structure change, add support for ESP32-S2       | [Docs online](https://docs.espressif.com/projects/espressif-esp-iot-solution/zh_CN) | active, new feature develop |
|   release/v1.1   |      v4.0.1       |  IDF version update, remove no longer supported code   | [v1.1 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.1#esp32-iot-solution-overview) |limited support |
|   release/v1.0   |      v3.2.2       |  legacy version                                        | [v1.0 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.0#esp32-iot-solution-overview) | archived |


## Quick Reference

### Development Board

You can choose any of the ESP32/ESP32-S series development boards to use ESP-IoT-Solution, or choose one of the boards supported in [boards component](./examples/common_components/boards) to quick start.

Powered by 40 nm technology, ESP32/ESP32-S series SOC provides a robust, highly integrated platform, which helps meet the continuous demands for efficient power usage, compact design, security, high performance, and reliability.

### Setup Build Environment

#### Setup ESP-IDF Environment

ESP-IoT-Solution is developed based on ESP-IDF functions and tools, so ESP-IDF development environment must be setup first, you can refer [Setting up Development Environment](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#setting-up-development-environment) for the detailed steps.

Please note that different versions of ESP-IoT-Solution may depend on different versions of ESP-IDF, please refer to [Versions](#Versions) for details.

#### Get ESP-IoT-Solution

* If select the `master` version, open terminal, and run the following command:

    ```
    git clone --recursive https://github.com/espressif/esp-iot-solution
    ```

* If select the `release/v1.1` version, open terminal, and run the following command:

    ```
    git clone -b release/v1.1 --recursive https://github.com/espressif/esp-iot-solution
    ```

For other versions, replace `release/v1.1` with the target branch name

#### Create Your Project

You can use the components of ESP-IoT-Solution in the following methods:

**Method 1**. Add all components' PATH of ESP-IoT-Solution to the project 

Add the following code directly to the project's CMakeLists.txt:

```
cmake_minimum_required(VERSION 3.5)

include($ENV{IOT_SOLUTION_PATH}/component.cmake)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(empty-project)
```

**Method 2**. Add specified components' PATH of ESP-IoT-Solution to the project

Add the following code directly to the project's CMakeLists.txt, please add the `REQUIRES` components also:

```
set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS} ${IOT_SOLUTION_PATH}/components/{component_you_choose}")
```

**Method 3**. Copy specified components of ESP-IoT-solution to the project's `components` directory

Please copy the `REQUIRES` components together.

> Note:
>CMake based build system is recommended (default in IDF v4.0 and later). If using GNU Make, you can refer [legacy build system based on GNU Make](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system-legacy.html).

#### Set up the environment variables

Add `ESP-IDF` environment variables:

* For Windows CMD terminal, run the following command:

    ```
    %userprofile%\esp\esp-idf\export.bat
    ```

* For Linux or macOS terminal, run the following command:

    ```
    . $HOME/esp/esp-idf/export.sh
    ```

Add `IOT_SOLUTION_PATH` environment variables:

* For Windows CMD terminal, run the following command:

    ```
    set IOT_SOLUTION_PATH=C:\esp\esp-iot-solution
    ```

* For Linux or macOS terminal, run the following command:

    ```
    export IOT_SOLUTION_PATH=~/esp/esp-iot-solution
    ```

> Note:
>   1. Please replace the example path with your actual installation path.
>   2. The environment variables set above only valid for the current terminal. please re-run the command if open new terminal.

### Resources

- Documentation for the latest version:https://docs.espressif.com/projects/espressif-esp-iot-solution/ . This documentation is built from the [docs directory](./docs) of this repository.
- ESP-IDF Programming Guide: https://docs.espressif.com/projects/esp-idf/zh_CN . Please refer to the version ESP-IoT-solution depends on.
- The [esp32.com forum](https://esp32.com/) is a place to ask questions and find community resources.
- [Check the Issues section on github]((https://github.com/espressif/esp-iot-solution/issues)) if you find a bug or have a feature request. Please check existing Issues before opening a new one.
- If you're interested in contributing to ESP-IDF, please check the [Contributions Guide](./CONTRIBUTING.rst).