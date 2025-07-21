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
#include "esp_timer.h"
#include "touch_sensor_lowlevel.h"
#include "touch_sensor_fsm.h"
#include "touch_button_sensor.h"

static const char *TAG = "touch_button_sensor";

#if CONFIG_TOUCH_BUTTON_SENSOR_DEBUG
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
#define PRINT_VALUE(pad_num, raw, smooth, benchmark) printf("vl,%"PRId64",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32"\n", get_time_in_ms(), pad_num, raw, smooth, benchmark)
// print trigger if touch active/inactive happens
#define PRINT_TRIGGER(pad_num, smooth, if_active) printf("tg,%"PRId64",%"PRIu32",%"PRIu32",%d,%d,%d,%d,%d\n", get_time_in_ms(), pad_num, smooth, if_active?1:0, if_active?0:1,0,0,0)
#else
#define PRINT_VALUE(pad_num, raw, smooth, benchmark)
#define PRINT_TRIGGER(pad_num, smooth, if_active)
#endif

// Time window constants in milliseconds (configurable via menuconfig)
#define SIMULTANEOUS_TRIGGER_WINDOW_MS CONFIG_TOUCH_BUTTON_SENSOR_SIMULTANEOUS_TRIGGER_WINDOW_MS
#define TRIGGER_CLEANUP_TIMEOUT_MS CONFIG_TOUCH_BUTTON_SENSOR_TRIGGER_CLEANUP_TIMEOUT_MS

// Optimized time retrieval function (called only once per event handling cycle)
static uint64_t get_current_time_ms(void)
{
#if CONFIG_TOUCH_BUTTON_SENSOR_DEBUG
    return get_time_in_ms();
#else
    return esp_timer_get_time() / 1000;  // Convert from microseconds to milliseconds
#endif
}

#define TOUCH_BUTTON_CHECK_GOTO(a, str, label) \
    if (!(a)) { \
        FSM_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        goto label; \
    }

typedef struct touch_button_sensor_t {
    uint32_t channel_num;
    uint32_t *channel_list;
    fsm_handle_t fsm_handles[SOC_TOUCH_SAMPLE_CFG_NUM];
    uint32_t last_smooth_data[SOC_TOUCH_SENSOR_NUM][SOC_TOUCH_SAMPLE_CFG_NUM];
    volatile uint32_t active_freq_bitmap[SOC_TOUCH_SENSOR_NUM];
    volatile uint32_t active_freq_bitmap_last[SOC_TOUCH_SENSOR_NUM];
    volatile uint64_t freq_trigger_time[SOC_TOUCH_SENSOR_NUM][SOC_TOUCH_SAMPLE_CFG_NUM];  // Time for each frequency trigger
    uint64_t current_time_ms;      // Current time cached for one handle_events cycle
    bool channels_active[SOC_TOUCH_SENSOR_NUM];
    touch_cb_t state_change_cb;
    void *user_data;
    bool is_initialized;
    touch_lowlevel_handle_t lowlevel_handle[SOC_TOUCH_SENSOR_NUM];
    bool skip_lowlevel_init;
} touch_button_sensor_t;

// Clean up old triggers that are no longer relevant
static void cleanup_old_triggers(touch_button_sensor_t *sensor, uint32_t channel_idx, uint64_t current_time)
{
    for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
        if (sensor->freq_trigger_time[channel_idx][i] > 0 &&
                (current_time - sensor->freq_trigger_time[channel_idx][i]) > TRIGGER_CLEANUP_TIMEOUT_MS) {
            // Clear both the trigger time and the corresponding bit in active_freq_bitmap
            sensor->freq_trigger_time[channel_idx][i] = 0;
            sensor->active_freq_bitmap[channel_idx] &= ~(1 << i);
        }
    }
}

