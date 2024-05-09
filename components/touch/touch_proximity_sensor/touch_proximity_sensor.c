/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "soc/soc_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/touch_pad.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "touch_proximity_sensor.h"

const static char *TAG = "touch-prox-sensor";
//todo: SOC_TOUCH_PROXIMITY_MEAS_DONE_SUPPORTED
#ifdef CONFIG_ENABLE_TOUCH_PROX_DEBUG
// print touch values(raw, smooth, benchmark) of each enabled channel, every 50ms
#define PRINT_VALUE(pad_num, raw, smooth, benchmark) printf("vl,%lld,%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32"\n", esp_timer_get_time() / 1000, pad_num, raw, smooth, benchmark)
// print trigger if touch active/inactive happens
#define PRINT_TRIGGER(pad_num, smooth, if_active) printf("tg,%lld,%"PRIu32",%"PRIu32",%d,%d,%d,%d,%d\n", esp_timer_get_time() / 1000, pad_num, smooth, if_active?1:0, if_active?0:1,0,0,0)
#else
#define PRINT_VALUE(pad_num, raw, smooth, benchmark)
#define PRINT_TRIGGER(pad_num, smooth, if_active)
#endif

typedef enum {
    OPERATE_RESET = -1,
    OPERATE_UPDATE,
    OPERATE_INIT,
    OPERATE_NONE,
} operate_t;

typedef struct {
    proxi_config_t configs;
    uint32_t baseline[TOUCH_PROXIMITY_NUM_MAX];
    uint32_t smooth[TOUCH_PROXIMITY_NUM_MAX];
    uint32_t active[TOUCH_PROXIMITY_NUM_MAX];
    proxi_cb_t proximity_callback;
    void *proximity_cb_arg;
    SemaphoreHandle_t proximity_daemon_task_stop;
} touch_proximity_sensor_t;

typedef struct touch_msg {
    touch_pad_intr_mask_t intr_mask;
    uint32_t pad_num;
    uint32_t pad_status;
    uint32_t pad_val;
} touch_event_t;

static QueueHandle_t s_queue = NULL;

static uint32_t preprocessing_proxi_raw_values(touch_proximity_sensor_t *sensor, uint32_t channel, uint32_t raw, operate_t opt)
{
    static int init_stage = 0;
    static float smooth_coef = 0.0;
    static float baseline_coef = 0.0;
    static float max_positive = 0.0;
    static float min_negative = 0.0;
    static float noise_positive = 0.0;
    static float noise_negative = 0.0;
    switch (opt) {
    case OPERATE_INIT:
        for (size_t i = 0; i < sensor->configs.channel_num; i++) {
            sensor->smooth[i] = 0;
            sensor->baseline[i] = 0;
        }
        smooth_coef = sensor->configs.smooth_coef;
        baseline_coef = sensor->configs.baseline_coef;
        max_positive = sensor->configs.max_p;
        min_negative = sensor->configs.min_n;
        noise_positive = sensor->configs.noise_p;
        noise_negative = sensor->configs.noise_n;
        init_stage = 1;
        break;
    case OPERATE_RESET:
        sensor->baseline[channel] = raw;
        break;
    case OPERATE_UPDATE: {
        int32_t diff = (int32_t)(raw - sensor->baseline[channel]);
        if (init_stage == 2 && ((diff > 0 && diff >= max_positive * sensor->baseline[channel])
                                || (diff < 0 && abs(diff) >= min_negative * sensor->baseline[channel]))) {
            ESP_LOGD(TAG, "CH%"PRIu32", %"PRIu32" is not an effective value", channel, raw);
        } else {
            if (init_stage == 1) {
                sensor->smooth[channel] = raw;
                sensor->baseline[channel] = sensor->smooth[channel];
                init_stage = 2;
            }
            sensor->smooth[channel] = sensor->smooth[channel] * (1.0 - smooth_coef) + raw * smooth_coef;
            diff = (int32_t)(sensor->smooth[channel] - sensor->baseline[channel]);
            if ((diff > 0 && diff <= noise_positive * sensor->baseline[channel])
                    || (diff < 0 && abs(diff) <= noise_negative * sensor->baseline[channel])) {
                sensor->baseline[channel] = sensor->baseline[channel] * (1.0 - baseline_coef) + sensor->smooth[channel] * baseline_coef;
            } else {
                sensor->baseline[channel] = sensor->baseline[channel] * (1.0 - baseline_coef / 4) + sensor->smooth[channel] * baseline_coef / 4;
            }
        }
    }
    break;
    default:
        break;
    }
    PRINT_VALUE(sensor->configs.channel_list[channel], raw, sensor->smooth[channel], sensor->baseline[channel]);
    return 0;
}

