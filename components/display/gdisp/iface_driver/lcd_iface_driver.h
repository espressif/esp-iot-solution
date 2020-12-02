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
#ifndef _IOT_LCD_INTERFACE_DRIVER_H_
#define _IOT_LCD_INTERFACE_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lcd_types.h"

typedef struct {
    esp_err_t (*init)(const lcd_config_t *lcd);
    esp_err_t (*deinit)(bool free_bus);
    esp_err_t (*write_cmd)(uint16_t cmd);
    esp_err_t (*write_data)(uint16_t data);
    esp_err_t (*write)(const uint8_t *data, uint32_t length);
    esp_err_t (*read)(uint8_t *data, uint32_t length);
    esp_err_t (*bus_acquire)(void);
    esp_err_t (*bus_release)(void);
} lcd_iface_driver_fun_t;

/**
 * @brief Get an lcd interface instance
 * 
 */
#if defined CONFIG_LCD_DRIVER_INTERFACE_I2S
extern lcd_iface_driver_fun_t g_lcd_i2s_iface_default_driver;
static lcd_iface_driver_fun_t *g_iface_driver = &g_lcd_i2s_iface_default_driver;
#elif defined CONFIG_LCD_DRIVER_INTERFACE_SPI
extern lcd_iface_driver_fun_t g_lcd_spi_iface_default_driver;
static lcd_iface_driver_fun_t *g_iface_driver = &g_lcd_spi_iface_default_driver;
#elif defined CONFIG_LCD_DRIVER_INTERFACE_I2C
extern lcd_iface_driver_fun_t g_lcd_i2c_iface_default_driver;
static lcd_iface_driver_fun_t *g_iface_driver = &g_lcd_i2c_iface_default_driver;
#else
#error "Must select one interface for screen"
#endif

/**< Define the function of interface instance */
#define LCD_IFACE_INIT(v) g_iface_driver->init((v))
#define LCD_IFACE_DEINIT(v) g_iface_driver->deinit((v))
#define LCD_WRITE_CMD(v) g_iface_driver->write_cmd((v))
#define LCD_WRITE_DATA(v) g_iface_driver->write_data((v))
#define LCD_WRITE(v, l) g_iface_driver->write((v), (l))
#define LCD_READ(v, l) g_iface_driver->read((v), (l))
#define LCD_IFACE_ACQUIRE() g_iface_driver->bus_acquire()
#define LCD_IFACE_RELEASE() g_iface_driver->bus_release()

static inline esp_err_t LCD_WRITE_REG(uint16_t cmd, uint16_t data)
{
    esp_err_t ret;
    ret = LCD_WRITE_CMD(cmd);
    ret |= LCD_WRITE_DATA(data);
    if (ESP_OK != ret) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

#ifdef __cplusplus
}
#endif

#endif
