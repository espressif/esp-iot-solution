/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"
#include "iot_button.h"
#include "button_matrix.h"

static const char *TAG = "MATRIX BUTTON TEST";

static void button_event_cb(void *arg, void *data)
{
    button_event_t event = iot_button_get_event(arg);
    ESP_LOGI(TAG, "BUTTON[%d] %s", (int)data, iot_button_get_event_str(event));
    if (BUTTON_PRESS_REPEAT == event || BUTTON_PRESS_REPEAT_DONE == event) {
        ESP_LOGI(TAG, "\tREPEAT[%d]", iot_button_get_repeat(arg));
    }

    if (BUTTON_PRESS_UP == event || BUTTON_LONG_PRESS_HOLD == event || BUTTON_LONG_PRESS_UP == event) {
        ESP_LOGI(TAG, "\tPressed Time[%"PRIu32"]", iot_button_get_pressed_time(arg));
    }

    if (BUTTON_MULTIPLE_CLICK == event) {
        ESP_LOGI(TAG, "\tMULTIPLE[%d]", (int)data);
    }
}

TEST_CASE("matrix keyboard button test", "[button][matrix key]")
{
    const button_config_t btn_cfg = {0};
    const button_matrix_config_t matrix_cfg = {
        .row_gpios = (int32_t[]){4, 5, 6, 7},
        .col_gpios = (int32_t[]){3, 8, 16, 15},
        .row_gpio_num = 4,
        .col_gpio_num = 4,
    };

    button_handle_t btns[16] = {0};
    size_t btn_num = 16;
    esp_err_t ret = iot_button_new_matrix_device(&btn_cfg, &matrix_cfg, btns, &btn_num);
    TEST_ASSERT(ret == ESP_OK);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int index = i * 4 + j;
            TEST_ASSERT_NOT_NULL(btns[index]);
            iot_button_register_cb(btns[index], BUTTON_PRESS_DOWN, NULL, button_event_cb, (void *)index);
            iot_button_register_cb(btns[index], BUTTON_PRESS_UP, NULL, button_event_cb, (void *)index);
            iot_button_register_cb(btns[index], BUTTON_PRESS_REPEAT, NULL, button_event_cb, (void *)index);
            iot_button_register_cb(btns[index], BUTTON_PRESS_REPEAT_DONE, NULL, button_event_cb, (void *)index);
            iot_button_register_cb(btns[index], BUTTON_SINGLE_CLICK, NULL, button_event_cb, (void *)index);
            iot_button_register_cb(btns[index], BUTTON_DOUBLE_CLICK, NULL, button_event_cb, (void *)index);
            iot_button_register_cb(btns[index], BUTTON_LONG_PRESS_START, NULL, button_event_cb, (void *)index);
            iot_button_register_cb(btns[index], BUTTON_LONG_PRESS_HOLD, NULL, button_event_cb, (void *)index);
            iot_button_register_cb(btns[index], BUTTON_LONG_PRESS_UP, NULL, button_event_cb, (void *)index);
            iot_button_register_cb(btns[index], BUTTON_PRESS_END, NULL, button_event_cb, (void *)index);
        }
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            iot_button_delete(btns[i * 4 + j]);
        }
    }
}
