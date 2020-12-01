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
#include "iot_xpt2046.h"
#include "bitmap.h"
#include "iot_lcd.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "freertos/event_groups.h"
#include "app_touch.h"

static const char* TAG = "TOUCH_SCREEN";

CXpt2046 *xpt;
//calibration parameters
float xFactor, yFactor;
int _offset_x, _offset_y;
int m_width, m_height;
int m_rotation;

void touch_calibration(int width, int height, int rotation)
{
    uint16_t px[2], py[2], xPot[4], yPot[4];

    m_width = width;
    m_height = height;
    m_rotation = rotation;

    lcd_display_top_left();
    ESP_LOGI(TAG, "please input top-left button...");

    while (!xpt->is_pressed()){
        vTaskDelay(50 / portTICK_RATE_MS);
    };
    xPot[0] = xpt->get_raw_position().x;
    yPot[0] = xpt->get_raw_position().y;
    ESP_LOGI(TAG, "X:%d; Y:%d.", xPot[0], yPot[0]);

    lcd_display_top_right();
    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "please input top-right button...");

    while (!xpt->is_pressed()){
        vTaskDelay(50 / portTICK_RATE_MS);
    };
    xPot[1] = xpt->get_raw_position().x;
    yPot[1] = xpt->get_raw_position().y;
    ESP_LOGI(TAG, "X:%d; Y:%d.", xPot[1], yPot[1]);

    lcd_display_bottom_right();
    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "please input bottom-right button...");

    while (!xpt->is_pressed()){
        vTaskDelay(50 / portTICK_RATE_MS);
    };
    xPot[2] = xpt->get_raw_position().x;
    yPot[2] = xpt->get_raw_position().y;
    ESP_LOGI(TAG, "X:%d; Y:%d.", xPot[2], yPot[2]);

    lcd_display_bottom_left();
    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "please input bottom-left button...");

    while (!xpt->is_pressed()){
        vTaskDelay(50 / portTICK_RATE_MS);
    };
    xPot[3] = xpt->get_raw_position().x;
    yPot[3] = xpt->get_raw_position().y;
    ESP_LOGI(TAG, "X:%d; Y:%d.", xPot[3], yPot[3]);

    px[0] = (xPot[0] + xPot[1]) / 2;
    py[0] = (yPot[0] + yPot[3]) / 2;

    px[1] = (xPot[2] + xPot[3]) / 2;
    py[1] = (yPot[2] + yPot[1]) / 2;
    ESP_LOGI(TAG, "X:%d; Y:%d.", px[0], py[0]);
    ESP_LOGI(TAG, "X:%d; Y:%d.", px[1], py[1]);

    xFactor = (float) m_height / (px[1] - px[0]);
    yFactor = (float) m_width / (py[1] - py[0]);

    _offset_x = (int16_t) m_height - ((float) px[1] * xFactor);
    _offset_y = (int16_t) m_width - ((float) py[1] * yFactor);

    ESP_LOGI(TAG, "xFactor:%f, yFactor:%f, Offset X:%d; Y:%d.", xFactor, yFactor, _offset_x, _offset_y);
    clear_screen();
}

position get_screen_position(position pos)
{
    position m_pos;
    int x = _offset_x + pos.x * xFactor;
    int y = _offset_y + pos.y * yFactor;

    if(x > m_height){
        x = m_height;
    } else if(x < 0){
        x = 0;
    }
    if(y > m_width){
        y = m_width;
    } else if(y < 0){
        y = 0;
    }
    switch (m_rotation) {
        case 0:
            m_pos.x = x;
            m_pos.y = y;
            break;
        case 3:
            m_pos.x = m_width - y;
            m_pos.y = x;
            break;
        case 2:
            m_pos.x = m_height - x;
            m_pos.y = m_width - y;
            break;
        case 1:
            m_pos.x = y;
            m_pos.y = m_height - x;
            break;
    }
    return m_pos;
}

void xpt2046_touch_init(void)
{
    xpt_conf_t xpt_conf = {
        .pin_num_cs = CONFIG_XPT2046_CS_GPIO,
        .pin_num_irq = CONFIG_XPT2046_IRQ_GPIO,
        .clk_freq = 1 * 1000 * 1000,
        .spi_host = HSPI_HOST,
        .pin_num_miso = -1,        /*!<MasterIn, SlaveOut pin*/
        .pin_num_mosi = -1,        /*!<MasterOut, SlaveIn pin*/
        .pin_num_clk = -1,         /*!<SPI Clock pin*/
        .dma_chan = 1,
        .init_spi_bus = false,
    };
    if (xpt == NULL) {
        xpt = new CXpt2046(&xpt_conf);
    }
    touch_calibration(320, 240, 1);
}
