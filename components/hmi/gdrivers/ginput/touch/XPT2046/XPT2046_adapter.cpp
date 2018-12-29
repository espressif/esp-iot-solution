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

/* C Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* RTOS Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* SPI Includes */
#include "iot_xpt2046.h"
#include "XPT2046_adapter.h"

/* uGFX Config Include */
#include "sdkconfig.h"

static CXpt2046 *xpt = NULL;

class CTouchAdapter : public CXpt2046
{
public:
    CTouchAdapter(xpt_conf_t *xpt_conf, int rotation) : CXpt2046(xpt_conf, rotation)
    {
        /* Code here*/
    }
};

#ifdef CONFIG_UGFX_GUI_ENABLE

void board_touch_init()
{
    xpt_conf_t xpt_conf;
    xpt_conf.pin_num_cs = CONFIG_UGFX_TOUCH_CS_GPIO;   /*!<SPI Chip Select Pin*/
    xpt_conf.pin_num_irq = CONFIG_UGFX_TOUCH_IRQ_GPIO; /*!< Touch screen IRQ pin */
    xpt_conf.clk_freq = XPT2046_CLK_FREQ;               /*!< spi clock frequency */
    xpt_conf.spi_host = (spi_host_device_t)CONFIG_UGFX_LCD_SPI_NUM;                     /*!< spi host index*/
    xpt_conf.pin_num_miso = -1;                        /*!<MasterIn, SlaveOut pin*/
    xpt_conf.pin_num_mosi = -1;                        /*!<MasterOut, SlaveIn pin*/
    xpt_conf.pin_num_clk = -1;                         /*!<SPI Clock pin*/
    xpt_conf.dma_chan = 1;
    xpt_conf.init_spi_bus = false; /*!< Whether to initialize SPI bus */

    if (xpt == NULL) {
        xpt = new CTouchAdapter(&xpt_conf, 0);
    }
}

#endif

bool board_touch_is_pressed()
{
    return xpt->is_pressed();
}

int board_touch_get_position(int port)
{
    return xpt->get_sample(port);
}

#ifdef CONFIG_LVGL_GUI_ENABLE

/* lvgl include */
#include "lvgl_indev_config.h"

/*Function pointer to read data. Return 'true' if there is still data to be read (buffered)*/
static bool ex_tp_read(lv_indev_data_t *data)
{
    static lv_coord_t x = 0xFFFF, y = 0xFFFF;
    data->state = LV_INDEV_STATE_REL;
    // please be sure that your touch driver every time return old (last clcked) value. 
    data->point.x = x;
    data->point.y = y;
    if (xpt->is_pressed()) {
        position pos = xpt->get_raw_position();
        data->point.x = pos.x;
        data->point.y = pos.y;
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

/* Input device interface，Initialize your touchpad */
lv_indev_drv_t lvgl_indev_init()
{
    xpt_conf_t xpt_conf = {
        .pin_num_cs = CONFIG_LVGL_TOUCH_CS_GPIO,   /*!<SPI Chip Select Pin*/
        .pin_num_irq = CONFIG_LVGL_TOUCH_IRQ_GPIO, /*!< Touch screen IRQ pin */
        .clk_freq = XPT2046_CLK_FREQ,               /*!< spi clock frequency */
        .spi_host = (spi_host_device_t)CONFIG_LVGL_LCD_SPI_NUM,                     /*!< spi host index */
        .pin_num_miso = -1,                        /*!<MasterIn, SlaveOut pin*/
        .pin_num_mosi = -1,                        /*!<MasterOut, SlaveIn pin*/
        .pin_num_clk = -1,                         /*!<SPI Clock pin*/
        .dma_chan = 1,
        .init_spi_bus = false,                     /*!< Whether to initialize SPI bus */
    };

    if (xpt == NULL) {
        xpt = new CTouchAdapter(&xpt_conf, 0);
    }

    lv_indev_drv_t indev_drv;               /*Descriptor of an input device driver*/
    lv_indev_drv_init(&indev_drv);          /*Basic initialization*/

    indev_drv.type = LV_INDEV_TYPE_POINTER; /*The touchpad is pointer type device*/
    indev_drv.read = ex_tp_read;            /*Library ready your touchpad via this function*/

    lv_indev_drv_register(&indev_drv);      /*Finally register the driver*/
    return indev_drv;
}

#endif /* CONFIG_LVGL_GUI_ENABLE */
