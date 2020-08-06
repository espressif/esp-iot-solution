/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#ifndef _GINPUT_LLD_MOUSE_TOUCH_H
#define _GINPUT_LLD_MOUSE_TOUCH_H

/* uGFX Config Includes */
#include "sdkconfig.h"

/* Touch Includes */
#include "XPT2046_adapter.h"

/* Param Include */
#include "iot_param.h"
#include "nvs_flash.h"

// Resolution and Accuracy Settings
#define GMOUSE_PEN_CALIBRATE_ERROR      8
#define GMOUSE_PEN_CLICK_ERROR          6
#define GMOUSE_PEN_MOVE_ERROR           4
#define GMOUSE_FINGER_CALIBRATE_ERROR   14
#define GMOUSE_FINGER_CLICK_ERROR       18
#define GMOUSE_FINGER_MOVE_ERROR        14

// How much extra data to allocate at the end of the GMouse structure for the board's use
#define GMOUSE_BOARD_DATA_SIZE          1

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

static bool_t touch_init_board(GMouse *m, unsigned driverinstance)
{
    board_touch_init();
    
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    return TRUE;
}

static GFXINLINE bool_t getpin_pressed(GMouse *m)
{
    return board_touch_is_pressed();
}

static GFXINLINE void aquire_bus(GMouse *m)
{
    /* Code here*/
}

static GFXINLINE void release_bus(GMouse *m)
{
    /* Code here*/
}

static GFXINLINE uint16_t read_value(GMouse *m, uint16_t port)
{
    return board_touch_get_position(port);
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

static GFXINLINE gBool touch_erase_calibration(GMouse *m, void *buf, size_t sz)
{
    esp_err_t res = iot_param_erase((const char *)TOUCH_CAL_VAL_NAMESPACE, (const char *) TOUCH_CAL_VAL_KEY);
    return (res == ESP_OK);
}

#endif /* _GINPUT_LLD_MOUSE_BOARD_H */
