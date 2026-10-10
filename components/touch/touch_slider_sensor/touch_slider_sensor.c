/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "soc/soc_caps.h"
#include "esp_check.h"
#include "esp_log.h"
#include "touch_sensor_fsm.h"
#include "touch_sensor_lowlevel.h"
#include "touch_slider_sensor.h"
#if CONFIG_TOUCH_SLIDER_SENSOR_DEBUG
#include "touch_sensor_fsm_private.h"
#endif

static const char *TAG = "touch_slider_sensor";

#if CONFIG_TOUCH_SLIDER_SENSOR_DEBUG
static uint64_t s_start_time;

static uint64_t get_time_in_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000U + (uint64_t)ts.tv_nsec / 1000000U;
    if (s_start_time == 0) {
        s_start_time = now;
    }
    return now - s_start_time;
}
#endif

typedef struct touch_slider_sensor_t {
    uint32_t channel_num;
    uint32_t *channel_list;
    uint32_t *thresholds;
    bool *channels_active;
    float *quantify_signal_array;

    uint32_t freq_num;
    uint32_t sample_period_ms;
    fsm_handle_t fsm_handle;
    touch_lowlevel_handle_t *lowlevel_handles;
    uint32_t registered_num;
    bool owns_lowlevel;
    bool fsm_started;

    uint32_t filter_reset_cnt;
    uint32_t filter_reset_times;
    uint32_t position_range;
    uint8_t calculate_window;
    float swipe_threshold;
    float swipe_hysterisis;
    float swipe_alpha;
    uint32_t last_position;
    uint32_t position;
    uint32_t start_position;
    float speed;
    bool pressed;
    touch_slider_event_t state;
    bool is_first_sample;
    uint32_t pos_filter_window[CONFIG_TOUCH_SLIDER_SENSOR_POS_FILTER_SIZE];
    uint32_t pos_window_idx;

    touch_slider_event_cb_t event_cb;
    void *user_data;
    bool is_initialized;
} touch_slider_sensor_t;

static int32_t find_channel_index(const touch_slider_sensor_t *sensor, uint32_t channel)
{
    for (uint32_t i = 0; i < sensor->channel_num; i++) {
        if (sensor->channel_list[i] == channel) {
            return (int32_t)i;
        }
    }
    return -1;
}

static int32_t lowlevel_channel_index(const touch_lowlevel_info_t *info, uint32_t channel)
{
    for (uint32_t i = 0; i < info->channel_num; i++) {
        if (info->channel_list[i] == channel) {
            return (int32_t)i;
        }
    }
    return -1;
}

static void slider_reset_filter(touch_slider_sensor_t *sensor)
{
    sensor->is_first_sample = true;
    sensor->position = 0;
    sensor->last_position = 0;
    sensor->speed = 0;
    sensor->state = TOUCH_SLIDER_EVENT_NONE;
    sensor->pos_window_idx = 0;
}

static void fsm_state_cb(fsm_handle_t handle, uint32_t channel, fsm_state_t state,
                         uint32_t data, void *user_data)
{
    (void)handle;
    (void)data;
    touch_slider_sensor_t *sensor = (touch_slider_sensor_t *)user_data;
    int32_t index = find_channel_index(sensor, channel);
    if (index >= 0) {
        sensor->channels_active[index] = (state == FSM_STATE_ACTIVE);
    }
}

static void touch_sensor_callback(uint32_t channel, touch_lowlevel_state_t state,
                                  void *state_data, void *arg)
{
    touch_slider_sensor_t *sensor = (touch_slider_sensor_t *)arg;
    if (state != TOUCH_LOWLEVEL_STATE_NEW_DATA || state_data == NULL ||
            sensor->fsm_handle == NULL || sensor->freq_num == 0) {
        return;
    }

    const uint32_t *raw_data = (const uint32_t *)state_data;
    (void)touch_sensor_fsm_update_data(sensor->fsm_handle, channel, raw_data,
                                       sensor->freq_num, true);
}

