# KeyBoard Example

The ESP-KeyBoard project is a keyboard example for **ESP32-S3** and **ESP32-H4** (USB / BLE). It supports N-key rollover, over 40 types of local lighting effects, and WIN11 lighting effect synchronization. BLE low-power mode significantly extends wireless usage time.

## Main Software Features

* Supports N-key rollover key scanning
* Supports low-power key scanning
* Supports USB 1K report rate
* Supports BLE 125Hz report rate
* Supports BLE battery level reporting
* Supports USB / BLE mode switching
* Supports local lighting control with over 40 types
* Supports `Windows 11` dynamic lighting control

![ESP-KeyBoard](https://dl.espressif.com/esp-iot-solution/static/keyboard.jpg)

## How to Use

### ESP-IDF 6.x / ESP32-H4 build notes

On ESP32-H4, WS2812, RGB matrix, and battery ADC are built by default. The ESP32-H4-Core-V0_2 board uses GPIO32 for WS2812 data and GPIO28 / ADC1_CH0 for battery sampling.

### Hardware Requirements

* Refer to the ESP-KeyBoard hardware design, open-source at [esp-keyboard](https://oshwhub.com/esp-college/esp-keyboard)

* Demo video [video](https://www.bilibili.com/video/BV1yi421C7qV/?share_source=copy_web&vd_source=7e24f4cefdafbd8477369f33616312a9)

* Board support package:
  * ESP32-S3: [esp32_s3_kbd_kit](./components/esp32_s3_kbd_kit/README.md)
  * ESP32-H4: [esp32_h4_kbd_kit](./components/esp32_h4_kbd_kit/README.md)

![keyboard_hardware](https://dl.espressif.com/esp-iot-solution/static/keyboard_1.gif)

### Software Design

#### Mode Combinations

| Connection Mode | Supported Target | Local Lighting | WIN11 Lighting | N-key Rollover | Report Rate | Battery Report |
| :-------------: | :--------------: | :------------: | :------------: | :------------: | :---------: | :------------: |
|       USB       |  ESP32-S3 / H4   |       √        |       √        |       √        |     1k      |       ×        |
|       BLE       |  ESP32-S3 / H4   | √(Default disabled) |       ×   |       √        |    125Hz    |       √        |

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

  * ESP32-S3: [keymap.h](./components/esp32_s3_kbd_kit/include/bsp/keymap.h)
  * ESP32-H4: [keymap.h](./components/esp32_h4_kbd_kit/include/bsp/keymap.h)

#### license

Note: keyboard_rgb_martix comes from the **QMK** project. Due to the use of the GPL license, if you have product plans based on this example, it is recommended to replace this component.

### Change LOG

* v0.4.0 - 2026-05-21

  * Added ESP32-H4 BSP (`esp32_h4_kbd_kit`) and unified `hid_transport` layer.
  * Gated the `led_strip` dependency by IDF version: `^3.0.3` on ESP-IDF >= 6.0 (fixes the `esp_heap_caps.h` / `MALLOC_CAP_*` compile error, so the previous local patch is no longer needed), `^2.5.3` on older IDF for backward compatibility.
  * Adapted `adc_battery_estimation` for ESP-IDF 6.x / ESP32-H4: the in-tree copy (used via `override_path` in `main/idf_component.yml`) now selects `SOC_ADC_DIGI_*` or `SOC_ADC_RTC_*` bitwidth macros per target, fixing `'SOC_ADC_RTC_*BITWIDTH' undeclared` on new SoCs.

* v0.3.0 - 2025-08-04

  * Added BLE battery level reporting with ADC-based battery monitoring.
  * Improved battery capacity estimation using resistor voltage divider circuit.

* v0.2.2 - 2024-11-21

  * Added support for waking up a PC from sleep mode (remote wakeup).

* v0.2.1 - 2024-08-12

- Added debounce time to prevent buttons from being triggered twice consecutively.
- The speed of the heatmap lighting effect can now be adjusted according to the effect speed, with levels ranging from 1 to 4.

* v0.2.0 - 2024-8-12

  * Added support for lighting effects in BLE mode, with the keyboard backlight turning off by default after 60 seconds.
  * Resolved the issue with adjusting the speed of lighting effects.
  * Increased the number of default lighting effects to 30.
