/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "touch_sensor_lowlevel.h"
#include "esp_timer.h"
#include "touch_channel.h"
#include "touch_digit.h"
#include "driver/touch_sensor.h"
#include "normalization_save.h"
#include "touch_image.h"
#include "dl_model_base.hpp"
#include "fbs_loader.hpp"
#include "dl_tensor_base.hpp"

#include "touch_digit_recognition.h"
#include "digital_tube.h"

static const char *TAG = "touch_digit";

#define REALLY_DATA_PRINT 0  // If you need to collect data using the host computer, please set this macro to true
#define SCAN_TIME_MEASURE 0

#define CHANNEL_NUM 13
#define CHANNEL_LIST {1,2,3,4,5,6,7,8,9,10,11,12,13}

#define ROW_CHANNEL_INDEX {4,6,8,7,10,9,12}
#define COL_CHANNEL_INDEX {2,1,3,5,13,11}

typedef uint32_t data_array_t[CHANNEL_NUM];

QueueHandle_t xImageQueue;

typedef enum {
    WAIT_WRITE,
    BEGIN_WRITE,
    END_WRITE,
} write_state_t;

static touch_digit_data_t g_data = {};
static TouchImage g_image = {};
static bool g_if_normalize = false;

esp_err_t printf_touch_digit_data(void)
{
    for (int i = 0; i < ROW_CHANNEL_NUM; i++) {
        g_data.row_data[i].print();
    }

    for (int i = 0; i < COL_CHANNEL_NUM; i++) {
        g_data.col_data[i].print();
    }
    return ESP_OK;
}

bool get_touch_dight_normalize_state(void)
{
    return g_if_normalize;
}

esp_err_t touch_dight_begin_normalize(void)
{
    ESP_LOGI(TAG, "touch_dight_begin_normalize");
    g_if_normalize = true;
    for (int i = 0; i < ROW_CHANNEL_NUM; i++) {
        g_data.row_data[i].reset();
    }

    for (int i = 0; i < COL_CHANNEL_NUM; i++) {
        g_data.col_data[i].reset();
    }
    return ESP_OK;
}

esp_err_t touch_dight_end_normalize(void)
{
    ESP_LOGI(TAG, "touch_dight_end_normalize");
    g_if_normalize = false;
    printf_touch_digit_data();
    touch_digit_data_t data = g_data;
    return set_normalization_data(&data);
}

static void touch_digit_normalize(data_array_t data_array)
{
    uint32_t row_channel_list[] = ROW_CHANNEL_INDEX;
    uint32_t col_channel_list[] = COL_CHANNEL_INDEX;
    for (int i = 0; i < ROW_CHANNEL_NUM; i++) {
        g_data.row_data[i].update_max_min(data_array[row_channel_list[i] - 1]);
    }

    for (int i = 0; i < COL_CHANNEL_NUM; i++) {
        g_data.col_data[i].update_max_min(data_array[col_channel_list[i] - 1]);
    }
}

static double compute_position(int position_a, int position_b, double value_a, double value_b)
{
    if (value_a + value_b == 0) {
        return 0;
    }

    return (position_a * value_a + position_b * value_b) / (value_a + value_b);
}

static bool touch_digit_detect(data_array_t data_array, int *x, int *y)
{
    uint32_t row_channel_list[] = ROW_CHANNEL_INDEX;
    uint32_t col_channel_list[] = COL_CHANNEL_INDEX;

    // 1. normalize
    int8_t max_row[2] = {0};
    double max_value = 0;
    for (int i = 0; i < ROW_CHANNEL_NUM; i++) {
        g_data.row_data[i].normalize(data_array[row_channel_list[i] - 1]);
        if (g_data.row_data[i].normalized_data > max_value) {
            max_value = g_data.row_data[i].normalized_data;
            max_row[0] = i;
        }
    }

    if (max_value < 0.15) {
        return false;
    }

    // if 0 or 7, make it's neighbor
    if (max_row[0] == 0) {
        max_row[1] = max_row[0] + 1;
    } else if (max_row[0] == ROW_CHANNEL_NUM - 1) {
        max_row[1] = max_row[0];
        max_row[0] = max_row[0] - 1;
    } else {
        if (g_data.row_data[max_row[0] - 1].normalized_data > g_data.row_data[max_row[0] + 1].normalized_data) {
            max_row[1] = max_row[0];
            max_row[0] = max_row[0] - 1;
        } else {
            max_row[1] = max_row[0] + 1;
        }
    }

    int8_t max_col[2] = {0};
    max_value = 0;
    for (int i = 0; i < COL_CHANNEL_NUM; i++) {
        g_data.col_data[i].normalize(data_array[col_channel_list[i] - 1]);
        if (g_data.col_data[i].normalized_data > max_value) {
            max_value = g_data.col_data[i].normalized_data;
            max_col[0] = i;
        }
    }

    if (max_value < 0.15) {
        return false;
    }

    if (max_col[0] == 0) {
        max_col[1] = max_col[0] + 1;
    } else if (max_col[0] == COL_CHANNEL_NUM - 1) {
        max_col[1] = max_col[0];
        max_col[0] = max_col[0] - 1;
    } else {
        if (g_data.col_data[max_col[0] - 1].normalized_data > g_data.col_data[max_col[0] + 1].normalized_data) {
            max_col[1] = max_col[0];
            max_col[0] = max_col[0] - 1;
        } else {
            max_col[1] = max_col[0] + 1;
        }
    }

    double error = compute_position(0, 1, g_data.row_data[max_row[0]].normalized_data, g_data.row_data[max_row[1]].normalized_data);
    int _x = max_row[0] * PRECISION + round(error * (PRECISION - 1));

    error = compute_position(0, 1, g_data.col_data[max_col[0]].normalized_data, g_data.col_data[max_col[1]].normalized_data);
    int _y = max_col[0] * PRECISION + round(error * (PRECISION - 1));

    *x = _x;
    *y = _y;
    return true;
}

