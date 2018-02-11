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
#include "include/app_touch.h"

static const char* TAG = "TOUCH_LCD";

extern CXpt2046 *xpt;
extern CEspLcd* tft;
SemaphoreHandle_t change = NULL;

int x = 0;
int y = 0;

void get_coordinate(void *arg)
{

    while (1) {
        xpt->sample();
        if (xpt->is_pressed()) {
            x = xpt->x();
            y = xpt->y();
            xSemaphoreGive(change);
            ESP_LOGI("XPT2046", "getSample x: %d ; y: %d", xpt->x(), xpt->y());
        } else {
            vTaskDelay(100 / portTICK_RATE_MS);
        }
    }
}

void show_coordinate(void *arg)
{
    tft->drawString("coordinate:", 2, 2);
    tft->drawString("X:", 2, 10);
    tft->drawString("Y:", 2, 19);
    lcd_change_text_size(1);
    while (1) {
        xSemaphoreTake(change, portMAX_DELAY);
        lcd_display_coordinate(x, y);
    }
}

void write_coordinate(void *arg)
{
    while (1) {
        xSemaphoreTake(change, portMAX_DELAY);
        lcd_display_point(x, y);
    }
}

extern "C" void app_main()
{
    //Initialization lcd display
    lcd_tft_init();
    //Initialization touch screen
    xpt2046_touch_init();

    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "Free heap: %u", xPortGetFreeHeapSize());

    change = xSemaphoreCreateMutex();

    xTaskCreate(get_coordinate, "get_coordinate", 2048, NULL, 5, NULL);
    xTaskCreate(show_coordinate, "show_coordinate", 2048, NULL, 5, NULL);
    xTaskCreate(write_coordinate, "write_coordinate", 2048, NULL, 5, NULL);
}