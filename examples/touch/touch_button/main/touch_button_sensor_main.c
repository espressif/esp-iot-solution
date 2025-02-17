/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch_button_sensor.h"

#if CONFIG_IDF_TARGET_ESP32P4
uint32_t channel_list[] = {0, 1, 2, 3, 4, 5}; // gpio 2, 3, 4, 5, 6, 7
float channel_threshold[] = {0.01, 0.01, 0.01, 0.01, 0.01, 0.01}; // 1%
#elif CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32S2
uint32_t channel_list[] = {1, 2, 3, 4, 5, 6}; // gpio 1, 2, 3, 4, 5, 6
float channel_threshold[] = {0.01, 0.01, 0.01, 0.01, 0.01, 0.01}; // 1%
#endif

#if CONFIG_LOG_DEFAULT_LEVEL > 3
#define ENABLE_TOUCH_VALUE_PRINT 1
#define ENABLE_ACTIVE_BITMAP_PRINT 1
#endif

static void touch_state_callback(touch_button_handle_t handle, uint32_t channel, touch_state_t state, void *user_data)
{
    uint32_t bitmap = 0;
    touch_button_sensor_get_state_bitmap(handle, channel, &bitmap);

    uint32_t data[SOC_TOUCH_SAMPLE_CFG_NUM] = {0};
    if (state == TOUCH_STATE_ACTIVE) {
        for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
            touch_button_sensor_get_data(handle, channel, i, &data[i]);
        }
        printf("Channel %"PRIu32" is Active", channel);
        // if log level is set to debug, print the bitmap
#if ENABLE_ACTIVE_BITMAP_PRINT
        printf(", state bitmap: 0x%08" PRIx32, bitmap);
#endif
        printf("\n");
    } else if (state == TOUCH_STATE_INACTIVE) {
        for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
            touch_button_sensor_get_data(handle, channel, i, &data[i]);
        }
        printf("Channel %"PRIu32" is Inactive", channel);
#if ENABLE_ACTIVE_BITMAP_PRINT
        printf(", state bitmap:0x%08" PRIx32, bitmap);
#endif
        printf("\n");
    }
}

void app_main(void)
{
    touch_button_config_t config = {
        .channel_num = sizeof(channel_list) / sizeof(channel_list[0]),
        .channel_list = channel_list,
        .channel_threshold = channel_threshold,
        .debounce_times = 2,
        .skip_lowlevel_init = false
    };

    touch_button_handle_t handle;
    ESP_ERROR_CHECK(touch_button_sensor_create(&config, &handle, touch_state_callback, handle));

#if ENABLE_TOUCH_VALUE_PRINT
    uint32_t *data = malloc(SOC_TOUCH_SAMPLE_CFG_NUM * sizeof(uint32_t) * config.channel_num);
    size_t str_len = 10 + (6 + 10) * config.channel_num;
    // Calculate required string length: "freq[x]: " + ("chX: YYYY, " * channel_num) for each line
    char *print_string = malloc(str_len);
#endif

    while (1) {
        touch_button_sensor_handle_events(handle);
#if ENABLE_TOUCH_VALUE_PRINT
        for (int i = 0; i < config.channel_num; i++) {
            for (int j = 0; j < SOC_TOUCH_SAMPLE_CFG_NUM; j++) {
                touch_button_sensor_get_data(handle, i, j, &data[i * SOC_TOUCH_SAMPLE_CFG_NUM + j]);
            }
        }
        for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
            int offset = snprintf(print_string, str_len, "freq[%d]: ", i);
            for (int j = 0; j < config.channel_num; j++) {
                offset += snprintf(print_string + offset, str_len - offset, "ch%d: %"PRIu32 ", ", j, data[j * SOC_TOUCH_SAMPLE_CFG_NUM + i]);
            }
            printf("%s\n", print_string);
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
