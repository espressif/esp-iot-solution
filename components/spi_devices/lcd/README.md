##TFT LCD Example Code for ESP32##

This example is based on [Adafruit’s Library](https://github.com/adafruit/Adafruit-GFX-Library.git) for ILI9341. This component can display<br />
1. Text<br />
2. Bitmap Images<br />
3. Shapes<br />
on the 320*240 TFT LCD<br />
Users can refer `ui_example.jpg` in the root folder of this example to learn more about the APIs & what they do.

LCD has Serial & Parallel Interfaces, esp-wrover-kit is designed for Serial SPI interfacing. However, LCD requires some additional pins except the normal SPI pins. 


> Tip: TFT LCDs work on a basic bitmap, where you should tell each pixel what to do. The library is able to display data by a concept of foreground and background.<br />For eg: If white text is required on a black background, screen is filled with black, the foreground color is set to white. Only the bitmap pixels which define the font are cleared, and the rest are kept as they are. This is how bitmaps, fonts and shapes are printed on the screen.


**Additional files required for the GUI** 

`components\lcd` folder<br />
1. `Adafruit-GFX-Library`: Adafruit-GFX-Library<br /> 
2. `Adafruit_lcd_fast_as.cpp`: Used for drawing pixels and some lines on the screen, and this subclass overrides most of the superclass methods in  `Adafruit_GFX.cpp`<br />
3. `Adafruit-GFX-Library/Font files`: It is the bitmap for various sizes of fonts (Prefer to only #include your desired fonts to save space)<br />
4. `spi_lcd.c` : It has some SPI structures & functions which use the spi_master driver for sending out the data as required by Adafruit Libraries.

> Note: To reduce the number of pins between the LCD & ESP32<br />
>  -  It is okay to leave out the MISO pin<br />
>  -  Short the backlight pin to always ON<br />
>  -  Reset pin can be shorted with the ESP32’s reset pin, but it might lead to unexpected behavior depending on the code.


There have been  multiple additions to the [Adafruit repository](https://github.com/adafruit/Adafruit_ILI9341) for the LCD, users can replace these files by new library if needed. Adafruit has made a good [documentation](https://cdn-learn.adafruit.com/downloads/pdf/adafruit-2-8-tft-touch-shield-v2.pdf) on TFT LCDs.

If you are willing to share your User Interface for ESP32, you can do so by posting on the forum [here](http://bbs.esp32.com/).