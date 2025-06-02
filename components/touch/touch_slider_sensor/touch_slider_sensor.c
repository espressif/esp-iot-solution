/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include "soc/soc_caps.h"
#include "esp_check.h"
#include "esp_log.h"
#include "touch_sensor_lowlevel.h"
#include "touch_sensor_fsm.h"
#include "touch_slider_sensor.h"
#include "math.h"

static const char *TAG = "touch_slider_sensor";

#if CONFIG_TOUCH_SLIDER_SENSOR_DEBUG
static uint64_t start_time = 0;
static uint64_t get_time_in_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t current_time = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    if (start_time == 0) {
        start_time = current_time;
    }
    return current_time - start_time;
}

// print touch values(raw, smooth, benchmark) of each enabled channel, every 50ms
#define PRINT_VALUE(pad_num, raw, smooth, benchmark) printf("vl,%" PRId64 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 "\n", get_time_in_ms(), pad_num, raw, smooth, benchmark)
// print trigger if touch active/inactive happens
#define PRINT_TRIGGER(pad_num, smooth, if_active) printf("tg,%" PRId64 ",%" PRIu32 ",%" PRIu32 ",%d,%d,%d,%d,%d\n", get_time_in_ms(), pad_num, smooth, if_active ? 1 : 0, if_active ? 0 : 1, 0, 0, 0)
#else
#define PRINT_VALUE(pad_num, raw, smooth, benchmark)
#define PRINT_TRIGGER(pad_num, smooth, if_active)
#endif

#define TOUCH_SLIDER_CHECK_GOTO(a, str, label)                    \
    if (!(a))                                                     \
    {                                                             \
        FSM_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        goto label;                                               \
    }

typedef struct touch_slider_sensor_t {
    uint32_t channel_num;
    uint32_t *channel_list;
    fsm_handle_t fsm_handles[SOC_TOUCH_SAMPLE_CFG_NUM];
    float channel_threshold[SOC_TOUCH_SENSOR_NUM];
    uint32_t channel_bcm[SOC_TOUCH_SENSOR_NUM];                           // Channel benchmark array
    uint32_t channel_bcm_update_cnt;                                      // Channel benchmark update counter
    uint32_t filter_reset_cnt;                                            // Slider reset counter
    uint32_t filter_reset_times;
    float quantify_signal_array[SOC_TOUCH_SENSOR_NUM];                    // Slider re-quantization array
    uint32_t position_range;
    float swipe_threshold;
    float swipe_hysterisis;
    float swipe_alpha;
    uint32_t last_position;
    uint32_t position;
    uint32_t start_position;
    float speed;
    bool pressed;
    touch_slider_event_t state;
    bool is_first_sample; // Slider first time sample record bit
    uint32_t pos_filter_window[CONFIG_TOUCH_SLIDER_SENSOR_POS_FILTER_SIZE];
    uint8_t pos_window_idx;
    bool channels_active[SOC_TOUCH_SENSOR_NUM];
    touch_slider_event_cb_t event_cb;
    void *user_data;
    bool is_initialized;
    touch_lowlevel_handle_t lowlevel_handle[SOC_TOUCH_SENSOR_NUM];
    bool skip_lowlevel_init;
} touch_slider_sensor_t;

static void slider_reset_filter(touch_slider_sensor_t *sensor)
{
    sensor->is_first_sample = true;
    sensor->position = 0;
    sensor->speed = 0;
    sensor->state = TOUCH_SLIDER_EVENT_NONE;
    sensor->pos_window_idx = 0;
}

