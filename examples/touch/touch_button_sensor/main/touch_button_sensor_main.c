/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "touch_button_sensor.h"

#define TOUCH_BUTTON_EXAMPLE_PRINT_PERIOD_MS 1000
#define TOUCH_BUTTON_EXAMPLE_STATS_PERIOD_MS 10000
#define TOUCH_BUTTON_EXAMPLE_FALLBACK_SAMPLE_PERIOD_MS 20
#define TOUCH_BUTTON_EXAMPLE_WS2812_BRIGHTNESS 32

static const char *TAG = "touch_button_example";
static led_strip_handle_t s_status_led;

static uint32_t s_channels[] = {
#if CONFIG_TOUCH_BUTTON_EXAMPLE_CHANNEL_NUM >= 1
    CONFIG_TOUCH_BUTTON_EXAMPLE_CHANNEL_1,
#endif
#if CONFIG_TOUCH_BUTTON_EXAMPLE_CHANNEL_NUM >= 2
    CONFIG_TOUCH_BUTTON_EXAMPLE_CHANNEL_2,
#endif
#if CONFIG_TOUCH_BUTTON_EXAMPLE_CHANNEL_NUM >= 3
    CONFIG_TOUCH_BUTTON_EXAMPLE_CHANNEL_3,
#endif
#if CONFIG_TOUCH_BUTTON_EXAMPLE_CHANNEL_NUM >= 4
    CONFIG_TOUCH_BUTTON_EXAMPLE_CHANNEL_4,
#endif
};

static uint32_t s_thresholds[] = {
#if CONFIG_TOUCH_BUTTON_EXAMPLE_CHANNEL_NUM >= 1
    CONFIG_TOUCH_BUTTON_EXAMPLE_THRESHOLD,
#endif
#if CONFIG_TOUCH_BUTTON_EXAMPLE_CHANNEL_NUM >= 2
    CONFIG_TOUCH_BUTTON_EXAMPLE_THRESHOLD,
#endif
#if CONFIG_TOUCH_BUTTON_EXAMPLE_CHANNEL_NUM >= 3
    CONFIG_TOUCH_BUTTON_EXAMPLE_THRESHOLD,
#endif
#if CONFIG_TOUCH_BUTTON_EXAMPLE_CHANNEL_NUM >= 4
    CONFIG_TOUCH_BUTTON_EXAMPLE_THRESHOLD,
#endif
};

typedef struct {
    uint64_t presses;
    uint64_t releases;
    uint64_t previous_presses;
    int64_t pressed_at_us;
    int64_t longest_hold_us;
    bool active;
} channel_stats_t;

/* Callbacks and reporting run in the same handle_events task. */
static channel_stats_t s_stats[sizeof(s_channels) / sizeof(s_channels[0])];

static esp_err_t init_status_led(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_TOUCH_BUTTON_EXAMPLE_WS2812_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        },
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config,
                                             &s_status_led);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create WS2812 on GPIO %d: %s",
                 CONFIG_TOUCH_BUTTON_EXAMPLE_WS2812_GPIO, esp_err_to_name(ret));
        return ret;
    }
    return led_strip_clear(s_status_led);
}

static void refresh_status_led(void)
{
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    for (size_t i = 0; i < sizeof(s_stats) / sizeof(s_stats[0]); i++) {
        if (!s_stats[i].active) {
            continue;
        }
        if (i == 0) {
            red = TOUCH_BUTTON_EXAMPLE_WS2812_BRIGHTNESS;
        } else if (i == 1) {
            blue = TOUCH_BUTTON_EXAMPLE_WS2812_BRIGHTNESS;
        } else {
            green = TOUCH_BUTTON_EXAMPLE_WS2812_BRIGHTNESS;
        }
    }

    if (s_status_led == NULL) {
        return;
    }
    esp_err_t ret = led_strip_set_pixel(s_status_led, 0, red, green, blue);
    if (ret == ESP_OK) {
        ret = led_strip_refresh(s_status_led);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh WS2812: %s", esp_err_to_name(ret));
    }
}

