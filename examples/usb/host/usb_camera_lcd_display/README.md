## USB Stream Lcd Display Example

This routine demonstrates how to use the [usb_stream](https://components.espressif.com/components/espressif/usb_stream) component to get an image from a USB camera and display it dynamically on the RGB screen. 

* Pressing the boot button can switch the display resolution. 
* For better performance, please use ESP-IDF release/v5.0 or above versions.
* Currently only images with a resolution smaller than the screen resolution are displayed
* When the image width is equal to the screen width, the refresh rate is at its highest.

## Try the example through the esp-launchpad

<a href="https://espressif.github.io/esp-launchpad/?flashConfigURL=https://raw.githubusercontent.com/espressif/esp-dev-kits/master/launch.toml">
    <img alt="Try it with ESP Launchpad" src="https://espressif.github.io/esp-launchpad/assets/try_with_launchpad.png" width="200" height="56">
</a>

This example has been built for `ESP32-S3-LCD-EV-Board`, you can use `esp-launchpad` to flash the binary to your board directly.

Please choose `lcd_ev_subboard3_800_480_usb_camera` or `lcd_ev_subboard2_480_480_usb_camera`, then click flash to quickly start

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

## PSRAM 120M DDR

The PSRAM 120M DDR feature is intended to achieve the best performance of RGB LCD. It is only available with ESP-IDF **release/v5.1** and above. It can be used by enabling the `IDF_EXPERIMENTAL_FEATURES`, `SPIRAM_SPEED_120M`, `SPIRAM_MODE_OCT` options. see [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/flash_psram_config.html#all-supported-modes-and-speeds) for more details.

**Note: The PSRAM 120 MHz DDR is an experimental feature and it has temperature risks as below.**
  * Cannot guarantee normal functioning with a temperature higher than 65 degrees Celsius.
  * Temperature changes can also cause the crash of accessing to PSRAM/Flash, see [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/flash_psram_config.html#all-supported-modes-and-speeds) for more details.

### Performance

* Displaying 800*480 images on ESP32-S3-LCD-EV-Board with psram 120M and 800*480 screen can reach `16FPS`
* Displaying 480*320 images on ESP32-S3-LCD-Ev-Board with psram 120M and 480*480 screen can reach `29FPS`

### How To Use 

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