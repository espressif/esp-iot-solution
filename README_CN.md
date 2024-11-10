[![Documentation Status](https://dl.espressif.com/AE/docs/docs_latest.svg)](https://docs.espressif.com/projects/esp-iot-solution/zh_CN)

<a href="https://espressif.github.io/esp-launchpad/?flashConfigURL=https://dl.espressif.com/AE/esp-iot-solution/release_v2_0/config.toml">
    <img alt="Try it with ESP Launchpad" src="https://espressif.github.io/esp-launchpad/assets/try_with_launchpad.png" width="200" height="56">
</a>

## Espressif IoT Solution 概述

* [English Version](./README.md)

ESP-IoT-Solution 包含物联网系统开发中常用的外设驱动和代码框架，提供了 ESP-IDF 的扩展组件，方便用户实现更简单的开发。

ESP-IoT-Solution 包含的内容如下:

* 传感器、显示屏、音频设备、输入设备、执行机构等设备驱动；
* 低功耗、安全加密、存储方案等代码框架或说明文档；
* 从实际应用的角度出发，为乐鑫开源解决方案提供了入口指引。

## 文档中心

- 中文：https://docs.espressif.com/projects/esp-iot-solution/zh_CN
- English: https://docs.espressif.com/projects/esp-iot-solution/en

## 快速参考

### 硬件准备

您可以选择任意 ESP 系列开发板使用 ESP-IoT-Solution，或者选择[板级支持组件](./examples/common_components/boards)中支持的开发板快速开始。

ESP 系列 SoC 采用 40nm 工艺制成，具有最佳的功耗性能、射频性能、稳定性、通用性和可靠性，适用于各种应用场景和不同功耗需求。

### 环境搭建

#### 搭建 ESP-IDF 开发环境

由于 ESP-IoT-Solution 依赖 ESP-IDF 的基础功能和编译工具，因此首先需要参考 [ESP-IDF 详细安装步骤](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-step-by-step) 完成 ESP-IDF 开发环境的搭建。

请注意，不同 ESP-IoT-Solution 版本 依赖的 ESP-IDF 版本可能不同，请参考下表进行选择。

| ESP-IoT-Solution |  依赖的 ESP-IDF  |                  主要变更                  |                                                     文档                                                     |           支持状态           |
| :--------------: | :--------------: | :----------------------------------------: | :----------------------------------------------------------------------------------------------------------: | ---------------------------- |
|      master      |     >= v5.3      |                 新芯片支持                 |                  [Docs online](https://docs.espressif.com/projects/esp-iot-solution/zh_CN)                   | 新功能开发分支               |
|   release/v2.0   | <= v5.3, >= v4.4 |               支持组件管理器               |      [Docs online](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/release-v2.0/index.html)       | 仅限 Bugfix，维护到 v5.3 EOL |
|   release/v1.1   |      v4.0.1      | IDF 版本更新，删除已经移动到其它仓库的代码 | [v1.1 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.1#esp32-iot-solution-overview) | 备份，停止维护               |
|   release/v1.0   |      v3.2.2      |                  历史版本                  | [v1.0 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.0#esp32-iot-solution-overview) | 备份，停止维护               |

> `release/v2.0` `master` 分支使用 `ESP 组件管理器` 来管理组件，因此每个组件都是一个单独的软件包，每个包可能支持不同版本的 ESP-idf，这些版本将在组件的 `idf_component.yml` 文件中声明。

#### 从 ESP 组件注册表获取组件

如果您只想使用 ESP-IoT-Solution 中的组件，我们建议您从 ESP 组件注册表 [ESP Component Registry](https://components.espressif.com/) 中使用它。

ESP-IoT-Solution 中注册的组件如下:

<center>

| 组件 | 版本 |
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
| [knob](https://components.espressif.com/components/espressif/knob/versions/0.1.5) | [![0.1.5](https://img.shields.io/badge/Beta-0.1.5-yellow)](https://components.espressif.com/components/espressif/knob/versions/0.1.5) |
| [led_indicator](https://components.espressif.com/components/espressif/led_indicator/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/led_indicator/versions/1.0.0) |
| [lightbulb_driver](https://components.espressif.com/components/espressif/lightbulb_driver/versions/1.3.2) | [![1.3.2](https://img.shields.io/badge/Stable-1.3.2-blue)](https://components.espressif.com/components/espressif/lightbulb_driver/versions/1.3.2) |
| [ntc_driver](https://components.espressif.com/components/espressif/ntc_driver/versions/1.0.0) | [![1.0.0](https://img.shields.io/badge/Stable-1.0.0-blue)](https://components.espressif.com/components/espressif/ntc_driver/versions/1.0.0) |
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

可以在项目根目录下使用 `idf.py add-dependency` 命令直接将组件从 Component Registry 添加到项目中。例如，执行 `idf.py add-dependency "espressif/usb_stream"` 命令添加 `usb_stream`，该组件将在 `CMake` 步骤中自动下载。

> 请参考 [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html) 查看更多关于组件管理器的细节.

#### 从 ESP-IoT-Solution 仓库获取组件

如果您想为 `ESP-IoT-Solution` 中的组件或示例[贡献代码](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/contribute/index.html)，或者想基于 `ESP-IoT-Solution` 中的示例开发项目，您可以通过以下步骤下载 ESP-IoT-Solution 代码仓库:

* 如果选择 `master` 版本，可使用以下指令获取代码：

    ```
    git clone --recursive https://github.com/espressif/esp-iot-solution
    ```

* 如果选择 `release/v2.0` 版本，可使用以下指令获取代码：

    ```
    git clone -b release/v2.0 --recursive https://github.com/espressif/esp-iot-solution
    ```

#### 编译和下载示例

**我们强烈建议**您 [构建您的第一个项目](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#build-your-first-project) 以熟悉 ESP-IDF 并确保环境已经设置正确。

在 ESP-IoT-Solution 和 ESP-IDF 中构建和烧录示例没有区别。 在大多数情况下，您可以按照以下步骤在 ESP-IoT-Solution 中构建和烧录示例：

1. 将当前目录更改为示例目录，例如 `cd examples/usb/host/usb_audio_player`；
2. 运行 `idf.py set-target TARGET` 设置目标芯片。 例如 `idf.py set-target esp32s3` 将目标芯片设置为 `ESP32-S3`；
3. 运行 `idf.py build` 来构建示例；
4. 运行 `idf.py -p PORT flash monitor` 烧录示例，并查看串口输出。 例如 `idf.py -p /dev/ttyUSB0 flash monitor` 将示例烧录到 `/dev/ttyUSB0` 端口，并打开串口监视器。

> 某些示例可能需要额外的步骤来设置 `ESP-IoT-Solution` 环境变量，您可以在 Linux/MacOS 中运行 `export IOT_SOLUTION_PATH=~/esp/esp-iot-solution` 或 `set IOT_SOLUTION_PATH=C:\esp\esp- iot-solution` 在 Windows 中设置环境。

### 其它参考资源

- 最新版的文档：https://docs.espressif.com/projects/esp-iot-solution/zh_CN ，该文档是由本仓库 [docs](./docs) 目录 构建得到；
- ESP-IDF 编程指南 https://docs.espressif.com/projects/esp-idf/zh_CN ，请参考 ESP-IoT-Solution 依赖的版本；
- 可以在 [ESP Component Registry](https://components.espressif.com/)中找到 `ESP-IoT-Solution` 中的组件和其他已注册的组件;
- 可以前往 [esp32.com](https://www.esp32.com/) 论坛 提问，挖掘社区资源；
- 如果你在使用中发现了错误或者需要新的功能，请先查看 [GitHub Issues](https://github.com/espressif/esp-iot-solution/issues)，确保该问题不会被重复提交；
- 如果你有兴趣为 ESP-IoT-Solution 作贡献，请先阅读[贡献指南](./CONTRIBUTING.rst)。