static esp_err_t find_channel_index(touch_slider_sensor_t *sensor, uint32_t channel, uint32_t *channel_idx)
{
    for (int i = 0; i < sensor->channel_num; i++) {
        if (sensor->channel_list[i] == channel) {
            *channel_idx = i;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

static int get_fsm_frequency_index(touch_slider_sensor_t *sensor, fsm_handle_t handle)
{
    for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
        if (sensor->fsm_handles[i] == handle) {
            return i;
        }
    }
    return -1;
}

static void fsm_state_cb(fsm_handle_t handle, uint32_t channel, fsm_state_t state, uint32_t data, void *user_data)
{
    touch_slider_sensor_t *sensor = (touch_slider_sensor_t *)user_data;
    int frequency = get_fsm_frequency_index(sensor, handle);
    uint32_t channel_idx = 0;

    // Ignore other frequencies for now
    if (frequency != 0 || find_channel_index(sensor, channel, &channel_idx) != ESP_OK) {
        return;
    }

    sensor->channels_active[channel_idx] = state;
}

#if !CONFIG_IDF_TARGET_ESP32
static void touch_sensor_callback(uint32_t channel, touch_lowlevel_state_t state, void *state_data, void *arg)
{
    touch_slider_sensor_t *sensor = (touch_slider_sensor_t *)arg;
    if (state == TOUCH_LOWLEVEL_STATE_NEW_DATA) {
        uint32_t *raw_data = (uint32_t *)state_data;
        for (int j = 0; j < SOC_TOUCH_SAMPLE_CFG_NUM; j++) {
            if (sensor->fsm_handles[j] != NULL) {
                touch_sensor_fsm_update_data(sensor->fsm_handles[j], channel, raw_data[j], true);
            }
        }
    }
}
#else
static void polling_callback(fsm_handle_t handle, uint32_t channel, uint32_t *raw_data, void *user_data)
{
    uint32_t data[SOC_TOUCH_SAMPLE_CFG_NUM] = {0};
    // give the first frequency data only for esp32p4
    if (touch_sensor_lowlevel_get_data(channel, data) == ESP_OK) {
        // For simplicity, use the first data slot as raw data
        *raw_data = data[0];
    } else {
        *raw_data = 0;
    }
}
#endif

esp_err_t touch_slider_sensor_create(touch_slider_config_t *config, touch_slider_handle_t *handle, touch_slider_event_cb_t cb,  void *cb_arg)
{
    ESP_RETURN_ON_FALSE(config && handle, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    ESP_RETURN_ON_FALSE(config->channel_num > 1 && config->channel_list, ESP_ERR_INVALID_ARG, TAG, "Invalid channel config");

    touch_slider_sensor_t *sensor = calloc(1, sizeof(touch_slider_sensor_t));
    ESP_RETURN_ON_FALSE(sensor, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory");

    // Deep copy channel list
    sensor->channel_num = config->channel_num;
    sensor->channel_list = malloc(config->channel_num * sizeof(uint32_t));
    if (!sensor->channel_list) {
        free(sensor);
        return ESP_ERR_NO_MEM;
    }
    memcpy(sensor->channel_list, config->channel_list, config->channel_num * sizeof(uint32_t));
    memcpy(sensor->channel_threshold, config->channel_threshold, config->channel_num * sizeof(float));
    sensor->event_cb = cb;
    sensor->user_data = cb_arg;
    sensor->filter_reset_times = config->filter_reset_times;
    sensor->skip_lowlevel_init = config->skip_lowlevel_init;                           // Store this flag
    sensor->channel_bcm_update_cnt = CONFIG_TOUCH_SLIDER_SENSOR_BENCHMARK_UPDATE_TIME; // update at first time
    sensor->filter_reset_cnt = config->filter_reset_times;
    sensor->position_range = config->position_range;
    sensor->swipe_threshold = config->swipe_threshold;
    sensor->swipe_hysterisis = config->swipe_hysterisis;
    sensor->swipe_alpha = config->swipe_alpha;

    slider_reset_filter(sensor);

    // Allocate memory for noise thresholds
    float *noise_p = malloc(config->channel_num * sizeof(float));
    float *noise_n = malloc(config->channel_num * sizeof(float));
    if (!noise_p || !noise_n) {
        if (noise_p) {
            free(noise_p);
        }
        if (noise_n) {
            free(noise_n);
        }
        free(sensor->channel_list);
        free(sensor);
        return ESP_ERR_NO_MEM;
    }

    // Initialize noise thresholds for each channel
    for (uint32_t i = 0; i < config->channel_num; i++) {
        noise_p[i] = config->channel_threshold[i] / CONFIG_TOUCH_SLIDER_SENSOR_NOISE_P_SNR;
        noise_n[i] = config->channel_threshold[i] / CONFIG_TOUCH_SLIDER_SENSOR_NOISE_N_SNR;
    }

    // Initialize touch sensor low level if needed
    if (!config->skip_lowlevel_init) {
        touch_lowlevel_config_t ll_config = {
            .channel_num = config->channel_num,
            .channel_list = config->channel_list,
            .channel_type = NULL, // Using default channel type
        };
        TOUCH_SLIDER_CHECK_GOTO(
            touch_sensor_lowlevel_create(&ll_config) == ESP_OK,
            "Failed to create touch sensor low level", cleanup);
    }

    fsm_config_t fsm_cfg = DEFAULTS_TOUCH_SENSOR_FSM_CONFIG();
    // Register callbacks for each channel
#if !CONFIG_IDF_TARGET_ESP32
    for (uint32_t i = 0; i < config->channel_num; i++) {
        TOUCH_SLIDER_CHECK_GOTO(
            touch_sensor_lowlevel_register(config->channel_list[i],
                                           touch_sensor_callback,
                                           sensor,
                                           &sensor->lowlevel_handle[i]) == ESP_OK,
            "Failed to register touch sensor callback", cleanup);
    }

    // Initialize FSMs
    fsm_cfg.mode = FSM_MODE_USER_PUSH;
    fsm_cfg.active_low = false;
    fsm_cfg.calibration_times = CONFIG_TOUCH_SLIDER_SENSOR_CALIBRATION_TIMES;
    fsm_cfg.debounce_inactive = CONFIG_TOUCH_SLIDER_SENSOR_DEBOUNCE_INACTIVE;
#else
    fsm_cfg.mode = FSM_MODE_POLLING;
    fsm_cfg.polling_interval = CONFIG_TOUCH_SLIDER_SENSOR_POLLING_INTERVAL;
    fsm_cfg.polling_cb = polling_callback;
    fsm_cfg.active_low = true;
    fsm_cfg.calibration_times = CONFIG_TOUCH_SLIDER_SENSOR_CALIBRATION_TIMES;
    fsm_cfg.debounce_inactive = CONFIG_TOUCH_SLIDER_SENSOR_DEBOUNCE_INACTIVE;
#endif
    fsm_cfg.state_cb = fsm_state_cb;
    fsm_cfg.channel_num = config->channel_num;
    fsm_cfg.channel_list = config->channel_list;
    fsm_cfg.threshold_p = config->channel_threshold;
#if CONFIG_TOUCH_SLIDER_SENSOR_NEGATIVE_LOGIC
    fsm_cfg.threshold_n = config->channel_threshold;
#else
    fsm_cfg.threshold_n = NULL;
#endif
    fsm_cfg.gold_value = config->channel_gold_value;
    fsm_cfg.debounce_active = config->debounce_times;
    fsm_cfg.smooth_coef = CONFIG_TOUCH_SLIDER_SENSOR_SMOOTH_COEF_X1000 / 1000.0f;
    fsm_cfg.baseline_coef = CONFIG_TOUCH_SLIDER_SENSOR_BASELINE_COEF_X1000 / 1000.0f;
    fsm_cfg.max_p = CONFIG_TOUCH_SLIDER_SENSOR_MAX_P_X1000 / 1000.0f;
    fsm_cfg.min_n = CONFIG_TOUCH_SLIDER_SENSOR_MIN_N_X1000 / 1000.0f;
    fsm_cfg.hysteresis_active = 0.1f;
    fsm_cfg.hysteresis_inactive = 0.1f;
    fsm_cfg.noise_p = noise_p;
    fsm_cfg.noise_n = noise_n;
    fsm_cfg.reset_cover = CONFIG_TOUCH_SLIDER_SENSOR_RESET_COVER;
    fsm_cfg.reset_calibration = CONFIG_TOUCH_SLIDER_SENSOR_RESET_CALIBRATION;
    fsm_cfg.raw_buf_size = CONFIG_TOUCH_SLIDER_SENSOR_RAW_BUF_SIZE;
    fsm_cfg.scale_factor = CONFIG_TOUCH_SLIDER_SENSOR_SCALE_FACTOR;
    fsm_cfg.user_data = sensor;

    for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
        TOUCH_SLIDER_CHECK_GOTO(touch_sensor_fsm_create(&fsm_cfg, &sensor->fsm_handles[i]) == ESP_OK, "Failed to create FSM", cleanup);
        TOUCH_SLIDER_CHECK_GOTO(touch_sensor_fsm_control(sensor->fsm_handles[i], FSM_CTRL_START, NULL) == ESP_OK, "Failed to start FSM", cleanup);
    }

    if (!config->skip_lowlevel_init) {
        TOUCH_SLIDER_CHECK_GOTO(touch_sensor_lowlevel_start() == ESP_OK, "Failed to start touch sensor low level", cleanup);
    }

    sensor->is_initialized = true;
    *handle = sensor;
    return ESP_OK;

cleanup:
    touch_slider_sensor_delete(sensor);
    return ESP_FAIL;
}

esp_err_t touch_slider_sensor_delete(touch_slider_handle_t handle)
{
    if (!handle) {
        return ESP_OK;
    }

    touch_slider_sensor_t *sensor = handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG, "Sensor not initialized");

    if (!sensor->skip_lowlevel_init) {
        touch_sensor_lowlevel_stop();
    }

    // Stop all FSMs first
    for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
        if (sensor->fsm_handles[i]) {
            touch_sensor_fsm_control(sensor->fsm_handles[i], FSM_CTRL_STOP, NULL);
        }
    }

    // Unregister all channels
    for (int i = 0; i < sensor->channel_num; i++) {
        touch_sensor_lowlevel_unregister(sensor->lowlevel_handle[i]);
    }

    // Delete FSMs
    for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
        if (sensor->fsm_handles[i]) {
            touch_sensor_fsm_delete(sensor->fsm_handles[i]);
        }
    }

    // Delete touch sensor low level if we initialized it
    if (!sensor->skip_lowlevel_init) {
        touch_sensor_lowlevel_delete();
    }

    // Free resources
    free(sensor->channel_list);
    free(sensor);
    return ESP_OK;
}