static uint32_t processing_proxi_state(touch_proximity_sensor_t *sensor, uint32_t channel, operate_t opt)
{
    static uint32_t debounce_p = 0;
    static uint32_t debounce_n = 0;
    static uint32_t reset_p = 0;
    static uint32_t reset_n = 0;
    static uint32_t debounce_p_counter[TOUCH_PROXIMITY_NUM_MAX] = {0};
    static uint32_t debounce_n_counter[TOUCH_PROXIMITY_NUM_MAX] = {0};
    static uint32_t reset_n_counter[TOUCH_PROXIMITY_NUM_MAX] = {0};
    static float threshold_p[TOUCH_PROXIMITY_NUM_MAX] = {0.0};
    static float threshold_n[TOUCH_PROXIMITY_NUM_MAX] = {0.0};
    static float hysteresis_p = 0.0;
    switch (opt) {
    case OPERATE_INIT:
    case OPERATE_RESET:
        for (size_t i = 0; i < sensor->configs.channel_num; i++) {
            debounce_p_counter[i] = 0;
            debounce_n_counter[i] = 0;
            reset_n_counter[i] = 0;
            threshold_p[i] = sensor->configs.threshold_p[i];
            threshold_n[i] = sensor->configs.threshold_n[i];
        }
        debounce_p = sensor->configs.debounce_p;
        debounce_n = sensor->configs.debounce_n;
        reset_p = sensor->configs.reset_p;
        reset_n = sensor->configs.reset_n;
        hysteresis_p = sensor->configs.hysteresis_p;
        break;
    case OPERATE_UPDATE: {
        int32_t diff = (int32_t)(sensor->smooth[channel] - sensor->baseline[channel]);
        if (diff > 0) {
            if (sensor->active[channel] == 0) {
                if (diff > threshold_p[channel] * sensor->baseline[channel] * (hysteresis_p + 1.0)) {
                    debounce_p_counter[channel] += 1;
                    if (debounce_p_counter[channel] >= debounce_p) {
                        sensor->active[channel] = 1;
                        ESP_LOGD(TAG, "CH%"PRIu32", active !", channel);
                        PRINT_TRIGGER(sensor->configs.channel_list[channel], sensor->smooth[channel], 1);
                        sensor->proximity_callback(sensor->configs.channel_list[channel], PROXI_EVT_ACTIVE, sensor->proximity_cb_arg);
                    }
                } else {
                    debounce_p_counter[channel] = 0;
                }
            }
            if (sensor->active[channel] == 1) {
                if (diff > threshold_p[channel] * sensor->baseline[channel] * (hysteresis_p + 1.0)) {
                    debounce_p_counter[channel] += 1;
                    debounce_n_counter[channel] = 0;
                    if (debounce_p_counter[channel] >= reset_p) {
                        //reset baseline and trigger release
                        debounce_p_counter[channel] = 0;
                        debounce_n_counter[channel] = 0;
                        sensor->active[channel] = 0;
                        PRINT_TRIGGER(sensor->configs.channel_list[channel], sensor->smooth[channel], 0);
                        ESP_LOGD(TAG, "CH%"PRIu32", inactive", channel);
                        ESP_LOGD(TAG, "Reset baseline");
                        preprocessing_proxi_raw_values(sensor, channel, sensor->smooth[channel], OPERATE_RESET);
                        sensor->proximity_callback(sensor->configs.channel_list[channel], PROXI_EVT_INACTIVE, sensor->proximity_cb_arg);
                    }
                } else if (diff < threshold_p[channel] * sensor->baseline[channel] * (1.0 - hysteresis_p)) {
                    debounce_n_counter[channel] += 1;
                    debounce_p_counter[channel] = 0;
                    if (debounce_n_counter[channel] >= debounce_n) {
                        debounce_n_counter[channel] = 0;
                        sensor->active[channel] = 0;
                        PRINT_TRIGGER(sensor->configs.channel_list[channel], sensor->smooth[channel], 0);
                        ESP_LOGD(TAG, "CH%"PRIu32", inactive", channel);
                        sensor->proximity_callback(sensor->configs.channel_list[channel], PROXI_EVT_INACTIVE, sensor->proximity_cb_arg);
                    }
                } else {
                    debounce_p_counter[channel] = 0;
                    debounce_n_counter[channel] = 0;
                }
            }
        } else {
            if (abs(diff) > threshold_n[channel] * sensor->baseline[channel]) {
                reset_n_counter[channel] += 1;
                if (reset_n_counter[channel] >= reset_n) {
                    reset_n_counter[channel] = 0;
                    ESP_LOGD(TAG, "Reset baseline");
                    preprocessing_proxi_raw_values(sensor, channel, sensor->smooth[channel], OPERATE_RESET);
                }
            }
        }
    }
    break;
    default:
        break;
    }
    ESP_LOGD(TAG, "CH%"PRIu32" %"PRIu32", debounce_p_n = %"PRIu32", %"PRIu32";  reset_n = %"PRIu32"", channel, sensor->active[channel], debounce_p_counter[channel], debounce_n_counter[channel], reset_n_counter[channel]);
    return sensor->active[channel];
}

