/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch_slider_sensor.h"

static uint32_t channel_list[] = {2, 4, 6, 12, 10, 8};
static float threshold[] = {0.005f, 0.005f, 0.005f, 0.005f, 0.005f, 0.005f};
static int channel_num = 6;
static int slider_range = 10000;

static void touch_slider_event_callback(touch_slider_handle_t handle, touch_slider_event_t event, int32_t data, void *cb_arg)
{
    if (event == TOUCH_SLIDER_EVENT_RIGHT_SWIPE) {
        printf("Right swipe (speed)\n");
    } else if (event == TOUCH_SLIDER_EVENT_LEFT_SWIPE) {
        printf("Left swipe (speed)\n");
    } else if (event == TOUCH_SLIDER_EVENT_RELEASE) {
        printf("Slide %" PRId32 "\n", data);
        if (data > slider_range / 10) {
            printf("Right swipe (displacement)\n");
        } else if (data < -slider_range / 10) {
            printf("Left swipe (displacement)\n");
        }
    } else if (event == TOUCH_SLIDER_EVENT_POSITION) {
        printf("pos,%" PRId32 "\n", data);
    }
}
/* Task function to handle touch slider events */
static void touch_slider_task(void *pvParameters)
{
    touch_slider_handle_t handle = (touch_slider_handle_t)pvParameters;
    while (1) {
        touch_slider_sensor_handle_events(handle);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    touch_slider_config_t config = {
        .channel_num = channel_num,
        .channel_list = channel_list,
        .channel_threshold = threshold,
        .filter_reset_times = 5,
        .position_range = 10000,
        .swipe_alpha = 0.9,
        .swipe_threshold = 50,
        .swipe_hysterisis = 40,
        .channel_gold_value = NULL,
        .debounce_times = 0,
        .skip_lowlevel_init = false
    };

    touch_slider_handle_t handle;
    ESP_ERROR_CHECK(touch_slider_sensor_create(&config, &handle, touch_slider_event_callback, NULL));

    // Create a task to handle touch slider events
    xTaskCreate(touch_slider_task, "touch_slider_sensor", 3072, handle, 2, NULL);

    // Main loop can do other things now
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Main task can do other work or just idle
    }
}
