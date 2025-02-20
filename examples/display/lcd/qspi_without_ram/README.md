| Supported Targets | ESP32-S3 | ESP32-C5 |
| ----------------- | -------- | -------- |

| Supported LCD Controllers | ST77903 |
| ------------------------- | ------- |

# QSPI LCD (without RAM) Example

[esp_lcd](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd.html) provides several panel drivers out-of box, e.g. ST7789, SSD1306, NT35510. However, there're a lot of other panels on the market, it's beyond `esp_lcd` component's responsibility to include them all.

`esp_lcd` allows user to add their own panel drivers in the project scope (i.e. panel driver can live outside of esp-idf), so that the upper layer code like LVGL porting code can be reused without any modifications, as long as user-implemented panel driver follows the interface defined in the `esp_lcd` component.

This example shows how to use ST77903 display driver from Component manager in esp-idf project. These components are using API provided by `esp_lcd` component. This example will draw a fancy dash board with the LVGL library.

This example uses the [esp_timer](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html) to generate the ticks needed by LVGL and uses a dedicated task to run the `lv_timer_handler()`. Since the LVGL APIs are not thread-safe, this example uses a mutex which be invoked before the call of `lv_timer_handler()` and released after it. The same mutex needs to be used in other tasks and threads around every LVGL (lv_...) related function call and code. For more porting guides, please refer to [LVGL porting doc](https://docs.lvgl.io/master/porting/index.html).

## How to use the example

### Hardware Required

* An ESP32-S3R2 or ESP32-S3R8 development board
* A ST77903 LCD panel, with QSPI interface
* An USB cable for power supply and programming

### Hardware Connection

The connection between ESP Board and the LCD is as follows:

```
       ESP Board                       ST77903 Panel (QSPI)
┌──────────────────────┐              ┌────────────────────┐
│             GND      ├─────────────►│ GND                │
│                      │              │                    │
│             3V3      ├─────────────►│ VCC                │
│                      │              │                    │
│             CS       ├─────────────►│ CS                 │
│                      │              │                    │
│             SCK      ├─────────────►│ CLK                │
│                      │              │                    │
│             D3       ├─────────────►│ IO3                │
│                      │              │                    │
│             D2       ├─────────────►│ IO2                │
│                      │              │                    │
│             D1       ├─────────────►│ IO1                │
│                      │              │                    │
│             D0       ├─────────────►│ IO0                │
│                      │              │                    │
│             RST      ├─────────────►│ RSTN               │
└──────────────────────┘              └────────────────────┘
```

The LCD parameters and GPIO number used by this example can be changed in [lvgl_port.h](main/lvgl_port.h).
Especially, please pay attention to the **vendor specific initialization**, it can be different between manufacturers and should consult the LCD supplier for initialization sequence code.

### Build and Flash

Run `idf.py -p PORT build flash monitor` to build, flash and monitor the project. A fancy animation will show up on the LCD as expected.

The first time you run `idf.py` for the example will cost extra time as the build system needs to address the component dependencies and downloads the missing components from registry into `managed_components` folder.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Example Output

```bash
...
I (679) example: Turn off LCD backlight
I (684) gpio: GPIO[8]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (693) lvgl_port: Install ST77903 panel driver
I (699) gpio: GPIO[15]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (717) st77903: LCD panel create success, version: 0.3.0
I (1099) lvgl_port: Initialize LVGL library
I (1101) lvgl_port: Install LVGL tick timer
I (1102) lvgl_port: Starting LVGL task
I (1109) example: Turn on LCD backlight
I (1109) example: Display LVGL demos
I (1125) main_task: Returned from app_main()
...
```

## Performance Test

### Test Environment

#### For ESP32-S3R2

|          Item          |       Value        |
| :--------------------: | :----------------: |
|   Resolution of LCD    |     400 * 400      |
| Configuration of PSRAM |     Quad, 120M     |
| Configuration of Flash |     QIO, 120M      |
|    Version of LVGL     |       v8.3.9       |
|   Test Demo of LVGL    |    Music player    |
|  Basic Configurations  | sdkconfig.defaults |

#### For ESP32-S3R8

|          Item          |               Value                |
| :--------------------: | :--------------------------------: |
|   Resolution of LCD    |             400 * 400              |
| Configuration of PSRAM |            Octal, 120M             |
| Configuration of Flash |             QIO, 120M              |
|    Version of LVGL     |               v8.3.9               |
|   Test Demo of LVGL    |            Music player            |
|  Basic Configurations  | sdkconfig.defaults.psram_octal_ddr |

### Description of Buffering Mode

| Buffering Mode |                    Description                    |    Special Configurations     |
| :------------: | :-----------------------------------------------: | :---------------------------: |
|     Mode1      | One buffer with 100-line heights in internal SRAM |               *               |
|     Mode2      |   One buffer with frame-size in internal PSRAM    | sdkconfig.ci.avoid_tear_mode1 |
|     Mode3      |  Full-refresh with two frame-size PSRAM buffers   | sdkconfig.ci.avoid_tear_mode2 |
|     Mode4      |   Direct-mode with two frame-size PSRAM buffers   | sdkconfig.ci.avoid_tear_mode3 |
|     Mode5      | Full-refresh with three frame-size PSRAM buffers  | sdkconfig.ci.avoid_tear_mode4 |

**Note:** To test the above modes, run the following commands to configure project (take ESP32-S3R8 and `Mode4` as an example):
```
rm -rf build sdkconfig sdkconfig.old
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci.avoid_tear_mode3" reconfigure
```

### Average FPS using ESP32-S3R2

| Buffering Mode | Average FPS |
| :------------: | :---------: |
|     Mode1      |     22      |
|     Mode2      |     16      |
|     Mode3      |     14      |
|     Mode4      |     18      |
|     Mode5      |     16      |

### Average FPS using ESP32-S3R8

| Buffering Mode | Average FPS |
| :------------: | :---------: |
|     Mode1      |     30      |
|     Mode2      |     25      |
|     Mode3      |     25      |
|     Mode4      |     26      |
|     Mode5      |     29      |

## Troubleshooting

For any technical queries, please open an [issue] (https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
