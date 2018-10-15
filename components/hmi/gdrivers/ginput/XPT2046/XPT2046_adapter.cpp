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
    xpt_conf.clk_freq = 1 * 1000 * 1000;               /*!< spi clock frequency */
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

static const char *TAG = "TOUCH_SCREEN";

//calibration parameters
float xFactor, yFactor;
int _offset_x, _offset_y;
int m_width, m_height;
// #define NEED_CALI

void touch_calibration(int width, int height, int rotation)
{
    uint16_t px[2], py[2], xPot[4], yPot[4];

    m_width = width;
    m_height = height;

    ESP_LOGD(TAG, "please input top-left button...");

    while (!xpt->is_pressed()) {
        vTaskDelay(50 / portTICK_RATE_MS);
    };
    xPot[0] = xpt->get_raw_position().x;
    yPot[0] = xpt->get_raw_position().y;
    ESP_LOGD(TAG, "X:%d; Y:%d.", xPot[0], yPot[0]);

    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGD(TAG, "please input top-right button...");

    while (!xpt->is_pressed()) {
        vTaskDelay(50 / portTICK_RATE_MS);
    };
    xPot[1] = xpt->get_raw_position().x;
    yPot[1] = xpt->get_raw_position().y;
    ESP_LOGD(TAG, "X:%d; Y:%d.", xPot[1], yPot[1]);

    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGD(TAG, "please input bottom-right button...");

    while (!xpt->is_pressed()) {
        vTaskDelay(50 / portTICK_RATE_MS);
    };
    xPot[2] = xpt->get_raw_position().x;
    yPot[2] = xpt->get_raw_position().y;
    ESP_LOGD(TAG, "X:%d; Y:%d.", xPot[2], yPot[2]);

    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGD(TAG, "please input bottom-left button...");

    while (!xpt->is_pressed()) {
        vTaskDelay(50 / portTICK_RATE_MS);
    };
    xPot[3] = xpt->get_raw_position().x;
    yPot[3] = xpt->get_raw_position().y;
    ESP_LOGD(TAG, "X:%d; Y:%d.", xPot[3], yPot[3]);

    px[0] = (xPot[0] + xPot[1]) / 2;
    py[0] = (yPot[0] + yPot[3]) / 2;

    px[1] = (xPot[2] + xPot[3]) / 2;
    py[1] = (yPot[2] + yPot[1]) / 2;
    ESP_LOGD(TAG, "X:%d; Y:%d.", px[0], py[0]);
    ESP_LOGD(TAG, "X:%d; Y:%d.", px[1], py[1]);

    xFactor = (float)m_height / (px[1] - px[0]);
    yFactor = (float)m_width / (py[1] - py[0]);

    _offset_x = (int16_t)m_height - ((float)px[1] * xFactor);
    _offset_y = (int16_t)m_width - ((float)py[1] * yFactor);

    ESP_LOGD(TAG, "xFactor:%f, yFactor:%f, Offset X:%d; Y:%d.", xFactor, yFactor, _offset_x, _offset_y);
}

position get_screen_position(position pos)
{
    position m_pos;
    int x = _offset_x + pos.x * xFactor;
    int y = _offset_y + pos.y * yFactor;

    if (x > m_height) {
        x = m_height;
    } else if (x < 0) {
        x = 0;
    }
    if (y > m_width) {
        y = m_width;
    } else if (y < 0) {
        y = 0;
    }

#ifdef CONFIG_LVGL_DISP_ROTATE_0
    m_pos.x = LV_HOR_RES - x;
    m_pos.y = y;
#elif defined(CONFIG_LVGL_DISP_ROTATE_90)
    m_pos.x = LV_HOR_RES - y;
    m_pos.y = LV_VER_RES - x;
#elif defined(CONFIG_LVGL_DISP_ROTATE_180)
    m_pos.x = x;
    m_pos.y = LV_VER_RES - y;
#elif defined(CONFIG_LVGL_DISP_ROTATE_270)
    m_pos.x = y;
    m_pos.y = x;
#endif

    return m_pos;
}

/*Function pointer to read data. Return 'true' if there is still data to be read (buffered)*/
bool ex_tp_read(lv_indev_data_t *data)
{
    data->state = xpt->is_pressed() ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    position pos = get_screen_position(xpt->get_raw_position());
    data->point.x = pos.x;
    data->point.y = pos.y;
    return false;
}

/* Input device interface，Initialize your touchpad */
void lvgl_indev_init()
{
    xpt_conf_t xpt_conf = {
        .pin_num_cs = CONFIG_LVGL_TOUCH_CS_GPIO,   /*!<SPI Chip Select Pin*/
        .pin_num_irq = CONFIG_LVGL_TOUCH_IRQ_GPIO, /*!< Touch screen IRQ pin */
        .clk_freq = 1 * 1000 * 1000,               /*!< spi clock frequency */
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
#ifdef NEED_CALI
    touch_calibration(320, 240, 1);
#else
    xFactor = 0.070651;
    yFactor = 0.089435;
    _offset_x = -17;
    _offset_y = -26;
    m_width = 320;
    m_height = 240;
#endif

    lv_indev_drv_t indev_drv;               /*Descriptor of an input device driver*/
    lv_indev_drv_init(&indev_drv);          /*Basic initialization*/

    indev_drv.type = LV_INDEV_TYPE_POINTER; /*The touchpad is pointer type device*/
    indev_drv.read = ex_tp_read;            /*Library ready your touchpad via this function*/

    lv_indev_drv_register(&indev_drv);      /*Finally register the driver*/
}

#endif /* CONFIG_LVGL_GUI_ENABLE */
