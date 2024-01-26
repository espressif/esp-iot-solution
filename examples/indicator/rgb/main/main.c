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

#define GPIO_LED_RED_PIN       CONFIG_EXAMPLE_GPIO_RED_NUM
#define GPIO_LED_GREEN_PIN     CONFIG_EXAMPLE_GPIO_GREEN_NUM
#define GPIO_LED_BLUE_PIN      CONFIG_EXAMPLE_GPIO_BLUE_NUM
#define GPIO_ACTIVE_LEVEL      CONFIG_EXAMPLE_GPIO_ACTIVE_LEVEL
#define LEDC_LED_RED_CHANNEL   CONFIG_EXAMPLE_LEDC_RED_CHANNEL
#define LEDC_LED_GREEN_CHANNEL CONFIG_EXAMPLE_LEDC_GREEN_CHANNEL
#define LEDC_LED_BLUE_CHANNEL  CONFIG_EXAMPLE_LEDC_BLUE_CHANNEL

static const char *TAG = "led_rgb";
static led_indicator_handle_t led_handle = NULL;

/**
 * @brief Define blinking type and priority.
 *
 */
enum {
    BLINK_DOUBLE_RED = 0,
    BLINK_TRIPLE_GREEN,
    BLINK_WHITE_BREATHE_SLOW,
    BLINK_WHITE_BREATHE_FAST,
    BLINK_BLUE_BREATH,
    BLINK_COLOR_RING,
    BLINK_MAX,
};

/**
 * @brief Blinking twice times in red has a priority level of 0 (highest).
 *
 */
static const blink_step_t double_red_blink[] = {
    /*!< Set color to red by R:255 G:0 B:0 */
    {LED_BLINK_RGB, SET_RGB(255, 0, 0), 0},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_STOP, 0, 0},
};

/**
 * @brief Blinking three times in green with a priority level of 1.
 *
 */
static const blink_step_t triple_green_blink[] = {
    /*!< Set color to green by R:0 G:255 B:0 */
    {LED_BLINK_RGB, SET_RGB(0, 255, 0), 0},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_STOP, 0, 0},
};

/**
 * @brief Slow breathing in white with a priority level of 2.
 *
 */
static const blink_step_t breath_white_slow_blink[] = {
    /*!< Set Color to white and brightness to zero by H:0 S:0 V:0 */
    {LED_BLINK_HSV, SET_HSV(0, 0, 0), 0},
    {LED_BLINK_BREATHE, LED_STATE_ON, 1000},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 1000},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Fast breathing in white with a priority level of 3.
 *
 */
static const blink_step_t breath_white_fast_blink[] = {
    /*!< Set Color to white and brightness to zero by H:0 S:0 V:0 */
    {LED_BLINK_HSV, SET_HSV(0, 0, 0), 0},
    {LED_BLINK_BREATHE, LED_STATE_ON, 500},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 500},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Breathing in green with a priority level of 4.
 *
 */
static const blink_step_t breath_blue_blink[] = {
    /*!< Set Color to blue and brightness to zero by H:240 S:255 V:0 */
    {LED_BLINK_HSV, SET_HSV(240, MAX_SATURATION, 0), 0},
    {LED_BLINK_BREATHE, LED_STATE_ON, 1000},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 1000},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Color gradient with a priority level of 5(lowest).
 *
 */
static const blink_step_t color_ring_blink[] = {
    /*!< Set Color to RED */
    {LED_BLINK_HSV, SET_HSV(0, MAX_SATURATION, MAX_BRIGHTNESS), 0},
    {LED_BLINK_HSV_RING, SET_HSV(MAX_HUE, MAX_SATURATION, MAX_BRIGHTNESS), 2000},
    {LED_BLINK_LOOP, 0, 0},
};

blink_step_t const *led_mode[] = {
    [BLINK_DOUBLE_RED] = double_red_blink,
    [BLINK_TRIPLE_GREEN] = triple_green_blink,
    [BLINK_WHITE_BREATHE_SLOW] = breath_white_slow_blink,
    [BLINK_WHITE_BREATHE_FAST] = breath_white_fast_blink,
    [BLINK_BLUE_BREATH] = breath_blue_blink,
    [BLINK_COLOR_RING] = color_ring_blink,
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
    led_indicator_rgb_config_t rgb_config = {
        .is_active_level_high = GPIO_ACTIVE_LEVEL,
        .timer_inited = false,
        .timer_num = LEDC_TIMER_0,
        .red_gpio_num = GPIO_LED_RED_PIN,
        .green_gpio_num = GPIO_LED_GREEN_PIN,
        .blue_gpio_num = GPIO_LED_BLUE_PIN,
        .red_channel = LEDC_LED_RED_CHANNEL,
        .green_channel = LEDC_LED_GREEN_CHANNEL,
        .blue_channel = LEDC_LED_BLUE_CHANNEL,
    };

    const led_indicator_config_t config = {
        .mode = LED_RGB_MODE,
        .led_indicator_rgb_config = &rgb_config,
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