static esp_err_t slider_cleanup(touch_slider_sensor_t *sensor)
{
    if (sensor == NULL) {
        return ESP_OK;
    }

    esp_err_t first_error = ESP_OK;
    if (sensor->fsm_handle != NULL && sensor->fsm_started) {
        esp_err_t ret = touch_sensor_fsm_control(sensor->fsm_handle, FSM_CTRL_STOP, NULL);
        if (ret != ESP_OK && first_error == ESP_OK) {
            first_error = ret;
        }
    }
    if (sensor->owns_lowlevel) {
        esp_err_t ret = touch_sensor_lowlevel_stop();
        if (ret != ESP_OK && first_error == ESP_OK) {
            first_error = ret;
        }
    }
    for (uint32_t i = 0; i < sensor->registered_num; i++) {
        if (sensor->lowlevel_handles[i] != NULL) {
            esp_err_t ret = touch_sensor_lowlevel_unregister(sensor->lowlevel_handles[i]);
            if (ret != ESP_OK && first_error == ESP_OK) {
                first_error = ret;
            }
        }
    }
    if (sensor->fsm_handle != NULL) {
        esp_err_t ret = touch_sensor_fsm_delete(sensor->fsm_handle);
        if (ret != ESP_OK && first_error == ESP_OK) {
            first_error = ret;
        }
    }
    if (sensor->owns_lowlevel) {
        esp_err_t ret = touch_sensor_lowlevel_delete();
        if (ret != ESP_OK && first_error == ESP_OK) {
            first_error = ret;
        }
    }

    free(sensor->channel_list);
    free(sensor->thresholds);
    free(sensor->channels_active);
    free(sensor->quantify_signal_array);
    free(sensor->lowlevel_handles);
    free(sensor);
    return first_error;
}

