/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cmd_led_indicator.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "led_indicator.h"

#define GPIO_LED_PIN       CONFIG_EXAMPLE_GPIO_NUM
#define GPIO_ACTIVE_LEVEL  CONFIG_EXAMPLE_GPIO_ACTIVE_LEVEL
#define LEDC_LED_CHANNEL   CONFIG_EXAMPLE_LEDC_CHANNEL

static const char *TAG = "led_ledc";
static led_indicator_handle_t led_handle = NULL;

/**
 * @brief Define blinking type and priority.
 *
 */
enum {
    BLINK_DOUBLE = 0,
    BLINK_TRIPLE,
    BLINK_BRIGHT_75_PERCENT,
    BLINK_BRIGHT_25_PERCENT,
    BLINK_BREATHE_SLOW,
    BLINK_BREATHE_FAST,
    BLINK_MAX,
};

/**
 * @brief Blinking twice times has a priority level of 0 (highest).
 *
 */
static const blink_step_t double_blink[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_STOP, 0, 0},
};

/**
 * @brief Blinking three times has a priority level of 1.
 *
 */
static const blink_step_t triple_blink[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_STOP, 0, 0},
};

/**
 * @brief Brightness set to 75% with a priority level of 2.
 *
 */
static const blink_step_t bright_75_percent[] = {
    {LED_BLINK_BRIGHTNESS, LED_STATE_75_PERCENT, 0},
    {LED_BLINK_STOP, 0, 0},
};

/**
 * @brief Brightness set to 25% with a priority level of 3.
 *
 */
static const blink_step_t bright_25_percent[] = {
    {LED_BLINK_BRIGHTNESS, LED_STATE_25_PERCENT, 0},
    {LED_BLINK_STOP, 0, 0},
};

/**
 * @brief Slow breathing with a priority level of 4.
 *
 */
static const blink_step_t breath_slow_blink[] = {
    {LED_BLINK_HOLD, LED_STATE_OFF, 0},
    {LED_BLINK_BREATHE, LED_STATE_ON, 1000},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 1000},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Fast breathing with a priority level of 5(lowest).
 *
 */
static const blink_step_t breath_fast_blink[] = {
    {LED_BLINK_HOLD, LED_STATE_OFF, 0},
    {LED_BLINK_BREATHE, LED_STATE_ON, 500},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 500},
    {LED_BLINK_LOOP, 0, 0},
};

blink_step_t const *led_mode[] = {
    [BLINK_DOUBLE] = double_blink,
    [BLINK_TRIPLE] = triple_blink,
    [BLINK_BRIGHT_75_PERCENT] = bright_75_percent,
    [BLINK_BRIGHT_25_PERCENT] = bright_25_percent,
    [BLINK_BREATHE_SLOW] = breath_slow_blink,
    [BLINK_BREATHE_FAST] = breath_fast_blink,
    [BLINK_MAX] = NULL,
};

#if CONFIG_EXAMPLE_ENABLE_CONSOLE_CONTROL
static void led_cmd_cb(cmd_type_t cmd_type, uint32_t mode_index)
{
    switch (cmd_type) {
    case START:
        led_indicator_start(led_handle, mode_index);
        ESP_LOGI(TAG, "start blink: %"PRIu32"", mode_index);
        break;
    case STOP:
        led_indicator_stop(led_handle, mode_index);
        ESP_LOGI(TAG, "stop blink: %"PRIu32"", mode_index);
        break;
    case PREEMPT_START:
        led_indicator_preempt_start(led_handle, mode_index);
        ESP_LOGI(TAG, "preempt start blink: %"PRIu32"", mode_index);
        break;
    case PREEMPT_STOP:
        led_indicator_preempt_stop(led_handle, mode_index);
        ESP_LOGI(TAG, "preempt stop blink: %"PRIu32"", mode_index);
        break;
    default:
        ESP_LOGE(TAG, "unknown cmd type: %d", cmd_type);
        break;
    }
}
#endif

void app_main(void)
{
    led_indicator_ledc_config_t ledc_config = {
        .is_active_level_high = GPIO_ACTIVE_LEVEL,
        .timer_inited = false,
        .timer_num = LEDC_TIMER_0,
        .gpio_num = GPIO_LED_PIN,
        .channel = LEDC_LED_CHANNEL,
    };

    const led_indicator_config_t config = {
        .mode = LED_LEDC_MODE,
        .led_indicator_ledc_config = &ledc_config,
        .blink_lists = led_mode,
        .blink_list_num = BLINK_MAX,
    };

    led_handle = led_indicator_create(&config);
    assert(led_handle != NULL);

#if CONFIG_EXAMPLE_ENABLE_CONSOLE_CONTROL
    cmd_led_indicator_t cmd_led_indicator = {
        .cmd_cb = led_cmd_cb,
        .mode_count = BLINK_MAX,
    };

    ESP_ERROR_CHECK(cmd_led_indicator_init(&cmd_led_indicator));
#else
    while (1) {
        for (int i = 0; i < BLINK_MAX; i++) {
            led_indicator_start(led_handle, i);
            ESP_LOGI(TAG, "start blink: %d", i);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            led_indicator_stop(led_handle, i);
            ESP_LOGI(TAG, "stop blink: %d", i);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
#endif
}
