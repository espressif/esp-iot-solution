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
#include "iot_xpt2046.h"
#include "iot_lcd.h"
#include "app_touch.h"
#include "unity.h"

static const char* TAG = "TOUCH_LCD";

extern CXpt2046 *xpt;
extern CEspLcd* tft;
QueueHandle_t queue_change = NULL, queue_draw_point = NULL;

void get_coordinate(void *arg)
{
    while (1) {
        if (xpt->is_pressed()) {
            position pos;
            pos = get_screen_position(xpt->get_raw_position());
            xQueueSend(queue_change, &pos, 0);
            xQueueSend(queue_draw_point, &pos, 0);
            ESP_LOGI("XPT2046", "getSample x: %d ; y: %d", pos.x, pos.y);
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