esp_err_t touch_slider_sensor_create(const touch_slider_config_t *config,
                                     touch_slider_handle_t *handle,
                                     touch_slider_event_cb_t cb, void *cb_arg)
{
    ESP_RETURN_ON_FALSE(config != NULL && handle != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "Invalid arguments");
    *handle = NULL;
    ESP_RETURN_ON_FALSE(config->channel_num >= 2 &&
                        config->channel_num <= SOC_TOUCH_SENSOR_NUM &&
                        config->channel_list != NULL && config->thresholds != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid channel configuration");
    ESP_RETURN_ON_FALSE(config->position_range > 0, ESP_ERR_INVALID_ARG, TAG,
                        "Position range must be nonzero");
    ESP_RETURN_ON_FALSE(config->position_range <= INT32_MAX, ESP_ERR_INVALID_ARG, TAG,
                        "Position range must fit event data");
    ESP_RETURN_ON_FALSE(config->swipe_alpha >= 0.0f && config->swipe_alpha <= 1.0f,
                        ESP_ERR_INVALID_ARG, TAG, "Swipe alpha must be in [0, 1]");

    uint8_t calculate_window = config->calculate_window;
    if (calculate_window == 0) {
        calculate_window = config->channel_num == 2 ? 2 : 3;
    }
    ESP_RETURN_ON_FALSE(calculate_window >= 1 && calculate_window <= config->channel_num,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid position window");
    for (uint32_t i = 0; i < config->channel_num; i++) {
        ESP_RETURN_ON_FALSE(config->thresholds[i] > 0, ESP_ERR_INVALID_ARG, TAG,
                            "Slider thresholds must be nonzero");
        for (uint32_t j = 0; j < i; j++) {
            ESP_RETURN_ON_FALSE(config->channel_list[i] != config->channel_list[j],
                                ESP_ERR_INVALID_ARG, TAG,
                                "Duplicate slider channel");
        }
    }

    touch_slider_sensor_t *sensor = calloc(1, sizeof(*sensor));
    ESP_RETURN_ON_FALSE(sensor != NULL, ESP_ERR_NO_MEM, TAG, "No memory for slider");
    sensor->channel_num = config->channel_num;
    sensor->channel_list = malloc(config->channel_num * sizeof(uint32_t));
    sensor->thresholds = malloc(config->channel_num * sizeof(uint32_t));
    sensor->channels_active = calloc(config->channel_num, sizeof(bool));
    sensor->quantify_signal_array = calloc(config->channel_num, sizeof(float));
    sensor->lowlevel_handles = calloc(config->channel_num, sizeof(touch_lowlevel_handle_t));
    if (sensor->channel_list == NULL || sensor->thresholds == NULL ||
            sensor->channels_active == NULL || sensor->quantify_signal_array == NULL ||
            sensor->lowlevel_handles == NULL) {
        slider_cleanup(sensor);
        return ESP_ERR_NO_MEM;
    }
    memcpy(sensor->channel_list, config->channel_list, config->channel_num * sizeof(uint32_t));
    memcpy(sensor->thresholds, config->thresholds, config->channel_num * sizeof(uint32_t));
    sensor->event_cb = cb;
    sensor->user_data = cb_arg;
    sensor->filter_reset_times = config->filter_reset_times;
    sensor->position_range = config->position_range;
    sensor->calculate_window = calculate_window;
    sensor->swipe_threshold = config->swipe_threshold;
    sensor->swipe_hysterisis = config->swipe_hysterisis;
    sensor->swipe_alpha = config->swipe_alpha;
    sensor->start_position = UINT32_MAX;
    slider_reset_filter(sensor);

    esp_err_t ret;
    if (!config->skip_lowlevel_init) {
        touch_lowlevel_config_t lowlevel_config = {
            .channel_num = config->channel_num,
            .channel_list = sensor->channel_list,
            .channel_type = NULL,
            .sample_period_ms = config->sample_period_ms,
            .sample_cfg_num = config->sample_cfg_num,
        };
        ret = touch_sensor_lowlevel_create(&lowlevel_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create touch lowlevel: %s", esp_err_to_name(ret));
            slider_cleanup(sensor);
            return ret;
        }
        sensor->owns_lowlevel = true;
        ret = touch_sensor_lowlevel_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start touch lowlevel: %s", esp_err_to_name(ret));
            slider_cleanup(sensor);
            return ret;
        }
    }

    touch_lowlevel_info_t lowlevel_info = {0};
    ret = touch_sensor_lowlevel_get_info(&lowlevel_info);
    if (ret != ESP_OK) {
        slider_cleanup(sensor);
        return ret;
    }
    if (lowlevel_info.sample_cfg_num == 0 ||
            lowlevel_info.sample_cfg_num > FSM_MAX_FREQ) {
        ESP_LOGE(TAG, "Unsupported frequency count: %" PRIu32,
                 lowlevel_info.sample_cfg_num);
        slider_cleanup(sensor);
        return ESP_ERR_NOT_SUPPORTED;
    }
    for (uint32_t i = 0; i < sensor->channel_num; i++) {
        int32_t lowlevel_index = lowlevel_channel_index(&lowlevel_info,
                                                        sensor->channel_list[i]);
        if (lowlevel_index < 0) {
            ESP_LOGE(TAG, "Channel %" PRIu32 " is not in lowlevel",
                     sensor->channel_list[i]);
            slider_cleanup(sensor);
            return ESP_ERR_NOT_FOUND;
        }
        if (lowlevel_info.channel_type != NULL &&
                lowlevel_info.channel_type[lowlevel_index] != TOUCH_LOWLEVEL_TYPE_TOUCH) {
            ESP_LOGE(TAG, "Channel %" PRIu32 " is not an ordinary touch channel",
                     sensor->channel_list[i]);
            slider_cleanup(sensor);
            return ESP_ERR_INVALID_ARG;
        }
    }
    sensor->freq_num = lowlevel_info.sample_cfg_num;
    sensor->sample_period_ms = lowlevel_info.sample_period_ms;

    fsm_config_t fsm_config;
    ret = touch_sensor_fsm_get_default_config(sensor->freq_num,
                                              sensor->sample_period_ms,
                                              &fsm_config);
    if (ret != ESP_OK) {
        slider_cleanup(sensor);
        return ret;
    }
    fsm_config.mode = FSM_MODE_USER_PUSH;
    fsm_config.channel_num = sensor->channel_num;
    fsm_config.channel_list = sensor->channel_list;
    fsm_config.freq_num = sensor->freq_num;
    fsm_config.threshold_active = sensor->thresholds;
    fsm_config.debounce_active = config->debounce_active;
    fsm_config.debounce_inactive = config->debounce_inactive;
    fsm_config.sample_period_ms = sensor->sample_period_ms;
    fsm_config.state_cb = fsm_state_cb;
    fsm_config.user_data = sensor;

    ret = touch_sensor_fsm_create(&fsm_config, &sensor->fsm_handle);
    if (ret != ESP_OK) {
        slider_cleanup(sensor);
        return ret;
    }
    ret = touch_sensor_fsm_control(sensor->fsm_handle, FSM_CTRL_START, NULL);
    if (ret != ESP_OK) {
        slider_cleanup(sensor);
        return ret;
    }
    sensor->fsm_started = true;

    for (uint32_t i = 0; i < sensor->channel_num; i++) {
        ret = touch_sensor_lowlevel_register(sensor->channel_list[i],
                                             touch_sensor_callback, sensor,
                                             &sensor->lowlevel_handles[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register channel %" PRIu32 ": %s",
                     sensor->channel_list[i], esp_err_to_name(ret));
            slider_cleanup(sensor);
            return ret;
        }
        sensor->registered_num++;
    }

    sensor->is_initialized = true;
    *handle = sensor;
    ESP_LOGI(TAG, "Created with %" PRIu32 " channels, %" PRIu32
             " frequencies, period %" PRIu32 " ms",
             sensor->channel_num, sensor->freq_num, sensor->sample_period_ms);
    return ESP_OK;
}

esp_err_t touch_slider_sensor_delete(touch_slider_handle_t handle)
{
    return slider_cleanup((touch_slider_sensor_t *)handle);
}

esp_err_t touch_slider_sensor_get_data(touch_slider_handle_t handle,
                                       uint32_t channel, uint32_t *data)
{
    ESP_RETURN_ON_FALSE(handle != NULL && data != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "Invalid arguments");
    touch_slider_sensor_t *sensor = (touch_slider_sensor_t *)handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG,
                        "Sensor not initialized");
    ESP_RETURN_ON_FALSE(find_channel_index(sensor, channel) >= 0, ESP_ERR_NOT_FOUND, TAG,
                        "Channel is not configured");

    int32_t fused_filtered = 0;
    esp_err_t ret = touch_sensor_fsm_get_data(sensor->fsm_handle, channel, &fused_filtered);
    if (ret == ESP_OK) {
        /* Wrapper amplitudes are unsigned; preserve the FSM's signed data at its API boundary. */
        *data = fused_filtered > 0 ? (uint32_t)fused_filtered : 0;
    }
    return ret;
}

