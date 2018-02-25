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
#include "unity.h"

static const char* TAG = "TOUCH_LCD";

extern CXpt2046 *xpt;
extern CEspLcd* tft;
QueueHandle_t queue_change = NULL, queue_draw_point = NULL;

void get_coordinate(void *arg)
{
    while (1) {
        if (xpt->get_irq() == 0) {
            xpt->sample();
            if (xpt->is_pressed()) {
                position pos;
                pos.x = xpt->x();
                pos.y = xpt->y();
                xQueueSend(queue_change, &pos, 0);
                xQueueSend(queue_draw_point, &pos, 0);
                ESP_LOGI("XPT2046", "getSample x: %d ; y: %d", xpt->x(), xpt->y());
            }
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
    position pos;
    while (1) {
        if (pdTRUE == xQueueReceive(queue_change, &pos, portMAX_DELAY)) {
            lcd_display_coordinate(pos.x, pos.y);
        }
    }
}

void write_coordinate(void *arg)
{
    position pos;
    while (1) {
        if (pdTRUE == xQueueReceive(queue_draw_point, &pos, portMAX_DELAY)) {
            lcd_display_point(pos.x, pos.y);
        }
    }
}

extern "C" void touch_tft_test()
{
    //Initialization lcd display
    lcd_tft_init();
    //Initialization touch screen
    xpt2046_touch_init();

    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "Free heap: %u", xPortGetFreeHeapSize());

    queue_change = xQueueCreate(10, sizeof(position));
    queue_draw_point = xQueueCreate(10, sizeof(position));

    xTaskCreate(get_coordinate, "get_coordinate", 2048, NULL, 6, NULL);
    xTaskCreate(show_coordinate, "show_coordinate", 2048, NULL, 5, NULL);
    xTaskCreate(write_coordinate, "write_coordinate", 2048, NULL, 7, NULL);
}

TEST_CASE("TouchTFT cpp test", "[lcd][touchtft][iot]")
{
    touch_tft_test();
}
