/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "unity.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "driver/gpio.h"

static const char *TAG = "BUTTON AUTO TEST";

#define GPIO_OUTPUT_IO_45 45
#define BUTTON_IO_NUM  0
#define BUTTON_ACTIVE_LEVEL   0

static EventGroupHandle_t g_check = NULL;
static SemaphoreHandle_t g_auto_check_pass = NULL;

static button_event_t state = BUTTON_PRESS_DOWN;

static void button_auto_press_test_task(void *arg)
{
    // test BUTTON_PRESS_DOWN
    xEventGroupWaitBits(g_check, BIT(0) | BIT(1), pdTRUE, pdTRUE, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    // // test BUTTON_PRESS_UP
    xEventGroupWaitBits(g_check, BIT(0) | BIT(1), pdTRUE, pdTRUE, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    // test BUTTON_PRESS_REPEAT
    xEventGroupWaitBits(g_check, BIT(0) | BIT(1), pdTRUE, pdTRUE, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    // test BUTTON_PRESS_REPEAT_DONE
    xEventGroupWaitBits(g_check, BIT(0) | BIT(1), pdTRUE, pdTRUE, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    // test BUTTON_SINGLE_CLICK
    xEventGroupWaitBits(g_check, BIT(0) | BIT(1), pdTRUE, pdTRUE, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    // test BUTTON_DOUBLE_CLICK
    xEventGroupWaitBits(g_check, BIT(0) | BIT(1), pdTRUE, pdTRUE, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    // test BUTTON_MULTIPLE_CLICK
    xEventGroupWaitBits(g_check, BIT(0) | BIT(1), pdTRUE, pdTRUE, portMAX_DELAY);
    for (int i = 0; i < 4; i++) {
        gpio_set_level(GPIO_OUTPUT_IO_45, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(GPIO_OUTPUT_IO_45, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    // test BUTTON_LONG_PRESS_START
    xEventGroupWaitBits(g_check, BIT(0) | BIT(1), pdTRUE, pdTRUE, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 0);
    vTaskDelay(pdMS_TO_TICKS(1600));

    // test BUTTON_LONG_PRESS_HOLD and BUTTON_LONG_PRESS_UP
    xEventGroupWaitBits(g_check, BIT(0) | BIT(1), pdTRUE, pdTRUE, portMAX_DELAY);
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);

    ESP_LOGI(TAG, "Auto Press Success!");
    vTaskDelete(NULL);
}
static void button_auto_check_cb_1(void *arg, void *data)
{
    if (iot_button_get_event(arg) == state) {
        xEventGroupSetBits(g_check, BIT(1));
    }
}
static void button_auto_check_cb(void *arg, void *data)
{
    if (iot_button_get_event(arg) == state) {
        ESP_LOGI(TAG, "Auto check: button event %s pass", iot_button_get_event_str(state));
        xEventGroupSetBits(g_check, BIT(0));
        if (++state >= BUTTON_EVENT_MAX) {
            xSemaphoreGive(g_auto_check_pass);
            return;
        }
    }
}

TEST_CASE("gpio button auto-test", "[button][iot][auto]")
{
    state = BUTTON_PRESS_DOWN;
    g_check = xEventGroupCreate();
    g_auto_check_pass = xSemaphoreCreateBinary();
    xEventGroupSetBits(g_check, BIT(0) | BIT(1));
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BUTTON_IO_NUM,
        .active_level = BUTTON_ACTIVE_LEVEL,
    };

    button_handle_t btn = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NOT_NULL(btn);

    /* register iot_button callback for all the button_event */
    for (uint8_t i = 0; i < BUTTON_EVENT_MAX; i++) {
        if (i == BUTTON_MULTIPLE_CLICK) {
            button_event_args_t args = {
                .multiple_clicks.clicks = 4,
            };
            iot_button_register_cb(btn, BUTTON_MULTIPLE_CLICK, &args, button_auto_check_cb_1, NULL);
            iot_button_register_cb(btn, BUTTON_MULTIPLE_CLICK, &args, button_auto_check_cb, NULL);
        } else {
            iot_button_register_cb(btn, i, NULL, button_auto_check_cb_1, NULL);
            iot_button_register_cb(btn, i, NULL, button_auto_check_cb, NULL);
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, iot_button_set_param(btn, BUTTON_LONG_PRESS_TIME_MS, (void *)1500));

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_OUTPUT_IO_45),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);

    xTaskCreate(button_auto_press_test_task, "button_auto_press_test_task", 1024 * 4, NULL, 10, NULL);

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(g_auto_check_pass, pdMS_TO_TICKS(6000)));

    for (uint8_t i = 0; i < BUTTON_EVENT_MAX; i++) {
        button_event_args_t args;

        if (i == BUTTON_MULTIPLE_CLICK) {
            args.multiple_clicks.clicks = 4;
            iot_button_unregister_cb(btn, i, &args);
        } else if (i == BUTTON_LONG_PRESS_UP || i == BUTTON_LONG_PRESS_START) {
            args.long_press.press_time = 1500;
            iot_button_unregister_cb(btn, i, &args);
        } else {
            iot_button_unregister_cb(btn, i, NULL);
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, iot_button_delete(btn));
    vEventGroupDelete(g_check);
    vSemaphoreDelete(g_auto_check_pass);
    vTaskDelay(pdMS_TO_TICKS(100));
}

#define TOLERANCE (CONFIG_BUTTON_PERIOD_TIME_MS * 4)

uint16_t long_press_time[5] = {2000, 2500, 3000, 3500, 4000};
static SemaphoreHandle_t long_press_check = NULL;
static SemaphoreHandle_t long_press_auto_check_pass = NULL;
unsigned int status = 0;

static void button_auto_long_press_test_task(void *arg)
{
    // Test for BUTTON_LONG_PRESS_START
    for (int i = 0; i < 5; i++) {
        xSemaphoreTake(long_press_check, portMAX_DELAY);
        gpio_set_level(GPIO_OUTPUT_IO_45, 0);
        status = (BUTTON_LONG_PRESS_START << 16) | long_press_time[i];
        if (i > 0) {
            vTaskDelay(pdMS_TO_TICKS(long_press_time[i] - long_press_time[i - 1]));
        } else {
            vTaskDelay(pdMS_TO_TICKS(long_press_time[i]));
        }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    xSemaphoreGive(long_press_auto_check_pass);
    vTaskDelay(pdMS_TO_TICKS(100));
    // Test for BUTTON_LONG_PRESS_UP
    for (int i = 0; i < 5; i++) {
        xSemaphoreTake(long_press_check, portMAX_DELAY);
        status = (BUTTON_LONG_PRESS_UP << 16) | long_press_time[i];
        gpio_set_level(GPIO_OUTPUT_IO_45, 0);
        vTaskDelay(pdMS_TO_TICKS(long_press_time[i] + 10));
        gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    }

    ESP_LOGI(TAG, "Auto Long Press Success!");
    vTaskDelete(NULL);
}

static void button_long_press_auto_check_cb(void *arg, void *data)
{
    uint32_t value = (uint32_t)data;
    uint16_t event = (0xffff0000 & value) >> 16;
    uint16_t time = 0xffff & value;
    uint32_t pressed_time = iot_button_get_pressed_time(arg);
    int32_t diff = pressed_time - time;
    if (status == value && abs(diff) <= TOLERANCE) {
        ESP_LOGI(TAG, "Auto check: button event: %s and time: %d pass", iot_button_get_event_str(event), time);

        if (event == BUTTON_LONG_PRESS_UP && time == long_press_time[4]) {
            xSemaphoreGive(long_press_auto_check_pass);
        }

        xSemaphoreGive(long_press_check);
    }
}

TEST_CASE("gpio button long_press auto-test", "[button][long_press][auto]")
{
    ESP_LOGI(TAG, "Starting the test");
    long_press_check = xSemaphoreCreateBinary();
    long_press_auto_check_pass = xSemaphoreCreateBinary();
    xSemaphoreGive(long_press_check);
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BUTTON_IO_NUM,
        .active_level = BUTTON_ACTIVE_LEVEL,
    };

    button_handle_t btn = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NOT_NULL(btn);

    for (int i = 0; i < 5; i++) {
        button_event_args_t args = {
            .long_press.press_time = long_press_time[i],
        };

        uint32_t data = (BUTTON_LONG_PRESS_START << 16) | long_press_time[i];
        iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, &args, button_long_press_auto_check_cb, (void*)data);
    }

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_OUTPUT_IO_45),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    gpio_set_level(GPIO_OUTPUT_IO_45, 1);
    xTaskCreate(button_auto_long_press_test_task, "button_auto_long_press_test_task", 1024 * 4, NULL, 10, NULL);

    xSemaphoreTake(long_press_auto_check_pass, portMAX_DELAY);
    iot_button_unregister_cb(btn, BUTTON_LONG_PRESS_START, NULL);

    for (int i = 0; i < 5; i++) {
        button_event_args_t args = {
            .long_press.press_time = long_press_time[i]
        };

        uint32_t data = (BUTTON_LONG_PRESS_UP << 16) | long_press_time[i];
        iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, &args, button_long_press_auto_check_cb, (void*)data);
    }
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(long_press_auto_check_pass, pdMS_TO_TICKS(17000)));
    TEST_ASSERT_EQUAL(ESP_OK, iot_button_delete(btn));
    vSemaphoreDelete(long_press_check);
    vSemaphoreDelete(long_press_auto_check_pass);
    vTaskDelay(pdMS_TO_TICKS(100));
}
