[![Documentation Status](https://dl.espressif.com/AE/docs/docs_latest.svg)](https://docs.espressif.com/projects/esp-iot-solution/zh_CN)

<a href="https://espressif.github.io/esp-launchpad/?flashConfigURL=https://dl.espressif.com/AE/esp-iot-solution/config.toml">
    <img alt="Try it with ESP Launchpad" src="https://espressif.github.io/esp-launchpad/assets/try_with_launchpad.png" width="200" height="56">
</a>

## Espressif IoT Solution 概述

* [English Version](./README.md)

ESP-IoT-Solution 包含物联网系统开发中常用的外设驱动和代码框架，提供了 ESP-IDF 的扩展组件，方便用户实现更简单的开发。

ESP-IoT-Solution 包含的内容如下:

* 传感器、显示屏、音频设备、输入设备、电机控制等设备驱动；
* 低功耗、安全加密、存储方案等代码框架或说明文档；
* 从实际应用的角度出发，为乐鑫开源解决方案提供了入口指引。

## 文档中心

- 中文：https://docs.espressif.com/projects/esp-iot-solution/zh_CN
- English: https://docs.espressif.com/projects/esp-iot-solution/en

## 版本说明

自 `release/v2.0` 起，ESP-IoT-Solution 采用组件化管理，各组件与示例独立迭代，不再绑定仓库分支。具体依赖的 ESP-IDF 版本请查阅组件 `idf_component.yml` 文件中的声明。Release 分支仅用于维护组件的历史大版本，例如 `button` `v3.x` 版本在 `release/v2.0` 分支中维护，`master` 分支中维护最新的 `button` 版本（例如 `v4.x`）。


| ESP-IoT-Solution |  依赖的 ESP-IDF  |                  主要变更                  |                                                     文档                                                     |           支持状态           |
| :--------------: | :--------------: | :----------------------------------------: | :----------------------------------------------------------------------------------------------------------: | ---------------------------- |
|      master      |     >= v5.3      |          新芯片支持                              |                  [Docs online](https://docs.espressif.com/projects/esp-iot-solution/zh_CN)                   | 新功能开发分支               |
|   release/v2.0   | <= v5.3, >= v4.4 |               支持组件管理器               |                  [Docs online](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/release-v2.0/index.html)                   | 仅限组件历史版本 Bugfix，维护到 v5.3 EOL |
|   release/v1.1   |      v4.0.1      | IDF 版本更新，删除已经移动到其它仓库的代码 | [v1.1 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.1#esp32-iot-solution-overview) | 备份，停止维护               |
|   release/v1.0   |      v3.2.2      |                  历史版本                  | [v1.0 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.0#esp32-iot-solution-overview) | 备份，停止维护               |

> **Note**
>
> 不同芯片推荐的 ESP-IDF 首选版本也不同，具体可参考 [ESP-IDF 版本与乐鑫芯片版本兼容性](https://github.com/espressif/esp-idf/blob/master/COMPATIBILITY_CN.md)。

## 快速参考

### 硬件准备

您可以选择任意 ESP 系列开发板使用 ESP-IoT-Solution，或者选择[esp-bsp](https://github.com/espressif/esp-bsp)中支持的开发板快速开始。

ESP 系列 SoC 采用先进工艺制程，提供业界领先的射频性能、低功耗特性和稳定可靠性，适用于物联网、工业控制、智能家居、可穿戴设备等多种应用场景。各系列芯片的具体规格和功能请参考 [ESP 产品选型工具](https://products.espressif.com/)。

### 组件使用或二次开发

请参考 [ESP-IDF 详细安装步骤](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-step-by-step) 先完成 ESP-IDF 开发环境的搭建。

#### 从 ESP 组件注册表获取组件

如果您只想使用 ESP-IoT-Solution 中的组件，我们建议您从 ESP 组件注册表 [ESP Component Registry](https://components.espressif.com/) 中使用它。

可以在项目根目录下使用 `idf.py add-dependency` 命令直接将组件从 Component Registry 添加到项目中。例如，执行 `idf.py add-dependency "espressif/button"` 命令添加 `button`，该组件将在 `CMake` 步骤中自动下载。

> 请参考 [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html) 查看更多关于组件管理器的细节.

ESP-IoT-Solution 中注册的组件如下:

<center>

| 组件 | 版本 |
| --- | --- |
| [adc_battery_estimation](https://components.espressif.com/components/espressif/adc_battery_estimation) | [![Component Registry](https://components.espressif.com/components/espressif/adc_battery_estimation/badge.svg)](https://components.espressif.com/components/espressif/adc_battery_estimation) |
| [adc_mic](https://components.espressif.com/components/espressif/adc_mic) | [![Component Registry](https://components.espressif.com/components/espressif/adc_mic/badge.svg)](https://components.espressif.com/components/espressif/adc_mic) |
| [adc_tp_calibration](https://components.espressif.com/components/espressif/adc_tp_calibration) | [![Component Registry](https://components.espressif.com/components/espressif/adc_tp_calibration/badge.svg)](https://components.espressif.com/components/espressif/adc_tp_calibration) |
| [aht20](https://components.espressif.com/components/espressif/aht20) | [![Component Registry](https://components.espressif.com/components/espressif/aht20/badge.svg)](https://components.espressif.com/components/espressif/aht20) |
| [apds9960](https://components.espressif.com/components/espressif/apds9960) | [![Component Registry](https://components.espressif.com/components/espressif/apds9960/badge.svg)](https://components.espressif.com/components/espressif/apds9960) |
| [at24c02](https://components.espressif.com/components/espressif/at24c02) | [![Component Registry](https://components.espressif.com/components/espressif/at24c02/badge.svg)](https://components.espressif.com/components/espressif/at24c02) |
| [at581x](https://components.espressif.com/components/espressif/at581x) | [![Component Registry](https://components.espressif.com/components/espressif/at581x/badge.svg)](https://components.espressif.com/components/espressif/at581x) |
| [avi_player](https://components.espressif.com/components/espressif/avi_player) | [![Component Registry](https://components.espressif.com/components/espressif/avi_player/badge.svg)](https://components.espressif.com/components/espressif/avi_player) |
| [ble_anp](https://components.espressif.com/components/espressif/ble_anp) | [![Component Registry](https://components.espressif.com/components/espressif/ble_anp/badge.svg)](https://components.espressif.com/components/espressif/ble_anp) |
| [ble_conn_mgr](https://components.espressif.com/components/espressif/ble_conn_mgr) | [![Component Registry](https://components.espressif.com/components/espressif/ble_conn_mgr/badge.svg)](https://components.espressif.com/components/espressif/ble_conn_mgr) |
| [ble_hci](https://components.espressif.com/components/espressif/ble_hci) | [![Component Registry](https://components.espressif.com/components/espressif/ble_hci/badge.svg)](https://components.espressif.com/components/espressif/ble_hci) |
| [ble_hrp](https://components.espressif.com/components/espressif/ble_hrp) | [![Component Registry](https://components.espressif.com/components/espressif/ble_hrp/badge.svg)](https://components.espressif.com/components/espressif/ble_hrp) |
| [ble_htp](https://components.espressif.com/components/espressif/ble_htp) | [![Component Registry](https://components.espressif.com/components/espressif/ble_htp/badge.svg)](https://components.espressif.com/components/espressif/ble_htp) |
| [ble_midi](https://components.espressif.com/components/espressif/ble_midi) | [![Component Registry](https://components.espressif.com/components/espressif/ble_midi/badge.svg)](https://components.espressif.com/components/espressif/ble_midi) |
| [ble_ota](https://components.espressif.com/components/espressif/ble_ota) | [![Component Registry](https://components.espressif.com/components/espressif/ble_ota/badge.svg)](https://components.espressif.com/components/espressif/ble_ota) |
| [ble_services](https://components.espressif.com/components/espressif/ble_services) | [![Component Registry](https://components.espressif.com/components/espressif/ble_services/badge.svg)](https://components.espressif.com/components/espressif/ble_services) |
| [bme280](https://components.espressif.com/components/espressif/bme280) | [![Component Registry](https://components.espressif.com/components/espressif/bme280/badge.svg)](https://components.espressif.com/components/espressif/bme280) |
| [bme690](https://components.espressif.com/components/espressif/bme690) | [![Component Registry](https://components.espressif.com/components/espressif/bme690/badge.svg)](https://components.espressif.com/components/espressif/bme690) |
| [bootloader_support_plus](https://components.espressif.com/components/espressif/bootloader_support_plus) | [![Component Registry](https://components.espressif.com/components/espressif/bootloader_support_plus/badge.svg)](https://components.espressif.com/components/espressif/bootloader_support_plus) |
| [bq27220](https://components.espressif.com/components/espressif/bq27220) | [![Component Registry](https://components.espressif.com/components/espressif/bq27220/badge.svg)](https://components.espressif.com/components/espressif/bq27220) |
| [bthome](https://components.espressif.com/components/espressif/bthome) | [![Component Registry](https://components.espressif.com/components/espressif/bthome/badge.svg)](https://components.espressif.com/components/espressif/bthome) |
| [button](https://components.espressif.com/components/espressif/button) | [![Component Registry](https://components.espressif.com/components/espressif/button/badge.svg)](https://components.espressif.com/components/espressif/button) |
| [cmake_utilities](https://components.espressif.com/components/espressif/cmake_utilities) | [![Component Registry](https://components.espressif.com/components/espressif/cmake_utilities/badge.svg)](https://components.espressif.com/components/espressif/cmake_utilities) |
| [drv10987](https://components.espressif.com/components/espressif/drv10987) | [![Component Registry](https://components.espressif.com/components/espressif/drv10987/badge.svg)](https://components.espressif.com/components/espressif/drv10987) |
| [elf_loader](https://components.espressif.com/components/espressif/elf_loader) | [![Component Registry](https://components.espressif.com/components/espressif/elf_loader/badge.svg)](https://components.espressif.com/components/espressif/elf_loader) |
| [esp_lcd_axs15231b](https://components.espressif.com/components/espressif/esp_lcd_axs15231b) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_axs15231b/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_axs15231b) |
| [esp_lcd_co5300](https://components.espressif.com/components/espressif/esp_lcd_co5300) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_co5300/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_co5300) |
| [esp_lcd_ek79007](https://components.espressif.com/components/espressif/esp_lcd_ek79007) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_ek79007/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_ek79007) |
| [esp_lcd_er88577](https://components.espressif.com/components/espressif/esp_lcd_er88577) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_er88577/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_er88577) |
| [esp_lcd_gc9107](https://components.espressif.com/components/espressif/esp_lcd_gc9107) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_gc9107/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_gc9107) |
| [esp_lcd_gc9b71](https://components.espressif.com/components/espressif/esp_lcd_gc9b71) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_gc9b71/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_gc9b71) |
| [esp_lcd_gc9d01](https://components.espressif.com/components/espressif/esp_lcd_gc9d01) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_gc9d01/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_gc9d01) |
| [esp_lcd_hx8399](https://components.espressif.com/components/espressif/esp_lcd_hx8399) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_hx8399/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_hx8399) |
| [esp_lcd_jd9165](https://components.espressif.com/components/espressif/esp_lcd_jd9165) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_jd9165/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_jd9165) |
| [esp_lcd_jd9365](https://components.espressif.com/components/espressif/esp_lcd_jd9365) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_jd9365/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_jd9365) |
| [esp_lcd_nv3022b](https://components.espressif.com/components/espressif/esp_lcd_nv3022b) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_nv3022b/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_nv3022b) |
| [esp_lcd_nv3052](https://components.espressif.com/components/espressif/esp_lcd_nv3052) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_nv3052/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_nv3052) |
| [esp_lcd_panel_io_additions](https://components.espressif.com/components/espressif/esp_lcd_panel_io_additions) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_panel_io_additions/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_panel_io_additions) |
| [esp_lcd_sh8601](https://components.espressif.com/components/espressif/esp_lcd_sh8601) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_sh8601/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_sh8601) |
| [esp_lcd_spd2010](https://components.espressif.com/components/espressif/esp_lcd_spd2010) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_spd2010/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_spd2010) |
| [esp_lcd_st7123](https://components.espressif.com/components/espressif/esp_lcd_st7123) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_st7123/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_st7123) |
| [esp_lcd_st7701](https://components.espressif.com/components/espressif/esp_lcd_st7701) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_st7701/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_st7701) |
| [esp_lcd_st7703](https://components.espressif.com/components/espressif/esp_lcd_st7703) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_st7703/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_st7703) |
| [esp_lcd_st77903_qspi](https://components.espressif.com/components/espressif/esp_lcd_st77903_qspi) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_st77903_qspi/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_st77903_qspi) |
| [esp_lcd_st77903_rgb](https://components.espressif.com/components/espressif/esp_lcd_st77903_rgb) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_st77903_rgb/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_st77903_rgb) |
| [esp_lcd_st77916](https://components.espressif.com/components/espressif/esp_lcd_st77916) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_st77916/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_st77916) |
| [esp_lcd_st77922](https://components.espressif.com/components/espressif/esp_lcd_st77922) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_st77922/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_st77922) |
| [esp_lcd_touch_ili2118](https://components.espressif.com/components/espressif/esp_lcd_touch_ili2118) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_touch_ili2118/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_touch_ili2118) |
| [esp_lcd_touch_spd2010](https://components.espressif.com/components/espressif/esp_lcd_touch_spd2010) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_touch_spd2010/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_touch_spd2010) |
| [esp_lcd_touch_st7123](https://components.espressif.com/components/espressif/esp_lcd_touch_st7123) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_touch_st7123/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_touch_st7123) |
| [esp_lcd_usb_display](https://components.espressif.com/components/espressif/esp_lcd_usb_display) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_usb_display/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_usb_display) |
| [esp_lv_decoder](https://components.espressif.com/components/espressif/esp_lv_decoder) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lv_decoder/badge.svg)](https://components.espressif.com/components/espressif/esp_lv_decoder) |
| [esp_lv_eaf_player](https://components.espressif.com/components/espressif/esp_lv_eaf_player) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lv_eaf_player/badge.svg)](https://components.espressif.com/components/espressif/esp_lv_eaf_player) |
| [esp_lv_fs](https://components.espressif.com/components/espressif/esp_lv_fs) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lv_fs/badge.svg)](https://components.espressif.com/components/espressif/esp_lv_fs) |
| [esp_lvgl_adapter](https://components.espressif.com/components/espressif/esp_lvgl_adapter) | [![Component Registry](https://components.espressif.com/components/espressif/esp_lvgl_adapter/badge.svg)](https://components.espressif.com/components/espressif/esp_lvgl_adapter) |
| [esp_mmap_assets](https://components.espressif.com/components/espressif/esp_mmap_assets) | [![Component Registry](https://components.espressif.com/components/espressif/esp_mmap_assets/badge.svg)](https://components.espressif.com/components/espressif/esp_mmap_assets) |
| [esp_msc_ota](https://components.espressif.com/components/espressif/esp_msc_ota) | [![Component Registry](https://components.espressif.com/components/espressif/esp_msc_ota/badge.svg)](https://components.espressif.com/components/espressif/esp_msc_ota) |
| [esp_sensorless_bldc_control](https://components.espressif.com/components/espressif/esp_sensorless_bldc_control) | [![Component Registry](https://components.espressif.com/components/espressif/esp_sensorless_bldc_control/badge.svg)](https://components.espressif.com/components/espressif/esp_sensorless_bldc_control) |
| [esp_simplefoc](https://components.espressif.com/components/espressif/esp_simplefoc) | [![Component Registry](https://components.espressif.com/components/espressif/esp_simplefoc/badge.svg)](https://components.espressif.com/components/espressif/esp_simplefoc) |
| [esp_tinyuf2](https://components.espressif.com/components/espressif/esp_tinyuf2) | [![Component Registry](https://components.espressif.com/components/espressif/esp_tinyuf2/badge.svg)](https://components.espressif.com/components/espressif/esp_tinyuf2) |
| [extended_vfs](https://components.espressif.com/components/espressif/extended_vfs) | [![Component Registry](https://components.espressif.com/components/espressif/extended_vfs/badge.svg)](https://components.espressif.com/components/espressif/extended_vfs) |
| [gprof](https://components.espressif.com/components/espressif/gprof) | [![Component Registry](https://components.espressif.com/components/espressif/gprof/badge.svg)](https://components.espressif.com/components/espressif/gprof) |
| [hdc2010](https://components.espressif.com/components/espressif/hdc2010) | [![Component Registry](https://components.espressif.com/components/espressif/hdc2010/badge.svg)](https://components.espressif.com/components/espressif/hdc2010) |
| [i2c_bus](https://components.espressif.com/components/espressif/i2c_bus) | [![Component Registry](https://components.espressif.com/components/espressif/i2c_bus/badge.svg)](https://components.espressif.com/components/espressif/i2c_bus) |
| [ina236](https://components.espressif.com/components/espressif/ina236) | [![Component Registry](https://components.espressif.com/components/espressif/ina236/badge.svg)](https://components.espressif.com/components/espressif/ina236) |
| [iot_eth](https://components.espressif.com/components/espressif/iot_eth) | [![Component Registry](https://components.espressif.com/components/espressif/iot_eth/badge.svg)](https://components.espressif.com/components/espressif/iot_eth) |
| [iot_usbh](https://components.espressif.com/components/espressif/iot_usbh) | [![Component Registry](https://components.espressif.com/components/espressif/iot_usbh/badge.svg)](https://components.espressif.com/components/espressif/iot_usbh) |
| [iot_usbh_cdc](https://components.espressif.com/components/espressif/iot_usbh_cdc) | [![Component Registry](https://components.espressif.com/components/espressif/iot_usbh_cdc/badge.svg)](https://components.espressif.com/components/espressif/iot_usbh_cdc) |
| [iot_usbh_ecm](https://components.espressif.com/components/espressif/iot_usbh_ecm) | [![Component Registry](https://components.espressif.com/components/espressif/iot_usbh_ecm/badge.svg)](https://components.espressif.com/components/espressif/iot_usbh_ecm) |
| [iot_usbh_modem](https://components.espressif.com/components/espressif/iot_usbh_modem) | [![Component Registry](https://components.espressif.com/components/espressif/iot_usbh_modem/badge.svg)](https://components.espressif.com/components/espressif/iot_usbh_modem) |
| [iot_usbh_rndis](https://components.espressif.com/components/espressif/iot_usbh_rndis) | [![Component Registry](https://components.espressif.com/components/espressif/iot_usbh_rndis/badge.svg)](https://components.espressif.com/components/espressif/iot_usbh_rndis) |
| [ir_learn](https://components.espressif.com/components/espressif/ir_learn) | [![Component Registry](https://components.espressif.com/components/espressif/ir_learn/badge.svg)](https://components.espressif.com/components/espressif/ir_learn) |
| [keyboard_button](https://components.espressif.com/components/espressif/keyboard_button) | [![Component Registry](https://components.espressif.com/components/espressif/keyboard_button/badge.svg)](https://components.espressif.com/components/espressif/keyboard_button) |
| [knob](https://components.espressif.com/components/espressif/knob) | [![Component Registry](https://components.espressif.com/components/espressif/knob/badge.svg)](https://components.espressif.com/components/espressif/knob) |
| [led_indicator](https://components.espressif.com/components/espressif/led_indicator) | [![Component Registry](https://components.espressif.com/components/espressif/led_indicator/badge.svg)](https://components.espressif.com/components/espressif/led_indicator) |
| [lightbulb_driver](https://components.espressif.com/components/espressif/lightbulb_driver) | [![Component Registry](https://components.espressif.com/components/espressif/lightbulb_driver/badge.svg)](https://components.espressif.com/components/espressif/lightbulb_driver) |
| [lis2dh12](https://components.espressif.com/components/espressif/lis2dh12) | [![Component Registry](https://components.espressif.com/components/espressif/lis2dh12/badge.svg)](https://components.espressif.com/components/espressif/lis2dh12) |
| [log_router](https://components.espressif.com/components/espressif/log_router) | [![Component Registry](https://components.espressif.com/components/espressif/log_router/badge.svg)](https://components.espressif.com/components/espressif/log_router) |
| [max17048](https://components.espressif.com/components/espressif/max17048) | [![Component Registry](https://components.espressif.com/components/espressif/max17048/badge.svg)](https://components.espressif.com/components/espressif/max17048) |
| [mcp-c-sdk](https://components.espressif.com/components/espressif/mcp-c-sdk) | [![Component Registry](https://components.espressif.com/components/espressif/mcp-c-sdk/badge.svg)](https://components.espressif.com/components/espressif/mcp-c-sdk) |
| [mcp23017](https://components.espressif.com/components/espressif/mcp23017) | [![Component Registry](https://components.espressif.com/components/espressif/mcp23017/badge.svg)](https://components.espressif.com/components/espressif/mcp23017) |
| [mcp3201](https://components.espressif.com/components/espressif/mcp3201) | [![Component Registry](https://components.espressif.com/components/espressif/mcp3201/badge.svg)](https://components.espressif.com/components/espressif/mcp3201) |
| [modem_at](https://components.espressif.com/components/espressif/modem_at) | [![Component Registry](https://components.espressif.com/components/espressif/modem_at/badge.svg)](https://components.espressif.com/components/espressif/modem_at) |
| [mvh3004d](https://components.espressif.com/components/espressif/mvh3004d) | [![Component Registry](https://components.espressif.com/components/espressif/mvh3004d/badge.svg)](https://components.espressif.com/components/espressif/mvh3004d) |
| [ntc_driver](https://components.espressif.com/components/espressif/ntc_driver) | [![Component Registry](https://components.espressif.com/components/espressif/ntc_driver/badge.svg)](https://components.espressif.com/components/espressif/ntc_driver) |
| [openai](https://components.espressif.com/components/espressif/openai) | [![Component Registry](https://components.espressif.com/components/espressif/openai/badge.svg)](https://components.espressif.com/components/espressif/openai) |
| [power_measure](https://components.espressif.com/components/espressif/power_measure) | [![Component Registry](https://components.espressif.com/components/espressif/power_measure/badge.svg)](https://components.espressif.com/components/espressif/power_measure) |
| [pwm_audio](https://components.espressif.com/components/espressif/pwm_audio) | [![Component Registry](https://components.espressif.com/components/espressif/pwm_audio/badge.svg)](https://components.espressif.com/components/espressif/pwm_audio) |
| [sensor_hub](https://components.espressif.com/components/espressif/sensor_hub) | [![Component Registry](https://components.espressif.com/components/espressif/sensor_hub/badge.svg)](https://components.espressif.com/components/espressif/sensor_hub) |
| [servo](https://components.espressif.com/components/espressif/servo) | [![Component Registry](https://components.espressif.com/components/espressif/servo/badge.svg)](https://components.espressif.com/components/espressif/servo) |
| [sht3x](https://components.espressif.com/components/espressif/sht3x) | [![Component Registry](https://components.espressif.com/components/espressif/sht3x/badge.svg)](https://components.espressif.com/components/espressif/sht3x) |
| [spi_bus](https://components.espressif.com/components/espressif/spi_bus) | [![Component Registry](https://components.espressif.com/components/espressif/spi_bus/badge.svg)](https://components.espressif.com/components/espressif/spi_bus) |
| [touch_button](https://components.espressif.com/components/espressif/touch_button) | [![Component Registry](https://components.espressif.com/components/espressif/touch_button/badge.svg)](https://components.espressif.com/components/espressif/touch_button) |
| [touch_button_sensor](https://components.espressif.com/components/espressif/touch_button_sensor) | [![Component Registry](https://components.espressif.com/components/espressif/touch_button_sensor/badge.svg)](https://components.espressif.com/components/espressif/touch_button_sensor) |
| [touch_proximity_sensor](https://components.espressif.com/components/espressif/touch_proximity_sensor) | [![Component Registry](https://components.espressif.com/components/espressif/touch_proximity_sensor/badge.svg)](https://components.espressif.com/components/espressif/touch_proximity_sensor) |
| [touch_slider_sensor](https://components.espressif.com/components/espressif/touch_slider_sensor) | [![Component Registry](https://components.espressif.com/components/espressif/touch_slider_sensor/badge.svg)](https://components.espressif.com/components/espressif/touch_slider_sensor) |
| [usb_device_uac](https://components.espressif.com/components/espressif/usb_device_uac) | [![Component Registry](https://components.espressif.com/components/espressif/usb_device_uac/badge.svg)](https://components.espressif.com/components/espressif/usb_device_uac) |
| [usb_device_uvc](https://components.espressif.com/components/espressif/usb_device_uvc) | [![Component Registry](https://components.espressif.com/components/espressif/usb_device_uvc/badge.svg)](https://components.espressif.com/components/espressif/usb_device_uvc) |
| [usb_stream](https://components.espressif.com/components/espressif/usb_stream) | [![Component Registry](https://components.espressif.com/components/espressif/usb_stream/badge.svg)](https://components.espressif.com/components/espressif/usb_stream) |
| [veml6040](https://components.espressif.com/components/espressif/veml6040) | [![Component Registry](https://components.espressif.com/components/espressif/veml6040/badge.svg)](https://components.espressif.com/components/espressif/veml6040) |
| [veml6075](https://components.espressif.com/components/espressif/veml6075) | [![Component Registry](https://components.espressif.com/components/espressif/veml6075/badge.svg)](https://components.espressif.com/components/espressif/veml6075) |
| [xz](https://components.espressif.com/components/espressif/xz) | [![Component Registry](https://components.espressif.com/components/espressif/xz/badge.svg)](https://components.espressif.com/components/espressif/xz) |
| [zero_detection](https://components.espressif.com/components/espressif/zero_detection) | [![Component Registry](https://components.espressif.com/components/espressif/zero_detection/badge.svg)](https://components.espressif.com/components/espressif/zero_detection) |

</center>

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

### 构建和烧录示例

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