// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
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
#include "freertos/FreeRTOS.h"
#include "iot_lcd.h"
#include "iot_touch.h"
#include "lcd_paint.h"
#include "unity.h"

static const char *TAG = "TOUCH LCD TEST";

static touch_driver_fun_t touch;
static lcd_driver_fun_t lcd;


TEST_CASE("Touch screen calibration test", "[lcd][touchtft][iot]")
{
    TEST_ASSERT(ESP_OK == iot_lcd_init(&lcd));
    TEST_ASSERT(ESP_OK == iot_touch_init(&touch));

    touch.calibration_run(&lcd, true);

    touch.deinit(false);
    lcd.deinit();
}


TEST_CASE("Touch drawing board test", "[lcd][touchtft][iot]")
{
    TEST_ASSERT(ESP_OK == iot_lcd_init(&lcd));
    TEST_ASSERT(ESP_OK == iot_touch_init(&touch));
    touch.calibration_run(&lcd, false);

    iot_paint_init(&lcd);
    iot_paint_clear(COLOR_WHITE);

    while (1) {
        touch_info_t touch_info;
        touch.read_info(&touch_info);
        for (int i = 0; i < touch_info.point_num; i++) {
            ESP_LOGI(TAG, "touch point %d: (%d, %d)", i, touch_info.curx[i], touch_info.cury[i]);
        }

        if (TOUCH_EVT_PRESS == touch_info.event) {
            int32_t x = touch_info.curx[0];
            int32_t y = touch_info.cury[0];
            ESP_LOGI(TAG, "Draw point at (%d, %d)", x, y);
            lcd.draw_pixel(x, y, COLOR_RED);
            lcd.draw_pixel(x+1, y, COLOR_RED);
            lcd.draw_pixel(x, y+1, COLOR_RED);
            lcd.draw_pixel(x+1, y+1, COLOR_RED);

        } else {
            vTaskDelay(50 / portTICK_RATE_MS);
        }
    }
    touch.deinit(false);
    lcd.deinit();
}
