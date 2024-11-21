# KeyBoard 键盘示例

ESP-KeyBoard 项目是使用 ESP32-S3 的键盘项目，支持全键无冲，USB 蓝牙双模输出，以及 40+种的本地灯效和 WIN11 灯效同步，并支持了低功耗蓝牙，使得键盘无线使用时间大大延长。

主要的软件功能：

    * 支持全键无冲的按键扫描
    * 支持按键扫描低功耗
    * 支持 USB 1K 回报率
    * 支持 BLE 125hz 回报率
    * 支持本地灯效控制 40+ 种
    * 支持 `Windows 11` 动态灯效控制

![ESP-KeyBoard](https://dl.espressif.com/esp-iot-solution/static/keyboard.jpg)

## 如何使用

### 硬件需求

* 参考 ESP-KeyBoard 硬件设计，开源地址 [esp-keyboard](https://oshwhub.com/esp-college/esp-keyboard)

* Demo 视频 [video](https://www.bilibili.com/video/BV1yi421C7qV/?share_source=copy_web&vd_source=7e24f4cefdafbd8477369f33616312a9)

* BSP 介绍文档 [BSP](./components/esp32_s3_kbd_kit/README.md)

![keyboard_hardware](https://dl.espressif.com/esp-iot-solution/static/keyboard_1.gif)

### 软件设计

#### 模式组合

|     连接模式     |    本地灯效     | WIN11 灯效 | 全键无冲 | 回报率 |
| :--------------: | :-------------: | :--------: | :------: | :----: |
|       USB        |        √        |     √      |    √     |   1k   |
|       BLE        | √（默认不开启） |     ×      |    √     | 125Hz  |
| 2.4G（暂不支持） |        √        |     ×      |    √     |   1k   |

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

  * 请修改 [keymap.h](./components/esp32_s3_kbd_kit/include/bsp/keymap.h) 文件中的按键映射表

#### license

Note: keyboard_rgb_martix 来自 **QMK** 工程，由于使用 GPL 协议，如果您有基于本示例的产品计划，建议替换该组件。

#### Change LOG

* v0.2.2 - 2024-11-21

  * 支持唤醒睡眠状态下的 PC (remote wakeup)

* v0.2.1 - 2024-8-12

  * 增加消抖时间，避免按键连续触发两次
  * 热力图灯效的速度可以跟随灯效速度调节，为 1-4 档

* v0.2.0 - 2024-8-12

  * BLE 模式下支持灯效，默认 60s 关闭键盘灯光
  * 解决灯效速度调节问题
  * 默认支持灯效增加到 30 种
