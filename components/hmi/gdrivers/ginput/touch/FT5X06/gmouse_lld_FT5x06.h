/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#ifndef _GINPUT_LLD_MOUSE_BOARD_H
#define _GINPUT_LLD_MOUSE_BOARD_H

/* uGFX Config Includes */
#include "sdkconfig.h"

/* Touch Includes */
#include "driver/i2c.h"
#include "iot_ft5x06.h"
#include "iot_i2c_bus.h"

/* Param Include */
#include "iot_param.h"
#include "nvs_flash.h"

// Resolution and Accuracy Settings
#define GMOUSE_FT5x06_PEN_CALIBRATE_ERROR       8
#define GMOUSE_FT5x06_PEN_CLICK_ERROR           6
#define GMOUSE_FT5x06_PEN_MOVE_ERROR            4
#define GMOUSE_FT5x06_FINGER_CALIBRATE_ERROR    14
#define GMOUSE_FT5x06_FINGER_CLICK_ERROR        18
#define GMOUSE_FT5x06_FINGER_MOVE_ERROR         14

// How much extra data to allocate at the end of the GMouse structure for the board's use
#define GMOUSE_FT5x06_BOARD_DATA_SIZE           0

// Set this to GFXON if you want self-calibration.
// NOTE: This is not as accurate as real calibration.
//       It requires the orientation of the touch panel to match the display.
//       It requires the active area of the touch panel to exactly match the display size.
#define GMOUSE_FT5x06_SELF_CALIBRATE            GFXOFF

#if CONFIG_UGFX_LCD_DRIVER_FRAMEBUFFER_MODE
#define TOUCH_CAL_VAL_NAMESPACE   "FbUGfxParam"
#define TOUCH_CAL_VAL_KEY         "TouchCalVal"
#elif CONFIG_UGFX_LCD_DRIVER_API_MODE
#define TOUCH_CAL_VAL_NAMESPACE   "LcdGfxParam"
#define TOUCH_CAL_VAL_KEY         "TouchCalVal"
#else
#define TOUCH_CAL_VAL_NAMESPACE   "uGfxParam"
#define TOUCH_CAL_VAL_KEY         "touchCalVal"
#endif

static ft5x06_handle_t dev = NULL;

static bool_t init_board(GMouse *m, unsigned driverinstance)
{
    i2c_bus_handle_t i2c_bus = NULL;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_UGFX_TOUCH_SDA_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = CONFIG_UGFX_TOUCH_SCL_GPIO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 200000,
    };
    i2c_bus = iot_i2c_bus_create(CONFIG_UGFX_TOUCH_IIC_NUM, &conf);
    dev = iot_ft5x06_create(i2c_bus, FT5X06_ADDR_DEF);
    
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    return true;
}

static GFXINLINE void aquire_bus(GMouse *m)
{
    /* Code here*/
}

static GFXINLINE void release_bus(GMouse *m)
{
    /* Code here*/
}

static void write_reg(GMouse *m, uint8_t reg, uint8_t val)
{
    iot_ft5x06_write(dev, reg, 1, &val);
}

static uint8_t read_byte(GMouse *m, uint8_t reg)
{
    uint8_t data;
    iot_ft5x06_read(dev, reg, 1, &data);
    return data;
}

static uint16_t read_word(GMouse *m, uint8_t reg)
{
    uint8_t data[2];
    uint16_t result;
    iot_ft5x06_read(dev, reg, 2, data);
    result = data[0] << 8 | data[1];
    return result;
}

static GFXINLINE void touch_save_calibration(GMouse *m, const void *buf, size_t sz)
{
    iot_param_save((const char *)TOUCH_CAL_VAL_NAMESPACE, (const char *) TOUCH_CAL_VAL_KEY, (void *) buf, sz);
}

static GFXINLINE gBool touch_load_calibration(GMouse *m, void *buf, size_t sz)
{
    esp_err_t res = iot_param_load((const char *)TOUCH_CAL_VAL_NAMESPACE, (const char *) TOUCH_CAL_VAL_KEY, (void *) buf);
    return (res == ESP_OK);
}

static GFXINLINE bool touch_erase_calibration(GMouse *m, void *buf, size_t sz)
{
    esp_err_t res = iot_param_erase((const char *)TOUCH_CAL_VAL_NAMESPACE, (const char *) TOUCH_CAL_VAL_KEY);
    return (res == ESP_OK);
}

#endif /* _GINPUT_LLD_MOUSE_BOARD_H */
