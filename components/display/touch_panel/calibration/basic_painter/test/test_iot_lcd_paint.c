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
#include <stdio.h>
#include "unity.h"
#include "test_utils.h"
#include "esp_log.h"

#include "iot_lcd.h"
#include "lcd_paint.h"

static const char *TAG = "spi lcd test";

static lcd_driver_fun_t lcd;


TEST_CASE("LCD paint ASCII character test", "[paint][iot]")
{
    TEST_ASSERT(ESP_OK == iot_lcd_init(&lcd));

    iot_paint_init(&lcd);
    iot_paint_clear(COLOR_WHITE);
    
    char str[] = "ESPRESSIF";
    font_t fonts[] = {Font8, Font12, Font16, Font20, Font24};
    uint16_t x = 5, y = 5;

    for (size_t i = 0; i < sizeof(fonts) / sizeof(font_t); i++) {
        iot_paint_draw_string(x, y, str, &fonts[i], COLOR_RED);
        y += fonts[i].Height;
    }

    char test_char[] = "Espressif IOT solution";
    for (size_t i = 0; i < sizeof(test_char)-1; i++) {
        iot_paint_draw_char(x, y, test_char[i], &Font12, COLOR_ESP_BKGD);
        x += Font12.Width;
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }

    lcd.deinit();
}

TEST_CASE("LCD paint show number test", "[paint][iot]")
{
    TEST_ASSERT(ESP_OK == iot_lcd_init(&lcd));

    iot_paint_init(&lcd);
    iot_paint_clear(COLOR_WHITE);

    lcd_info_t info;
    lcd.get_info(&info);
    ESP_LOGI(TAG, "get screen width=%d, height=%d", info.width, info.height);
    iot_paint_draw_num(0, 16, info.width, 3, &Font16, COLOR_RED);
    iot_paint_draw_num(35, 16, info.height, 3, &Font16, COLOR_RED);
    iot_paint_draw_float(160, 16, 1234, 5, 1, &Font16, COLOR_RED);

    lcd.deinit();
}

TEST_CASE("LCD paint draw graphics test", "[paint][iot]")
{
    TEST_ASSERT(ESP_OK == iot_lcd_init(&lcd));

    iot_paint_init(&lcd);
    iot_paint_clear(COLOR_WHITE);

    uint16_t center_x=60, center_y=60;
    uint16_t length = 100;
    uint16_t len_2 = length/2;
    for (size_t i = 0; i < length; i += 4) {
        iot_paint_draw_line(center_x, center_y, center_x-len_2+i, center_y+len_2, COLOR_RED);
    }

    for (size_t i = 0; i < length; i += 4) {
        iot_paint_draw_line(center_x, center_y, center_x+len_2, center_y-len_2+i, COLOR_RED);
    }

    for (size_t i = 0; i < length; i += 4) {
        iot_paint_draw_line(center_x, center_y, center_x-len_2+i, center_y-len_2, COLOR_RED);
    }

    for (size_t i = 0; i < length; i += 4) {
        iot_paint_draw_line(center_x, center_y, center_x-len_2, center_y-len_2+i, COLOR_RED);
    }

    iot_paint_draw_rectangle(10, 120, 60, 170, COLOR_RED);
    iot_paint_draw_circle(100, 150, 30, COLOR_RED);

    iot_paint_draw_filled_rectangle(20, 130, 50, 160, COLOR_RED);
    iot_paint_draw_filled_circle(100, 150, 25, COLOR_RED);

    lcd.deinit();
}