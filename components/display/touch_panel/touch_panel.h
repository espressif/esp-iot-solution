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

#ifndef _IOT_LCD_TOUCH_H
#define _IOT_LCD_TOUCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "screen_driver.h"

typedef enum {
    TOUCH_EVT_RELEASE = 0x0,  /*!< Release event */
    TOUCH_EVT_PRESS   = 0x1,  /*!< Press event */
} touch_evt_t;

typedef struct {
    touch_evt_t event;   /*!< Event of touch */
    uint8_t point_num;   /*!< Touch point number */
    uint16_t curx[5];    /*!< Current x coordinate */
    uint16_t cury[5];    /*!< Current y coordinate */
} touch_info_t;

/**
 * @brief Define all screen direction
 *
 */
typedef enum {
    /* @---> X
       |
       Y
    */
    TOUCH_DIR_LRTB,   /**< From left to right then from top to bottom */

    /*  Y
        |
        @---> X
    */
    TOUCH_DIR_LRBT,   /**< From left to right then from bottom to top */

    /* X <---@
             |
             Y
    */
    TOUCH_DIR_RLTB,   /**< From right to left then from top to bottom */

    /*       Y
             |
       X <---@
    */
    TOUCH_DIR_RLBT,   /**< From right to left then from bottom to top */

    /* @---> Y
       |
       X
    */
    TOUCH_DIR_TBLR,   /**< From top to bottom then from left to right */

    /*  X
        |
        @---> Y
    */
    TOUCH_DIR_BTLR,   /**< From bottom to top then from left to right */

    /* Y <---@
             |
             X
    */
    TOUCH_DIR_TBRL,   /**< From top to bottom then from right to left */

    /*       X
             |
       Y <---@
    */
    TOUCH_DIR_BTRL,   /**< From bottom to top then from right to left */

    TOUCH_DIR_MAX,

    /** Another way to represent rotation with 3 bit*/
    TOUCH_MIRROR_X = 0x40,
    TOUCH_MIRROR_Y = 0x20,
    TOUCH_SWAP_XY  = 0x80,
} touch_dir_t;

typedef enum {
    TOUCH_IFACE_I2C,            /*!< I2C interface */
    TOUCH_IFACE_SPI,            /*!< SPI interface */
} touch_iface_type_t;

/**
 * @brief All supported touch panel controllers
 *
 */
typedef enum {
    /**< Capacitive touch panel */
    TOUCH_CONTROLLER_FT5X06,

    /**< Resistance touch panel */
    TOUCH_CONTROLLER_XPT2046,
    TOUCH_CONTROLLER_NS2016,
} touch_controller_t;

typedef struct {
    union {
        struct {
            int8_t pin_num_sda;          /*!< i2c SDA pin */
            int8_t pin_num_scl;          /*!< i2c SCL pin */
            int clk_freq;                /*!< i2c clock frequency */
            i2c_port_t i2c_port;         /*!< i2c port */
            uint8_t i2c_addr;            /*!< screen i2c slave adddress */
        } iface_i2c;

        struct {
            int8_t pin_num_miso;         /*!< MasterIn, SlaveOut pin */
            int8_t pin_num_mosi;         /*!< MasterOut, SlaveIn pin */
            int8_t pin_num_clk;          /*!< SPI Clock pin */
            int8_t pin_num_cs;           /*!< SPI Chip Select Pin */
            int clk_freq;                /*!< spi clock frequency */
            spi_host_device_t spi_host;  /*!< spi host index */
            int dma_chan;                /*!< DMA channel used */
            bool init_spi_bus;           /*!< Whether to initialize SPI bus */
        } iface_spi;
    };

    touch_iface_type_t iface_type;   /*!< Interface bus type, see touch_iface_type_t struct */
    int8_t pin_num_int;              /*!< Interrupt pin of touch panel */
    touch_dir_t direction;           /*!< Rotate direction */
    uint16_t width;                  /*!< touch panel width */
    uint16_t height;                 /*!< touch panel height */
} touch_panel_config_t;

typedef struct {
    esp_err_t (*init)(const touch_panel_config_t *config);
    esp_err_t (*deinit)(bool free_bus);
    esp_err_t (*calibration_run)(const scr_driver_fun_t *screen, bool recalibrate);
    esp_err_t (*set_direction)(touch_dir_t dir);
    int (*is_pressed)(void);
    esp_err_t (*read_info)(touch_info_t *info);
} touch_driver_fun_t;


esp_err_t touch_panel_init(touch_controller_t controller, const touch_panel_config_t *conf, touch_driver_fun_t *driver);

esp_err_t touch_panel_deinit(const touch_driver_fun_t *touch);

#ifdef __cplusplus
}
#endif

#endif
