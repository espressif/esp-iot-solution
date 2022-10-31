// Copyright 2022-2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "soc/soc_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/touch_pad.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "touch_proximity_sensor.h"

static const char *TAG = "touch_prox";
//todo: SOC_TOUCH_PROXIMITY_MEAS_DONE_SUPPORTED
#ifdef CONFIG_ENABLE_TOUCH_PROX_DEBUG
// print touch values(raw, smooth, benchmark) of each enabled channel, every 50ms
#define PRINT_VALUE(pad_num, raw, smooth, benchmark) printf("vl,%lld,%u,%u,%u,%u\n", esp_timer_get_time() / 1000, pad_num, raw, smooth, benchmark)
// print trigger if touch active/inactive happens
#define PRINT_TRIGGER(pad_num, smooth, if_active) printf("tg,%lld,%u,%u,%d,%d,%d,%d,%d\n", esp_timer_get_time() / 1000, pad_num, smooth, if_active?1:0, if_active?0:1,0,0,0)
#else
#define PRINT_VALUE(pad_num, raw, smooth, benchmark)
#define PRINT_TRIGGER(pad_num, smooth, if_active)
#endif

typedef enum {
    OPERATE_RESET = -1,
    OPERATE_UPDATE = 0,
    OPERATE_INIT,
    OPERATE_NONE,
} operate_t;

static proxi_config_t s_proxi_configs = {0.0};
static QueueHandle_t s_queue = NULL;
static volatile uint32_t s_proxi_task_state = 1;
static uint32_t s_baseline[TOUCH_PROXIMITY_NUM_MAX] = {0};
static uint32_t s_smooth[TOUCH_PROXIMITY_NUM_MAX] = {0};
static uint32_t s_active[TOUCH_PROXIMITY_NUM_MAX] = {0};
static proxi_cb_t s_proxi_callback = NULL;
static void *s_proxi_arg = NULL;

uint32_t preprocessing_proxi_raw_values(proxi_config_t *config, uint32_t channel, uint32_t raw, operate_t opt)
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
        for (size_t i = 0; i < s_proxi_configs.channel_num; i++) {
            s_smooth[i] = 0;
            s_baseline[i] = 0;
        }
        smooth_coef = config->smooth_coef;
        baseline_coef = config->baseline_coef;
        max_positive = config->max_p;
        min_negative = config->min_n;
        noise_positive = config->noise_p;
        noise_negative = config->noise_n;
        init_stage = 1;
        break;
    case OPERATE_RESET:
        s_baseline[channel] = raw;
        break;
    case OPERATE_UPDATE: {
        int32_t diff = (int32_t)(raw - s_baseline[channel]);
        if (init_stage == 2 && ((diff > 0 && diff >= max_positive * s_baseline[channel])
                                || (diff < 0 && abs(diff) >= min_negative * s_baseline[channel]))) {
            ESP_LOGD(TAG, "CH%u, %u is not an effective value", channel, raw);
        } else {
            if (init_stage == 1) {
                s_smooth[channel] = raw;
                s_baseline[channel] = s_smooth[channel];
                init_stage = 2;
            }
            s_smooth[channel] = s_smooth[channel] * (1.0 - smooth_coef) + raw * smooth_coef;
            diff = (int32_t)(s_smooth[channel] - s_baseline[channel]);
            if ((diff > 0 && diff <= noise_positive * s_baseline[channel])
                    || (diff < 0 && abs(diff) <= noise_negative * s_baseline[channel])) {
                s_baseline[channel] = s_baseline[channel] * (1.0 - baseline_coef) + s_smooth[channel] * baseline_coef;
            } else {
                s_baseline[channel] = s_baseline[channel] * (1.0 - baseline_coef / 4) + s_smooth[channel] * baseline_coef / 4;
            }
        }
    }
    break;
    default:
        break;
    }
    PRINT_VALUE(s_proxi_configs.channel_list[channel], raw, s_smooth[channel], s_baseline[channel]);
    return 0;
}