static esp_err_t find_channel_index(touch_button_sensor_t *sensor, uint32_t channel, uint32_t *channel_idx)
{
    for (int i = 0; i < sensor->channel_num; i++) {
        if (sensor->channel_list[i] == channel) {
            *channel_idx = i;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

static int get_fsm_frequency_index(touch_button_sensor_t *sensor, fsm_handle_t handle)
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
    touch_button_sensor_t *sensor = (touch_button_sensor_t *)user_data;
    int frequency = get_fsm_frequency_index(sensor, handle);
    uint32_t channel_idx = 0;
    uint64_t current_time = sensor->current_time_ms;  // Use cached time from handle_events

    if (frequency < 0 || find_channel_index(sensor, channel, &channel_idx) != ESP_OK) {
        return;
    }

    if (state == FSM_STATE_ACTIVE) {
        sensor->active_freq_bitmap[channel_idx] |= (1 << frequency);
        sensor->freq_trigger_time[channel_idx][frequency] = current_time;
        if (frequency) {
            PRINT_TRIGGER(channel + frequency, data, true);
        }
    } else if (state == FSM_STATE_INACTIVE) {
        sensor->active_freq_bitmap[channel_idx] &= ~(1 << frequency);
        sensor->active_freq_bitmap_last[channel_idx] &= ~(1 << frequency);
        sensor->freq_trigger_time[channel_idx][frequency] = 0;  // Clear trigger time
        if (frequency) {
            PRINT_TRIGGER(channel + frequency, data, false);
        }
    }

    // Count active frequencies and check if they are within the time window
    uint32_t simultaneous_active_count = 0;
    uint64_t earliest_trigger_time = UINT64_MAX;

    // Find the earliest trigger time among active frequencies
    for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
        if (sensor->active_freq_bitmap[channel_idx] & (1 << i)) {
            uint64_t trigger_time = sensor->freq_trigger_time[channel_idx][i];
            if (trigger_time > 0 && trigger_time < earliest_trigger_time) {
                earliest_trigger_time = trigger_time;
            }
        }
    }

    // Count frequencies that triggered within the time window
    if (earliest_trigger_time != UINT64_MAX) {
        for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
            if (sensor->active_freq_bitmap[channel_idx] & (1 << i)) {
                uint64_t trigger_time = sensor->freq_trigger_time[channel_idx][i];
                if (trigger_time > 0 &&
                        (trigger_time - earliest_trigger_time) <= SIMULTANEOUS_TRIGGER_WINDOW_MS) {
                    simultaneous_active_count++;
                }
            }
        }
    }

    // State change detection - require simultaneous triggers within time window
    if (simultaneous_active_count >= SOC_TOUCH_SAMPLE_CFG_NUM - 1 && !sensor->channels_active[channel_idx]) {
        sensor->channels_active[channel_idx] = true;
        sensor->active_freq_bitmap_last[channel_idx] = sensor->active_freq_bitmap[channel_idx];
        PRINT_TRIGGER(channel, data, true);
        if (sensor->state_change_cb) {
            sensor->state_change_cb(sensor, channel, TOUCH_STATE_ACTIVE, sensor->user_data);
        }
    } else if (sensor->active_freq_bitmap_last[channel_idx] == 0 && sensor->channels_active[channel_idx]) {
        sensor->channels_active[channel_idx] = false;
        PRINT_TRIGGER(channel, data, false);
        if (sensor->state_change_cb) {
            sensor->state_change_cb(sensor, channel, TOUCH_STATE_INACTIVE, sensor->user_data);
        }
        sensor->active_freq_bitmap[channel_idx] = 0;
        // Clear all trigger times for this channel
        for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
            sensor->freq_trigger_time[channel_idx][i] = 0;
        }
    }
}

