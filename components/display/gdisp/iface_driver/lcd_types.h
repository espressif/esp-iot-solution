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
#ifndef _IOT_LCD_TYPES_H_
#define _IOT_LCD_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/i2s.h"
#include "driver/i2c.h"

// Color definitions, RGB565 format          R ,  G ,  B
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


typedef enum {
    LCD_DIR_LRTB,   /**< From left to right then from top to bottom */
    LCD_DIR_LRBT,   /**< From left to right then from bottom to top */
    LCD_DIR_RLTB,   /**< From right to left then from top to bottom */
    LCD_DIR_RLBT,   /**< From right to left then from bottom to top */
    LCD_DIR_TBLR,   /**< From top to bottom then from left to right */
    LCD_DIR_BTLR,   /**< From bottom to top then from left to right */
    LCD_DIR_TBRL,   /**< From top to bottom then from right to left */
    LCD_DIR_BTRL,   /**< From bottom to top then from right to left */
    LCD_DIR_MAX,
    /** Another way to represent rotation with 3 bit*/
    LCD_MIRROR_X = 0x40,
    LCD_MIRROR_Y = 0x20,
    LCD_SWAP_XY  = 0x80,
} lcd_dir_t;

typedef enum {
    LCD_COLOR_TYPE_MONO,
    LCD_COLOR_TYPE_GRAY,
    LCD_COLOR_TYPE_65K,
} lcd_color_type_t;

typedef enum {
    LCD_IFACE_I2C,
    LCD_IFACE_I2S,
    LCD_IFACE_SPI,
} lcd_iface_type_t;

typedef struct {
    union {
        struct {
            int8_t pin_num_sda;         /*!< IIC SDA pin*/
            int8_t pin_num_scl;         /*!< IIC SCL pin*/
            int clk_freq;               /*!< IIC clock frequency */
            i2c_port_t i2c_port;        /*!< IIC port number */
            uint8_t i2c_addr;           /*!< Screen IIC slave adddress */
        } iface_i2c;

        struct {
            int8_t pin_num_miso;         /*!< MasterIn, SlaveOut pin */
            int8_t pin_num_mosi;         /*!< MasterOut, SlaveIn pin */
            int8_t pin_num_clk;          /*!< SPI Clock pin*/
            int8_t pin_num_cs;           /*!< SPI Chip Select Pin*/
            int8_t pin_num_dc;           /*!< Pin to select Data or Command for LCD */
            int clk_freq;                /*!< SPI clock frequency */
            spi_host_device_t spi_host;  /*!< SPI host index */
            int dma_chan;                /*!< DMA channel used */
            bool init_spi_bus;           /*!< Whether to initialize SPI host */
        } iface_spi;

        struct {
            int8_t data_width;           /*!< Parallel data width, 16bit or 8bit available */
            int8_t pin_data_num[16];     /*!< Parallel data output IO*/
            int8_t pin_num_wr;           /*!< Write clk io*/
            int8_t pin_num_rd;           /*!< Read clk io (UNUSED NOW) */
            int8_t pin_num_rs;           /*!< Rs io num */
            int clk_freq;                /*!< I2s clock frequency */
            i2s_port_t i2s_port;         /*!< I2S port number */
        } iface_8080;
    };

    lcd_iface_type_t iface_type; /*!< Type of screen interface */
    int8_t pin_num_rst;          /*!< Pin to hardreset LCD*/
    int8_t pin_num_bckl;         /*!< Pin for adjusting Backlight- can use PWM/DAC too*/
    uint8_t rst_active_level;    /*!< Reset pin active level */
    uint8_t bckl_active_level;   /*!< Back-light active level */
    uint16_t width;              /*!< Screen width */
    uint16_t height;             /*!< Screen height */
    lcd_dir_t rotate;            /*!< Screen rotate direction */

} lcd_config_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    lcd_dir_t dir;
    lcd_color_type_t color_type;
    const char *name;
} lcd_info_t;



typedef struct {
    esp_err_t (*init)(const lcd_config_t *lcd_conf);                                            /*!< initialize LCD screen */
    esp_err_t (*deinit)(void);                                                            /*!< deinitialize LCD screen */
    esp_err_t (*set_direction)(lcd_dir_t dir);                                            /*!< control lcd scan direction */
    esp_err_t (*set_window)(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);          /*!<  */
    esp_err_t (*write_ram_data)(uint16_t color);                                                         /*!<  */
    esp_err_t (*draw_pixel)(uint16_t x, uint16_t y, uint16_t color);                                     /*!<  */
    esp_err_t (*draw_bitmap)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
    esp_err_t (*get_info)(lcd_info_t *info);
} lcd_driver_fun_t;


#ifdef __cplusplus
}
#endif

#endif
