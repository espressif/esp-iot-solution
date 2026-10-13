/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "airmouse_test.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#define TAG "AIRMOUSE_TEST"
#define AIRMOUSE_TEST_SYNC_SWITCH_THRESHOLD_US 50000
#define AIRMOUSE_TEST_SAMPLE_DELAY_US 3000000
#define AIRMOUSE_TEST_ANGLE_STEP_COUNT 14
#define AIRMOUSE_REALTIME_TRIAL_COUNT 60
// TODO: Tune with stationary gyro logs and real servo-motion data.
#define AIRMOUSE_REALTIME_GYRO_THRESHOLD_DPS 15.0f

typedef enum {
    AIRMOUSE_TEST_ANGLE_ACCURACY = 0,
    AIRMOUSE_TEST_STATIC_STABILITY,
    AIRMOUSE_TEST_REALTIME,
    AIRMOUSE_TEST_COUNT
} airmouse_test_mode_t;

typedef struct {
    const char *axis;
    int target_deg;
} airmouse_test_angle_point_t;

typedef struct {
    uint32_t sensor_us;
    uint32_t pose_us;
    uint32_t event_us;
    uint32_t hid_us;
    bool valid;
} airmouse_realtime_result_t;

static const airmouse_test_angle_point_t s_angle_test_points[AIRMOUSE_TEST_ANGLE_STEP_COUNT] = {
    {"YAW", -30},
    {"YAW", -20},
    {"YAW", -10},
    {"YAW", 0},
    {"YAW", 10},
    {"YAW", 20},
    {"YAW", 30},
    {"PITCH", -30},
    {"PITCH", -20},
    {"PITCH", -10},
    {"PITCH", 0},
    {"PITCH", 10},
    {"PITCH", 20},
    {"PITCH", 30},
};

static const int s_static_targets[3] = {
    30,
    -30,
    0,
};

static volatile int64_t s_test_sync_rise_us;
static volatile int64_t s_step_event_rise_us;
static volatile bool s_step_event_pending;
static volatile bool s_mode_switch_pending;
static portMUX_TYPE s_test_sync_lock = portMUX_INITIALIZER_UNLOCKED;

static airmouse_test_mode_t s_current_test = AIRMOUSE_TEST_ANGLE_ACCURACY;
static uint8_t s_test_step;
static bool s_pending_sample;
static int64_t s_sample_deadline_us;
static uint8_t s_pending_step;
static bool s_pose_origin_valid;
static float s_pose_origin_yaw_deg;
static float s_pose_origin_pitch_deg;
static uint8_t s_static_group;
static uint8_t s_static_step;
static bool s_static_window_active;
static uint32_t s_static_sample_count;
static double s_static_sum;
static double s_static_sum_sq;
static float s_static_first;
static float s_static_last;
static float s_static_min;
static float s_static_max;
static airmouse_realtime_result_t
    s_realtime_results[AIRMOUSE_REALTIME_TRIAL_COUNT];
static uint8_t s_realtime_group;
static uint8_t s_realtime_trial;
static bool s_realtime_armed;
static int64_t s_realtime_t0;
static int64_t s_realtime_t1;
static int64_t s_realtime_t2;
static int64_t s_realtime_t3;
static int64_t s_realtime_trigger_sample_us;
static bool s_realtime_imu_found;
static bool s_realtime_pose_done;
static bool s_realtime_event_done;
static bool s_test_initialized;

static const char *airmouse_test_mode_name(airmouse_test_mode_t mode)
{
    switch (mode) {
    case AIRMOUSE_TEST_ANGLE_ACCURACY:
        return "ANGLE_ACCURACY";
    case AIRMOUSE_TEST_STATIC_STABILITY:
        return "STATIC_STABILITY";
    case AIRMOUSE_TEST_REALTIME:
        return "REALTIME";
    default:
        return "UNKNOWN";
    }
}

static void airmouse_test_static_reset_window(void)
{
    s_static_sample_count = 0;
    s_static_sum = 0.0;
    s_static_sum_sq = 0.0;
    s_static_first = 0.0f;
    s_static_last = 0.0f;
    s_static_min = 0.0f;
    s_static_max = 0.0f;
}

