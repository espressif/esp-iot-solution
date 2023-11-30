# Freetype Example

This demo shows how to add [freetype](https://docs.lvgl.io/8.3/libs/freetype.html) to the lvgl project.

## How to use example

Please first read the [User Guide](https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html#esp32-s3-lcd-ev-board) of the ESP32-S3-LCD-EV-Board to learn about its software and hardware information.

### Hardware Required

* An ESP32-S3-LCD-EV-Board development board with subboard3 (800x480) or subboard2 (480x480)
* An USB Type-C cable for Power supply and programming

### Configurations

Run `idf.py menuconfig` and go to `Board Support Package`:
* `BSP_LCD_SUB_BOARD`: Choose a LCD subboard according to hardware. Default use subboard3 (800x480).
* More configurations see BSP's [README](https://github.com/espressif/esp-bsp/tree/master/esp32_s3_lcd_ev_board#bsp-esp32-s3-lcd-ev-board).

### Build and Flash

1. Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-iot-solution/issues)

We will get back to you as soon as possible.
