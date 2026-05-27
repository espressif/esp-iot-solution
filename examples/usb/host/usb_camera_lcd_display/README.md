| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

## USB Stream Lcd Display Example

This routine demonstrates how to get an image from a USB camera and display it dynamically on the RGB screen. 

* Pressing the boot button can switch the display resolution. 
* The example starts with the same resolution as the LCD first if the camera exposes it. If not, it uses the current camera resolution order.
* Images smaller than the LCD are displayed in the center of the screen.
* Images equal to the LCD resolution are decoded directly into the LCD frame buffer for the best refresh rate.
* Images larger than the LCD are scaled down to fit inside the screen by the JPEG decoder.
* The on-screen overlay shows the camera FPS and display resolution.

## Hardware

* An [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) development board with 800\*480/480\*480 LCD.
* A USB camera (Can be connected to USB port or Expansion Connector)
* An USB Type-C cable for Power supply and programming (Please connect to UART port instead of USB port)

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