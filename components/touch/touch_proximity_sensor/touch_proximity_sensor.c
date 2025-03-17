/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include "soc/soc_caps.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "touch_proximity_sensor.h"
#include "touch_sensor_fsm.h"
#include "touch_sensor_lowlevel.h"

const static char *TAG = "touch-prox-sensor";

#if CONFIG_TOUCH_PROXIMITY_SENSOR_DEBUG
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

typedef struct {
    uint32_t channel_num;
    uint32_t *channel_list;
    fsm_handle_t fsm_handle;
    proxi_cb_t proximity_callback;
    void *proximity_cb_arg;
    bool channels_active[SOC_TOUCH_PROXIMITY_CHANNEL_NUM];
    touch_lowlevel_handle_t lowlevel_handle[SOC_TOUCH_PROXIMITY_CHANNEL_NUM];
    bool skip_lowlevel_init;
    bool is_initialized;
} proxi_sensor_t;

static void fsm_state_callback(fsm_handle_t handle, uint32_t channel, fsm_state_t state, uint32_t data, void *user_data)
{
    proxi_sensor_t *sensor = (proxi_sensor_t *)user_data;

    bool is_active = (state == FSM_STATE_ACTIVE);
    for (int i = 0; i < sensor->channel_num; i++) {
        if (sensor->channel_list[i] == channel) {
            sensor->channels_active[i] = is_active;
            PRINT_TRIGGER(channel, data, is_active);
            if (sensor->proximity_callback) {
                proxi_state_t event = is_active ? PROXI_STATE_ACTIVE : PROXI_STATE_INACTIVE;
                sensor->proximity_callback(channel, event, sensor->proximity_cb_arg);
            }
            return;
        }
    }
}

static void _touch_intr_cb(uint32_t channel, touch_lowlevel_state_t state, void *state_data, void *arg)
{
    proxi_sensor_t *sensor = (proxi_sensor_t *)arg;
    if (state == TOUCH_LOWLEVEL_STATE_NEW_DATA) {
        uint32_t raw_data = *(uint32_t *)state_data;
        touch_sensor_fsm_update_data(sensor->fsm_handle, channel, raw_data, true);
    }
}

esp_err_t touch_proximity_sensor_create(touch_proxi_config_t *config, touch_proximity_handle_t *sensor_handle, proxi_cb_t cb, void *cb_arg)
{
    ESP_RETURN_ON_FALSE(config && sensor_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    ESP_RETURN_ON_FALSE(config->channel_num > 0 && config->channel_list, ESP_ERR_INVALID_ARG, TAG, "Invalid channel config");
    ESP_RETURN_ON_FALSE(config->channel_num <= SOC_TOUCH_PROXIMITY_CHANNEL_NUM, ESP_ERR_INVALID_ARG, TAG, "Too many channels");

    proxi_sensor_t *sensor = calloc(1, sizeof(proxi_sensor_t));
    ESP_RETURN_ON_FALSE(sensor, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory");

    sensor->channel_num = config->channel_num;
    sensor->channel_list = malloc(config->channel_num * sizeof(uint32_t));

    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(sensor->channel_list, ESP_ERR_NO_MEM, cleanup, TAG, "Failed to allocate channel list");
    memcpy(sensor->channel_list, config->channel_list, config->channel_num * sizeof(uint32_t));

    sensor->proximity_callback = cb;
    sensor->proximity_cb_arg = cb_arg;
    sensor->skip_lowlevel_init = config->skip_lowlevel_init;

    if (!config->skip_lowlevel_init) {
        touch_lowlevel_type_t *channel_type = calloc(config->channel_num, sizeof(touch_lowlevel_type_t));
        ESP_GOTO_ON_FALSE(channel_type, ESP_ERR_NO_MEM, cleanup, TAG, "Failed to allocate channel types");

        for (int i = 0; i < config->channel_num; i++) {
            channel_type[i] = TOUCH_LOWLEVEL_TYPE_PROXIMITY;
        }

        touch_lowlevel_config_t low_config = {
            .channel_num = config->channel_num,
            .channel_list = config->channel_list,
            .channel_type = channel_type,
            .proximity_count = CONFIG_TOUCH_PROXIMITY_MEAS_COUNT,
        };
        esp_err_t ret = touch_sensor_lowlevel_create(&low_config);
        free(channel_type);
        ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "Failed to create touch sensor lowlevel");
    }

    fsm_config_t fsm_config = {
        .mode = FSM_MODE_USER_PUSH,
        .state_cb = fsm_state_callback,
        .channel_num = config->channel_num,
        .channel_list = config->channel_list,
        .threshold_p = config->channel_threshold,
        .threshold_n = NULL, // not use negative logic
        .gold_value = config->channel_gold_value,
        .debounce_p = config->debounce_times,
        .debounce_n = config->debounce_times,
        .smooth_coef = CONFIG_TOUCH_PROXIMITY_SMOOTH_COEF_X1000 / 1000.0f,
        .baseline_coef = CONFIG_TOUCH_PROXIMITY_BASELINE_COEF_X1000 / 1000.0f,
        .max_p = CONFIG_TOUCH_PROXIMITY_MAX_P_X1000 / 1000.0f,
        .min_n = CONFIG_TOUCH_PROXIMITY_MIN_N_X1000 / 1000.0f,
        .hysteresis_p = 0.1f, // 10% hysteresis
        .noise_p = config->channel_threshold[0] / CONFIG_TOUCH_PROXIMITY_NOISE_P_SNR,
        .noise_n = config->channel_threshold[0] / CONFIG_TOUCH_PROXIMITY_NOISE_N_SNR,
        .reset_p = CONFIG_TOUCH_PROXIMITY_RESET_P,
        .reset_n = CONFIG_TOUCH_PROXIMITY_RESET_N,
        .raw_buf_size = CONFIG_TOUCH_PROXIMITY_RAW_BUF_SIZE,
        .scale_factor = 1,
        .user_data = sensor,
    };

    ESP_GOTO_ON_ERROR(
        touch_sensor_fsm_create(&fsm_config, &sensor->fsm_handle),
        cleanup, TAG, "Failed to create FSM"
    );

    for (int i = 0; i < config->channel_num; i++) {
        ESP_GOTO_ON_ERROR(
            touch_sensor_lowlevel_register(config->channel_list[i], _touch_intr_cb, sensor, &sensor->lowlevel_handle[i]),
            cleanup, TAG, "Failed to register channel %d", i
        );
    }

    ESP_GOTO_ON_ERROR(
        touch_sensor_fsm_control(sensor->fsm_handle, FSM_CTRL_START, NULL),
        cleanup, TAG, "Failed to start FSM"
    );

    if (!config->skip_lowlevel_init) {
        ESP_GOTO_ON_ERROR(
            touch_sensor_lowlevel_start(),
            cleanup, TAG, "Failed to start touch sensor lowlevel"
        );
    }

    sensor->is_initialized = true;
    *sensor_handle = (touch_proximity_handle_t)sensor;
    ESP_LOGI(TAG, "Touch Proximity Driver Version: %d.%d.%d",
             TOUCH_PROXIMITY_SENSOR_VER_MAJOR, TOUCH_PROXIMITY_SENSOR_VER_MINOR, TOUCH_PROXIMITY_SENSOR_VER_PATCH);
    return ESP_OK;

cleanup:
    touch_proximity_sensor_delete((touch_proximity_handle_t)sensor);
    return ret;
}

esp_err_t touch_proximity_sensor_delete(touch_proximity_handle_t proxi_sensor)
{
    if (!proxi_sensor) {
        return ESP_OK;
    }
    proxi_sensor_t *sensor = (proxi_sensor_t *)proxi_sensor;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG, "Sensor not initialized");

    if (!sensor->skip_lowlevel_init) {
        touch_sensor_lowlevel_stop();
    }

    if (sensor->fsm_handle) {
        touch_sensor_fsm_control(sensor->fsm_handle, FSM_CTRL_STOP, NULL);
        touch_sensor_fsm_delete(sensor->fsm_handle);
    }

    for (int i = 0; i < sensor->channel_num; i++) {
        touch_sensor_lowlevel_unregister(sensor->lowlevel_handle[i]);
    }

    if (!sensor->skip_lowlevel_init) {
        touch_sensor_lowlevel_delete();
    }

    free(sensor->channel_list);
    free(sensor);
    return ESP_OK;
}