typedef struct {
    uint8_t *data;
    size_t size;
} image_data_t;

static void touch_digit_task(void *arg)
{
#define IDEL_CNT_MAX 90
#if SCAN_TIME_MEASURE
    static uint32_t laster_time = esp_timer_get_time();
#endif
    QueueHandle_t data_queue = (QueueHandle_t)arg;
    data_array_t data_array = {0};
    write_state_t state = WAIT_WRITE;
    int idle_cnt = 0;
    while (1) {
        if (xQueueReceive(data_queue, &data_array, portMAX_DELAY) == pdTRUE) {

#if REALLY_DATA_PRINT
            for (int i = 0; i < CHANNEL_NUM; i++) {
                printf("%"PRIu32, data_array[i]);
                if (i != CHANNEL_NUM - 1) {
                    printf(",");
                }
            }
            printf("\n");
#else

#if SCAN_TIME_MEASURE
            uint32_t now_time = esp_timer_get_time();
            ESP_LOGI(TAG, "time: %lu ms\n", ((now_time - laster_time) / 1000));
            laster_time = now_time;
#endif
            if (g_if_normalize) {
                touch_digit_normalize(data_array);
                continue;
            }

            int x, y = -1;
            bool if_touch = touch_digit_detect(data_array, &x, &y);
            switch (state) {
            case WAIT_WRITE:
                if (if_touch) {
                    state = BEGIN_WRITE;
                }
                break;
            case BEGIN_WRITE:
                if (if_touch) {
                    g_image.set_pixel(x, y, 1);
                    idle_cnt = 0;
                } else {
                    idle_cnt++;
                    if (idle_cnt > IDEL_CNT_MAX) {
                        state = END_WRITE;
                        idle_cnt = 0;
                    }
                }
                break;
            case END_WRITE:
                // send data to dl inference
                g_image.print();
                image_data_t image_data;
                image_data.size = g_image.col_length * g_image.row_length;
                image_data.data = new uint8_t[image_data.size];
                if (image_data.data != NULL) {
                    memcpy(image_data.data, g_image.data, image_data.size);
                    xQueueSend(xImageQueue, &image_data, portMAX_DELAY);
                }

                g_image.clear();
                state = WAIT_WRITE;
                break;
            default:
                break;
            }
#endif
        }
    }
}

static void state_cb(uint32_t channel, touch_lowlevel_state_t state, void *state_values, void *arg)
{
    QueueHandle_t data_queue = (QueueHandle_t)arg;
    data_array_t data_array = {0};
    for (int i = 0; i < CHANNEL_NUM; i++) {
        touch_sensor_lowlevel_get_data(i + 1, &data_array[i]);
    }

    xQueueSendFromISR(data_queue, &data_array, NULL);
    return;
}

void touch_digit_recognition_task(void *arg)
{
    image_data_t image_data = {};

    TouchDigitRecognition *touch_digit_recognition = new TouchDigitRecognition("model", 25 * 30);

    while (1) {
        if (xQueueReceive(xImageQueue, &image_data, portMAX_DELAY) == pdTRUE) {
            digital_tube_write_num(0, touch_digit_recognition->predict(image_data.data));
            delete [] image_data.data;
        }
    }

    delete touch_digit_recognition;
    vTaskDelete(NULL);
}

esp_err_t touch_digit_init(void)
{
    // Init touch_digit_data
    get_normalization_data(&g_data);
    printf_touch_digit_data();

    // Create queue
    xImageQueue = xQueueCreate(10, sizeof(image_data_t));

    uint32_t channel_list[] = CHANNEL_LIST;
    touch_lowlevel_type_t channel_type[CHANNEL_NUM];
    for (int i = 0; i < CHANNEL_NUM; i++) {
        channel_type[i] = TOUCH_LOWLEVEL_TYPE_TOUCH;
    }

    touch_lowlevel_config_t low_config = {
        .channel_num = CHANNEL_NUM,
        .channel_list = channel_list,
        .channel_type = channel_type,
        .proximity_count = 0,
    };
    ESP_ERROR_CHECK(touch_sensor_lowlevel_create(&low_config));
    ESP_ERROR_CHECK(touch_sensor_lowlevel_start());
    touch_pad_set_charge_discharge_times(100);

    QueueHandle_t data_queue = xQueueCreate(10, sizeof(data_array_t));
    assert(data_queue != NULL);

    touch_lowlevel_handle_t handle = NULL;
    TaskHandle_t task_handle = NULL;
    xTaskCreate(touch_digit_task, "touch_digit_task", 4096, data_queue, 5, &task_handle);
    xTaskCreate(touch_digit_recognition_task, "touch_digit_recognition_task", 4096, NULL, 5, &task_handle);
    touch_sensor_lowlevel_register(channel_list[0], &state_cb, data_queue, &handle);
    return ESP_OK;
}