#if !CONFIG_IDF_TARGET_ESP32
static void touch_sensor_callback(uint32_t channel, touch_lowlevel_state_t state, void *state_data, void *arg)
{
    touch_button_sensor_t *sensor = (touch_button_sensor_t *)arg;
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

esp_err_t touch_button_sensor_create(touch_button_config_t *config, touch_button_handle_t *handle, touch_cb_t cb, void *cb_arg)
{
    ESP_RETURN_ON_FALSE(config && handle, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    ESP_RETURN_ON_FALSE(config->channel_num > 0 && config->channel_list, ESP_ERR_INVALID_ARG, TAG, "Invalid channel config");

    touch_button_sensor_t *sensor = calloc(1, sizeof(touch_button_sensor_t));
    ESP_RETURN_ON_FALSE(sensor, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory");

    // Deep copy channel list
    sensor->channel_num = config->channel_num;
    sensor->channel_list = malloc(config->channel_num * sizeof(uint32_t));
    if (!sensor->channel_list) {
        free(sensor);
        return ESP_ERR_NO_MEM;
    }
    memcpy(sensor->channel_list, config->channel_list, config->channel_num * sizeof(uint32_t));
    sensor->state_change_cb = cb;
    sensor->user_data = cb_arg;
    sensor->skip_lowlevel_init = config->skip_lowlevel_init;  // Store this flag

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
        noise_p[i] = config->channel_threshold[i] / CONFIG_TOUCH_BUTTON_SENSOR_NOISE_P_SNR;
        noise_n[i] = config->channel_threshold[i] / CONFIG_TOUCH_BUTTON_SENSOR_NOISE_N_SNR;
    }

    // Initialize touch sensor low level if needed
    if (!config->skip_lowlevel_init) {
        touch_lowlevel_config_t ll_config = {
            .channel_num = config->channel_num,
            .channel_list = config->channel_list,
            .channel_type = NULL,  // Using default channel type
            .sample_period_ms = config->channel_num > 7 ? 20 : 10,  // Use 20ms for more than 7 channels, otherwise 10ms
        };
        TOUCH_BUTTON_CHECK_GOTO(
            touch_sensor_lowlevel_create(&ll_config) == ESP_OK,
            "Failed to create touch sensor low level", cleanup
        );
    }

    // Initialize FSMs
    fsm_config_t fsm_cfg = DEFAULTS_TOUCH_SENSOR_FSM_CONFIG();
    // Register callbacks for each channel
#if !CONFIG_IDF_TARGET_ESP32
    for (uint32_t i = 0; i < config->channel_num; i++) {
        TOUCH_BUTTON_CHECK_GOTO(
            touch_sensor_lowlevel_register(config->channel_list[i],
                                           touch_sensor_callback,
                                           sensor,
                                           &sensor->lowlevel_handle[i]) == ESP_OK,
            "Failed to register touch sensor callback", cleanup
        );
    }
    fsm_cfg.mode = FSM_MODE_USER_PUSH;
    fsm_cfg.active_low = false;
    fsm_cfg.calibration_times = CONFIG_TOUCH_BUTTON_SENSOR_CALIBRATION_TIMES;
    fsm_cfg.debounce_inactive = CONFIG_TOUCH_BUTTON_SENSOR_DEBOUNCE_INACTIVE;
#else
    fsm_cfg.mode = FSM_MODE_POLLING;
    fsm_cfg.polling_interval = CONFIG_TOUCH_BUTTON_SENSOR_POLLING_INTERVAL;
    fsm_cfg.polling_cb = polling_callback;
    fsm_cfg.active_low = true;
    fsm_cfg.calibration_times = CONFIG_TOUCH_BUTTON_SENSOR_CALIBRATION_TIMES;
    fsm_cfg.debounce_inactive = CONFIG_TOUCH_BUTTON_SENSOR_DEBOUNCE_INACTIVE;
#endif
    fsm_cfg.state_cb = fsm_state_cb;
    fsm_cfg.channel_num = config->channel_num;
    fsm_cfg.channel_list = config->channel_list;
    fsm_cfg.threshold_p = config->channel_threshold;
#if CONFIG_TOUCH_BUTTON_SENSOR_NEGATIVE_LOGIC
    fsm_cfg.threshold_n = config->channel_threshold;
#else
    fsm_cfg.threshold_n = NULL;
#endif
    fsm_cfg.gold_value = config->channel_gold_value;
    fsm_cfg.debounce_active = config->debounce_times;
    fsm_cfg.smooth_coef = CONFIG_TOUCH_BUTTON_SENSOR_SMOOTH_COEF_X1000 / 1000.0f;
    fsm_cfg.baseline_coef = CONFIG_TOUCH_BUTTON_SENSOR_BASELINE_COEF_X1000 / 1000.0f;
    if (CONFIG_TOUCH_BUTTON_SENSOR_MAX_P_X1000 == 0) {
        // set max_p to the 2 times of maximum of the threshold_p
        float max_threshold = 0.0f;
        for (uint32_t i = 0; i < config->channel_num; i++) {
            if (config->channel_threshold[i] > max_threshold) {
                max_threshold = config->channel_threshold[i];
            }
        }
        fsm_cfg.max_p = max_threshold * 2.0f;
    } else {
        fsm_cfg.max_p = CONFIG_TOUCH_BUTTON_SENSOR_MAX_P_X1000 / 1000.0f;
    }
    if (CONFIG_TOUCH_BUTTON_SENSOR_MIN_N_X1000 == 0) {
        // set min_n to the 2 times of minimum of the threshold_p
        float min_threshold = 0.0f;
        for (uint32_t i = 0; i < config->channel_num; i++) {
            if (config->channel_threshold[i] > min_threshold) {
                min_threshold = config->channel_threshold[i];
            }
        }
        fsm_cfg.min_n = min_threshold * 2.0f;
    } else {
        fsm_cfg.min_n = CONFIG_TOUCH_BUTTON_SENSOR_MIN_N_X1000 / 1000.0f;
    }
    fsm_cfg.hysteresis_active = 0.1f;
    fsm_cfg.hysteresis_inactive = 0.1f;
    fsm_cfg.noise_p = noise_p;
    fsm_cfg.noise_n = noise_n;
    fsm_cfg.reset_cover = CONFIG_TOUCH_BUTTON_SENSOR_RESET_COVER;
    fsm_cfg.reset_calibration = CONFIG_TOUCH_BUTTON_SENSOR_RESET_CALIBRATION;
    fsm_cfg.raw_buf_size = CONFIG_TOUCH_BUTTON_SENSOR_RAW_BUF_SIZE;
    fsm_cfg.scale_factor = CONFIG_TOUCH_BUTTON_SENSOR_SCALE_FACTOR;
    fsm_cfg.user_data = sensor;

    for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
        TOUCH_BUTTON_CHECK_GOTO(touch_sensor_fsm_create(&fsm_cfg, &sensor->fsm_handles[i]) == ESP_OK, "Failed to create FSM", cleanup);
        TOUCH_BUTTON_CHECK_GOTO(touch_sensor_fsm_control(sensor->fsm_handles[i], FSM_CTRL_START, NULL) == ESP_OK, "Failed to start FSM", cleanup);
    }

    // Free the noise threshold arrays as they've been copied during FSM creation
    free(noise_p);
    free(noise_n);

    if (!config->skip_lowlevel_init) {
        TOUCH_BUTTON_CHECK_GOTO(touch_sensor_lowlevel_start() == ESP_OK, "Failed to start touch sensor low level", cleanup);
    }

    sensor->is_initialized = true;
    *handle = sensor;
    return ESP_OK;

cleanup:
    if (noise_p) {
        free(noise_p);
    }
    if (noise_n) {
        free(noise_n);
    }
    touch_button_sensor_delete(sensor);
    return ESP_FAIL;
}

esp_err_t touch_button_sensor_delete(touch_button_handle_t handle)
{
    if (!handle) {
        return ESP_OK;
    }

    touch_button_sensor_t *sensor = (touch_button_sensor_t *)handle;
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
#if !CONFIG_IDF_TARGET_ESP32
    for (int i = 0; i < sensor->channel_num; i++) {
        touch_sensor_lowlevel_unregister(sensor->lowlevel_handle[i]);
    }
#endif

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

esp_err_t touch_button_sensor_get_data(touch_button_handle_t handle, uint32_t channel, uint32_t channel_alt, uint32_t *data)
{
    ESP_RETURN_ON_FALSE(handle && data, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    ESP_RETURN_ON_FALSE(channel_alt < SOC_TOUCH_SAMPLE_CFG_NUM, ESP_ERR_INVALID_ARG, TAG, "Invalid channel_alt");

    touch_button_sensor_t *sensor = (touch_button_sensor_t *)handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG, "Sensor not initialized");

    uint32_t channel_idx;
    ESP_RETURN_ON_ERROR(find_channel_index(sensor, channel, &channel_idx), TAG, "Channel not found");

    // Get data from specified frequency FSM
    uint32_t raw_data[3] = {0};
    ESP_RETURN_ON_ERROR(
        touch_sensor_fsm_get_data(sensor->fsm_handles[channel_alt], channel, raw_data),
        TAG, "Failed to get FSM data"
    );

    *data = raw_data[FSM_DATA_SMOOTH];
    return ESP_OK;
}

esp_err_t touch_button_sensor_get_state(touch_button_handle_t handle, uint32_t channel, touch_state_t *state)
{
    ESP_RETURN_ON_FALSE(handle && state, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    touch_button_sensor_t *sensor = (touch_button_sensor_t *)handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG, "Sensor not initialized");

    uint32_t channel_idx;
    ESP_RETURN_ON_ERROR(find_channel_index(sensor, channel, &channel_idx), TAG, "Channel not found");

    *state = sensor->channels_active[channel_idx] ? TOUCH_STATE_ACTIVE : TOUCH_STATE_INACTIVE;
    return ESP_OK;
}

esp_err_t touch_button_sensor_get_state_bitmap(touch_button_handle_t handle, uint32_t channel, uint32_t *bitmap)
{
    ESP_RETURN_ON_FALSE(handle && bitmap, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    touch_button_sensor_t *sensor = (touch_button_sensor_t *)handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG, "Sensor not initialized");

    uint32_t channel_idx;
    ESP_RETURN_ON_ERROR(find_channel_index(sensor, channel, &channel_idx), TAG, "Channel not found");

    *bitmap = sensor->active_freq_bitmap[channel_idx];
    return ESP_OK;
}

esp_err_t touch_button_sensor_handle_events(touch_button_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    touch_button_sensor_t *sensor = (touch_button_sensor_t *)handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG, "Sensor not initialized");

    // Get current time once for this entire event handling cycle
    sensor->current_time_ms = get_current_time_ms();

    // Periodic cleanup of old triggers for all channels using the cached time
    for (uint32_t ch = 0; ch < sensor->channel_num; ch++) {
        cleanup_old_triggers(sensor, ch, sensor->current_time_ms);
    }

    for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
        if (sensor->fsm_handles[i]) {
            ESP_RETURN_ON_ERROR(
                touch_sensor_fsm_handle_events(sensor->fsm_handles[i]),
                TAG, "Failed to handle FSM events"
            );
#if CONFIG_TOUCH_BUTTON_SENSOR_DEBUG
            uint32_t raw_data[3] = {0};
            for (int j = 0; j < sensor->channel_num; j++) {
                ESP_RETURN_ON_ERROR(
                    touch_sensor_fsm_get_data(sensor->fsm_handles[i], sensor->channel_list[j], raw_data),
                    TAG, "Failed to get FSM data"
                );
                PRINT_VALUE(sensor->channel_list[j] + i * 100, raw_data[FSM_DATA_RAW], raw_data[FSM_DATA_SMOOTH], raw_data[FSM_DATA_BASELINE]);
            }
#endif
        }
    }
    return ESP_OK;
}
