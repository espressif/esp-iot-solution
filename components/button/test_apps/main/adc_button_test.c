/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "unity.h"
#include "iot_button.h"
#include "button_adc.h"

static const char *TAG = "ADC BUTTON TEST";

static void button_event_cb(void *arg, void *data)
{
    button_event_t event = iot_button_get_event(arg);
    ESP_LOGI(TAG, "BTN[%d] %s", (int)data, iot_button_get_event_str(event));
    if (BUTTON_PRESS_REPEAT == event || BUTTON_PRESS_REPEAT_DONE == event) {
        ESP_LOGI(TAG, "\tREPEAT[%d]", iot_button_get_repeat(arg));
    }

    if (BUTTON_PRESS_UP == event || BUTTON_LONG_PRESS_HOLD == event || BUTTON_LONG_PRESS_UP == event) {
        ESP_LOGI(TAG, "\tTICKS[%"PRIu32"]", iot_button_get_ticks_time(arg));
    }

    if (BUTTON_MULTIPLE_CLICK == event) {
        ESP_LOGI(TAG, "\tMULTIPLE[%d]", (int)data);
    }
}

TEST_CASE("adc button test", "[button][adc]")
{
    /** ESP32-S3-Korvo2 board */
    const button_config_t btn_cfg = {0};
    button_adc_config_t btn_adc_cfg = {
        .unit_id = ADC_UNIT_1,
        .adc_channel = 4,
    };

    button_handle_t btns[6] = {NULL};

    const uint16_t vol[6] = {380, 820, 1180, 1570, 1980, 2410};
    for (size_t i = 0; i < 6; i++) {
        btn_adc_cfg.button_index = i;
        if (i == 0) {
            btn_adc_cfg.min = (0 + vol[i]) / 2;
        } else {
            btn_adc_cfg.min = (vol[i - 1] + vol[i]) / 2;
        }

        if (i == 5) {
            btn_adc_cfg.max = (vol[i] + 3000) / 2;
        } else {
            btn_adc_cfg.max = (vol[i] + vol[i + 1]) / 2;
        }

        esp_err_t ret = iot_button_new_adc_device(&btn_cfg, &btn_adc_cfg, &btns[i]);
        TEST_ASSERT(ret == ESP_OK);
        TEST_ASSERT_NOT_NULL(btns[i]);
        iot_button_register_cb(btns[i], BUTTON_PRESS_DOWN, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_PRESS_UP, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_PRESS_REPEAT, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_PRESS_REPEAT_DONE, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_SINGLE_CLICK, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_DOUBLE_CLICK, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_LONG_PRESS_START, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_LONG_PRESS_HOLD, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_LONG_PRESS_UP, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_PRESS_END, NULL, button_event_cb, (void *)i);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    for (size_t i = 0; i < 6; i++) {
        iot_button_delete(btns[i]);
    }
}

TEST_CASE("adc button test memory leak", "[button][adc][memory leak]")
{
    /** ESP32-S3-Korvo2 board */
    const button_config_t btn_cfg = {0};
    button_adc_config_t btn_adc_cfg = {
        .unit_id = ADC_UNIT_1,
        .adc_channel = 4,
    };

    button_handle_t btns[6] = {NULL};

    const uint16_t vol[6] = {380, 820, 1180, 1570, 1980, 2410};
    for (size_t i = 0; i < 6; i++) {
        btn_adc_cfg.button_index = i;
        if (i == 0) {
            btn_adc_cfg.min = (0 + vol[i]) / 2;
        } else {
            btn_adc_cfg.min = (vol[i - 1] + vol[i]) / 2;
        }

        if (i == 5) {
            btn_adc_cfg.max = (vol[i] + 3000) / 2;
        } else {
            btn_adc_cfg.max = (vol[i] + vol[i + 1]) / 2;
        }

        esp_err_t ret = iot_button_new_adc_device(&btn_cfg, &btn_adc_cfg, &btns[i]);
        TEST_ASSERT(ret == ESP_OK);

        TEST_ASSERT_NOT_NULL(btns[i]);
        iot_button_register_cb(btns[i], BUTTON_PRESS_DOWN, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_PRESS_UP, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_PRESS_REPEAT, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_PRESS_REPEAT_DONE, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_SINGLE_CLICK, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_DOUBLE_CLICK, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_LONG_PRESS_START, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_LONG_PRESS_HOLD, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_LONG_PRESS_UP, NULL, button_event_cb, (void *)i);
        iot_button_register_cb(btns[i], BUTTON_PRESS_END, NULL, button_event_cb, (void *)i);
    }

    for (size_t i = 0; i < 6; i++) {
        iot_button_delete(btns[i]);
    }
}
