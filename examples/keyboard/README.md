# KeyBoard Example

The ESP-KeyBoard project is a keyboard project using the ESP32-S3, supporting N-key rollover, dual-mode USB and Bluetooth output, over 40 types of local lighting effects, and WIN11 lighting effect synchronization. It also supports low-power Bluetooth, significantly extending the keyboard's wireless usage time.

## Main Software Features

* Supports N-key rollover key scanning
* Supports low-power key scanning
* Supports USB 1K report rate
* Supports BLE 125Hz report rate
* Supports local lighting control with over 40 types
* Supports `Windows 11` dynamic lighting control

![ESP-KeyBoard](https://dl.espressif.com/esp-iot-solution/static/keyboard.jpg)

## How to Use

### Hardware Requirements

* Refer to the ESP-KeyBoard hardware design, open-source at [esp-keyboard](https://oshwhub.com/esp-college/esp-keyboard)

* Demo video [video](https://www.bilibili.com/video/BV1yi421C7qV/?share_source=copy_web&vd_source=7e24f4cefdafbd8477369f33616312a9)

* Board support package introduction documentation [BSP](./components/esp32_s3_kbd_kit/README.md)

![keyboard_hardware](https://dl.espressif.com/esp-iot-solution/static/keyboard_1.gif)

### Software Design

#### Mode Combinations

|   Connection Mode    |   Local Lighting    | WIN11 Lighting | N-key Rollover | Report Rate |
| :------------------: | :-----------------: | :------------: | :------------: | :---------: |
|         USB          |          √          |       √        |       √        |     1k      |
|         BLE          | √(Default disabled) |       ×        |       √        |    125Hz    |
| 2.4G (Not Supported) |          √          |       ×        |       √        |     1k      |

本地灯效

![local_light](https://dl.espressif.com/esp-iot-solution/static/keyboard_2.gif)

Win11 灯效

![win11_light](https://dl.espressif.com/esp-iot-solution/static/keyboard_3.gif)

#### Shortcut Keys

| Key Combination |               Function               |
| :-------------: | :----------------------------------: |
|     FN + F1     |               USB Mode               |
|     FN + F2     |            Bluetooth Mode            |
|    FN + F10     |             Volume Mute              |
|    FN + F11     |             Volume Down              |
|    FN + F12     |              Volume Up               |
|    FN + Home    |       Switch Local Light Color       |
|    FN + PgUP    |      Forward Local Light Effect      |
|    FN + PgDN    |     Backward Local Light Effect      |
|    FN + END     | Switch to WIN11 Dynamic Light Effect |
|   FN + Space    |        Toggle Local Lighting         |
|     FN + ↑      |   Increase Local Light Brightness    |
|     FN + ↓      |   Decrease Local Light Brightness    |
|     FN + ←      |      Decrease Local Light Speed      |
|     FN + →      |      Increase Local Light Speed      |

#### QA

How to modify local lighting effects:

  * Use `idf.py menuconfig` and enable or disable relevant lighting effects under `(Top) → Component config → rgb matrix`.

How to modify key mappings:

  * Please modify the key mapping table in [keymap.h](./components/esp32_s3_kbd_kit/include/bsp/keymap.h) file.

#### license

Note: keyboard_rgb_martix comes from the **QMK** project. Due to the use of the GPL license, if you have product plans based on this example, it is recommended to replace this component.

### Change LOG

* v0.2.2 - 2024-11-21

  * Added support for waking up a PC from sleep mode (remote wakeup).

* v0.2.1 - 2024-08-12

- Added debounce time to prevent buttons from being triggered twice consecutively.
- The speed of the heatmap lighting effect can now be adjusted according to the effect speed, with levels ranging from 1 to 4.

* v0.2.0 - 2024-8-12

  * Added support for lighting effects in BLE mode, with the keyboard backlight turning off by default after 60 seconds.
  * Resolved the issue with adjusting the speed of lighting effects.
  * Increased the number of default lighting effects to 30.