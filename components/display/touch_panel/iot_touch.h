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
#include "iot_lcd.h"

typedef enum {
    TOUCH_EVT_RELEASE = 0x0,
    TOUCH_EVT_PRESS   = 0x1,
} touch_evt_t;

typedef struct {
    touch_evt_t event;
    uint8_t point_num;
    uint16_t curx[5];
    uint16_t cury[5];
} touch_info_t;

typedef enum {
    TOUCH_DIR_LRTB,   /**< From left to right then from top to bottom */
    TOUCH_DIR_LRBT,   /**< From left to right then from bottom to top */
    TOUCH_DIR_RLTB,   /**< From right to left then from top to bottom */
    TOUCH_DIR_RLBT,   /**< From right to left then from bottom to top */
    TOUCH_DIR_TBLR,   /**< From top to bottom then from left to right */
    TOUCH_DIR_BTLR,   /**< From bottom to top then from left to right */
    TOUCH_DIR_TBRL,   /**< From top to bottom then from right to left */
    TOUCH_DIR_BTRL,   /**< From bottom to top then from right to left */
}touch_dir_t;

typedef struct {
    struct {
        int8_t pin_num_sda;          /*!< i2c SDA pin */
        int8_t pin_num_scl;          /*!< i2c SCL pin */
        int clk_freq;                /*!< i2c clock frequency */
        i2c_port_t i2c_port;         /*!< i2c port */
        uint8_t i2c_addr;            /*!< screen i2c slave adddress */
    }iface_i2c;

    struct {
        int8_t pin_num_miso;         /*!< MasterIn, SlaveOut pin */
        int8_t pin_num_mosi;         /*!< MasterOut, SlaveIn pin */
        int8_t pin_num_clk;          /*!< SPI Clock pin */
        int8_t pin_num_cs;           /*!< SPI Chip Select Pin */
        int clk_freq;                /*!< spi clock frequency */
        spi_host_device_t spi_host;  /*!< spi host index */
        int dma_chan;                /*!< DMA channel used */
        bool init_spi_bus;           /*!< Whether to initialize SPI bus */
    }iface_spi;
    
    int8_t pin_num_int;              /*!< Interrupt pin of touch panel */
    touch_dir_t direction;            /*!< Rotate direction */
    uint16_t width;                  /*!< width */
    uint16_t height;
    
}lcd_touch_config_t;


typedef struct {
    esp_err_t (*init)(lcd_touch_config_t *config);
    esp_err_t (*deinit)(bool free_bus);
    esp_err_t (*calibration_run)(const lcd_driver_fun_t *screen, bool recalibrate);
    esp_err_t (*set_direction)(touch_dir_t dir);
    int (*is_pressed)(void);
    esp_err_t (*read_info)(touch_info_t *info);

}touch_driver_fun_t;


esp_err_t iot_touch_init(touch_driver_fun_t *driver);

#ifdef __cplusplus
}
#endif

#endif