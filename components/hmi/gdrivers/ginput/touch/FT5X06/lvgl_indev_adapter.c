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

/* SDK Includes */
#include "sdkconfig.h"
#include "esp_log.h"

#ifdef CONFIG_LVGL_GUI_ENABLE

/*C Includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* I2C Includes */
#include "driver/i2c.h"
#include "iot_ft5x06.h"
#include "iot_i2c_bus.h"

/* lvgl includes */
#include "iot_lvgl.h"

/* lvgl calibration includes */
#include "lv_calibration.h"

/* FT5x06 Include */
#include "FT5x06.h"

static ft5x06_handle_t dev = NULL;

static void write_reg(uint8_t reg, uint8_t val)
{
    iot_ft5x06_write(dev, reg, 1, &val);
}

static uint8_t read_byte(uint8_t reg)
{
    uint8_t data;
    iot_ft5x06_read(dev, reg, 1, &data);
    return data;
}

static uint16_t read_word(uint8_t reg)
{
    uint8_t data[2];
    uint16_t result;
    iot_ft5x06_read(dev, reg, 2, &data);
    result = data[0] << 8 | data[1];
    return result;
}

/*Function pointer to read data. Return 'true' if there is still data to be read (buffered)*/
static bool ex_tp_read(lv_indev_data_t *data)
{
    static lv_coord_t x = 0xFFFF, y = 0xFFFF;
    data->state = LV_INDEV_STATE_REL;
    // please be sure that your touch driver every time return old (last clcked) value. 
    data->point.x = x;
    data->point.y = y;
    // Only take a reading if we are touched.
    if ((read_byte(FT5x06_TOUCH_POINTS) & 0x07)) {

        /* Get the X, Y, values */
        data->point.x = (lv_coord_t)(read_word(FT5x06_TOUCH1_XH) & 0x0fff);
        data->point.y = (lv_coord_t)read_word(FT5x06_TOUCH1_YH);
        data->state = LV_INDEV_STATE_PR;

        // Apply calibration, rotation
        // Transform the co-ordinates
        if (lvgl_calibration_transform(&(data->point))) {
            lv_coord_t t;
            // Rescale X,Y if we are using self-calibration
            #ifdef CONFIG_LVGL_DISP_ROTATE_0
                data->point.x = data->point.x;
                data->point.y = data->point.y;
            #elif defined(CONFIG_LVGL_DISP_ROTATE_90)
                t = data->point.x;
                data->point.x = LV_HOR_RES - 1 - data->point.y;
                data->point.y = t;
            #elif defined(CONFIG_LVGL_DISP_ROTATE_180)
                data->point.x = LV_HOR_RES - 1 - data->point.x;
                data->point.y = LV_VER_RES - 1 - data->point.y;
            #elif defined(CONFIG_LVGL_DISP_ROTATE_270)
                t = data->point.y;
                data->point.y = LV_VER_RES - 1 - data->point.x;
                data->point.x = t;
            #endif
            x = data->point.x;
            y = data->point.y;
        }
    }
    return false;
}

/* Input device interface */
/* Initialize your touchpad */
lv_indev_drv_t lvgl_indev_init()
{
    i2c_bus_handle_t i2c_bus = NULL;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_LVGL_TOUCH_SDA_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = CONFIG_LVGL_TOUCH_SCL_GPIO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 200000,
    };
    i2c_bus = iot_i2c_bus_create(CONFIG_LVGL_TOUCH_IIC_NUM, &conf);
    dev = iot_ft5x06_create(i2c_bus, FT5X06_ADDR_DEF);

    // Init default values. (From NHD-3.5-320240MF-ATXL-CTP-1 datasheet)
    // Valid touching detect threshold
    write_reg(FT5x06_ID_G_THGROUP, 0x16);

    // valid touching peak detect threshold
    write_reg(FT5x06_ID_G_THPEAK, 0x3C);

    // Touch focus threshold
    write_reg(FT5x06_ID_G_THCAL, 0xE9);

    // threshold when there is surface water
    write_reg(FT5x06_ID_G_THWATER, 0x01);

    // threshold of temperature compensation
    write_reg(FT5x06_ID_G_THTEMP, 0x01);

    // Touch difference threshold
    write_reg(FT5x06_ID_G_THDIFF, 0xA0);

    // Delay to enter 'Monitor' status (s)
    write_reg(FT5x06_ID_G_TIME_ENTER_MONITOR, 0x0A);

    // Period of 'Active' status (ms)
    write_reg(FT5x06_ID_G_PERIODACTIVE, 0x06);

    // Timer to enter 'idle' when in 'Monitor' (ms)
    write_reg(FT5x06_ID_G_PERIODMONITOR, 0x28);

    lv_indev_drv_t indev_drv;      /*Descriptor of an input device driver*/
    lv_indev_drv_init(&indev_drv); /*Basic initialization*/

    indev_drv.type = LV_INDEV_TYPE_POINTER; /*The touchpad is pointer type device*/
    indev_drv.read = ex_tp_read;            /*Library ready your touchpad via this function*/

    lv_indev_drv_register(&indev_drv); /*Finally register the driver*/
    return indev_drv;
}

#endif /* CONFIG_LVGL_GUI_ENABLE */