esp_err_t touch_slider_sensor_get_data(touch_slider_handle_t handle, uint32_t channel, uint32_t channel_alt, uint32_t *data)
{
    ESP_RETURN_ON_FALSE(handle && data, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    ESP_RETURN_ON_FALSE(channel_alt < SOC_TOUCH_SAMPLE_CFG_NUM, ESP_ERR_INVALID_ARG, TAG, "Invalid channel_alt");

    touch_slider_sensor_t *sensor = (touch_slider_sensor_t *)handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG, "Sensor not initialized");

    uint32_t channel_idx;
    ESP_RETURN_ON_ERROR(find_channel_index(sensor, channel, &channel_idx), TAG, "Channel not found");

    // Get data from specified frequency FSM
    uint32_t raw_data[3] = {0};
    ESP_RETURN_ON_ERROR(
        touch_sensor_fsm_get_data(sensor->fsm_handles[channel_alt], channel, raw_data),
        TAG, "Failed to get FSM data");

    *data = raw_data[FSM_DATA_SMOOTH];
    return ESP_OK;
}

static void slider_update_benchmark(touch_slider_handle_t sensor)
{
    for (int idx = 0; idx < sensor->channel_num; idx++) {
        uint32_t bcm_val;
        ESP_ERROR_CHECK(touch_slider_sensor_get_data(sensor, sensor->channel_list[idx], 0, &bcm_val));
        sensor->channel_bcm[idx] = bcm_val;
    }
}

