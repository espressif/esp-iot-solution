#ifndef _ADAFRUIT_LCDH_
#define _ADAFRUIT_LCDH_
/*This is the Adafruit subclass graphics file*/

#include "string.h"
#include "stdio.h"
#include "Adafruit_GFX_AS.h"
#include "spi_lcd.h"

#define LCD_TFTWIDTH  240
#define LCD_TFTHEIGHT 320

#define LCD_INVOFF    0x20
#define LCD_INVON     0x21

#define LCD_CASET   0x2A
#define LCD_PASET   0x2B
#define LCD_RAMWR   0x2C
#define LCD_MADCTL  0x36

// Color definitions
#define COLOR_BLACK       0x0000      /*   0,   0,   0 */
#define COLOR_NAVY        0x000F      /*   0,   0, 128 */
#define COLOR_DARKGREEN   0x03E0      /*   0, 128,   0 */
#define COLOR_DARKCYAN    0x03EF      /*   0, 128, 128 */
#define COLOR_MAROON      0x7800      /* 128,   0,   0 */
#define COLOR_PURPLE      0x780F      /* 128,   0, 128 */
#define COLOR_OLIVE       0x7BE0      /* 128, 128,   0 */
#define COLOR_LIGHTGREY   0xC618      /* 192, 192, 192 */
#define COLOR_DARKGREY    0x7BEF      /* 128, 128, 128 */
#define COLOR_BLUE        0x001F      /*   0,   0, 255 */
#define COLOR_GREEN       0x07E0      /*   0, 255,   0 */
#define COLOR_CYAN        0x07FF      /*   0, 255, 255 */
#define COLOR_RED         0xF800      /* 255,   0,   0 */
#define COLOR_MAGENTA     0xF81F      /* 255,   0, 255 */
#define COLOR_YELLOW      0xFFE0      /* 255, 255,   0 */
#define COLOR_WHITE       0xFFFF      /* 255, 255, 255 */
#define COLOR_ORANGE      0xFD20      /* 255, 165,   0 */
#define COLOR_GREENYELLOW 0xAFE5      /* 173, 255,  47 */
#define COLOR_PINK        0xF81F
#define COLOR_SILVER      0xC618
#define COLOR_GRAY        0x8410
#define COLOR_LIME        0x07E0
#define COLOR_TEAL        0x0410
#define COLOR_FUCHSIA     0xF81F
#define COLOR_ESP_BKGD    0xD185

#define MAKEWORD(b1, b2, b3, b4) (uint32_t(b1) | ((b2) << 8) | ((b3) << 16) | ((b4) << 24))

class Adafruit_lcd: public Adafruit_GFX_AS
{
private:

    spi_device_handle_t spi;
    uint8_t tabcolor;
    bool dma_mode;
    int dma_buf_size;

    /*Below are the functions which actually send data, defined in spi_ili.c*/
    void transmitCmdData(uint8_t cmd, const uint8_t data, uint8_t numDataByte);
    inline void transmitData(uint16_t data)
    {
        lcd_data(spi, (uint8_t *)&data, 2);
    }
    inline void transmitCmdData(uint8_t cmd, uint32_t data)
    {
        lcd_cmd(spi, cmd);
        lcd_data(spi, (uint8_t *)&data, 4);
    }
    inline void transmitData(uint16_t data, int32_t repeats)
    {
        lcd_send_uint16_r(spi, data, repeats);
    }
    inline void transmitData(uint8_t* data, int length)
    {
        lcd_data(spi, (uint8_t *)data, length);
    }
    inline void transmitCmd(uint8_t cmd)
    {
        lcd_cmd(spi, cmd);
    }
    void _fastSendBuf(const uint16_t* buf, int point_num, bool swap = true);
    void _fastSendRep(uint16_t val, int rep_num);
public:
    Adafruit_lcd(spi_device_handle_t spi_t, bool dma_en = true, int dma_word_size = 1024);

    void setSpiBus(spi_device_handle_t spi_dev);

    /**
     * @brief fill screen background with color
     * @param color Color to be filled
     */
    void fillScreen(uint16_t color);

    /**
     * @brief fill one of the 320*240 pixels: hero function of the library
     * @param x x co-ordinate of set orientation
     * @param y y co-ordinate of set orientation
     * @param color New color of the pixel
     */
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    
    /**
     * @brief Print an array of pixels: Used to display pictures usually
     * @param x position X
     * @param y position Y
     * @param bitmap pointer to bmp array
     * @param w width of image in bmp array
     * @param h height of image in bmp array
     */
    void drawBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h);

    /**
     * @brief Avoid using it, Internal use for main class drawChar API
     */
    void drawBitmapFont(int16_t x, int16_t y, uint8_t w, uint8_t h, const uint16_t *bitmap);


    /**
     * @brief Draw a Vertical line
     * @param x & y co-ordinates of start point
     * @param h length of line
     * @param color of the line
     */
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);

    /**
     * @brief Draw a Horizontal line
     * @param x & y co-ordinates of start point
     * @param w length of line
     * @param color of the line
     */
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);

    /**
     * @brief Draw a filled rectangle
     * @param x & y co-ordinates of start point
     * @param w & h of rectangle to be displayed
     * @param object color
     */
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    /**
     * @brief Draw a filled rectangle
     * @param r rotation between 0 to 3, landscape/portrait
     */
    void setRotation(uint8_t r);

    /*Yet to figure out what this does*/
    void invertDisplay(bool i);

    /*Not useful for user, sets the Region of Interest window*/
    inline void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
    {
        transmitCmdData(LCD_CASET, MAKEWORD(x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF));
        transmitCmdData(LCD_PASET, MAKEWORD(y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF));
        transmitCmd(LCD_RAMWR); // write to RAM
    }

    /**
     * @brief Scroll on Y-axis
     * @param y scroll by y num of pixels
     */
    void scrollTo(uint16_t y);

    /**
     * @brief pass 8-bit colors, get 16bit packed number
     */
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief write 7-segment float
     */
    int drawFloatSevSeg(float floatNumber, uint8_t decimal, uint16_t poX, uint16_t poY, uint8_t size);

    /**
     * @brief write 7-segment unicode
     */
    int drawUnicodeSevSeg(uint16_t uniCode, uint16_t x, uint16_t y, uint8_t size);

    /**
     * @brief write 7-segment string
     */
    int drawStringSevSeg(const char *string, uint16_t poX, uint16_t poY, uint8_t size);

    /**
     * @brief write 7-segment number
     */
    int drawNumberSevSeg(int long_num, uint16_t poX, uint16_t poY, uint8_t size);
};

#endif
