[![Documentation Status](https://dl.espressif.com/AE/docs/docs_latest.svg)](https://docs.espressif.com/projects/esp-iot-solution/en)

<a href="https://espressif.github.io/esp-launchpad/?flashConfigURL=https://dl.espressif.com/AE/esp-iot-solution/release_v2_0/config.toml">
    <img alt="Try it with ESP Launchpad" src="https://espressif.github.io/esp-launchpad/assets/try_with_launchpad.png" width="200" height="56">
</a>

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
|      master      |      >= v5.3      |                  New Chips Support                  |                     [Docs online](https://docs.espressif.com/projects/esp-iot-solution/)                     | New features                |
|   release/v2.0   | <= v5.3, >= v4.4  |              Support component manager              |        [Docs online](https://docs.espressif.com/projects/esp-iot-solution/en/release-v2.0/index.html)        | Bugfix only，until v5.3 EOL |
|   release/v1.1   |      v4.0.1       | IDF version update, remove no longer supported code | [v1.1 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.1#esp32-iot-solution-overview) | archived                    |
|   release/v1.0   |      v3.2.2       |                   legacy version                    | [v1.0 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.0#esp32-iot-solution-overview) | archived                    |

> Since the `master` branch uses the `ESP Component Manager` to manager components, each of them is a separate package, and each package may support a different version of the ESP-IDF, which will be declared in the component's `idf_component.yml` file

#### Get Components from ESP Component Registry

If you just want to use the components in ESP-IoT-Solution, we recommend you use it from the [ESP Component Registry](https://components.espressif.com/).

The registered components in ESP-IoT-Solution are listed below:

<center>

| Component | Version |
| --- | --- |
| [aht20](https://components.espressif.com/components/espressif/aht20/versions/0.1.0~1) | [![0.1.0~1](https://img.shields.io/badge/Beta-0.1.0~1-yellow)](https://components.espressif.com/components/espressif/aht20/versions/0.1.0~1) |
| [at581x](https://components.espressif.com/components/espressif/at581x/versions/0.1.0~1) | [![0.1.0~1](https://img.shields.io/badge/Beta-0.1.0~1-yellow)](https://components.espressif.com/components/espressif/at581x/versions/0.1.0~1) |
| [avi_player](https://components.espressif.com/components/espressif/avi_player/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/avi_player/versions/1.0.0) |
| [ble_anp](https://components.espressif.com/components/espressif/ble_anp/versions/0.1.0) | [![0.1.0](https://img.shields.io/badge/Beta-0.1.0-yellow)](https://components.espressif.com/components/espressif/ble_anp/versions/0.1.0) |
| [ble_conn_mgr](https://components.espressif.com/components/espressif/ble_conn_mgr/versions/0.1.2) | [![0.1.2](https://img.shields.io/badge/Beta-0.1.2-yellow)](https://components.espressif.com/components/espressif/ble_conn_mgr/versions/0.1.2) |
| [ble_hci](https://components.espressif.com/components/espressif/ble_hci/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/ble_hci/versions/1.0.0) |
| [ble_hrp](https://components.espressif.com/components/espressif/ble_hrp/versions/0.1.0) | [![0.1.0](https://img.shields.io/badge/Beta-0.1.0-yellow)](https://components.espressif.com/components/espressif/ble_hrp/versions/0.1.0) |
| [ble_htp](https://components.espressif.com/components/espressif/ble_htp/versions/0.1.0) | [![0.1.0](https://img.shields.io/badge/Beta-0.1.0-yellow)](https://components.espressif.com/components/espressif/ble_htp/versions/0.1.0) |
| [ble_ota](https://components.espressif.com/components/espressif/ble_ota/versions/0.1.12) | [![0.1.12](https://img.shields.io/badge/Beta-0.1.12-yellow)](https://components.espressif.com/components/espressif/ble_ota/versions/0.1.12) |
| [ble_services](https://components.espressif.com/components/espressif/ble_services/versions/0.1.0) | [![0.1.0](https://img.shields.io/badge/Beta-0.1.0-yellow)](https://components.espressif.com/components/espressif/ble_services/versions/0.1.0) |
| [bootloader_support_plus](https://components.espressif.com/components/espressif/bootloader_support_plus/versions/0.2.6) | [![0.2.6](https://img.shields.io/badge/Beta-0.2.6-yellow)](https://components.espressif.com/components/espressif/bootloader_support_plus/versions/0.2.6) |
| [button](https://components.espressif.com/components/espressif/button/versions/3.4.0) | [![3.4.0](https://img.shields.io/badge/Stable-3.4.0-blue)](https://components.espressif.com/components/espressif/button/versions/3.4.0) |
| [cmake_utilities](https://components.espressif.com/components/espressif/cmake_utilities/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/cmake_utilities/versions/1.0.0) |
| [drv10987](https://components.espressif.com/components/espressif/drv10987/versions/0.1.0) | [![0.1.0](https://img.shields.io/badge/Beta-0.1.0-yellow)](https://components.espressif.com/components/espressif/drv10987/versions/0.1.0) |
| [elf_loader](https://components.espressif.com/components/espressif/elf_loader/versions/0.1.0) | [![0.1.0](https://img.shields.io/badge/Beta-0.1.0-yellow)](https://components.espressif.com/components/espressif/elf_loader/versions/0.1.0) |
| [esp_lcd_axs15231b](https://components.espressif.com/components/espressif/esp_lcd_axs15231b/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/esp_lcd_axs15231b/versions/1.0.0) |
| [esp_lcd_ek79007](https://components.espressif.com/components/espressif/esp_lcd_ek79007/versions/1.0.1) | [![1.0.1](https://img.shields.io/badge/Stable-1.0.1-blue)](https://components.espressif.com/components/espressif/esp_lcd_ek79007/versions/1.0.1) |
| [esp_lcd_gc9b71](https://components.espressif.com/components/espressif/esp_lcd_gc9b71/versions/1.0.2) | [![1.0.2](https://img.shields.io/badge/Stable-1.0.2-blue)](https://components.espressif.com/components/espressif/esp_lcd_gc9b71/versions/1.0.2) |
| [esp_lcd_hx8399](https://components.espressif.com/components/espressif/esp_lcd_hx8399/versions/1.0.1) | [![1.0.1](https://img.shields.io/badge/Stable-1.0.1-blue)](https://components.espressif.com/components/espressif/esp_lcd_hx8399/versions/1.0.1) |
| [esp_lcd_jd9165](https://components.espressif.com/components/espressif/esp_lcd_jd9165/versions/1.0.1) | [![1.0.1](https://img.shields.io/badge/Stable-1.0.1-blue)](https://components.espressif.com/components/espressif/esp_lcd_jd9165/versions/1.0.1) |
| [esp_lcd_jd9365](https://components.espressif.com/components/espressif/esp_lcd_jd9365/versions/1.0.1) | [![1.0.1](https://img.shields.io/badge/Stable-1.0.1-blue)](https://components.espressif.com/components/espressif/esp_lcd_jd9365/versions/1.0.1) |
| [esp_lcd_nv3022b](https://components.espressif.com/components/espressif/esp_lcd_nv3022b/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/esp_lcd_nv3022b/versions/1.0.0) |
| [esp_lcd_panel_io_additions](https://components.espressif.com/components/espressif/esp_lcd_panel_io_additions/versions/1.0.1) | [![1.0.1](https://img.shields.io/badge/Stable-1.0.1-blue)](https://components.espressif.com/components/espressif/esp_lcd_panel_io_additions/versions/1.0.1) |
| [esp_lcd_sh8601](https://components.espressif.com/components/espressif/esp_lcd_sh8601/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/esp_lcd_sh8601/versions/1.0.0) |
| [esp_lcd_spd2010](https://components.espressif.com/components/espressif/esp_lcd_spd2010/versions/1.0.2) | [![1.0.2](https://img.shields.io/badge/Stable-1.0.2-blue)](https://components.espressif.com/components/espressif/esp_lcd_spd2010/versions/1.0.2) |
| [esp_lcd_st7701](https://components.espressif.com/components/espressif/esp_lcd_st7701/versions/1.1.1) | [![1.1.1](https://img.shields.io/badge/Stable-1.1.1-blue)](https://components.espressif.com/components/espressif/esp_lcd_st7701/versions/1.1.1) |
| [esp_lcd_st7703](https://components.espressif.com/components/espressif/esp_lcd_st7703/versions/1.0.1) | [![1.0.1](https://img.shields.io/badge/Stable-1.0.1-blue)](https://components.espressif.com/components/espressif/esp_lcd_st7703/versions/1.0.1) |
| [esp_lcd_st77903_qspi](https://components.espressif.com/components/espressif/esp_lcd_st77903_qspi/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/esp_lcd_st77903_qspi/versions/1.0.0) |
| [esp_lcd_st77903_rgb](https://components.espressif.com/components/espressif/esp_lcd_st77903_rgb/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/esp_lcd_st77903_rgb/versions/1.0.0) |
| [esp_lcd_st77916](https://components.espressif.com/components/espressif/esp_lcd_st77916/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/esp_lcd_st77916/versions/1.0.0) |
| [esp_lcd_st77922](https://components.espressif.com/components/espressif/esp_lcd_st77922/versions/1.0.2) | [![1.0.2](https://img.shields.io/badge/Stable-1.0.2-blue)](https://components.espressif.com/components/espressif/esp_lcd_st77922/versions/1.0.2) |
| [esp_lcd_touch_spd2010](https://components.espressif.com/components/espressif/esp_lcd_touch_spd2010/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/esp_lcd_touch_spd2010/versions/1.0.0) |
| [esp_lcd_touch_st7123](https://components.espressif.com/components/espressif/esp_lcd_touch_st7123/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/esp_lcd_touch_st7123/versions/1.0.0) |
| [esp_lcd_usb_display](https://components.espressif.com/components/espressif/esp_lcd_usb_display/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/esp_lcd_usb_display/versions/1.0.0) |
| [esp_lv_decoder](https://components.espressif.com/components/espressif/esp_lv_decoder/versions/0.1.2) | [![0.1.2](https://img.shields.io/badge/Beta-0.1.2-yellow)](https://components.espressif.com/components/espressif/esp_lv_decoder/versions/0.1.2) |
| [esp_lv_fs](https://components.espressif.com/components/espressif/esp_lv_fs/versions/0.1.0) | [![0.1.0](https://img.shields.io/badge/Beta-0.1.0-yellow)](https://components.espressif.com/components/espressif/esp_lv_fs/versions/0.1.0) |
| [esp_mmap_assets](https://components.espressif.com/components/espressif/esp_mmap_assets/versions/1.3.0) | [![1.3.0](https://img.shields.io/badge/Stable-1.3.0-blue)](https://components.espressif.com/components/espressif/esp_mmap_assets/versions/1.3.0) |
| [esp_msc_ota](https://components.espressif.com/components/espressif/esp_msc_ota/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/esp_msc_ota/versions/1.0.0) |
| [esp_sensorless_bldc_control](https://components.espressif.com/components/espressif/esp_sensorless_bldc_control/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/esp_sensorless_bldc_control/versions/1.0.0) |
| [esp_simplefoc](https://components.espressif.com/components/espressif/esp_simplefoc/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/esp_simplefoc/versions/1.0.0) |
| [esp_tinyuf2](https://components.espressif.com/components/espressif/esp_tinyuf2/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/esp_tinyuf2/versions/1.0.0) |
| [extended_vfs](https://components.espressif.com/components/espressif/extended_vfs/versions/0.3.2) | [![0.3.2](https://img.shields.io/badge/Beta-0.3.2-yellow)](https://components.espressif.com/components/espressif/extended_vfs/versions/0.3.2) |
| [gprof](https://components.espressif.com/components/espressif/gprof/versions/0.1.0) | [![0.1.0](https://img.shields.io/badge/Beta-0.1.0-yellow)](https://components.espressif.com/components/espressif/gprof/versions/0.1.0) |
| [i2c_bus](https://components.espressif.com/components/espressif/i2c_bus/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/i2c_bus/versions/1.0.0) |
| [ina236](https://components.espressif.com/components/espressif/ina236/versions/0.1.0) | [![0.1.0](https://img.shields.io/badge/Beta-0.1.0-yellow)](https://components.espressif.com/components/espressif/ina236/versions/0.1.0) |
| [iot_usbh](https://components.espressif.com/components/espressif/iot_usbh/versions/0.2.1) | [![0.2.1](https://img.shields.io/badge/Beta-0.2.1-yellow)](https://components.espressif.com/components/espressif/iot_usbh/versions/0.2.1) |
| [iot_usbh_cdc](https://components.espressif.com/components/espressif/iot_usbh_cdc/versions/0.2.2) | [![0.2.2](https://img.shields.io/badge/Beta-0.2.2-yellow)](https://components.espressif.com/components/espressif/iot_usbh_cdc/versions/0.2.2) |
| [iot_usbh_modem](https://components.espressif.com/components/espressif/iot_usbh_modem/versions/0.2.1) | [![0.2.1](https://img.shields.io/badge/Beta-0.2.1-yellow)](https://components.espressif.com/components/espressif/iot_usbh_modem/versions/0.2.1) |
| [ir_learn](https://components.espressif.com/components/espressif/ir_learn/versions/0.2.0) | [![0.2.0](https://img.shields.io/badge/Beta-0.2.0-yellow)](https://components.espressif.com/components/espressif/ir_learn/versions/0.2.0) |
| [keyboard_button](https://components.espressif.com/components/espressif/keyboard_button/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/keyboard_button/versions/1.0.0) |
| [knob](https://components.espressif.com/components/espressif/knob/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/knob/versions/1.0.0) |
| [led_indicator](https://components.espressif.com/components/espressif/led_indicator/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/led_indicator/versions/1.0.0) |
| [lightbulb_driver](https://components.espressif.com/components/espressif/lightbulb_driver/versions/1.3.3) | [![1.3.3](https://img.shields.io/badge/Stable-1.3.3-blue)](https://components.espressif.com/components/espressif/lightbulb_driver/versions/1.3.3) |
| [ntc_driver](https://components.espressif.com/components/espressif/ntc_driver/versions/1.1.0) | [![1.1.0](https://img.shields.io/badge/Stable-1.1.0-blue)](https://components.espressif.com/components/espressif/ntc_driver/versions/1.1.0) |
| [openai](https://components.espressif.com/components/espressif/openai/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/openai/versions/1.0.0) |
| [pwm_audio](https://components.espressif.com/components/espressif/pwm_audio/versions/1.1.2) | [![1.1.2](https://img.shields.io/badge/Stable-1.1.2-blue)](https://components.espressif.com/components/espressif/pwm_audio/versions/1.1.2) |
| [spi_bus](https://components.espressif.com/components/espressif/spi_bus/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/spi_bus/versions/1.0.0) |
| [touch_proximity_sensor](https://components.espressif.com/components/espressif/touch_proximity_sensor/versions/0.1.2) | [![0.1.2](https://img.shields.io/badge/Beta-0.1.2-yellow)](https://components.espressif.com/components/espressif/touch_proximity_sensor/versions/0.1.2) |
| [usb_device_uac](https://components.espressif.com/components/espressif/usb_device_uac/versions/0.2.0) | [![0.2.0](https://img.shields.io/badge/Beta-0.2.0-yellow)](https://components.espressif.com/components/espressif/usb_device_uac/versions/0.2.0) |
| [usb_device_uvc](https://components.espressif.com/components/espressif/usb_device_uvc/versions/1.1.2) | [![1.1.2](https://img.shields.io/badge/Stable-1.1.2-blue)](https://components.espressif.com/components/espressif/usb_device_uvc/versions/1.1.2) |
| [usb_stream](https://components.espressif.com/components/espressif/usb_stream/versions/1.4.0) | [![1.4.0](https://img.shields.io/badge/Stable-1.4.0-blue)](https://components.espressif.com/components/espressif/usb_stream/versions/1.4.0) |
| [xz](https://components.espressif.com/components/espressif/xz/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/xz/versions/1.0.0) |
| [zero_detection](https://components.espressif.com/components/espressif/zero_detection/versions/0.0.6) | [![0.0.6](https://img.shields.io/badge/Beta-0.0.6-yellow)](https://components.espressif.com/components/espressif/zero_detection/versions/0.0.6) |

</center>

You can directly add the components from the Component Registry to your project by using the `idf.py add-dependency` command under your project's root directory. eg run `idf.py add-dependency "espressif/usb_stream"` to add the `usb_stream`, the component will be downloaded automatically during the `CMake` step.

> Please refer to [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html) for details.

#### Get ESP-IoT-Solution Repository

If you want to [Contribute](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/contribute/index.html) to the components in ESP-IoT-Solution or want to start from the examples in ESP-IoT-Solution, you can get the ESP-IoT-Solution repository by following the steps:

* If select the `master` version, open the terminal, and run the following command:

    ```
    git clone --recursive https://github.com/espressif/esp-iot-solution
    ```

* If select the `release/v2.0` version, open the terminal, and run the following command:

    ```
    git clone -b release/v2.0 --recursive https://github.com/espressif/esp-iot-solution
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
- [Check the Issues section on GitHub](https://github.com/espressif/esp-iot-solution/issues) if you find a bug or have a feature request. Please check existing Issues before opening a new one.
- If you're interested in contributing to ESP-IDF, please check the [Contributions Guide](./CONTRIBUTING.rst).
