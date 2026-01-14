[![Documentation Status](https://dl.espressif.com/AE/docs/docs_latest.svg)](https://docs.espressif.com/projects/esp-iot-solution/en)

<a href="https://espressif.github.io/esp-launchpad/?flashConfigURL=https://dl.espressif.com/AE/esp-iot-solution/config.toml">
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
|      master      |      >= v5.3      |                New Chips Support                                     |                     [Docs online](https://docs.espressif.com/projects/esp-iot-solution/)                     | New features                |
|   release/v2.0   | <= v5.3, >= v4.4  |              Support component manager              |                     [Docs online](https://docs.espressif.com/projects/esp-iot-solution/en/release-v2.0/index.html)                     | Bugfix only，until v5.3 EOL |
|   release/v1.1   |      v4.0.1       | IDF version update, remove no longer supported code | [v1.1 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.1#esp32-iot-solution-overview) | archived                    |
|   release/v1.0   |      v3.2.2       |                   legacy version                    | [v1.0 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.0#esp32-iot-solution-overview) | archived                    |

> **Note**
>
> The recommended ESP-IDF version varies for different chips. For details, please refer to [Compatibility Between ESP-IDF Releases and Revisions of Espressif SoCs](https://github.com/espressif/esp-idf/blob/master/COMPATIBILITY.md).

> Since the `master` branch uses the `ESP Component Manager` to manager components, each of them is a separate package, and each package may support a different version of the ESP-IDF, which will be declared in the component's `idf_component.yml` file

#### Get Components from ESP Component Registry

If you just want to use the components in ESP-IoT-Solution, we recommend you use it from the [ESP Component Registry](https://components.espressif.com/).

The registered components in ESP-IoT-Solution are listed below:

<center>

| Component | Version |
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
