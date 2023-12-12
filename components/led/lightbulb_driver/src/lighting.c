/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lightbulb.h"

static const char *TAG = "output_test";

void lightbulb_lighting_output_test(lightbulb_lighting_unit_t mask, uint16_t speed_ms)
{
    // BIT0 rainbow (only color)
    if (mask & LIGHTING_RAINBOW) {
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
    if (mask & LIGHTING_WARM_TO_COLD) {
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
    if (mask & LIGHTING_COLD_TO_WARM) {
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
    if (mask & LIGHTING_BASIC_FIVE) {
        ESP_LOGW(TAG, "basic five");
        lightbulb_set_switch(false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(0, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(120, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(240, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(0, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_switch(false);
    }

    // BIT4 color<->white
    if (mask & LIGHTING_COLOR_MUTUAL_WHITE) {
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

    // BIT5 effect (only color)
    if (mask & LIGHTING_COLOR_EFFECT) {
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

    // BIT6 effect (only white)
    if (mask & LIGHTING_WHITE_EFFECT) {
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
    if (mask & LIGHTING_ALEXA) {
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
    if (mask & LIGHTING_COLOR_VALUE_INCREMENT) {
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
    if (mask & LIGHTING_WHITE_BRIGHTNESS_INCREMENT) {
        ESP_LOGW(TAG, "white increment");
        lightbulb_set_switch(false);
        for (int i = 0; i <= 100; i += 5) {
            lightbulb_set_cctb(50, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }

        lightbulb_set_switch(false);
    }

    ESP_LOGW(TAG, "TEST DONE");
}