static void print_stats(int64_t now_us)
{
    uint64_t total_presses = 0;
    uint64_t total_releases = 0;
    for (size_t i = 0; i < sizeof(s_stats) / sizeof(s_stats[0]); i++) {
        total_presses += s_stats[i].presses;
        total_releases += s_stats[i].releases;
    }
    ESP_LOGI(TAG, "STATS uptime_s=%" PRId64 " presses=%" PRIu64 " releases=%" PRIu64
             " heap_free=%zu heap_min=%zu", now_us / 1000000, total_presses, total_releases,
             heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    for (size_t i = 0; i < sizeof(s_stats) / sizeof(s_stats[0]); i++) {
        channel_stats_t *stats = &s_stats[i];
        int64_t held_us = stats->active ? now_us - stats->pressed_at_us : 0;
        int64_t longest_us = held_us > stats->longest_hold_us ? held_us : stats->longest_hold_us;
        ESP_LOGI(TAG, "STATS channel=%" PRIu32 " presses=%" PRIu64 " releases=%" PRIu64
                 " interval_presses=%" PRIu64 " state=%s held_ms=%" PRId64 " longest_hold_ms=%" PRId64,
                 s_channels[i], stats->presses, stats->releases,
                 stats->presses - stats->previous_presses, stats->active ? "ACTIVE" : "INACTIVE",
                 held_us / 1000, longest_us / 1000);
        stats->previous_presses = stats->presses;
    }
}

static void touch_button_callback(touch_button_handle_t handle, uint32_t channel,
                                  touch_state_t state, void *user_data)
{
    (void)user_data;
    int64_t now_us = esp_timer_get_time();
    for (size_t i = 0; i < sizeof(s_stats) / sizeof(s_stats[0]); i++) {
        if (s_channels[i] != channel) {
            continue;
        }
        channel_stats_t *stats = &s_stats[i];
        if (state == TOUCH_STATE_ACTIVE && !stats->active) {
            stats->active = true;
            stats->pressed_at_us = now_us;
            stats->presses++;
        } else if (state == TOUCH_STATE_INACTIVE && stats->active) {
            int64_t held_us = now_us - stats->pressed_at_us;
            stats->active = false;
            stats->releases++;
            if (held_us > stats->longest_hold_us) {
                stats->longest_hold_us = held_us;
            }
        }
        break;
    }
    refresh_status_led();
    uint32_t amplitude = 0;
    if (touch_button_sensor_get_data(handle, channel, &amplitude) != ESP_OK) {
        return;
    }

    ESP_LOGW(TAG, "channel=%" PRIu32 " state=%s amplitude=%" PRIu32 "\n",
             channel, state == TOUCH_STATE_ACTIVE ? "ACTIVE" : "INACTIVE",
             amplitude);
}

void app_main(void)
{
    ESP_ERROR_CHECK(init_status_led());

    touch_button_config_t config = {
        .channel_num = sizeof(s_channels) / sizeof(s_channels[0]),
        .channel_list = s_channels,
        .thresholds = s_thresholds,
        .sample_period_ms = CONFIG_TOUCH_BUTTON_EXAMPLE_SAMPLE_PERIOD_MS,
        .sample_cfg_num = CONFIG_TOUCH_BUTTON_EXAMPLE_SAMPLE_CFG_NUM,
        .debounce_active = 0,
        .debounce_inactive = 0,
        .skip_lowlevel_init = false,
    };

    touch_button_handle_t button = NULL;
    ESP_ERROR_CHECK(touch_button_sensor_create(&config, &button,
                                               touch_button_callback, NULL));
    ESP_LOGI(TAG, "Touch channel count=%" PRIu32 " threshold=%" PRIu32,
             config.channel_num, (uint32_t)CONFIG_TOUCH_BUTTON_EXAMPLE_THRESHOLD);
    for (uint32_t i = 0; i < config.channel_num; i++) {
        ESP_LOGI(TAG, "Touch channel[%" PRIu32 "]=%" PRIu32, i, s_channels[i]);
    }

    uint32_t sample_period_ms = config.sample_period_ms != 0 ? config.sample_period_ms :
                                TOUCH_BUTTON_EXAMPLE_FALLBACK_SAMPLE_PERIOD_MS;
    TickType_t sample_ticks = pdMS_TO_TICKS(sample_period_ms);
    if (sample_ticks == 0) {
        sample_ticks = 1;
    }
    TickType_t last_wake = xTaskGetTickCount();

    int64_t last_print_us = esp_timer_get_time();
    int64_t last_stats_us = last_print_us;
    while (true) {
        ESP_ERROR_CHECK(touch_button_sensor_handle_events(button));

        int64_t now_us = esp_timer_get_time();
        if (now_us - last_print_us >= TOUCH_BUTTON_EXAMPLE_PRINT_PERIOD_MS * 1000LL) {
            for (uint32_t i = 0; i < config.channel_num; i++) {
                uint32_t amplitude = 0;
                ESP_ERROR_CHECK(touch_button_sensor_get_data(button, s_channels[i],
                                                             &amplitude));
                ESP_LOGI(TAG, "channel=%" PRIu32 " amplitude=%" PRIu32,
                         s_channels[i], amplitude);
            }
            last_print_us = now_us;
        }
        if (now_us - last_stats_us >= TOUCH_BUTTON_EXAMPLE_STATS_PERIOD_MS * 1000LL) {
            print_stats(now_us);
            last_stats_us = now_us;
        }
        /* Include processing/printing time in the sampling-period schedule. */
        xTaskDelayUntil(&last_wake, sample_ticks);
    }
}
