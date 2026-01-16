/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define LEAKS (120)

static const char *TAG = "test_main";

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    unity_utils_evaluate_leaks_direct(LEAKS);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Touch IC BS8112A3 Test Application");
    ESP_LOGI(TAG, "==================================");
    unity_run_menu();
}