uint32_t processing_proxi_state(proxi_config_t *config, uint32_t channel, operate_t opt)
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
        for (size_t i = 0; i < s_proxi_configs.channel_num; i++) {
            debounce_p_counter[i] = 0;
            debounce_n_counter[i] = 0;
            reset_n_counter[i] = 0;
            threshold_p[i] = config->threshold_p[i];
            threshold_n[i] = config->threshold_n[i];
        }
        debounce_p = config->debounce_p;
        debounce_n = config->debounce_n;
        reset_p = config->reset_p;
        reset_n = config->reset_n;
        hysteresis_p = config->hysteresis_p;
        break;
    case OPERATE_UPDATE: {
        int32_t diff = (int32_t)(s_smooth[channel] - s_baseline[channel]);
        if (diff > 0) {
            if ( s_active[channel] == 0 ) {
                if (diff > threshold_p[channel] * s_baseline[channel] * (hysteresis_p + 1.0)) {
                    debounce_p_counter[channel] += 1;
                    if (debounce_p_counter[channel] >= debounce_p) {
                        s_active[channel] = 1;
                        ESP_LOGD(TAG, "CH%u, active !", channel);
                        PRINT_TRIGGER(s_proxi_configs.channel_list[channel], s_smooth[channel], 1);
                        s_proxi_callback(s_proxi_configs.channel_list[channel], PROXI_EVT_ACTIVE, s_proxi_arg);
                    }
                } else {
                    debounce_p_counter[channel] = 0;
                }
            }
            if ( s_active[channel] == 1 ) {
                if (diff > threshold_p[channel] * s_baseline[channel] * (hysteresis_p + 1.0)) {
                    debounce_p_counter[channel] += 1;
                    debounce_n_counter[channel] = 0;
                    if (debounce_p_counter[channel] >= reset_p) {
                        //reset baseline and trigger release
                        debounce_p_counter[channel] = 0;
                        debounce_n_counter[channel] = 0;
                        s_active[channel] = 0;
                        PRINT_TRIGGER(s_proxi_configs.channel_list[channel], s_smooth[channel], 0);
                        ESP_LOGD(TAG, "CH%u, inactive", channel);
                        ESP_LOGD(TAG, "Reset baseline");
                        preprocessing_proxi_raw_values(&s_proxi_configs, channel, s_smooth[channel], OPERATE_RESET);
                        s_proxi_callback(s_proxi_configs.channel_list[channel], PROXI_EVT_INACTIVE, s_proxi_arg);
                    }
                } else if (diff < threshold_p[channel] * s_baseline[channel] * (1.0 - hysteresis_p)) {
                    debounce_n_counter[channel] += 1;
                    debounce_p_counter[channel] = 0;
                    if (debounce_n_counter[channel] >= debounce_n) {
                        debounce_n_counter[channel] = 0;
                        s_active[channel] = 0;
                        PRINT_TRIGGER(s_proxi_configs.channel_list[channel], s_smooth[channel], 0);
                        ESP_LOGD(TAG, "CH%u, inactive", channel);
                        s_proxi_callback(s_proxi_configs.channel_list[channel], PROXI_EVT_INACTIVE, s_proxi_arg);
                    }
                } else {
                    debounce_p_counter[channel] = 0;
                    debounce_n_counter[channel] = 0;
                }
            }
        } else {
            if (abs(diff) > threshold_n[channel] * s_baseline[channel]) {
                reset_n_counter[channel] += 1;
                if (reset_n_counter[channel] >= reset_n) {
                    reset_n_counter[channel] = 0;
                    ESP_LOGD(TAG, "Reset baseline");
                    preprocessing_proxi_raw_values(&s_proxi_configs, channel, s_smooth[channel], OPERATE_RESET);
                }
            }
        }
    }
    break;
    default:
        break;
    }
    ESP_LOGD(TAG, "CH%u %u, debounce_p_n = %u, %u;  reset_n = %u", channel, s_active[channel], debounce_p_counter[channel], debounce_n_counter[channel], reset_n_counter[channel]);
    return s_active[channel];
}

