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

#define WS2812_GPIO_NUM       CONFIG_EXAMPLE_WS2812_GPIO_NUM
#define WS2812_STRIPS_NUM     CONFIG_EXAMPLE_WS2812_STRIPS_NUM

#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

static const char *TAG = "led_strips";
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
#if WS2812_STRIPS_NUM > 1
    BLINK_FLOWING,
#endif
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
 * @brief Color gradient with a priority level of 5.
 *
 */
static const blink_step_t color_ring_blink[] = {
    /*!< Set Color to RED */
    {LED_BLINK_HSV, SET_HSV(0, MAX_SATURATION, MAX_BRIGHTNESS), 0},
    {LED_BLINK_HSV_RING, SET_HSV(MAX_HUE, MAX_SATURATION, MAX_BRIGHTNESS), 2000},
    {LED_BLINK_LOOP, 0, 0},
};

#if WS2812_STRIPS_NUM > 1
/**
 * @brief Flowing lights with a priority level of 6(lowest).
 *        Insert the index:MAX_INDEX to control all the strips
 *
 */
static const blink_step_t flowing_blink[] = {
    {LED_BLINK_HSV, SET_IHSV(MAX_INDEX, 0, MAX_SATURATION, MAX_BRIGHTNESS), 0},
    {LED_BLINK_HSV_RING, SET_IHSV(MAX_INDEX, MAX_HUE, MAX_SATURATION, MAX_BRIGHTNESS), 2000},
    {LED_BLINK_LOOP, 0, 0},
};
#endif

blink_step_t const *led_mode[] = {
    [BLINK_DOUBLE_RED] = double_red_blink,
    [BLINK_TRIPLE_GREEN] = triple_green_blink,
    [BLINK_WHITE_BREATHE_SLOW] = breath_white_slow_blink,
    [BLINK_WHITE_BREATHE_FAST] = breath_white_fast_blink,
    [BLINK_BLUE_BREATH] = breath_blue_blink,
    [BLINK_COLOR_RING] = color_ring_blink,
#if WS2812_STRIPS_NUM > 1
    [BLINK_FLOWING] = flowing_blink,
#endif
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

    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_GPIO_NUM,              // The GPIO that connected to the LED strip's data line
        .max_leds = WS2812_STRIPS_NUM,                  // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,       // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,                  // LED strip model
        .flags.invert_out = false,                      // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
#endif
    };

    led_indicator_strips_config_t strips_config = {
        .led_strip_cfg = strip_config,
        .led_strip_driver = LED_STRIP_RMT,
        .led_strip_rmt_cfg = rmt_config,
    };

    const led_indicator_config_t config = {
        .mode = LED_STRIPS_MODE,
        .led_indicator_strips_config = &strips_config,
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