/**
 * @brief Slider channel difference-rate re-quantization
 *
 * This function will re-quantifies the touch sensor slider channel difference-rate
 * so as to make the different size of touch pad in PCB has the same difference value
 *
 */
static inline void slider_quantify_signal(touch_slider_handle_t slider_handle)
{
    float weight_sum = 0;
    for (int idx = 0; idx < slider_handle->channel_num; idx++) {
        weight_sum += slider_handle->channel_threshold[idx];
        uint32_t current_signal;
        ESP_ERROR_CHECK(touch_slider_sensor_get_data(slider_handle, slider_handle->channel_list[idx], 0, &current_signal));
        int ans = current_signal - slider_handle->channel_bcm[idx];
        float diff_rate = (float)ans / slider_handle->channel_bcm[idx];
        slider_handle->quantify_signal_array[idx] = diff_rate / slider_handle->channel_threshold[idx];
        if (slider_handle->quantify_signal_array[idx] < (CONFIG_TOUCH_SLIDER_SENSOR_QUANTIFY_LOWER_THRESHOLD_X1000 / 1000.f)) {
            slider_handle->quantify_signal_array[idx] = 0;
        }
    }
    for (int idx = 0; idx < slider_handle->channel_num; idx++) {
        slider_handle->quantify_signal_array[idx] = slider_handle->quantify_signal_array[idx] * weight_sum / slider_handle->channel_threshold[idx];
    }
}

