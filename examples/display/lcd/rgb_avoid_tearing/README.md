| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

| Supported LCD Controller    | ST7701 |
| ----------------------------| -------|

| Supported Touch Controller  |  GT911 |
| ----------------------------| -------|

# RGB Avoid Tearing Example

[esp_lcd](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd.html) provides several panel drivers out-of box, e.g. ST7789, SSD1306, NT35510. However, there're a lot of other panels on the market, it's beyond `esp_lcd` component's responsibility to include them all.

`esp_lcd` allows user to add their own panel drivers in the project scope (i.e. panel driver can live outside of esp-idf), so that the upper layer code like LVGL porting code can be reused without any modifications, as long as user-implemented panel driver follows the interface defined in the `esp_lcd` component.

This example demonstrates how to avoid tearing when using LVGL with RGB interface screens in an esp-idf project. The example will use the LVGL library to draw a stylish music player.

The LVGL-related parameter configurations, such as LVGL's registered resolution, LVGL task-related parameters, and tearing prevention methods, can be configured in lvgl_port_v8.h.

This example uses the [esp_timer](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html) to generate the ticks needed by LVGL and uses a dedicated task to run the `lv_timer_handler()`. Since the LVGL APIs are not thread-safe, this example uses a mutex which be invoked before the call of `lv_timer_handler()` and released after it. The same mutex needs to be used in other tasks and threads around every LVGL (lv_...) related function call and code. For more porting guides, please refer to [LVGL porting doc](https://docs.lvgl.io/master/porting/index.html).

## Mode 4 optimization and temporary patch

### Why prioritize Mode 4

- In 90°/270° rotation scenarios, Mode 4’s data path is more efficient, and combined with PPA, it can greatly improve rotation performance.
- Trade-off: consumes more SRAM (used for LVGL rendering buffers). Actual usage can be allocated as needed; in principle, the more allocated, the higher the frame rate.

### Temporary patch (PPA freeze fix)

To fully leverage Mode 4’s performance benefits in rotation and anti-tearing, and to avoid a known sporadic PPA freeze, you need to **temporarily apply the patch provided in this repository**:

`examples/display/lcd/mipi_dsi_avoid_tearing/main/0001-bugfix-lcd-Fixed-PPA-freeze.patch`

This patch targets ESP-IDF release v5.5 at commit `02c5f2dbb95859bc4a35fb6d82bbc0784968efc5`. It fixes a register configuration on the PPA SRM path to avoid freezes under heavy workloads.

### How to apply the patch on ESP-IDF v5.5 (commit 02c5f2d)

1. Prepare the exact ESP-IDF commit:

```
# assuming you already have an esp-idf repository locally
export IDF_PATH=/path/to/esp-idf
cd $IDF_PATH
git fetch --all
git checkout 02c5f2dbb95859bc4a35fb6d82bbc0784968efc5
git submodule update --init --recursive
```

2. Apply the example-provided patch (using `git am`):

```
# point to this repository path (replace with your actual path)
export IOT_SOLUTION=/path/to/esp-iot-solution

cd $IDF_PATH
git am $IOT_SOLUTION/examples/display/lcd/mipi_dsi_avoid_tearing/main/0001-bugfix-lcd-Fixed-PPA-freeze.patch
```

3. With the patched ESP-IDF, go back to this example’s root and follow the “Build and Flash” steps below to build, flash, and verify.

To revert or if you encounter conflicts:

```
# if conflicts occur during apply
git am --abort

# if already applied and you want to undo the last patch
git reset --hard HEAD~1
```

## How to use the example

## ESP-IDF Required

### Hardware Required

* An ESP32-S3R8 development board
* A ST7701 LCD panel, with RGB interface
* An USB cable for power supply and programming

### Hardware Connection

The connection between ESP Board and the LCD is as follows:

```
       ESP Board                           RGB  Panel
+-----------------------+              +-------------------+
|                   GND +--------------+GND                |
|                       |              |                   |
|                   3V3 +--------------+VCC                |
|                       |              |                   |
|                   PCLK+--------------+PCLK               |
|                       |              |                   |
|             DATA[15:0]+--------------+DATA[15:0]         |
|                       |              |                   |
|                  HSYNC+--------------+HSYNC              |
|                       |              |                   |
|                  VSYNC+--------------+VSYNC              |
|                       |              |                   |
|                     DE+--------------+DE                 |
|                       |              |                   |
|               BK_LIGHT+--------------+BLK                |
+-----------------------+              |                   |
                               3V3-----+DISP_EN            |
                                       |                   |
                                       +-------------------+
```

* The LCD parameters and GPIO number used by this example can be changed in [example_rgb_avoid_tearing.c](main/example_rgb_avoid_tearing.c). Especially, please pay attention to the **vendor specific initialization**, it can be different between manufacturers and should consult the LCD supplier for initialization sequence code.
* The LVGL parameters can be changed not only through `menuconfig` but also directly in `lvgl_conf.h`

### Configure the Project

Run `idf.py menuconfig` and navigate to `Example Configuration` menu.

### Build and Flash

Run `idf.py set-target esp32s3` to select the target chip.

Run `idf.py -p PORT build flash monitor` to build, flash and monitor the project. A fancy animation will show up on the LCD as expected.

The first time you run `idf.py` for the example will cost extra time as the build system needs to address the component dependencies and downloads the missing components from registry into `managed_components` folder.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
