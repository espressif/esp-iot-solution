/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lightbulb.h"

static const char *TAG = "output_test";

void lightbulb_lighting_output_test(lightbulb_handle_t handle, lightbulb_lighting_unit_t mask, uint16_t speed_ms)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "handle is NULL");
        return;
    }

    // BIT0 rainbow (only color)
    if (mask & LIGHTING_RAINBOW) {
        ESP_LOGW(TAG, "rainbow");
        lightbulb_set_switch(handle, false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
        // red
        lightbulb_set_hsv(handle, 0, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        // orange
        lightbulb_set_hsv(handle, 30, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        // yellow
        lightbulb_set_hsv(handle, 60, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        // green
        lightbulb_set_hsv(handle, 120, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        // blue
        lightbulb_set_hsv(handle, 210, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        // indigo
        lightbulb_set_hsv(handle, 240, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        // purple
        lightbulb_set_hsv(handle, 300, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
        lightbulb_set_switch(handle, false);
    }

    // BIT1 warm->cold
    if (mask & LIGHTING_WARM_TO_COLD) {
        ESP_LOGW(TAG, "warm->cold");
        lightbulb_set_switch(handle, false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(handle, 0, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(handle, 50, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(handle, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_switch(handle, false);
    }

    // BIT2 cold->warm
    if (mask & LIGHTING_COLD_TO_WARM) {
        ESP_LOGW(TAG, "cold->warm");
        lightbulb_set_switch(handle, false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(handle, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(handle, 50, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(handle, 0, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_switch(handle, false);
    }

    // BIT3 red green blue cct brightness 0->100
    if (mask & LIGHTING_BASIC_FIVE) {
        ESP_LOGW(TAG, "basic five lighting");
        lightbulb_set_switch(handle, false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 0, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 120, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 240, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(handle, 0, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(handle, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_switch(handle, false);
    }

    // BIT4 color<->white
    if (mask & LIGHTING_COLOR_MUTUAL_WHITE) {
        ESP_LOGW(TAG, "color<->white");
        lightbulb_set_switch(handle, false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_cctb(handle, 50, 50);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
        lightbulb_set_hsv(handle, 50, 50, 50);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
        lightbulb_set_cctb(handle, 50, 50);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
        lightbulb_set_hsv(handle, 50, 50, 50);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_switch(handle, false);
    }

    // BIT5 effect (only color)
    if (mask & LIGHTING_COLOR_EFFECT) {
        ESP_LOGW(TAG, "color effect");
        lightbulb_set_switch(handle, false);
        lightbulb_effect_config_t effect1 = {
            .hue = 0,
            .saturation = 0,
            .max_value_brightness = 100,
            .min_value_brightness = 0,
            .effect_cycle_ms = 1000,
            .effect_type = EFFECT_BREATH,
            .mode = WORK_COLOR,
        };
        lightbulb_basic_effect_start(handle, &effect1);
        vTaskDelay(pdMS_TO_TICKS(speed_ms) * 2);
        lightbulb_basic_effect_stop(handle);
        vTaskDelay(pdMS_TO_TICKS(speed_ms) * 2);
        lightbulb_effect_config_t effect2 = {
            .hue = 0,
            .saturation = 0,
            .max_value_brightness = 100,
            .min_value_brightness = 0,
            .effect_cycle_ms = 1000,
            .effect_type = EFFECT_BLINK,
            .mode = WORK_COLOR,
        };
        lightbulb_basic_effect_start(handle, &effect2);
        vTaskDelay(pdMS_TO_TICKS(speed_ms) * 2);
        lightbulb_basic_effect_stop_and_restore(handle);
    }

    // BIT6 effect (only white)
    if (mask & LIGHTING_WHITE_EFFECT) {
        ESP_LOGW(TAG, "white effect");
        lightbulb_set_switch(handle, false);
        lightbulb_effect_config_t effect1 = {
            .cct = 0,
            .max_value_brightness = 100,
            .min_value_brightness = 0,
            .effect_cycle_ms = 1000,
            .effect_type = EFFECT_BREATH,
            .mode = WORK_WHITE,
        };
        lightbulb_basic_effect_start(handle, &effect1);
        vTaskDelay(pdMS_TO_TICKS(speed_ms) * 2);
        lightbulb_basic_effect_stop(handle);
        vTaskDelay(pdMS_TO_TICKS(speed_ms) * 2);
        lightbulb_effect_config_t effect2 = {
            .cct = 100,
            .max_value_brightness = 100,
            .min_value_brightness = 0,
            .effect_cycle_ms = 1000,
            .effect_type = EFFECT_BLINK,
            .mode = WORK_WHITE,
        };
        lightbulb_basic_effect_start(handle, &effect2);
        vTaskDelay(pdMS_TO_TICKS(speed_ms) * 2);
        lightbulb_basic_effect_stop_and_restore(handle);
    }

    // BIT7 Alexa
    if (mask & LIGHTING_ALEXA) {
        ESP_LOGW(TAG, "Alexa");
        lightbulb_set_switch(handle, false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 0, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 348, 90, 86); // 357 100 98
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 17, 51, 100); // 17 90 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 39, 100, 100); // 24 100 98
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 50, 100, 100); // 38 100 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 60, 100, 100); // 47 100 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 120, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 174, 71, 88); // 174 100 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 180, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 197, 42, 91); // 197 80 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 240, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 277, 86, 93); // 265 100 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 300, 100, 100);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 348, 25, 100); // 348 50 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));

        lightbulb_set_hsv(handle, 255, 50, 100); // 255 70 100
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
    }

    // BIT 8
    if (mask & LIGHTING_COLOR_VALUE_INCREMENT) {
        ESP_LOGW(TAG, "color increment");
        lightbulb_set_switch(handle, false);
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
        int i = 0;

        for (i = 0; i <= 100; i += 10) {
            lightbulb_set_hsv(handle, 17, 51, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }
        lightbulb_set_switch(handle, false);
    }

    //BIT 9
    if (mask & LIGHTING_WHITE_BRIGHTNESS_INCREMENT) {
        ESP_LOGW(TAG, "white increment");
        lightbulb_set_switch(handle, false);
        for (int i = 0; i <= 100; i += 5) {
            lightbulb_set_cctb(handle, 50, i);
            vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }

        lightbulb_set_switch(handle, false);
    }

    ESP_LOGW(TAG, "TEST DONE");
}
