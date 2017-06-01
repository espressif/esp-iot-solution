#TFT LCD Example Code for ESP32

This example is based on [Adafruit’s Library](https://github.com/adafruit/Adafruit_ILI9341) for ILI9341. This code/library can display<br />
1. Text<br />
2. Bitmap Images<br />
3. Shapes<br />
on the 320*240 TFT LCD<br />
Users can refer `ui_example.jpg` in the root folder of this example to learn more about the functions & what they do.

ILI9341 has Serial & Parallel Interfaces, esp-wrover-kit is designed for Serial SPI interfacing. However, ILI9341 requires some additional pins except the normal SPI pins. 

The pins are defined in the Macros of the `components/lcd/spi_ili.c` file. Users can change them if using a board other than esp-wrover-kit. The refresh rate measured for the screen with the code is around : Hz


> Tip: TFT LCDs work on a basic bitmap, where you should tell each pixel what to do. The library is able to display data by a concept of foreground and background.<br />For eg: If white text is required on a black background, screen is filled with black, the foreground color is set to white, & only the bitmap pixels which define the font are cleared, and the rest are kept as they are. 


**Additional files required for the GUI** 

`components\lcd` folder<br />
1) `Adafruit_GFX_AS.cpp`: Used for drawing shapes & characters on the screen<br /> 
2) `Adafruit_ILI9341_fast_as.cpp`: Used for drawing pixels and some lines on the screen<br />
3) `Font files`: It is the bitmap for various sizes of fonts (Prefer to only #include your desired fonts to save space)<br />
4) `spi_ili.c` : It has some SPI structures & functions which use the spi_master driver for sending out the data as required by Adafruit Libraries

> Note: To reduce the number of Pins taken by the LCD<br />
>  -  It is okay to leave out the MISO pin<br />
>  -  Short the Backlight pin to always ON<br />
>  -  Reset pin can be shorted with the ESP32’s reset pin, but it might lead to unexpected behavior depending on the code.


There have been  multiple additions to the [Adafruit repository](https://github.com/adafruit/Adafruit_ILI9341) for the LCD, users can replace these files by new library if needed. Adafruit has made a good [documentation](https://cdn-learn.adafruit.com/downloads/pdf/adafruit-2-8-tft-touch-shield-v2.pdf) on TFT LCDs.

If you are willing to share your User Interface for ESP32, you can do so by posting on the forum [here](http://bbs.esp32.com/).