typedef struct touch_msg {
    touch_pad_intr_mask_t intr_mask;
    uint32_t pad_num;
    uint32_t pad_status;
    uint32_t pad_val;
} touch_event_t;

static void test_touch_intr_cb(void *arg)
{
    QueueHandle_t queue = (QueueHandle_t)arg;
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

    xQueueSendFromISR(queue, &evt, &task_awoken);
    if (task_awoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void proxi_daemon_task(void *pvParameter)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameter;
    touch_event_t evt = {0};
    ESP_LOGI(TAG, "proxi daemon task start!\n");
    touch_pad_intr_enable(TOUCH_PAD_INTR_MASK_ALL);
    /* Wait touch sensor init done */
    vTaskDelay(200 / portTICK_RATE_MS);
    xQueueReset(queue);
    preprocessing_proxi_raw_values(&s_proxi_configs, 0, 0, OPERATE_INIT);
    processing_proxi_state(&s_proxi_configs, 0, OPERATE_INIT);
    while (s_proxi_task_state) {
        int ret = xQueueReceive(queue, &evt, pdMS_TO_TICKS(50));
        if (ret != pdTRUE) {
            continue;
        }
        if (evt.intr_mask & TOUCH_PAD_INTR_MASK_PROXI_MEAS_DONE) {
            size_t i = 0;
            for (; i < s_proxi_configs.channel_num; i++) {
                if (s_proxi_configs.channel_list[i] == evt.pad_num) {
                    break;
                }
            }
            preprocessing_proxi_raw_values(&s_proxi_configs, i, evt.pad_val, OPERATE_UPDATE);
            processing_proxi_state(&s_proxi_configs, i, OPERATE_UPDATE);
        }
    }
    s_proxi_task_state = 1;
    ESP_LOGI(TAG, "proxi daemon task exit!\n");
}

esp_err_t touch_proximity_sense_start(proxi_config_t *config, proxi_cb_t cb, void *cb_arg)
{
    /* Initialize touch pad peripheral. */
    s_proxi_configs = *config;
    s_proxi_callback = cb;
    s_proxi_arg = cb_arg;
    touch_pad_init();
    for (int i = 0; i < s_proxi_configs.channel_num; i++) {
        touch_pad_config(s_proxi_configs.channel_list[i]);
    }
    s_queue = xQueueCreate(32, sizeof(touch_event_t));
    /* Enable touch sensor clock. Work mode is "timer trigger". */
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    for (int i = 0; i < s_proxi_configs.channel_num; i++) {
        touch_pad_proximity_enable(s_proxi_configs.channel_list[i], true);
    }
    uint32_t count = s_proxi_configs.response_ms / s_proxi_configs.channel_num / 2;
    touch_pad_proximity_set_count(TOUCH_PAD_MAX, count); //50 约 100ms？？
    touch_pad_isr_register(test_touch_intr_cb, (void *)s_queue, TOUCH_PAD_INTR_MASK_ALL);
    touch_pad_fsm_start();
    vTaskDelay(40 / portTICK_PERIOD_MS);
    /* Start task to read values by pads. */
    xTaskCreate(&proxi_daemon_task, "proxi_daemon", 4096, (void *)s_queue, 5, NULL);
    return ESP_OK;
}


esp_err_t touch_proximity_sense_stop(void)
{
    touch_pad_fsm_stop();
    s_proxi_task_state = 0;
    while (s_proxi_task_state != 1) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGI(TAG, "proxi daemon task exit!\n");
    touch_pad_deinit();
    vQueueDelete(s_queue);
    memset(&s_proxi_configs, 0, sizeof(s_proxi_configs));
    return ESP_OK;
}