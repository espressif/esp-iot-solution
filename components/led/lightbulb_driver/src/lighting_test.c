// Copyright 2020-2022 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lightbulb.h"

static const char *TAG = "output_test";

void lightbulb_lighting_output_test(lightbulb_lighting_unit_t mask, uint16_t speed_ms)
{
    // BIT0 rainbow (only color)
    if (mask & BIT(0)) {
        ESP_LOGW(TAG, "rainbow");
        lightbulb_set_switch(false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
        // red
        lightbulb_set_hsv(0, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        // orange
        lightbulb_set_hsv(30, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        // yellow
        lightbulb_set_hsv(60, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        // green
        lightbulb_set_hsv(120, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        // blue
        lightbulb_set_hsv(210, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        // indigo
        lightbulb_set_hsv(240, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        // purple
        lightbulb_set_hsv(300, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
        lightbulb_set_switch(false);
    }

    // BIT1 warm->cold
    if (mask & BIT(1)) {
        ESP_LOGW(TAG, "warm->cold");
        lightbulb_set_switch(false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(0, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(50, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_switch(false);
    }

    // BIT2 cold->warm
    if (mask & BIT(2)) {
        ESP_LOGW(TAG, "cold->warm");
        lightbulb_set_switch(false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(50, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(0, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_switch(false);
    }

    // BIT3 red green blue cct brightness 0->100
    if (mask & BIT(3)) {
        ESP_LOGW(TAG, "basic five");
        lightbulb_set_switch(false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
        int i = 0;
        for (i = 0; i <= 100; i += 10) {
            lightbulb_set_hsv(0, 100, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }
        for (i = 0; i <= 100; i += 10) {
            lightbulb_set_hsv(120, 100, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }
        for (i = 0; i <= 100; i += 10) {
            lightbulb_set_hsv(240, 100, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }
        for (i = 0; i <= 100; i += 10) {
            lightbulb_set_hsv(30, 100, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }
        for (i = 0; i <= 100; i += 10) {
            lightbulb_set_hsv(160, 100, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }
        for (i = 0; i <= 100; i += 10) {
            lightbulb_set_hsv(270, 100, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }
        for (i = 0; i <= 100; i += 10) {
            lightbulb_set_cctb(0, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }
        for (i = 0; i <= 100; i += 10) {
            lightbulb_set_cctb(50, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }
        for (i = 0; i <= 100; i += 10) {
            lightbulb_set_cctb(100, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }
        lightbulb_set_switch(false);
    }

    // BIT4 color<->white
    if (mask & BIT(4)) {
        ESP_LOGW(TAG, "color<->white");
        lightbulb_set_switch(false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(50, 50);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
        lightbulb_set_hsv(50, 50, 50);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
        lightbulb_set_cctb(50, 50);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
        lightbulb_set_hsv(50, 50, 50);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_switch(false);
    }

    // BIT5 action (only color)
    if (mask & BIT(5)) {
        ESP_LOGW(TAG, "color effect");
        lightbulb_set_switch(false);
        lightbulb_effect_config_t effect1 = {
            .red = 255,
            .green = 0,
            .blue = 0,
            .max_brightness = 100,
            .min_brightness = 0,
            .effect_cycle_ms = 1000,
            .effect_type = EFFECT_BREATH,
            .mode = WORK_COLOR,
        };
        lightbulb_basic_effect_start(&effect1);
        vTaskDelay(pdMS_TO_TICKS(speed_ms) * 5);
        lightbulb_basic_effect_stop();
        vTaskDelay(pdMS_TO_TICKS(speed_ms) * 5);
        lightbulb_effect_config_t effect2 = {
            .red = 255,
            .green = 0,
            .blue = 0,
            .max_brightness = 100,
            .min_brightness = 0,
            .effect_cycle_ms = 1000,
            .effect_type = EFFECT_BLINK,
            .mode = WORK_COLOR,
        };
        lightbulb_basic_effect_start(&effect2);
        vTaskDelay(pdMS_TO_TICKS(speed_ms) * 5);
        lightbulb_basic_effect_stop_and_restore();
    }

    // BIT6 action (only white)
    if (mask & BIT(6)) {
        ESP_LOGW(TAG, "white effect");
        lightbulb_set_switch(false);
        lightbulb_effect_config_t effect1 = {
            .cct = 0,
            .max_brightness = 100,
            .min_brightness = 0,
            .effect_cycle_ms = 1000,
            .effect_type = EFFECT_BREATH,
            .mode = WORK_WHITE,
        };
        lightbulb_basic_effect_start(&effect1);
        vTaskDelay(pdMS_TO_TICKS(speed_ms) * 5);
        lightbulb_basic_effect_stop();
        vTaskDelay(pdMS_TO_TICKS(speed_ms) * 5);
        lightbulb_effect_config_t effect2 = {
            .cct = 100,
            .max_brightness = 100,
            .min_brightness = 0,
            .effect_cycle_ms = 1000,
            .effect_type = EFFECT_BLINK,
            .mode = WORK_WHITE,
        };
        lightbulb_basic_effect_start(&effect2);
        vTaskDelay(pdMS_TO_TICKS(speed_ms) * 5);
        lightbulb_basic_effect_stop_and_restore();
    }

    // BIT7 Alexa
    if (mask & BIT(7)) {
        ESP_LOGW(TAG, "Alexa");
        lightbulb_set_switch(false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(0, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(348, 90, 86); // 357 100 98
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(17, 51, 100); // 17 90 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(39, 100, 100); // 24 100 98
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(50, 100, 100); // 38 100 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(60, 100, 100); // 47 100 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(120, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(174, 71, 88); // 174 100 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(180, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(197, 42, 91); // 197 80 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(240, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(277, 86, 93); // 265 100 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(300, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(348, 25, 100); // 348 50 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(255, 50, 100); // 255 70 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
    }

    // BIT 8
    if (mask & BIT(8)) {
        ESP_LOGW(TAG, "color increment");
        lightbulb_set_switch(false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
        int i = 0;

        for (i = 0; i <= 100; i += 10) {
            lightbulb_set_hsv(17, 51, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }
        lightbulb_set_switch(false);
    }

    //BIT 9
    if (mask & BIT(9)) {
        ESP_LOGW(TAG, "white increment");
        lightbulb_set_switch(false);
        for (int i = 0; i <= 100; i += 10) {
            lightbulb_set_cctb(100, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }

        lightbulb_set_switch(false);
    }

    ESP_LOGW(TAG, "TEST DONE");
}

void alexa_light_query_hue_saturation_mapping(uint16_t hue, uint8_t saturation, uint16_t *out_hue, uint8_t *out_saturation)
{
    bool found_hue = false;
    bool found_saturation = false;
    int8_t index = -1;

    uint16_t src_hue[10] = { 348, 17, 39, 50, 60, 174, 197, 277, 348, 255 };
    uint16_t src_saturation[10] = { 90, 51, 100, 100, 100, 71, 42, 86, 25, 50 };
    uint16_t target_hue[10] = { 357, 17, 24, 38, 47, 174, 197, 265, 348, 255 };
    uint16_t target_saturation[10] = { 100, 90, 100, 100, 100, 100, 80, 100, 50, 70 };

    for (int i = 0; i < 10; i++) {
        if (hue == src_hue[i]) {
            found_hue = true;
            if (src_saturation[i] == saturation) {
                found_saturation = true;
                index = i;
                break;
            }
        }
    }

    if (found_hue && found_saturation && index != -1) {
        ESP_LOGW(TAG, "mapping success, index:%d", index);
        *out_hue = target_hue[index];
        *out_saturation = target_saturation[index];
    } else {
        *out_hue = hue;
        *out_saturation = saturation;
    }
    ESP_LOGI(TAG, "input[%d %d] output[%d %d]", hue, saturation, *out_hue, *out_saturation);
}
