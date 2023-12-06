/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "iot_board.h"

static const char *TAG = "example_blink";
static bool s_led_state = false;
#define BLINK_PERIOD_MS 600

void app_main(void)
{
    /* LED must be enable during board init, config through menuconfig */
    iot_board_init();

    while (1) {
        ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
        /* Toggle the LED state */
        iot_board_led_all_set_state(s_led_state);
        s_led_state = !s_led_state;
        vTaskDelay(BLINK_PERIOD_MS / portTICK_PERIOD_MS);
    }
}
