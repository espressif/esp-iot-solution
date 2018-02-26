# ESP32 Touch_screen

This demo shows how the touch screen works and can display the point where you touch.

## Quick Start

Follow the following steps to the touch-tft to ESP32 module and flash application to the ESP32.

### Connect

Specific pins used in this example to connect ESP32 and touch-tft are shown in table below.   

| touch screen Pin | Pin Mapping for ESP32_Core_board |
| :---: | :---: | :---: |
| T_CS | IO26 |
| SDO | IO25 |
| LED | IO5 |
| SCK | IO19 |
| SDI | IO23 |
| D/C | IO21 |
| RESET | IO18 |
| CS | IO22 |
| GND | GND |
| VCC | 3V3 |

Notes:

whether use LCD display configed by `make menuconfig`.

### Bulid & Flash

Clone the code provided in this repository to your PC, compile with the latest [esp-idf](https://github.com/espressif/esp-idf) installed from GitHub and download to the module.

If all h/w components are connected properly you are likely to see the following message during download:

```
$ make flash monitor
```

### Display

#### Touch Calibration  

First of all, the screen should be calibrated, you can follow the tips on screen. This step determines the accuracy of the screen display.  

#### touch-tft display  

The screen displays the current touch point and acts as a drawing board.