static void airmouse_test_realtime_reset_group(void)
{
    for (uint8_t i = 0; i < AIRMOUSE_REALTIME_TRIAL_COUNT; i++) {
        s_realtime_results[i].valid = false;
        s_realtime_results[i].sensor_us = 0;
        s_realtime_results[i].pose_us = 0;
        s_realtime_results[i].event_us = 0;
        s_realtime_results[i].hid_us = 0;
    }

    s_realtime_trial = 0;
    s_realtime_armed = false;
    s_realtime_t0 = 0;
    s_realtime_t1 = 0;
    s_realtime_t2 = 0;
    s_realtime_t3 = 0;
    s_realtime_trigger_sample_us = 0;
    s_realtime_imu_found = false;
    s_realtime_pose_done = false;
    s_realtime_event_done = false;
}

static void airmouse_test_realtime_sort_u32(uint32_t values[], uint8_t count)
{
    for (uint8_t i = 1; i < count; i++) {
        const uint32_t key = values[i];
        uint8_t j = i;
        while (j > 0 && values[j - 1] > key) {
            values[j] = values[j - 1];
            j--;
        }
        values[j] = key;
    }
}

static void airmouse_test_realtime_metric_summary(
    const char *name,
    const uint32_t values[],
    uint8_t count)
{
    if (count == 0) {
        ESP_LOGI(TAG, "TEST3_SUMMARY,%s_count=0", name);
        return;
    }

    uint32_t sorted[AIRMOUSE_REALTIME_TRIAL_COUNT] = {};
    uint64_t sum = 0;
    uint32_t max_value = 0;
    for (uint8_t i = 0; i < count; i++) {
        sorted[i] = values[i];
        sum += values[i];
        if (values[i] > max_value) {
            max_value = values[i];
        }
    }

    airmouse_test_realtime_sort_u32(sorted, count);
    uint8_t p95_index = (uint8_t)(((uint16_t)count * 95 + 99) / 100);
    if (p95_index == 0) {
        p95_index = 1;
    }
    p95_index--;
    if (p95_index >= count) {
        p95_index = count - 1;
    }

    ESP_LOGI(TAG,
             "TEST3_SUMMARY,%s_mean_us=%llu,%s_p95_us=%u,%s_max_us=%u",
             name,
             (unsigned long long)(sum / count),
             name,
             (unsigned int)sorted[p95_index],
             name,
             (unsigned int)max_value);
}

static void airmouse_test_realtime_output_group(void)
{
    const char *axis = (s_realtime_group == 0) ? "YAW" : "PITCH";
    uint32_t sensor_values[AIRMOUSE_REALTIME_TRIAL_COUNT] = {};
    uint32_t pose_values[AIRMOUSE_REALTIME_TRIAL_COUNT] = {};
    uint32_t event_values[AIRMOUSE_REALTIME_TRIAL_COUNT] = {};
    uint32_t hid_values[AIRMOUSE_REALTIME_TRIAL_COUNT] = {};
    uint8_t valid_count = 0;

    for (uint8_t i = 0; i < AIRMOUSE_REALTIME_TRIAL_COUNT; i++) {
        const airmouse_realtime_result_t *result = &s_realtime_results[i];
        if (!result->valid) {
            continue;
        }

        ESP_LOGI(TAG,
                 "TEST3,group=%u,axis=%s,trial=%u,sensor_us=%u,pose_us=%u,event_us=%u,hid_us=%u",
                 (unsigned int)s_realtime_group,
                 axis,
                 (unsigned int)i,
                 (unsigned int)result->sensor_us,
                 (unsigned int)result->pose_us,
                 (unsigned int)result->event_us,
                 (unsigned int)result->hid_us);

        sensor_values[valid_count] = result->sensor_us;
        pose_values[valid_count] = result->pose_us;
        event_values[valid_count] = result->event_us;
        hid_values[valid_count] = result->hid_us;
        valid_count++;
    }

    ESP_LOGI(TAG, "TEST3_SUMMARY,group=%u,axis=%s,count=%u",
             (unsigned int)s_realtime_group,
             axis,
             (unsigned int)valid_count);
    airmouse_test_realtime_metric_summary("sensor", sensor_values, valid_count);
    airmouse_test_realtime_metric_summary("pose", pose_values, valid_count);
    airmouse_test_realtime_metric_summary("event", event_values, valid_count);
    airmouse_test_realtime_metric_summary("hid", hid_values, valid_count);

    s_realtime_group++;
    if (s_realtime_group >= 2) {
        s_realtime_group = 0;
    }
    airmouse_test_realtime_reset_group();
}

