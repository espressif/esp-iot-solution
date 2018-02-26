/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include "xpt2046_obj.h"
#include "bitmap.h"
#include "iot_lcd.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "freertos/event_groups.h"
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
