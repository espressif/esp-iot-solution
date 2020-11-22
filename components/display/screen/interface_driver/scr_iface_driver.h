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

#ifndef _IOT_SCREEN_INTERFACE_DRIVER_H_
#define _IOT_SCREEN_INTERFACE_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "i2s_lcd_driver.h"
#include "i2c_bus.h"
#include "driver/spi_master.h"

typedef struct {
    int8_t pin_num_miso;         /*!< MasterIn, SlaveOut pin */
    int8_t pin_num_mosi;         /*!< MasterOut, SlaveIn pin */
    int8_t pin_num_clk;          /*!< SPI Clock pin*/
    int8_t pin_num_cs;           /*!< SPI Chip Select Pin*/
    int8_t pin_num_dc;           /*!< Pin to select Data or Command for LCD */
    int clk_freq;                /*!< SPI clock frequency */
    spi_host_device_t spi_host;  /*!< SPI host index */
    int dma_chan;                /*!< DMA channel used */
    bool init_spi_bus;           /*!< Whether to initialize SPI host */
    bool swap_data;              /*!< Whether to swap data */
} iface_spi_config_t;

typedef struct {
    i2c_port_t i2c_num;          /*!< I2C port number */
    int8_t sda_io_num;           /*!< GPIO number for I2C sda signal */
    int8_t scl_io_num;           /*!< GPIO number for I2C scl signal */
    uint32_t clk_speed;          /*!< I2C clock frequency for master mode, (no higher than 1MHz for now) */
    uint16_t slave_addr;         /*!< I2C slave address */
} iface_i2c_config_t;

typedef enum {
    SCREEN_IFACE_I2C,            /*!< I2C interface */
    SCREEN_IFACE_8080,           /*!< 8080 parallel interface */
    SCREEN_IFACE_SPI,            /*!< SPI interface */
} scr_iface_type_t;

/**
 * @brief Define common function for screen interface driver
 * 
 */
typedef struct {
    scr_iface_type_t type;                                                      /*!< Interface bus type, see scr_iface_type_t struct */
    esp_err_t (*write_cmd)(void *handle, uint16_t cmd);                         /*!< Function to write a command */
    esp_err_t (*write_data)(void *handle, uint16_t data);                       /*!< Function to write a data */
    esp_err_t (*write)(void *handle, const uint8_t *data, uint32_t length);     /*!< Function to write a block data */
    esp_err_t (*read)(void *handle, uint8_t *data, uint32_t length);            /*!< Function to read a block data */
    esp_err_t (*bus_acquire)(void *handle);                                     /*!< Function to acquire interface bus */
    esp_err_t (*bus_release)(void *handle);                                     /*!< Function to release interface bus */
} scr_iface_driver_fun_t;

/**
 * @brief Create screen interface driver
 * 
 * @param type Type of screen interface
 * @param config configuration of interface driver
 * @param out_driver Pointer to a screen interface driver
 * 
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL Initialize failed
 *      - ESP_ERR_NO_MEM: Cannot allocate memory.
 */
esp_err_t scr_iface_create(scr_iface_type_t type, void *config, scr_iface_driver_fun_t **out_driver);

/**
 * @brief Delete screen interface driver
 * 
 * @param driver screen interface driver to delete
 * 
 * @return
 *      - ESP_OK on success
 */
esp_err_t scr_iface_delete(const scr_iface_driver_fun_t *driver);

#ifdef __cplusplus
}
#endif

#endif
