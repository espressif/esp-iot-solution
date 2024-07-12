# BSP: ESP32-S3-KBD-KIT

ESP32-S3-KBD-KIT is an ESP32S3-based development board produced by Espressif.
* [Hardware Reference](https://oshwhub.com/esp-college/esp-keyboard)

![ESP-KeyBoard](https://dl.espressif.com/esp-iot-solution/static/keyboard.jpg)

ESP32-S3-KBD-KIT features the following integrated components:

* ESP32-S3-WROOM-1-N4 module
* WS2812 RGB LED
* 6x15 Key Scan Circuit
* Battery Charge and Discharge Management

ESP32-S3-KBD-KIT only supports hardware version V1.1. If you need to use the V1.0 hardware version, please modify it according to the hardware GPIO.

## Hardware Version

### v1.1

Make the following hardware modifications:
1. Added a 32.768K crystal oscillator to reduce power consumption when the BLE connection enters light sleep mode (reduces standby current by about 2mA).
2. Added total power control for the WS2812 LED group, allowing complete disconnection of WS2812 power supply when using battery power (reduces standby current by about 40mA).
3. Added battery voltage detection.

|      Name       |  Status  |  GPIO  |     Descriptor      |
| :-------------: | :------: | :----: | :-----------------: |
|    ROW_IO_0     | Maintain | GPIO40 |    Row Scan GPIO    |
|    ROW_IO_1     | Maintain | GPIO39 |    Row Scan GPIO    |
|    ROW_IO_2     | Maintain | GPIO38 |    Row Scan GPIO    |
|    ROW_IO_3     | Maintain | GPIO45 |    Row Scan GPIO    |
|    ROW_IO_4     | Maintain | GPIO48 |    Row Scan GPIO    |
|    ROW_IO_5     | Maintain | GPIO47 |    Row Scan GPIO    |
|    COL_IO_0     | Maintain | GPIO21 |    COL Scan GPIO    |
|    COL_IO_1     | Maintain | GPIO14 |    COL Scan GPIO    |
|    COL_IO_2     | Maintain | GPIO13 |    COL Scan GPIO    |
|    COL_IO_3     | Maintain | GPIO12 |    COL Scan GPIO    |
|    COL_IO_4     | Maintain | GPIO11 |    COL Scan GPIO    |
|    COL_IO_5     | Maintain | GPIO10 |    COL Scan GPIO    |
|    COL_IO_6     | Maintain | GPIO9  |    COL Scan GPIO    |
|    COL_IO_7     | Maintain | GPIO4  |    COL Scan GPIO    |
|    COL_IO_8     | Maintain | GPIO5  |    COL Scan GPIO    |
|    COL_IO_9     | Maintain | GPIO6  |    COL Scan GPIO    |
|    COL_IO_10    | Maintain | GPIO7  |    COL Scan GPIO    |
|    COL_IO_11    |  Modify  | GPIO17 |    COL Scan GPIO    |
|    COL_IO_12    |  Modify  | GPIO3  |    COL Scan GPIO    |
|    COL_IO_13    |  Modify  | GPIO18 |    COL Scan GPIO    |
|    COL_IO_14    |  Modify  | GPIO8  |    COL Scan GPIO    |
|    WS2812_EN    |   Add    | GPIO1  | WS2812 Control GPIO |
|   WS2812_GPIO   |  Modify  | GPIO0  |     WS2812 GPIO     |
| Battery_Monitor |   Add    | GPIO2  | Battery ADC Monitor |

### v1.0

Features include:
1. 6*15 row-column scanning method
2. WS2812 LED strip

|    Name     |  Status  |  GPIO  |  Descriptor   |
| :---------: | :------: | :----: | :-----------: |
|  ROW_IO_0   | Maintain | GPIO40 | Row Scan GPIO |
|  ROW_IO_1   | Maintain | GPIO39 | Row Scan GPIO |
|  ROW_IO_2   | Maintain | GPIO38 | Row Scan GPIO |
|  ROW_IO_3   | Maintain | GPIO45 | Row Scan GPIO |
|  ROW_IO_4   | Maintain | GPIO48 | Row Scan GPIO |
|  ROW_IO_5   | Maintain | GPIO47 | Row Scan GPIO |
|  COL_IO_0   | Maintain | GPIO21 | COL Scan GPIO |
|  COL_IO_1   | Maintain | GPIO14 | COL Scan GPIO |
|  COL_IO_2   | Maintain | GPIO13 | COL Scan GPIO |
|  COL_IO_3   | Maintain | GPIO12 | COL Scan GPIO |
|  COL_IO_4   | Maintain | GPIO11 | COL Scan GPIO |
|  COL_IO_5   | Maintain | GPIO10 | COL Scan GPIO |
|  COL_IO_6   | Maintain | GPIO9  | COL Scan GPIO |
|  COL_IO_7   | Maintain | GPIO4  | COL Scan GPIO |
|  COL_IO_8   | Maintain | GPIO5  | COL Scan GPIO |
|  COL_IO_9   | Maintain | GPIO6  | COL Scan GPIO |
|  COL_IO_10  | Maintain | GPIO7  | COL Scan GPIO |
|  COL_IO_11  | Maintain | GPIO15 | COL Scan GPIO |
|  COL_IO_12  | Maintain | GPIO16 | COL Scan GPIO |
|  COL_IO_13  | Maintain | GPIO17 | COL Scan GPIO |
|  COL_IO_14  | Maintain | GPIO18 | COL Scan GPIO |
| WS2812_GPIO | Maintain | GPIO8  |  WS2812 GPIO  |