# KeyBoard 键盘示例

ESP-KeyBoard 项目是键盘示例，支持 **ESP32-S3** 和 **ESP32-H4**（USB / BLE）。支持全键无冲、40+ 种本地灯效和 WIN11 灯效同步，并支持低功耗蓝牙，可显著延长无线使用时间。

主要的软件功能：

    * 支持全键无冲的按键扫描
    * 支持按键扫描低功耗
    * 支持 USB 1K 回报率
    * 支持 BLE 125hz 回报率
    * 支持 BLE 电池电量报告
    * 支持 USB / BLE 双模切换
    * 支持本地灯效控制 40+ 种
    * 支持 `Windows 11` 动态灯效控制

![ESP-KeyBoard](https://dl.espressif.com/esp-iot-solution/static/keyboard.jpg)

## 如何使用

### ESP-IDF 6.x / ESP32-H4 编译说明

ESP32-H4 默认编译 WS2812、RGB 矩阵和电量 ADC。ESP32-H4-Core-V0_2 使用 GPIO32 作为 WS2812 数据脚，GPIO28 / ADC1_CH0 作为电池采样脚。

### 硬件需求

* 参考 ESP-KeyBoard 硬件设计，开源地址 [esp-keyboard](https://oshwhub.com/esp-college/esp-keyboard)

* Demo 视频 [video](https://www.bilibili.com/video/BV1yi421C7qV/?share_source=copy_web&vd_source=7e24f4cefdafbd8477369f33616312a9)

* BSP 介绍文档：
  * ESP32-S3：[esp32_s3_kbd_kit](./components/esp32_s3_kbd_kit/README.md)
  * ESP32-H4：[esp32_h4_kbd_kit](./components/esp32_h4_kbd_kit/README.md)

![keyboard_hardware](https://dl.espressif.com/esp-iot-solution/static/keyboard_1.gif)

### 软件设计

#### 模式组合

| 连接模式 | 适用芯片 | 本地灯效 | WIN11 灯效 | 全键无冲 | 回报率 | 电池上报 |
| :------: | :------: | :------: | :--------: | :------: | :----: | :------: |
|   USB    | ESP32-S3 / H4 |   √    |     √      |    √     |   1k   |    ×     |
|   BLE    | ESP32-S3 / H4 | √（默认不开启） |  ×  |    √     | 125Hz  |    √     |

本地灯效

![local_light](https://dl.espressif.com/esp-iot-solution/static/keyboard_2.gif)

Win11 灯效

![win11_light](https://dl.espressif.com/esp-iot-solution/static/keyboard_3.gif)

#### 快捷按键

|  组合键   |        功能         |
| :-------: | :-----------------: |
|  FN + F1  |      USB 模式       |
|  FN + F2  |      蓝牙模式       |
| FN + F10  |     音量开关键      |
| FN + F11  |       音量 -        |
| FN + F12  |       音量 +        |
| FN + Home |  切换本地灯效颜色   |
| FN + PgUP |  向前切换本地灯效   |
| FN + PgDN |  向后切换本地灯效   |
| FN + END  | 切换为 W11 动态灯效 |
| FN + 空格 |    开关本地灯效     |
|  FN + ↑   |  本地灯效亮度增加   |
|  FN + ↓   |  本地灯效亮度减少   |
|  FN + ←   |  本地灯效速度减少   |
|  FN + →   |  本地灯效速度增加   |

#### QA

如何修改本地灯效：

  * 使用 `idf.py menuconfig` 后在 `(Top) → Component config → rgb matrix` 中启用和关闭相关灯效

如何修改按键映射：

  * ESP32-S3：[keymap.h](./components/esp32_s3_kbd_kit/include/bsp/keymap.h)
  * ESP32-H4：[keymap.h](./components/esp32_h4_kbd_kit/include/bsp/keymap.h)

#### license

Note: keyboard_rgb_martix 来自 **QMK** 工程，由于使用 GPL 协议，如果您有基于本示例的产品计划，建议替换该组件。

#### Change LOG

* v0.4.0 - 2026-5-21

  * 新增 ESP32-H4 BSP（`esp32_h4_kbd_kit`）及统一 `hid_transport` 传输层
  * `led_strip` 依赖按 IDF 版本区分：ESP-IDF >= 6.0 使用 `^3.0.3`（修复 `esp_heap_caps.h` / `MALLOC_CAP_*` 编译报错，不再需要原先的本地补丁），旧版本 IDF 保持 `^2.5.3` 以兼容历史版本
  * 适配 ESP-IDF 6.x / ESP32-H4 的 `adc_battery_estimation`：仓库内源码（经 `main/idf_component.yml` 的 `override_path` 引用）按芯片自动选择 `SOC_ADC_DIGI_*` 或 `SOC_ADC_RTC_*` 位宽宏，修复新芯片上 `'SOC_ADC_RTC_*BITWIDTH' undeclared` 报错

* v0.3.0 - 2025-8-4

  * 增加了基于 ADC 的 BLE 电池电量监控和报告功能
  * 改进了使用电阻分压电路的电池容量估算

* v0.2.2 - 2024-11-21

  * 支持唤醒睡眠状态下的 PC (remote wakeup)

* v0.2.1 - 2024-8-12

  * 增加消抖时间，避免按键连续触发两次
  * 热力图灯效的速度可以跟随灯效速度调节，为 1-4 档

* v0.2.0 - 2024-8-12

  * BLE 模式下支持灯效，默认 60s 关闭键盘灯光
  * 解决灯效速度调节问题
  * 默认支持灯效增加到 30 种