static void slider_quantify_signal(touch_slider_sensor_t *sensor)
{
    const float lower_threshold =
        CONFIG_TOUCH_SLIDER_SENSOR_QUANTIFY_LOWER_THRESHOLD_X1000 / 1000.0f;
    for (uint32_t i = 0; i < sensor->channel_num; i++) {
        uint32_t signal = 0;
        if (touch_slider_sensor_get_data(sensor, sensor->channel_list[i], &signal) != ESP_OK) {
            sensor->quantify_signal_array[i] = 0;
            continue;
        }
        float quantified = (float)signal / (float)sensor->thresholds[i];
        sensor->quantify_signal_array[i] = quantified >= lower_threshold ? quantified : 0.0f;
    }
}

static float slider_search_max_subarray(const float *array, uint32_t array_size,
                                        uint32_t window_size, uint32_t *start_index)
{
    float max_sum = 0.0f;
    *start_index = 0;
    if (window_size == 0 || window_size > array_size) {
        return max_sum;
    }
    for (uint32_t start = 0; start + window_size <= array_size; start++) {
        float sum = 0.0f;
        for (uint32_t i = start; i < start + window_size; i++) {
            sum += array[i];
        }
        if (sum > max_sum) {
            max_sum = sum;
            *start_index = start;
        }
    }
    return max_sum;
}

static uint32_t slider_get_nonzero_num(const float *array, uint32_t start,
                                       uint32_t window_size, uint32_t array_size)
{
    uint32_t count = 0;
    uint32_t end = start + window_size;
    if (end > array_size) {
        end = array_size;
    }
    for (uint32_t i = start; i < end; i++) {
        if (array[i] > 0.0f) {
            count++;
        }
    }
    return count;
}

