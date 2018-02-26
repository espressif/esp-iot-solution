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
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "freertos/event_groups.h"
#include "bitmap.h"
#include "iot_lcd.h"
#include "iot_xpt2046.h"
#include "app_touch.h"

static const char* TAG = "TOUCH_SCREEN";

CXpt2046 *xpt;

void touch_calibration()
{
    uint16_t px[2], py[2], xPot[4], yPot[4];
    float xFactor, yFactor;
    int _offset_x, _offset_y;

    lcd_display_top_left();
    ESP_LOGD(TAG, "please input top-left button...");
    do {
        xpt->sample();
    } while (!xpt->is_pressed());
    xPot[0] = xpt->getposition().x;
    yPot[0] = xpt->getposition().y;
    ESP_LOGD(TAG, "X:%d; Y:%d.", xPot[0], yPot[0]);

    lcd_display_top_right();
    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGD(TAG, "please input top-right button...");
    do {
        xpt->sample();
    } while (!xpt->is_pressed());
    xPot[1] = xpt->getposition().x;
    yPot[1] = xpt->getposition().y;
    ESP_LOGD(TAG, "X:%d; Y:%d.", xPot[1], yPot[1]);

    lcd_display_bottom_right();
    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGD(TAG, "please input bottom-right button...");
    do {
        xpt->sample();
    } while (!xpt->is_pressed());
    xPot[2] = xpt->getposition().x;
    yPot[2] = xpt->getposition().y;
    ESP_LOGD(TAG, "X:%d; Y:%d.", xPot[2], yPot[2]);

    lcd_display_bottom_left();
    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGD(TAG, "please input bottom-left button...");
    do {
        xpt->sample();
    } while (!xpt->is_pressed());
    xPot[3] = xpt->getposition().x;
    yPot[3] = xpt->getposition().y;
    ESP_LOGD(TAG, "X:%d; Y:%d.", xPot[3], yPot[3]);

    px[0] = (xPot[0] + xPot[1]) / 2;
    py[0] = (yPot[0] + yPot[3]) / 2;

    px[1] = (xPot[2] + xPot[3]) / 2;
    py[1] = (yPot[2] + yPot[1]) / 2;
    ESP_LOGD(TAG, "X:%d; Y:%d.", px[0], py[0]);
    ESP_LOGD(TAG, "X:%d; Y:%d.", px[1], py[1]);

    xFactor = (float) 240 / (px[1] - px[0]);
    yFactor = (float) 320 / (py[1] - py[0]);

    _offset_x = (int16_t) 240 - ((float) px[1] * xFactor);
    _offset_y = (int16_t) 320 - ((float) py[1] * yFactor);

    xpt->set_offset(xFactor, yFactor, _offset_x, _offset_y);
    ESP_LOGD(TAG, "xFactor:%f, yFactor:%f, Offset X:%d; Y:%d.", xFactor, yFactor, _offset_x, _offset_y);
    clear_screen();
}

void xpt2046_touch_init(void)
{
    xpt_conf_t xpt_conf = {
        .pin_num_cs = GPIO_NUM_26,
        .pin_num_irq = GPIO_NUM_4,
        .clk_freq = 1 * 1000 * 1000,
        .spi_host = HSPI_HOST,
        .pin_num_miso = -1,        /*!<MasterIn, SlaveOut pin*/
        .pin_num_mosi = -1,        /*!<MasterOut, SlaveIn pin*/
        .pin_num_clk = -1,         /*!<SPI Clock pin*/
        .dma_chan = 1,
        .init_spi_bus = false,
    };
    if (xpt == NULL) {
        xpt = new CXpt2046(&xpt_conf, 320, 240);
    }
    xpt->set_rotation(1);
    touch_calibration();
}