static void airmouse_test_sync_isr(void *arg)
{
    (void)arg;

    const int level = gpio_get_level((gpio_num_t)AIRMOUSE_TEST_SYNC_GPIO);
    const int64_t now_us = esp_timer_get_time();

    if (level != 0) {
        s_test_sync_rise_us = now_us;
        return;
    }

    const int64_t rise_us = s_test_sync_rise_us;
    if (rise_us <= 0 || now_us <= rise_us) {
        return;
    }

    const int64_t pulse_width_us = now_us - rise_us;
    portENTER_CRITICAL_ISR(&s_test_sync_lock);
    if (pulse_width_us < AIRMOUSE_TEST_SYNC_SWITCH_THRESHOLD_US) {
        s_step_event_rise_us = rise_us;
        s_step_event_pending = true;
    } else {
        s_mode_switch_pending = true;
    }
    portEXIT_CRITICAL_ISR(&s_test_sync_lock);
}

static void airmouse_test_handle_mode_switch(void)
{
    s_current_test = (airmouse_test_mode_t)((s_current_test + 1) %
                                           AIRMOUSE_TEST_COUNT);
    s_test_step = 0;
    s_pending_sample = false;
    s_static_group = 0;
    s_static_step = 0;
    s_static_window_active = false;
    airmouse_test_static_reset_window();
    s_realtime_group = 0;
    airmouse_test_realtime_reset_group();
    if (s_current_test == AIRMOUSE_TEST_ANGLE_ACCURACY) {
        s_pose_origin_valid = false;
    }

    ESP_LOGI(TAG, "TEST_MODE,%u,%s",
             (unsigned int)s_current_test + 1,
             airmouse_test_mode_name(s_current_test));
}

static void airmouse_test_handle_step(int64_t now_us,
                                      int64_t step_rise_us)
{
    if (s_current_test == AIRMOUSE_TEST_ANGLE_ACCURACY) {
        s_pending_step = s_test_step;
        s_sample_deadline_us = now_us + AIRMOUSE_TEST_SAMPLE_DELAY_US;
        s_pending_sample = true;
        s_test_step = (uint8_t)((s_test_step + 1) %
                                AIRMOUSE_TEST_ANGLE_STEP_COUNT);
        return;
    }

    if (s_current_test == AIRMOUSE_TEST_REALTIME) {
        if (s_realtime_armed ||
                s_realtime_trial >= AIRMOUSE_REALTIME_TRIAL_COUNT) {
            return;
        }

        s_realtime_t0 = step_rise_us;
        s_realtime_t1 = 0;
        s_realtime_t2 = 0;
        s_realtime_t3 = 0;
        s_realtime_trigger_sample_us = 0;
        s_realtime_imu_found = false;
        s_realtime_pose_done = false;
        s_realtime_event_done = false;
        s_realtime_armed = true;
        return;
    }

    if (s_current_test != AIRMOUSE_TEST_STATIC_STABILITY) {
        return;
    }

    const char *axis = (s_static_group == 0) ? "YAW" : "PITCH";
    if (!s_static_window_active) {
        s_static_step = 0;
        s_static_window_active = true;
        airmouse_test_static_reset_window();
        ESP_LOGI(TAG, "TEST2_START,group=%s,target=%d",
                 axis,
                 s_static_targets[s_static_step]);
        return;
    }

    if (s_static_sample_count > 0) {
        const double count = (double)s_static_sample_count;
        const double mean = s_static_sum / count;
        double variance = s_static_sum_sq / count - mean * mean;
        if (variance < 0.0) {
            variance = 0.0;
        }
        const double std = sqrt(variance);
        const float peak_to_peak = s_static_max - s_static_min;
        const float drift = s_static_last - s_static_first;

        ESP_LOGI(TAG,
                 "TEST2,group=%u,step=%u,axis=%s,target=%d,mean=%.3f,std=%.3f,p2p=%.3f,drift=%.3f,count=%u",
                 (unsigned int)s_static_group,
                 (unsigned int)s_static_step,
                 axis,
                 s_static_targets[s_static_step],
                 mean,
                 std,
                 peak_to_peak,
                 drift,
                 (unsigned int)s_static_sample_count);
    } else {
        ESP_LOGW(TAG, "TEST2 no pose samples,group=%u,step=%u",
                 (unsigned int)s_static_group,
                 (unsigned int)s_static_step);
    }

    if (s_static_step < 2) {
        s_static_step++;
        airmouse_test_static_reset_window();
        ESP_LOGI(TAG, "TEST2_START,group=%s,target=%d",
                 axis,
                 s_static_targets[s_static_step]);
        return;
    }

    s_static_window_active = false;
    s_static_step = 0;
    s_static_group++;
    if (s_static_group >= 2) {
        s_static_group = 0;
    }
    airmouse_test_static_reset_window();
}