static uint32_t slider_calculate_position(const touch_slider_sensor_t *sensor,
                                          uint32_t start, float sum,
                                          uint32_t nonzero_num)
{
    if (nonzero_num == 0 || sum <= 0.0f) {
        return sensor->position;
    }
    uint32_t end = start + sensor->calculate_window;
    if (end > sensor->channel_num) {
        end = sensor->channel_num;
    }
    if (nonzero_num == 1) {
        for (uint32_t i = start; i < end; i++) {
            if (sensor->quantify_signal_array[i] > 0.0f) {
                if (i == sensor->channel_num - 1) {
                    return sensor->position_range;
                }
                return (uint32_t)((float)i * (float)sensor->position_range /
                                  (float)(sensor->channel_num - 1));
            }
        }
        return sensor->position;
    }

    float weighted_index = 0.0f;
    for (uint32_t i = start; i < end; i++) {
        weighted_index += (float)i * sensor->quantify_signal_array[i];
    }
    float position = weighted_index * (float)sensor->position_range /
                     ((float)(sensor->channel_num - 1) * sum);
    if (position <= 0.0f) {
        return 0;
    }
    if (position >= (float)sensor->position_range) {
        return sensor->position_range;
    }
    return (uint32_t)(position + 0.5f);
}

static uint32_t slider_filter_average(touch_slider_sensor_t *sensor, uint32_t current)
{
    if (sensor->is_first_sample) {
        for (uint32_t i = 0; i < CONFIG_TOUCH_SLIDER_SENSOR_POS_FILTER_SIZE; i++) {
            sensor->pos_filter_window[i] = current;
        }
        sensor->is_first_sample = false;
    } else {
        sensor->pos_filter_window[sensor->pos_window_idx++] = current;
        if (sensor->pos_window_idx >= CONFIG_TOUCH_SLIDER_SENSOR_POS_FILTER_SIZE) {
            sensor->pos_window_idx = 0;
        }
    }

    uint64_t sum = 0;
    for (uint32_t i = 0; i < CONFIG_TOUCH_SLIDER_SENSOR_POS_FILTER_SIZE; i++) {
        sum += sensor->pos_filter_window[i];
    }
    return (uint32_t)((sum + CONFIG_TOUCH_SLIDER_SENSOR_POS_FILTER_SIZE / 2) /
                      CONFIG_TOUCH_SLIDER_SENSOR_POS_FILTER_SIZE);
}

static uint32_t slider_filter_iir(uint32_t current, uint32_t previous, uint32_t factor)
{
    if (factor <= 1) {
        return current;
    }
    uint64_t weighted = (uint64_t)current + (uint64_t)(factor - 1) * previous;
    return (uint32_t)(weighted / factor);
}

static void slider_update_position(touch_slider_sensor_t *sensor)
{
    slider_quantify_signal(sensor);
    uint32_t start = 0;
    float sum = slider_search_max_subarray(sensor->quantify_signal_array,
                                           sensor->channel_num,
                                           sensor->calculate_window, &start);
    uint32_t nonzero = slider_get_nonzero_num(sensor->quantify_signal_array, start,
                                              sensor->calculate_window,
                                              sensor->channel_num);
    uint32_t current = slider_calculate_position(sensor, start, sum, nonzero);
    uint32_t average = slider_filter_average(sensor, current);
    sensor->last_position = sensor->position == 0 ? average : sensor->position;
    sensor->position = slider_filter_iir(average, sensor->last_position,
                                         CONFIG_TOUCH_SLIDER_SENSOR_POS_FILTER_FACTOR);
}

