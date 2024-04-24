# Freetype Example

This demo shows how to add [freetype](https://github.com/espressif/idf-extra-components/tree/master/freetype) to the lvgl project.

## How to use example

Please first read the [User Guide](https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html#esp32-s3-lcd-ev-board) of the ESP32-S3-LCD-EV-Board to learn about its software and hardware information.

### Hardware Required

* An ESP32-S3-LCD-EV-Board development board with subboard3 (800x480) or subboard2 (480x480)
* An USB Type-C cable for Power supply and programming

### Configurations

Run `idf.py menuconfig` and go to `Board Support Package`:
* `BSP_LCD_SUB_BOARD`: Choose a LCD subboard according to hardware. Default use subboard3 (800x480).
* More configurations see BSP's [README](https://github.com/espressif/esp-bsp/tree/master/bsp/esp32_s3_lcd_ev_board#bsp-esp32-s3-lcd-ev-board).

### PSRAM 120M DDR

The PSRAM 120M DDR feature is intended to achieve the best performance of RGB LCD. It is only available with ESP-IDF **release/v5.1** and above. It can be used by enabling the `IDF_EXPERIMENTAL_FEATURES`, `SPIRAM_SPEED_120M`, `SPIRAM_MODE_OCT` options. see [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/flash_psram_config.html#all-supported-modes-and-speeds) for more details.

**Note: The PSRAM 120 MHz DDR is an experimental feature and it has temperature risks as below.**
  * Cannot guarantee normal functioning with a temperature higher than 65 degrees Celsius.
  * Temperature changes can also cause the crash of accessing to PSRAM/Flash, see [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/flash_psram_config.html#all-supported-modes-and-speeds) for more details.

#### How To Use

Note:

1. First delete existing `build`, `sdkconfig` , `sdkconfig.old`
```
rm -rf build sdkconfig sdkconfig.old
```

2. Use the command line to enable the relevant configuration
```
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.psram_octal_120m" reconfigure
```

3. Compile and burn
```
idf.py build flash monitor
```

**Note:** It is suggested to increase the LVGL task stack to over 30 KB

### Build and Flash

1. Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-iot-solution/issues)

We will get back to you as soon as possible.
