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

#ifndef _IOT_TOUCH_PANEL_H
#define _IOT_TOUCH_PANEL_H

#include "esp_log.h"
#include "screen_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * max point number on touch panel
 */
#define TOUCH_MAX_POINT_NUMBER (5)

/**
 * @brief Touch events
 * 
 */
typedef enum {
    TOUCH_EVT_RELEASE = 0x0,  /*!< Release event */
    TOUCH_EVT_PRESS   = 0x1,  /*!< Press event */
} touch_panel_event_t;

/**
 * @brief Information of touch panel
 * 
 */
typedef struct {
    touch_panel_event_t event;   /*!< Event of touch */
    uint8_t point_num;           /*!< Touch point number */
    uint16_t curx[TOUCH_MAX_POINT_NUMBER];            /*!< Current x coordinate */
    uint16_t cury[TOUCH_MAX_POINT_NUMBER];            /*!< Current y coordinate */
} touch_panel_points_t;

/**
 * @brief Define all screen direction
 *
 */
typedef enum {
    /* @---> X
       |
       Y
    */
    TOUCH_DIR_LRTB,   /**< From left to right then from top to bottom, this consider as the original direction of the touch panel*/

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

    /* Another way to represent rotation with 3 bit*/
    TOUCH_MIRROR_X = 0x40, /**< Mirror X-axis */
    TOUCH_MIRROR_Y = 0x20, /**< Mirror Y-axis */
    TOUCH_SWAP_XY  = 0x80, /**< Swap XY axis */
} touch_panel_dir_t;

typedef enum {
    TOUCH_PANEL_IFACE_I2C,            /*!< I2C interface */
    TOUCH_PANEL_IFACE_SPI,            /*!< SPI interface */
} touch_panel_interface_type_t;

/**
 * @brief All supported touch panel controllers
 *
 */
typedef enum {
    /* Capacitive touch panel */
    TOUCH_PANEL_CONTROLLER_FT5X06,

    /* Resistance touch panel */
    TOUCH_PANEL_CONTROLLER_XPT2046,
    TOUCH_PANEL_CONTROLLER_NS2016,
} touch_panel_controller_t;

/**
 * @brief Configuration of touch panel
 * 
 */
typedef struct {
    /** Interface configuration */
    union {
        /** I2c interface */
        struct {
            i2c_bus_handle_t i2c_bus;    /*!< Handle of i2c bus */
            int clk_freq;                /*!< i2c clock frequency */
            uint8_t i2c_addr;            /*!< screen i2c slave adddress */
        } interface_i2c;

        /** SPI interface */
        struct {
            spi_bus_handle_t spi_bus;    /*!< Handle of spi bus */
            int8_t pin_num_cs;           /*!< SPI Chip Select Pin */
            int clk_freq;                /*!< spi clock frequency */
        } interface_spi;
    };

    touch_panel_interface_type_t interface_type;   /*!< Interface bus type, see touch_interface_type_t struct */
    int8_t pin_num_int;                            /*!< Interrupt pin of touch panel. NOTE: Now this line is not used, you can set to -1 and no connection with hardware */
    touch_panel_dir_t direction;                   /*!< Rotate direction */
    uint16_t width;                                /*!< touch panel width */
    uint16_t height;                               /*!< touch panel height */
} touch_panel_config_t;

/**
 * @brief Define screen common function
 * 
 */
typedef struct {
    /**
    * @brief Initial touch panel
    * @attention If you have been called function touch_panel_init() that will call this function automatically, and should not be called it again.
    *
    * @param config Pointer to a structure with touch config arguments.
    *
    * @return
    *     - ESP_OK Success
    *     - ESP_FAIL Fail
    */
    esp_err_t (*init)(const touch_panel_config_t *config);

    /**
    * @brief Deinitial touch panel
    *
    * @return
    *     - ESP_OK Success
    *     - ESP_FAIL Fail
    */
    esp_err_t (*deinit)(void);

    /**
    * @brief Start run touch panel calibration
    *
    * @param screen Screen driver for display prompts
    * @param recalibrate Is mandatory, set true to force calibrate
    *
    * @return
    *     - ESP_OK Success
    *     - ESP_FAIL Fail
    */
    esp_err_t (*calibration_run)(const scr_driver_t *screen, bool recalibrate);

    /**
    * @brief Set touch rotate rotation
    *
    * @param dir rotate direction
    *
    * @return
    *     - ESP_OK Success
    *     - ESP_FAIL Fail
    */
    esp_err_t (*set_direction)(touch_panel_dir_t dir);

    /**
    * @brief Get current touch information, see struct touch_panel_points_t
    *
    * @param point a pointer of touch_panel_points_t contained touch information.
    *
    * @return
    *     - ESP_OK Success
    *     - ESP_FAIL Fail
    */
    esp_err_t (*read_point_data)(touch_panel_points_t *point);
} touch_panel_driver_t;

/**
 * @brief Find a touch panel controller driver
 * 
 * @param controller Touch panel controller to initialize
 * @param out_driver Pointer to a touch driver
 * 
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is NULL.
 *      - ESP_ERR_NOT_FOUND: Touch panel controller was not found.
 */
esp_err_t touch_panel_find_driver(touch_panel_controller_t controller, touch_panel_driver_t *out_driver);

#ifdef __cplusplus
}
#endif

#endif
