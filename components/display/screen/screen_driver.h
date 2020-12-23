// Copyright 2020 Espressif Systems (Shanghai) Co. Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _IOT_SCREEN_DRIVER_H_
#define _IOT_SCREEN_DRIVER_H_

#include "scr_iface_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * @brief Define all screen direction
 * 
 */
typedef enum {
    /* @---> X
       |
       Y
    */
    SCR_DIR_LRTB,   /**< From left to right then from top to bottom, this consider as the original direction of the screen */

    /*  Y
        |
        @---> X
    */
    SCR_DIR_LRBT,   /**< From left to right then from bottom to top */

    /* X <---@
             |
             Y
    */
    SCR_DIR_RLTB,   /**< From right to left then from top to bottom */

    /*       Y
             |
       X <---@
    */
    SCR_DIR_RLBT,   /**< From right to left then from bottom to top */

    /* @---> Y
       |
       X
    */
    SCR_DIR_TBLR,   /**< From top to bottom then from left to right */

    /*  X
        |
        @---> Y
    */
    SCR_DIR_BTLR,   /**< From bottom to top then from left to right */

    /* Y <---@
             |
             X
    */
    SCR_DIR_TBRL,   /**< From top to bottom then from right to left */

    /*       X
             |
       Y <---@
    */
    SCR_DIR_BTRL,   /**< From bottom to top then from right to left */

    SCR_DIR_MAX,

    /** Another way to represent rotation with 3 bit*/
    SCR_MIRROR_X = 0x40,
    SCR_MIRROR_Y = 0x20,
    SCR_SWAP_XY  = 0x80,
} scr_dir_t;

/**
 * @brief The types of colors that can be displayed on the screen
 * 
 */
typedef enum {
    SCR_COLOR_TYPE_MONO,
    SCR_COLOR_TYPE_GRAY,
    SCR_COLOR_TYPE_RGB565,
} scr_color_type_t;

/**
 * @brief All supported screen controllers
 * 
 */
typedef enum {
    /**< color screen */
    SCREEN_CONTROLLER_ILI9341,
    SCREEN_CONTROLLER_ILI9806,
    SCREEN_CONTROLLER_ILI9486,
    SCREEN_CONTROLLER_NT35510,
    SCREEN_CONTROLLER_RM68120,
    SCREEN_CONTROLLER_ST7789,
    SCREEN_CONTROLLER_ST7796,
    SCREEN_CONTROLLER_SSD1351,

    /**< monochrome screen */
    SCREEN_CONTROLLER_SSD1306,
    SCREEN_CONTROLLER_SSD1307,
    SCREEN_CONTROLLER_SSD1322,

} scr_controller_t;

/**
 * @brief configuration of screen controller
 * 
 */
typedef struct {
    scr_iface_driver_fun_t *iface_drv;   /*!< Interface driver for screen */
    int8_t pin_num_rst;                  /*!< Pin to hardreset LCD*/
    int8_t pin_num_bckl;                 /*!< Pin for adjusting Backlight- can use PWM/DAC too*/
    uint8_t rst_active_level;            /*!< Reset pin active level */
    uint8_t bckl_active_level;           /*!< Back-light active level */
    uint16_t width;                      /*!< Screen width */
    uint16_t height;                     /*!< Screen height */
    scr_dir_t rotate;                    /*!< Screen rotate direction */
} scr_controller_config_t;

/**
 * @brief Information of screen
 * 
 */
typedef struct {
    uint16_t width;                      /*!< Current screen width, it may change when apply to rotate */
    uint16_t height;                     /*!< Current screen height, it may change when apply to rotate */
    scr_dir_t dir;                       /*!< Current screen direction */
    scr_color_type_t color_type;         /*!< Color type of the screen, See scr_color_type_t struct */
    uint8_t bpp;                         /*!< Bits per pixel */
    const char *name;                    /*!< Name of the screen */
} scr_info_t;

/**
 * @brief Define a screen common function
 * 
 */
typedef struct {
    esp_err_t (*init)(const scr_controller_config_t *lcd_conf);                                     /*!< Initialize LCD screen */
    esp_err_t (*deinit)(void);                                                                      /*!< Deinitialize LCD screen */
    esp_err_t (*set_direction)(scr_dir_t dir);                                                      /*!< Control lcd scan direction */
    esp_err_t (*set_window)(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);                    /*!< Set a window area */
    esp_err_t (*write_ram_data)(uint16_t color);                                                    /*!< Write a GRAM data */
    esp_err_t (*draw_pixel)(uint16_t x, uint16_t y, uint16_t color);                                /*!< Draw a pixel on screen */
    esp_err_t (*draw_bitmap)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);     /*!< Draw a bitmap on screen */
    esp_err_t (*get_info)(scr_info_t *info);                                                        /*!< Get information of screen */
} scr_driver_fun_t;

/**
 * @brief Initialize a screen
 * 
 * @param controller Screen controller to initialize
 * @param lcd_conf configuration of screen, see scr_controller_config_t
 * @param out_screen Pointer to a screen driver
 * 
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is NULL.
 *      - ESP_FAIL Initialize failed
 *      - ESP_ERR_NOT_FOUND: Screen controller was not found.
 */
esp_err_t scr_init(scr_controller_t controller, const scr_controller_config_t *lcd_conf, scr_driver_fun_t *out_screen);

/**
 * @brief Deinitialize a screen
 * 
 * @param screen screen driver to deinitialize
 * 
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is NULL.
 *      - ESP_FAIL Deinitialize failed
 */
esp_err_t scr_deinit(const scr_driver_fun_t *screen);

#ifdef __cplusplus
}
#endif

#endif
