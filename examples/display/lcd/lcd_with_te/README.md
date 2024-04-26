| Supported Targets | ESP32 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- |

## How to use the example

### Hardware Required

* An ESP development board
* An Intel 8080 interfaced (so called MCU interface or parallel interface) LCD
* An USB cable for power supply and programming

### Hardware Connection

The connection between ESP Board and the LCD is as follows:

```text
   ESP Board                      LCD Screen
┌─────────────┐              ┌────────────────┐
│             │              │                │
│         3V3 ├─────────────►│ VCC            │
│             │              │                │
│         GND ├──────────────┤ GND            │
│             │              │                │
│  DATA[0..7] │◄────────────►│ DATA[0..7]     │
│             │              │                │
│        PCLK ├─────────────►│ PCLK           │
│             │              │                │
│          CS ├─────────────►│ CS             │
│             │              │                │
│         D/C ├─────────────►│ D/C            │
│             │              │                │
│         RST ├─────────────►│ RST            │
│             │              │                │
│    BK_LIGHT ├─────────────►│ BCKL           │
│             │              │                │
│             │              └────────────────┘
│             │                   LCD TOUCH
│             │              ┌────────────────┐
│             │              │                │
│     I2C SCL ├─────────────►│ I2C SCL        │
│             │              │                │
│     I2C SDA │◄────────────►│ I2C SDA        │
│             │              │                │
└─────────────┘              └────────────────┘

```

The GPIO number used by this example can be changed in [esp_bsp.h](components/esp_bsp/include/esp_bsp.h).

### Build and Flash

Run `idf.py -p PORT build flash monitor` to build, flash and monitor the project. A fancy animation will show up on the LCD as expected.

The first time you run `idf.py` for the example will cost extra time as the build system needs to address the component dependencies and downloads the missing components from registry into `managed_components` folder.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Example Output

```bash
I (1168) main_task: Started on CPU0
I (1172) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1181) main_task: Calling app_main()
I (1185) LVGL: Starting LVGL task
I (1190) gpio: GPIO[2]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1199) gpio: GPIO[3]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1208) example: Initialize Intel 8080 bus
I (1213) example: Install LCD driver of axs15231b
I (1218) gpio: GPIO[5]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (2288) lcd_panel.axs15231b: send init commands success
I (2288) gpio: GPIO[16]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:1
I (2295) gpio: GPIO[21]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:2
I (2309) example: Setting LCD backlight: 100%
I (2327) main_task: Returned from app_main()
```


## Troubleshooting

For any technical queries, please open an [issue] (https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.