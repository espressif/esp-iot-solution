/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gprof.h"
#include "test_func.h"

void app_main(void)
{
    ESP_ERROR_CHECK(esp_gprof_init());
    test_func();
    ESP_ERROR_CHECK(esp_gprof_save());
    esp_gprof_deinit();
}