/**
 * @brief Calculate max sum subarray
 *
 * This function will figure out the max sum subarray from the
 * input array, return the max sum and max sum start index
 *
 */
static inline float slider_search_max_subarray(const float *array, int array_size, int *max_array_idx)
{
    *max_array_idx = 0;
    float max_array_sum = 0;
    float current_array_sum = 0;
    for (int idx = 0; idx <= (array_size - CONFIG_TOUCH_SLIDER_SENSOR_CALCULATE_CHANNEL); idx++) {
        for (int x = idx; x < idx + CONFIG_TOUCH_SLIDER_SENSOR_CALCULATE_CHANNEL; x++) {
            current_array_sum += array[x];
        }
        if (max_array_sum < current_array_sum) {
            max_array_sum = current_array_sum;
            *max_array_idx = idx;
        }
        current_array_sum = 0;
    }
    return max_array_sum;
}

/**
 * @brief Calculate zero number
 *
 * This function will figure out the number of non-zero items from
 * the subarray
 */
static inline uint8_t slider_get_non_zero_num(const float *array, uint8_t array_idx)
{
    uint8_t zero_cnt = 0;
    for (int idx = array_idx; idx < array_idx + CONFIG_TOUCH_SLIDER_SENSOR_CALCULATE_CHANNEL; idx++) {
        zero_cnt += (array[idx] > 0) ? 1 : 0;
    }
    return zero_cnt;
}

static inline uint32_t slider_calculate_position(touch_slider_handle_t slider_handle, int subarray_index, float subarray_sum, int non_zero_num)
{
    int range = slider_handle->position_range;
    int array_size = slider_handle->channel_num;
    float scale = (float)range / (slider_handle->channel_num - 1);
    const float *array = slider_handle->quantify_signal_array;
    uint32_t position = 0;
    if (non_zero_num == 0) {
        position = slider_handle->position;
    } else if (non_zero_num == 1) {
        for (int index = subarray_index; index < subarray_index + CONFIG_TOUCH_SLIDER_SENSOR_CALCULATE_CHANNEL; index++) {
            if (0 != array[index]) {
                if (index == array_size - 1) {
                    position = range;
                } else {
                    position = (uint32_t)((float)index * scale);
                }
                break;
            }
        }
    } else {
        for (int idx = subarray_index; idx < subarray_index + CONFIG_TOUCH_SLIDER_SENSOR_CALCULATE_CHANNEL; idx++) {
            position += ((float)idx * array[idx]);
        }
        position = position * scale / subarray_sum;
    }
    return position;
}

