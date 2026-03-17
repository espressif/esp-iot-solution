/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file main.c
 * @brief LVGL benchmark demo
 *
 * Runs the official LVGL benchmark on the configured display.
 * All LCD / adapter / input-device setup is handled by example_lvgl_init().
 */

#include "esp_log.h"
#include "example_lvgl_init.h"
#include "lv_demos.h"

static const char *TAG = "main";

void app_main(void)
{
    example_lvgl_ctx_t ctx;
    ESP_ERROR_CHECK(example_lvgl_init(&ctx));

    ESP_LOGI(TAG, "Starting LVGL benchmark demo");
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        lv_demo_benchmark();
        esp_lv_adapter_unlock();
    }
}
