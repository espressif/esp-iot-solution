// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _IOT_LCD_H_
#define _IOT_LCD_H_
/*This is the Adafruit subclass graphics file*/
#define PROGMEM

#include "string.h"
#include "stdio.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_partition.h"
#include "freertos/semphr.h"

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

#define MAKEWORD(b1, b2, b3, b4) ((uint32_t) ((b1) | ((b2) << 8) | ((b3) << 16) | ((b4) << 24)))


typedef enum {
    LCD_MOD_ILI9341 = 0,
    LCD_MOD_ST7789 = 1,
    LCD_MOD_AUTO_DET = 3,
} lcd_model_t;

/**
 * @brief struct to map GPIO to LCD pins
 */
typedef struct {
    lcd_model_t lcd_model;
    int8_t pin_num_miso;        /*!<MasterIn, SlaveOut pin*/
    int8_t pin_num_mosi;        /*!<MasterOut, SlaveIn pin*/
    int8_t pin_num_clk;         /*!<SPI Clock pin*/
    int8_t pin_num_cs;          /*!<SPI Chip Select Pin*/
    int8_t pin_num_dc;          /*!<Pin to select Data or Command for LCD*/
    int8_t pin_num_rst;         /*!<Pin to hardreset LCD*/
    int8_t pin_num_bckl;        /*!<Pin for adjusting Backlight- can use PWM/DAC too*/
    int clk_freq;                /*!< spi clock frequency */
    uint8_t rst_active_level;    /*!< reset pin active level */
    uint8_t bckl_active_level;   /*!< back-light active level */
    spi_host_device_t spi_host;  /*!< spi host index*/
    bool init_spi_bus;
} lcd_conf_t;

/**
 * @brief struct holding LCD IDs
 */
typedef struct {
    uint8_t mfg_id;         /*!<Manufacturer's ID*/
    uint8_t lcd_driver_id;  /*!<LCD driver Version ID*/
    uint8_t lcd_id;         /*!<LCD Unique ID*/
    uint32_t id;
} lcd_id_t;

typedef struct {
    uint8_t dc_io;
    uint8_t dc_level;
} lcd_dc_t;

#ifdef __cplusplus
#include "Adafruit_GFX.h"

class CEspLcd: public Adafruit_GFX
{
private:
    spi_device_handle_t spi_wr = NULL;
    uint8_t tabcolor;
    bool dma_mode;
    int dma_buf_size;
    uint8_t m_dma_chan;
    uint16_t m_height;
    uint16_t m_width;
    SemaphoreHandle_t spi_mux;
    gpio_num_t cmd_io = GPIO_NUM_MAX;
    lcd_dc_t dc;
//protected:
public:
    /*Below are the functions which actually send data, defined in spi_ili.c*/
    void transmitCmdData(uint8_t cmd, const uint8_t data, uint8_t numDataByte);
    void transmitData(uint16_t data);
    void transmitData(uint8_t data);
    void transmitCmdData(uint8_t cmd, uint32_t data);
    void transmitData(uint16_t data, int32_t repeats);
    void transmitData(uint8_t* data, int length);
    void transmitCmd(uint8_t cmd);
    void _fastSendBuf(const uint16_t* buf, int point_num, bool swap = true);
    void _fastSendRep(uint16_t val, int rep_num);
//public:
    lcd_id_t id;
    CEspLcd(lcd_conf_t* lcd_conf, int height = LCD_TFTHEIGHT, int width = LCD_TFTWIDTH, bool dma_en = true, int dma_word_size = 1024, int dma_chan = 1);
    virtual ~CEspLcd();
    /**
     * @brief init spi bus and lcd screen
     * @param lcd_conf LCD parameters
     */
    void setSpiBus(lcd_conf_t *lcd_conf);

    void acquireBus();
    void releaseBus();

    /**
     * @brief get LCD ID
     */
    uint32_t getLcdId();

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
     * @brief Load bitmap data from flash partition and fill the pixels on LCD screen
     * @param x Start position
     * @param y Start position
     * @param w width of image in bmp array
     * @param h height of image in bmp array
     * @param data_partition Flash storage that contains the bitmap data array.
     * @param data_offset bitmap array begin offset
     * @param malloc_pixal_size internal buffer size that driver would allocate.
     * @param swap_bytes_en Whether to enable byte swap for each pixel word
     *
     * @return
     *     - ESP_FAIL if partition is NULL
     *     - ESP_OK on success
     */
    esp_err_t drawBitmapFromFlashPartition(int16_t x, int16_t y, int16_t w, int16_t h, esp_partition_t* data_partition,
            int data_offset = 0, int malloc_pixal_size = 1024, bool swap_bytes_en = true);
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
    inline void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

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

    int write_char(uint8_t c);

    int drawString(const char *string, uint16_t x, uint16_t y);

    int drawNumber(int long_num, uint16_t poX, uint16_t poY);

    int drawFloat(float floatNumber, uint8_t decimal, uint16_t poX, uint16_t poY);

    size_t printf(const char *format, ...);
};

#endif

#endif