static uint32_t slider_filter_average(touch_slider_handle_t slider_handle, uint32_t current_position)
{
    uint32_t position_average = 0;
    if (slider_handle->is_first_sample) {
        for (int win_idx = 0; win_idx < CONFIG_TOUCH_SLIDER_SENSOR_POS_FILTER_SIZE; win_idx++) {
            slider_handle->pos_filter_window[win_idx] = current_position; // Preload filter buffer
        }
        slider_handle->is_first_sample = false;
    } else {
        slider_handle->pos_filter_window[slider_handle->pos_window_idx++] = current_position; // Moving average filter
        if (slider_handle->pos_window_idx >= CONFIG_TOUCH_SLIDER_SENSOR_POS_FILTER_SIZE) {
            slider_handle->pos_window_idx = 0;
        }
    }

    for (int win_idx = 0; win_idx < CONFIG_TOUCH_SLIDER_SENSOR_POS_FILTER_SIZE; win_idx++) {
        // Moving average filter
        position_average += slider_handle->pos_filter_window[win_idx];
    }
    position_average = (uint32_t)((float)position_average / CONFIG_TOUCH_SLIDER_SENSOR_POS_FILTER_SIZE + 0.5F);
    return position_average;
}

static inline uint32_t slider_filter_iir(uint32_t in_now, uint32_t out_last, uint32_t k)
{
    if (k == 0) {
        return in_now;
    } else {
        uint32_t out_now = (in_now + (k - 1) * out_last) / k;
        return out_now;
    }
}

/**
 * @brief touch sensor slider position update
 *
 * This function is the core algorithm about touch sensor slider
 * position update, mainly has several steps:
 *      1. Re-quantization
 *      2. Figure out changed channel
 *      3. Calculate position
 *      4. Filter
 *
 */
static void slider_update_position(touch_slider_handle_t slider_handle)
{
    int max_array_idx = 0;
    float max_array_sum;
    uint8_t non_zero_num;
    uint32_t current_position;

    slider_quantify_signal(slider_handle);
    max_array_sum = slider_search_max_subarray(slider_handle->quantify_signal_array, slider_handle->channel_num, &max_array_idx);
    non_zero_num = slider_get_non_zero_num(slider_handle->quantify_signal_array, max_array_idx);
    current_position = slider_calculate_position(slider_handle, max_array_idx, max_array_sum, non_zero_num);
    uint32_t position_average = slider_filter_average(slider_handle, current_position);
    slider_handle->last_position = slider_handle->position == 0 ? position_average : slider_handle->position;
    slider_handle->position = slider_filter_iir(position_average, slider_handle->last_position, CONFIG_TOUCH_SLIDER_SENSOR_POS_FILTER_FACTOR);
}

static touch_slider_event_t update_speed(touch_slider_sensor_t *sensor)
{
    touch_slider_event_t event = TOUCH_SLIDER_EVENT_NONE;
    if (sensor->position != 0) {
        float current_speed = (signed)(sensor->position - sensor->last_position);
        sensor->speed = sensor->speed * sensor->swipe_alpha + current_speed * (1 - sensor->swipe_alpha);
        if (sensor->state != TOUCH_SLIDER_EVENT_RIGHT_SWIPE && sensor->speed > sensor->swipe_threshold + sensor->swipe_hysterisis) {
            event = sensor->state = TOUCH_SLIDER_EVENT_RIGHT_SWIPE;
        } else if (sensor->state != TOUCH_SLIDER_EVENT_LEFT_SWIPE && -sensor->speed > sensor->swipe_threshold + sensor->swipe_hysterisis) {
            event = sensor->state = TOUCH_SLIDER_EVENT_LEFT_SWIPE;
        } else if (sensor->state != TOUCH_SLIDER_EVENT_NONE && fabs(sensor->speed) < sensor->swipe_threshold - sensor->swipe_hysterisis) {
            sensor->state = TOUCH_SLIDER_EVENT_NONE;
        }
    }
    return event;
}

