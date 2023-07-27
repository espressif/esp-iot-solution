## USB Stream Lcd Display Example

This routine demonstrates how to use the [usb_stream](https://components.espressif.com/components/espressif/usb_stream) component to get an image from a USB camera and display it dynamically on the RGB screen. 

* Pressing the boot button can switch the display resolution. 
* For better performance, please use ESP-IDF release/v5.0 or above versions.
* Currently only images with a resolution smaller than the screen resolution are displayed
* When the image width is equal to the screen width, the refresh rate is at its highest.

## Hardware

* An [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) development board with 800\*480/480\*480 LCD.
* A USB camera (Can be connected to USB port or Expansion Connector)
* An USB Type-C cable for Power supply and programming (Please connect to UART port instead of USB port)

Note:
  Error message with `A fatal error occurred: Could not open /dev/ttyACM0, the port doesn't exist`: Please first make sure development board connected, then make board into "Download Boot" mode to upload by following steps:
  1. keep pressing  "BOOT(SW2)" button
  2. short press "RST(SW1)" button
  3. release "BOOT(SW2)".
  4. upload program and reset

## Performance

* Displaying 800*480 images on ESP32-S3-LCD-EV-Board and 800*480 screen can reach `16FPS`
* Displaying 480*320 images on ESP32-S3-LCD-Ev-Board and 480*480 screen can reach `29FPS`

## IDF Patch

The patch is intended to achieve best performance of RGB LCD by using **Octal PSRAM 120MHz** feature. Now there are two versions of patch under the patch folder that are used for the respective branch of ESP-IDF (master or release/v5.0). 

**Please make sure your IDF project is clean** (use `git status` to check), then the patch can be applied by following commands:

```
cd <root directory of IDF>
git apply <path of the patch>/idf_psram_120m.patch # Nothing return if success
git status      # Check whether the operation is successful, the output should look like below:

HEAD detached at f315986401
Changes not staged for commit:
  (use "git add <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
        modified:   components/esp_hw_support/port/esp32s3/rtc_clk.c
        modified:   components/esp_psram/esp32s3/Kconfig.spiram
        modified:   components/hal/esp32s3/include/hal/spimem_flash_ll.h
        modified:   components/spi_flash/esp32s3/mspi_timing_tuning_configs.h
        modified:   components/spi_flash/esp32s3/spi_timing_config.c
        modified:   components/spi_flash/esp32s3/spi_timing_config.h
        modified:   components/spi_flash/spi_flash_timing_tuning.c

Untracked files:
  (use "git add <file>..." to include in what will be committed)
        tools/test_apps/system/flash_psram/sdkconfig.ci.f8r8_120ddr
        tools/test_apps/system/flash_psram/sdkconfig.ci.f8r8_120ddr_120ddr
```

### How To Use 
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