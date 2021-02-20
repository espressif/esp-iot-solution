# Screen Example

In this example, we configure a screen with spi or 8080 interface and then:
* Set to landscape mode,
* Draw a picture,
* Run speed test,
* Calculate and draw Zoomed Mandelbrot Set.

## How to Use Example

### Hardware Required

* A ESP32-LCDKit development board with ESP32-DevKitC
* A suitable LCD module to plug in ESP32-LCDKit
* A USB cable for power supply and programming

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

Reset your development board. The application will run the speed test after the logo is displayed, and then display the Mandelbrot set repeatedly. The following is the output log:

```
I (384) spi_bus: SPI2 bus created
I (388) Board: Board Info: ESP32-LCDKit
I (392) Board: Board Init Done ...
I (397) spi_bus: SPI2 bus device added, CS=5 Mode=0 Speed=20000000
I (904) lcd ili9341: MADCTL=e8
I (904) screen example: Screen name:ILI9341 | width:320 | height:240
I (18799) screen example: -------ILI9341 Test Result------
I (18799) screen example: @resolution 240x240          [time per frame=54.47MS, fps=18.36, speed=2.02MB/s]
I (18805) screen example: @resolution 320x240 infer to [time per frame=72.63MS, fps=13.77]
I (18813) screen example: -------------------------------------
I (20820) screen example: zoom = 50
I (20820) screen example: start(-3.820000, -1.980000)
I (21088) screen example: end  (2.560000, 2.800000)
I (21090) screen example: zoom = 55
I (21090) screen example: start(-3.527273, -1.763636)
I (21378) screen example: end  (2.272727, 2.581818)
I (21379) screen example: zoom = 60
I (21379) screen example: start(-3.283333, -1.583333)
I (21684) screen example: end  (2.033333, 2.400000)
I (21686) screen example: zoom = 66
I (21686) screen example: start(-3.045455, -1.393939)
I (22013) screen example: end  (1.787879, 2.227273)
I (22014) screen example: zoom = 73
I (22014) screen example: start(-2.808219, -1.219178)
I (22370) screen example: end  (1.561644, 2.054795)
```

## Troubleshooting

* Program upload failure

    * Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
    * The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.

* Nothing on screen

    * Check the hardware connection between esp32 and screen
    * Check you used screen controller is corresponding the configuration, otherwise, change the configuration

* The image is mirrored

    * Change the rotation configuration to suit the orientation of your screen

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