static void update_state(touch_slider_sensor_t *sensor)
{
    sensor->pressed = false;
    // Calculate position if any sensor is pressed
    for (int chan = 0; chan < sensor->channel_num; chan++) {
        if (sensor->channels_active[chan]) {
            sensor->pressed = true;
            break;
        }
    }
    if (sensor->pressed) {
        sensor->channel_bcm_update_cnt = 0;
        sensor->filter_reset_cnt = 0;
        slider_update_position(sensor);
        touch_slider_event_t event = update_speed(sensor);
        if (sensor->start_position == UINT32_MAX) {
            sensor->start_position = sensor->position;
        }
        if (sensor->event_cb) {
            sensor->event_cb(sensor, TOUCH_SLIDER_EVENT_POSITION, sensor->position, sensor->user_data);
        }
        if (event != TOUCH_SLIDER_EVENT_NONE && sensor->event_cb) {
            sensor->event_cb(sensor, event, 0, sensor->user_data);
        }
    } else {
        if (sensor->start_position != UINT32_MAX) {
            if (sensor->event_cb) {
                sensor->event_cb(sensor, TOUCH_SLIDER_EVENT_RELEASE, sensor->position - sensor->start_position, sensor->user_data);
            }
            sensor->start_position = UINT32_MAX;
        }
        // Update benchmark if no sensor is pressed for a while
        if (sensor->channel_bcm_update_cnt++ >= CONFIG_TOUCH_SLIDER_SENSOR_BENCHMARK_UPDATE_TIME) {
            sensor->channel_bcm_update_cnt = 0;
            slider_update_benchmark(sensor);
        }
        if (sensor->filter_reset_cnt++ >= sensor->filter_reset_times) {
            sensor->filter_reset_cnt = 0;
            slider_reset_filter(sensor); // Reset slider filter so as to speed up next time position calculation
        }
    }
}

esp_err_t touch_slider_sensor_handle_events(touch_slider_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    touch_slider_sensor_t *sensor = (touch_slider_sensor_t *)handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG, "Sensor not initialized");

    for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
        if (sensor->fsm_handles[i]) {
            ESP_RETURN_ON_ERROR(
                touch_sensor_fsm_handle_events(sensor->fsm_handles[i]),
                TAG, "Failed to handle FSM events");
#if CONFIG_TOUCH_SLIDER_SENSOR_DEBUG
            uint32_t raw_data[3] = {0};
            for (int j = 0; j < sensor->channel_num; j++) {
                // TODO: handle P4 using offset
                ESP_RETURN_ON_ERROR(
                    touch_sensor_fsm_get_data(sensor->fsm_handles[i], sensor->channel_list[j], raw_data),
                    TAG, "Failed to get FSM data");
                PRINT_VALUE(sensor->channel_list[j] * 100 + i, raw_data[FSM_DATA_RAW], raw_data[FSM_DATA_SMOOTH], raw_data[FSM_DATA_BASELINE]);
            }
#endif
        }
    }
    update_state(sensor);
    return ESP_OK;
}

void touch_slider_sensor_get_state(touch_slider_handle_t handle, bool *pressed, uint32_t *pos, float *speed)
{
    touch_slider_sensor_t *sensor = handle;
    *pressed = sensor->pressed;
    *pos = sensor->position;
    *speed = sensor->speed;
}