esp_err_t airmouse_test_init(void)
{
#if AIRMOUSE_TEST_MODE_ENABLE
    if (s_test_initialized) {
        return ESP_OK;
    }

    const gpio_num_t sync_gpio = (gpio_num_t)AIRMOUSE_TEST_SYNC_GPIO;
    if (!GPIO_IS_VALID_GPIO(sync_gpio)) {
        ESP_LOGE(TAG, "invalid TEST_SYNC GPIO: %d", (int)sync_gpio);
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t sync_gpio_config = {
        .pin_bit_mask = 1ULL << sync_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };

    esp_err_t ret = gpio_config(&sync_gpio_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST_SYNC GPIO config failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "GPIO ISR service install failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_isr_handler_add(sync_gpio, airmouse_test_sync_isr, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TEST_SYNC ISR add failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_current_test = AIRMOUSE_TEST_ANGLE_ACCURACY;
    s_test_step = 0;
    s_pending_sample = false;
    s_sample_deadline_us = 0;
    s_pending_step = 0;
    s_pose_origin_valid = false;
    s_pose_origin_yaw_deg = 0.0f;
    s_pose_origin_pitch_deg = 0.0f;
    s_static_group = 0;
    s_static_step = 0;
    s_static_window_active = false;
    airmouse_test_static_reset_window();
    s_realtime_group = 0;
    airmouse_test_realtime_reset_group();
    s_test_initialized = true;

    ESP_LOGI(TAG, "AirMouse test mode enabled");
    ESP_LOGI(TAG, "TEST_MODE,1,ANGLE_ACCURACY");
    return ESP_OK;
#else
    return ESP_OK;
#endif
}

void airmouse_test_update_pose(float yaw_deg,
                               float pitch_deg)
{
#if AIRMOUSE_TEST_MODE_ENABLE
    if (!s_test_initialized) {
        return;
    }

    bool step_pending = false;
    bool mode_switch_pending = false;
    int64_t step_event_rise_us = 0;
    portENTER_CRITICAL(&s_test_sync_lock);
    step_pending = s_step_event_pending;
    mode_switch_pending = s_mode_switch_pending;
    step_event_rise_us = s_step_event_rise_us;
    s_step_event_pending = false;
    s_mode_switch_pending = false;
    s_step_event_rise_us = 0;
    portEXIT_CRITICAL(&s_test_sync_lock);

    int64_t now_us = esp_timer_get_time();
    if (mode_switch_pending) {
        airmouse_test_handle_mode_switch();
    }
    if (s_current_test == AIRMOUSE_TEST_ANGLE_ACCURACY &&
            !s_pose_origin_valid) {
        s_pose_origin_yaw_deg = yaw_deg;
        s_pose_origin_pitch_deg = pitch_deg;
        s_pose_origin_valid = true;
    }
    if (step_pending) {
        airmouse_test_handle_step(now_us, step_event_rise_us);
    }

    now_us = esp_timer_get_time();
    if (s_current_test == AIRMOUSE_TEST_ANGLE_ACCURACY &&
            s_pending_sample &&
            now_us >= s_sample_deadline_us) {
        const uint8_t step = s_pending_step;
        const airmouse_test_angle_point_t *point =
            &s_angle_test_points[step % AIRMOUSE_TEST_ANGLE_STEP_COUNT];
        const float delta_yaw_deg = yaw_deg - s_pose_origin_yaw_deg;
        const float delta_pitch_deg = pitch_deg - s_pose_origin_pitch_deg;

        ESP_LOGI(TAG,
                 "TEST1,step=%u,axis=%s,target=%d,delta_yaw=%.3f,delta_pitch=%.3f",
                 (unsigned int)step,
                 point->axis,
                 point->target_deg,
                 delta_yaw_deg,
                 delta_pitch_deg);
        s_pending_sample = false;
    }

    if (s_current_test == AIRMOUSE_TEST_STATIC_STABILITY &&
            s_static_window_active &&
            s_pose_origin_valid) {
        const float value = (s_static_group == 0) ?
                            (yaw_deg - s_pose_origin_yaw_deg) :
                            (pitch_deg - s_pose_origin_pitch_deg);

        if (s_static_sample_count == 0) {
            s_static_first = value;
            s_static_min = value;
            s_static_max = value;
        }

        s_static_last = value;
        s_static_sum += value;
        s_static_sum_sq += (double)value * (double)value;
        if (value < s_static_min) {
            s_static_min = value;
        }
        if (value > s_static_max) {
            s_static_max = value;
        }
        s_static_sample_count++;
    }
#else
    (void)yaw_deg;
    (void)pitch_deg;
#endif
}

void airmouse_test_realtime_update_imu(const float gyro[3],
                                       int64_t sample_timestamp_us)
{
#if AIRMOUSE_TEST_MODE_ENABLE
    if (!s_test_initialized || gyro == NULL) {
        return;
    }

    bool step_pending = false;
    int64_t step_event_rise_us = 0;
    portENTER_CRITICAL(&s_test_sync_lock);
    if (s_current_test == AIRMOUSE_TEST_REALTIME &&
            s_step_event_pending &&
            !s_mode_switch_pending) {
        step_pending = true;
        step_event_rise_us = s_step_event_rise_us;
        s_step_event_pending = false;
        s_step_event_rise_us = 0;
    }
    portEXIT_CRITICAL(&s_test_sync_lock);

    if (step_pending) {
        airmouse_test_handle_step(sample_timestamp_us, step_event_rise_us);
    }

    if (s_current_test != AIRMOUSE_TEST_REALTIME ||
            !s_realtime_armed ||
            s_realtime_imu_found) {
        return;
    }

    const float gyro_norm_dps =
        sqrtf(gyro[0] * gyro[0] +
              gyro[1] * gyro[1] +
              gyro[2] * gyro[2]);
    if (gyro_norm_dps <= AIRMOUSE_REALTIME_GYRO_THRESHOLD_DPS) {
        return;
    }

    s_realtime_t1 = sample_timestamp_us;
    s_realtime_trigger_sample_us = sample_timestamp_us;
    s_realtime_imu_found = true;
#else
    (void)gyro;
    (void)sample_timestamp_us;
#endif
}

void airmouse_test_realtime_mark_pose(int64_t sample_timestamp_us,
                                      int64_t pose_done_us,
                                      bool has_mouse_event)
{
#if AIRMOUSE_TEST_MODE_ENABLE
    if (!s_test_initialized ||
            s_current_test != AIRMOUSE_TEST_REALTIME ||
            !s_realtime_armed ||
            !s_realtime_imu_found ||
            sample_timestamp_us != s_realtime_trigger_sample_us) {
        return;
    }

    if (!has_mouse_event) {
        s_realtime_t1 = 0;
        s_realtime_t2 = 0;
        s_realtime_t3 = 0;
        s_realtime_trigger_sample_us = 0;
        s_realtime_imu_found = false;
        s_realtime_pose_done = false;
        s_realtime_event_done = false;
        return;
    }

    s_realtime_t2 = pose_done_us;
    s_realtime_t3 = esp_timer_get_time();
    s_realtime_pose_done = true;
    s_realtime_event_done = true;
#else
    (void)sample_timestamp_us;
    (void)pose_done_us;
    (void)has_mouse_event;
#endif
}

void airmouse_test_realtime_mark_hid(int64_t source_timestamp_us,
                                     int64_t hid_us)
{
#if AIRMOUSE_TEST_MODE_ENABLE
    if (!s_test_initialized ||
            s_current_test != AIRMOUSE_TEST_REALTIME ||
            !s_realtime_armed ||
            !s_realtime_imu_found ||
            !s_realtime_pose_done ||
            !s_realtime_event_done ||
            source_timestamp_us != s_realtime_trigger_sample_us ||
            s_realtime_trial >= AIRMOUSE_REALTIME_TRIAL_COUNT) {
        return;
    }

    if (s_realtime_t0 <= 0 ||
            s_realtime_t1 <= s_realtime_t0 ||
            s_realtime_t2 < s_realtime_t1 ||
            s_realtime_t3 < s_realtime_t1 ||
            hid_us < s_realtime_t1) {
        s_realtime_armed = false;
        return;
    }

    airmouse_realtime_result_t *result =
        &s_realtime_results[s_realtime_trial];
    result->sensor_us = (uint32_t)(s_realtime_t1 - s_realtime_t0);
    result->pose_us = (uint32_t)(s_realtime_t2 - s_realtime_t1);
    result->event_us = (uint32_t)(s_realtime_t3 - s_realtime_t1);
    result->hid_us = (uint32_t)(hid_us - s_realtime_t1);
    result->valid = true;

    s_realtime_trial++;
    s_realtime_armed = false;
    s_realtime_imu_found = false;
    s_realtime_pose_done = false;
    s_realtime_event_done = false;
    s_realtime_trigger_sample_us = 0;

    if (s_realtime_trial >= AIRMOUSE_REALTIME_TRIAL_COUNT) {
        airmouse_test_realtime_output_group();
    }
#else
    (void)source_timestamp_us;
    (void)hid_us;
#endif
}