static void _touch_intr_cb(void *arg)
{
    int task_awoken = pdFALSE;
    touch_event_t evt = {0};
    evt.intr_mask = touch_pad_read_intr_status_mask();
    evt.pad_status = touch_pad_get_status();
    evt.pad_num = touch_pad_get_current_meas_channel();
    if (!evt.intr_mask) {
        return;
    }
    if (evt.intr_mask & TOUCH_PAD_INTR_MASK_PROXI_MEAS_DONE) {
        touch_pad_proximity_get_data(evt.pad_num, &evt.pad_val);
    }
    xQueueSendFromISR(s_queue, &evt, &task_awoken);
    if (task_awoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void proxi_daemon_task(void *pvParameter)
{
    touch_proximity_sensor_t *sensor = (touch_proximity_sensor_t *)pvParameter;
    touch_event_t evt = {0};
    ESP_LOGI(TAG, "proxi daemon task start!");
    sensor->proximity_daemon_task_stop = xSemaphoreCreateBinary();
    if (sensor->proximity_daemon_task_stop == NULL) {
        ESP_LOGE(TAG, "proxi daemon task ctrl semaphore create failed!");
    }
    touch_pad_intr_enable(TOUCH_PAD_INTR_MASK_ALL);
    /* Wait touch sensor init done */
    vTaskDelay(200 / portTICK_PERIOD_MS);
    xQueueReset(s_queue);
    preprocessing_proxi_raw_values(sensor, 0, 0, OPERATE_INIT);
    processing_proxi_state(sensor, 0, OPERATE_INIT);
    while (1) {
        if (xSemaphoreTake(sensor->proximity_daemon_task_stop, pdMS_TO_TICKS(1)) == pdTRUE) {
            break;
        }
        int ret = xQueueReceive(s_queue, &evt, pdMS_TO_TICKS(50));
        if (ret != pdTRUE) {
            continue;
        }
        if (evt.intr_mask & TOUCH_PAD_INTR_MASK_PROXI_MEAS_DONE) {
            int i = 0;
            for (; i < sensor->configs.channel_num; i++) {
                if (sensor->configs.channel_list[i] == evt.pad_num) {
                    break;
                }
            }
            preprocessing_proxi_raw_values(sensor, i, evt.pad_val, OPERATE_UPDATE);
            processing_proxi_state(sensor, i, OPERATE_UPDATE);
        }
    }
    touch_pad_intr_disable(TOUCH_PAD_INTR_MASK_ALL);
    touch_pad_intr_clear(TOUCH_PAD_INTR_MASK_ALL);
    ESP_LOGI(TAG, "proxi daemon task exit!");
    vSemaphoreDelete(sensor->proximity_daemon_task_stop);
    sensor->proximity_daemon_task_stop = NULL;
    vTaskDelete(NULL);
}

esp_err_t touch_proximity_sensor_create(proxi_config_t *config, touch_proximity_handle_t *sensor_handle, proxi_cb_t cb, void *cb_arg)
{
    touch_proximity_sensor_t *proxi_sensor = (touch_proximity_sensor_t *) calloc(1, sizeof(touch_proximity_sensor_t));
    ESP_RETURN_ON_FALSE(proxi_sensor, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for touch_proximity_sensor_t.");
    proxi_sensor->configs = *config;
    proxi_sensor->proximity_callback = cb;
    proxi_sensor->proximity_cb_arg = cb_arg;
    *sensor_handle = (touch_proximity_handle_t)proxi_sensor;
    return ESP_OK;
}

esp_err_t touch_proximity_sensor_start(touch_proximity_handle_t proxi_sensor)
{
    /* Initialize touch pad peripheral. */
    ESP_LOGI(TAG, "IoT Touch Proximity Driver Version: %d.%d.%d",
             TOUCH_PROXIMITY_SENSOR_VER_MAJOR, TOUCH_PROXIMITY_SENSOR_VER_MINOR, TOUCH_PROXIMITY_SENSOR_VER_PATCH);
    ESP_RETURN_ON_FALSE(proxi_sensor, ESP_ERR_INVALID_ARG, TAG, "touch proximity sense not created yet, please create first.");
    touch_proximity_sensor_t *sensor = (touch_proximity_sensor_t *)(proxi_sensor);
    touch_pad_init();
    for (int i = 0; i < sensor->configs.channel_num; i++) {
        touch_pad_config(sensor->configs.channel_list[i]);
    }
    s_queue = xQueueCreate(32, sizeof(touch_event_t));
    ESP_RETURN_ON_FALSE(s_queue, ESP_FAIL, TAG, "Failed to create queue for touch pad.");
    /* Enable touch sensor clock. Work mode is "timer trigger". */
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    ESP_RETURN_ON_FALSE(sensor->configs.channel_num, ESP_ERR_INVALID_ARG, TAG, "The number of touch proximity sensor channels equals zero.");
    for (int i = 0; i < sensor->configs.channel_num; i++) {
        touch_pad_proximity_enable(sensor->configs.channel_list[i], true);
    }
    uint32_t count = sensor->configs.response_ms / sensor->configs.channel_num / 2;
    touch_pad_proximity_set_count(TOUCH_PAD_MAX, count);
    touch_pad_isr_register(_touch_intr_cb, sensor, TOUCH_PAD_INTR_MASK_ALL);
    touch_pad_fsm_start();
    vTaskDelay(40 / portTICK_PERIOD_MS);
    /* Start task to read values by pads. */
    xTaskCreate(&proxi_daemon_task, "proxi_daemon", 4096, sensor, 5, NULL);
    return ESP_OK;
}

esp_err_t touch_proximity_sensor_stop(touch_proximity_handle_t proxi_sensor)
{
    if (proxi_sensor == NULL) {
        return ESP_OK;
    }
    touch_proximity_sensor_t *sensor = (touch_proximity_sensor_t *)(proxi_sensor);
    touch_pad_fsm_stop();
    xSemaphoreGive(sensor->proximity_daemon_task_stop);
    touch_pad_deinit();
    return ESP_OK;
}

esp_err_t touch_proximity_sensor_delete(touch_proximity_handle_t proxi_sensor)
{
    if (proxi_sensor == NULL) {
        return ESP_OK;
    }
    touch_proximity_sensor_t *sensor = (touch_proximity_sensor_t *)(proxi_sensor);
    vQueueDelete(s_queue);
    s_queue = NULL;
    memset(&sensor->configs, 0, sizeof(sensor->configs));
    free(sensor);
    proxi_sensor = NULL;
    return ESP_OK;
}