esp_err_t touch_proximity_sensor_get_data(touch_proximity_handle_t handle, uint32_t channel, uint32_t *data)
{
    ESP_RETURN_ON_FALSE(handle && data, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    proxi_sensor_t *sensor = (proxi_sensor_t *)handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG, "Sensor not initialized");

    uint32_t raw_data[3] = {0};
    ESP_RETURN_ON_ERROR(
        touch_sensor_fsm_get_data(sensor->fsm_handle, channel, raw_data),
        TAG, "Failed to get FSM data"
    );

    *data = raw_data[FSM_DATA_SMOOTH];
    return ESP_OK;
}

esp_err_t touch_proximity_sensor_get_state(touch_proximity_handle_t handle, uint32_t channel, proxi_state_t *state)
{
    ESP_RETURN_ON_FALSE(handle && state, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    proxi_sensor_t *sensor = (proxi_sensor_t *)handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG, "Sensor not initialized");

    for (int i = 0; i < sensor->channel_num; i++) {
        if (sensor->channel_list[i] == channel) {
            *state = sensor->channels_active[i] ? PROXI_STATE_ACTIVE : PROXI_STATE_INACTIVE;
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t touch_proximity_sensor_handle_events(touch_proximity_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    proxi_sensor_t *sensor = (proxi_sensor_t *)handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG, "Sensor not initialized");

    ESP_RETURN_ON_ERROR(touch_sensor_fsm_handle_events(sensor->fsm_handle), TAG, "Failed to handle FSM events");
#if CONFIG_TOUCH_PROXIMITY_SENSOR_DEBUG
    uint32_t raw_data[3] = {0};
    for (int j = 0; j < sensor->channel_num; j++) {
        // TODO: handle P4 using offset
        ESP_RETURN_ON_ERROR(
            touch_sensor_fsm_get_data(sensor->fsm_handle, sensor->channel_list[j], raw_data),
            TAG, "Failed to get FSM data"
        );
        PRINT_VALUE(sensor->channel_list[j], raw_data[FSM_DATA_RAW], raw_data[FSM_DATA_SMOOTH], raw_data[FSM_DATA_BASELINE]);
    }
#endif
    return ESP_OK;
}