static touch_slider_event_t slider_update_speed(touch_slider_sensor_t *sensor)
{
    touch_slider_event_t event = TOUCH_SLIDER_EVENT_NONE;
    int32_t delta = (int32_t)sensor->position - (int32_t)sensor->last_position;
    sensor->speed = sensor->speed * sensor->swipe_alpha +
                    (float)delta * (1.0f - sensor->swipe_alpha);
    if (sensor->position == 0) {
        return event;
    }
    if (sensor->state != TOUCH_SLIDER_EVENT_RIGHT_SWIPE &&
            sensor->speed > sensor->swipe_threshold + sensor->swipe_hysterisis) {
        sensor->state = event = TOUCH_SLIDER_EVENT_RIGHT_SWIPE;
    } else if (sensor->state != TOUCH_SLIDER_EVENT_LEFT_SWIPE &&
               -sensor->speed > sensor->swipe_threshold + sensor->swipe_hysterisis) {
        sensor->state = event = TOUCH_SLIDER_EVENT_LEFT_SWIPE;
    } else if (sensor->state != TOUCH_SLIDER_EVENT_NONE &&
               fabsf(sensor->speed) < sensor->swipe_threshold - sensor->swipe_hysterisis) {
        sensor->state = TOUCH_SLIDER_EVENT_NONE;
    }
    return event;
}

static void slider_update_state(touch_slider_sensor_t *sensor)
{
    sensor->pressed = false;
    for (uint32_t i = 0; i < sensor->channel_num; i++) {
        if (sensor->channels_active[i]) {
            sensor->pressed = true;
            break;
        }
    }

    if (sensor->pressed) {
        sensor->filter_reset_cnt = 0;
        slider_update_position(sensor);
        touch_slider_event_t event = slider_update_speed(sensor);
        if (sensor->start_position == UINT32_MAX) {
            sensor->start_position = sensor->position;
        }
        if (sensor->event_cb != NULL) {
            sensor->event_cb(sensor, TOUCH_SLIDER_EVENT_POSITION,
                             (int32_t)sensor->position, sensor->user_data);
            if (event != TOUCH_SLIDER_EVENT_NONE) {
                sensor->event_cb(sensor, event, 0, sensor->user_data);
            }
        }
    } else {
        if (sensor->start_position != UINT32_MAX) {
            int32_t displacement = (int32_t)sensor->position -
                                   (int32_t)sensor->start_position;
            if (sensor->event_cb != NULL) {
                sensor->event_cb(sensor, TOUCH_SLIDER_EVENT_RELEASE, displacement,
                                 sensor->user_data);
            }
            sensor->start_position = UINT32_MAX;
        }
        if (sensor->filter_reset_cnt < UINT32_MAX) {
            sensor->filter_reset_cnt++;
        }
        if (sensor->filter_reset_cnt >= sensor->filter_reset_times) {
            sensor->filter_reset_cnt = 0;
            slider_reset_filter(sensor);
        }
    }
}

esp_err_t touch_slider_sensor_handle_events(touch_slider_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    touch_slider_sensor_t *sensor = (touch_slider_sensor_t *)handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG,
                        "Sensor not initialized");

    esp_err_t ret = touch_sensor_fsm_handle_events(sensor->fsm_handle);
    if (ret != ESP_OK) {
        return ret;
    }
#if CONFIG_TOUCH_SLIDER_SENSOR_DEBUG
    for (uint32_t j = 0; j < sensor->channel_num; j++) {
        touch_fsm_info_t info = {0};
        if (touch_sensor_fsm_get_info(sensor->fsm_handle,
                                      sensor->channel_list[j], &info) != ESP_OK) {
            continue;
        }
        for (uint32_t f = 0; f < sensor->freq_num; f++) {
            printf("vl,%" PRIu64 "," TOUCH_FSM_INFO_PRINT_FORMAT "\n",
                   get_time_in_ms(),
                   TOUCH_FSM_INFO_PRINT_ARGS(sensor->channel_list[j], f, &info));
        }
    }
#endif
    slider_update_state(sensor);
    return ESP_OK;
}

void touch_slider_sensor_get_state(touch_slider_handle_t handle, bool *pressed,
                                   uint32_t *pos, float *speed)
{
    if (handle == NULL) {
        return;
    }
    touch_slider_sensor_t *sensor = (touch_slider_sensor_t *)handle;
    if (pressed != NULL) {
        *pressed = sensor->pressed;
    }
    if (pos != NULL) {
        *pos = sensor->position;
    }
    if (speed != NULL) {
        *speed = sensor->speed;
    }
}
