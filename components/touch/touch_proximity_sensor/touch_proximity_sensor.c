/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "soc/soc_caps.h"
#include "esp_check.h"
#include "esp_log.h"
#include "touch_sensor_fsm.h"
#include "touch_sensor_lowlevel.h"
#include "touch_proximity_sensor.h"
#if CONFIG_TOUCH_PROXIMITY_SENSOR_DEBUG
#include "touch_sensor_fsm_private.h"
#endif

static const char *TAG = "touch_proximity_sensor";

#ifndef CONFIG_TOUCH_PROXIMITY_MEAS_COUNT
#define CONFIG_TOUCH_PROXIMITY_MEAS_COUNT 20
#endif

typedef struct touch_proximity_sensor_t {
    uint32_t channel_num;
    uint32_t *channel_list;
    bool *channels_active;
    uint32_t freq_num;
    uint32_t sample_period_ms;
    fsm_handle_t fsm_handle;
    touch_lowlevel_handle_t *lowlevel_handles;
    uint32_t registered_num;
    bool owns_lowlevel;
    bool fsm_started;
    proxi_cb_t proximity_callback;
    void *proximity_cb_arg;
    bool is_initialized;
} proxi_sensor_t;

static int32_t find_channel_index(const proxi_sensor_t *sensor, uint32_t channel)
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

static void fsm_state_callback(fsm_handle_t handle, uint32_t channel, fsm_state_t state,
                               uint32_t data, void *user_data)
{
    (void)handle;
    (void)data;
    proxi_sensor_t *sensor = (proxi_sensor_t *)user_data;
    int32_t index = find_channel_index(sensor, channel);
    if (index < 0) {
        return;
    }

    bool active = state == FSM_STATE_ACTIVE;
    sensor->channels_active[index] = active;
    if (sensor->proximity_callback != NULL) {
        sensor->proximity_callback(channel,
                                   active ? PROXI_STATE_ACTIVE : PROXI_STATE_INACTIVE,
                                   sensor->proximity_cb_arg);
    }
}

static void touch_sensor_callback(uint32_t channel, touch_lowlevel_state_t state,
                                  void *state_data, void *arg)
{
    proxi_sensor_t *sensor = (proxi_sensor_t *)arg;
    if (state != TOUCH_LOWLEVEL_STATE_NEW_DATA || state_data == NULL ||
            sensor->fsm_handle == NULL || sensor->freq_num == 0) {
        return;
    }

    const uint32_t *raw_data = (const uint32_t *)state_data;
    (void)touch_sensor_fsm_update_data(sensor->fsm_handle, channel, raw_data,
                                       sensor->freq_num, true);
}

static esp_err_t proximity_cleanup(proxi_sensor_t *sensor)
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
    free(sensor->channels_active);
    free(sensor->lowlevel_handles);
    free(sensor);
    return first_error;
}

static uint32_t effective_proximity_period(uint32_t scan_period_ms,
                                           uint32_t proximity_count)
{
    uint64_t period = (uint64_t)scan_period_ms * proximity_count;
    return period > UINT32_MAX ? UINT32_MAX : (uint32_t)period;
}

esp_err_t touch_proximity_sensor_create(const touch_proxi_config_t *config,
                                        touch_proximity_handle_t *sensor_handle,
                                        proxi_cb_t cb, void *cb_arg)
{
    ESP_RETURN_ON_FALSE(config != NULL && sensor_handle != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "Invalid arguments");
    *sensor_handle = NULL;
    ESP_RETURN_ON_FALSE(config->channel_num > 0 && config->channel_list != NULL &&
                        config->thresholds != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "Invalid channel configuration");

#if !SOC_TOUCH_SUPPORT_PROX_SENSING
    (void)cb;
    (void)cb_arg;
    return ESP_ERR_NOT_SUPPORTED;
#else
    ESP_RETURN_ON_FALSE(config->channel_num <= SOC_TOUCH_PROXIMITY_CHANNEL_NUM,
                        ESP_ERR_INVALID_ARG, TAG, "Too many proximity channels");
    uint32_t proximity_count = config->proximity_count != 0 ?
                               config->proximity_count : CONFIG_TOUCH_PROXIMITY_MEAS_COUNT;
    ESP_RETURN_ON_FALSE(proximity_count > 0 && proximity_count <= 255,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid proximity count");
    for (uint32_t i = 0; i < config->channel_num; i++) {
        ESP_RETURN_ON_FALSE(config->thresholds[i] > 0, ESP_ERR_INVALID_ARG, TAG,
                            "Proximity thresholds must be nonzero");
        for (uint32_t j = 0; j < i; j++) {
            ESP_RETURN_ON_FALSE(config->channel_list[i] != config->channel_list[j],
                                ESP_ERR_INVALID_ARG, TAG,
                                "Duplicate proximity channel");
        }
    }

    proxi_sensor_t *sensor = calloc(1, sizeof(*sensor));
    ESP_RETURN_ON_FALSE(sensor != NULL, ESP_ERR_NO_MEM, TAG, "No memory for proximity sensor");
    sensor->channel_num = config->channel_num;
    sensor->channel_list = malloc(config->channel_num * sizeof(uint32_t));
    sensor->channels_active = calloc(config->channel_num, sizeof(bool));
    sensor->lowlevel_handles = calloc(config->channel_num, sizeof(touch_lowlevel_handle_t));
    if (sensor->channel_list == NULL || sensor->channels_active == NULL ||
            sensor->lowlevel_handles == NULL) {
        proximity_cleanup(sensor);
        return ESP_ERR_NO_MEM;
    }
    memcpy(sensor->channel_list, config->channel_list,
           config->channel_num * sizeof(uint32_t));
    sensor->proximity_callback = cb;
    sensor->proximity_cb_arg = cb_arg;

    esp_err_t ret;
    if (!config->skip_lowlevel_init) {
        touch_lowlevel_type_t *channel_type =
            calloc(config->channel_num, sizeof(touch_lowlevel_type_t));
        if (channel_type == NULL) {
            proximity_cleanup(sensor);
            return ESP_ERR_NO_MEM;
        }
        for (uint32_t i = 0; i < config->channel_num; i++) {
            channel_type[i] = TOUCH_LOWLEVEL_TYPE_PROXIMITY;
        }

        touch_lowlevel_config_t lowlevel_config = {
            .channel_num = config->channel_num,
            .channel_list = sensor->channel_list,
            .channel_type = channel_type,
            .sample_period_ms = config->sample_period_ms,
            .sample_cfg_num = config->sample_cfg_num,
            .proximity_count = proximity_count,
        };
        ret = touch_sensor_lowlevel_create(&lowlevel_config);
        free(channel_type);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create proximity lowlevel: %s",
                     esp_err_to_name(ret));
            proximity_cleanup(sensor);
            return ret;
        }
        sensor->owns_lowlevel = true;
        ret = touch_sensor_lowlevel_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start proximity lowlevel: %s",
                     esp_err_to_name(ret));
            proximity_cleanup(sensor);
            return ret;
        }
    }

    touch_lowlevel_info_t lowlevel_info = {0};
    ret = touch_sensor_lowlevel_get_info(&lowlevel_info);
    if (ret != ESP_OK) {
        proximity_cleanup(sensor);
        return ret;
    }
    if (lowlevel_info.sample_cfg_num == 0 ||
            lowlevel_info.sample_cfg_num > FSM_MAX_FREQ) {
        ESP_LOGE(TAG, "Unsupported proximity frame width: %" PRIu32,
                 lowlevel_info.sample_cfg_num);
        proximity_cleanup(sensor);
        return ESP_ERR_NOT_SUPPORTED;
    }
    for (uint32_t i = 0; i < sensor->channel_num; i++) {
        int32_t lowlevel_index = lowlevel_channel_index(&lowlevel_info,
                                                        sensor->channel_list[i]);
        if (lowlevel_index < 0) {
            ESP_LOGE(TAG, "Channel %" PRIu32 " is not in proximity lowlevel",
                     sensor->channel_list[i]);
            proximity_cleanup(sensor);
            return ESP_ERR_NOT_FOUND;
        }
        if (lowlevel_info.channel_type != NULL &&
                lowlevel_info.channel_type[lowlevel_index] != TOUCH_LOWLEVEL_TYPE_PROXIMITY) {
            ESP_LOGE(TAG, "Channel %" PRIu32 " is not a proximity channel",
                     sensor->channel_list[i]);
            proximity_cleanup(sensor);
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (lowlevel_info.proximity_count == 0) {
        ESP_LOGE(TAG, "Lowlevel did not report a proximity measurement count");
        proximity_cleanup(sensor);
        return ESP_ERR_INVALID_STATE;
    }
    if (config->proximity_count != 0 &&
            lowlevel_info.proximity_count != config->proximity_count) {
        ESP_LOGE(TAG, "Requested proximity count=%" PRIu32 " but lowlevel uses=%" PRIu32,
                 config->proximity_count, lowlevel_info.proximity_count);
        proximity_cleanup(sensor);
        return ESP_ERR_INVALID_STATE;
    }
    sensor->freq_num = lowlevel_info.sample_cfg_num;
    sensor->sample_period_ms = effective_proximity_period(lowlevel_info.sample_period_ms,
                                                          lowlevel_info.proximity_count);
    if (sensor->sample_period_ms == 0) {
        sensor->sample_period_ms = 20 * lowlevel_info.proximity_count;
    }

    fsm_config_t fsm_config;
    ret = touch_sensor_fsm_get_default_config(sensor->freq_num,
                                              sensor->sample_period_ms,
                                              &fsm_config);
    if (ret != ESP_OK) {
        proximity_cleanup(sensor);
        return ret;
    }
    fsm_config.mode = FSM_MODE_USER_PUSH;
    fsm_config.channel_num = sensor->channel_num;
    fsm_config.channel_list = sensor->channel_list;
    fsm_config.freq_num = sensor->freq_num;
    fsm_config.threshold_active = config->thresholds;
    fsm_config.debounce_active = config->debounce_active;
    fsm_config.debounce_inactive = config->debounce_inactive;
    fsm_config.sample_period_ms = sensor->sample_period_ms;
    fsm_config.state_cb = fsm_state_callback;
    fsm_config.user_data = sensor;

    ret = touch_sensor_fsm_create(&fsm_config, &sensor->fsm_handle);
    if (ret != ESP_OK) {
        proximity_cleanup(sensor);
        return ret;
    }
    ret = touch_sensor_fsm_control(sensor->fsm_handle, FSM_CTRL_START, NULL);
    if (ret != ESP_OK) {
        proximity_cleanup(sensor);
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
            proximity_cleanup(sensor);
            return ret;
        }
        sensor->registered_num++;
    }

    sensor->is_initialized = true;
    *sensor_handle = (touch_proximity_handle_t)sensor;
    ESP_LOGI(TAG, "Created with %" PRIu32 " channels, %" PRIu32
             " frequency samples, effective frame period %" PRIu32 " ms",
             sensor->channel_num, sensor->freq_num, sensor->sample_period_ms);
    return ESP_OK;
#endif
}

esp_err_t touch_proximity_sensor_delete(touch_proximity_handle_t handle)
{
    return proximity_cleanup((proxi_sensor_t *)handle);
}

esp_err_t touch_proximity_sensor_get_data(touch_proximity_handle_t handle,
                                          uint32_t channel, uint32_t *data)
{
    ESP_RETURN_ON_FALSE(handle != NULL && data != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "Invalid arguments");
    proxi_sensor_t *sensor = (proxi_sensor_t *)handle;
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

esp_err_t touch_proximity_sensor_get_state(touch_proximity_handle_t handle,
                                           uint32_t channel, proxi_state_t *state)
{
    ESP_RETURN_ON_FALSE(handle != NULL && state != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "Invalid arguments");
    proxi_sensor_t *sensor = (proxi_sensor_t *)handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG,
                        "Sensor not initialized");
    int32_t index = find_channel_index(sensor, channel);
    ESP_RETURN_ON_FALSE(index >= 0, ESP_ERR_NOT_FOUND, TAG, "Channel is not configured");
    *state = sensor->channels_active[index] ? PROXI_STATE_ACTIVE : PROXI_STATE_INACTIVE;
    return ESP_OK;
}

esp_err_t touch_proximity_sensor_handle_events(touch_proximity_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    proxi_sensor_t *sensor = (proxi_sensor_t *)handle;
    ESP_RETURN_ON_FALSE(sensor->is_initialized, ESP_ERR_INVALID_STATE, TAG,
                        "Sensor not initialized");

    esp_err_t ret = touch_sensor_fsm_handle_events(sensor->fsm_handle);
    if (ret != ESP_OK) {
        return ret;
    }
#if CONFIG_TOUCH_PROXIMITY_SENSOR_DEBUG
    for (uint32_t i = 0; i < sensor->channel_num; i++) {
        touch_fsm_info_t info = {0};
        if (touch_sensor_fsm_get_info(sensor->fsm_handle,
                                      sensor->channel_list[i], &info) != ESP_OK) {
            continue;
        }
        for (uint32_t f = 0; f < sensor->freq_num; f++) {
            printf("vl,%" PRIu64 "," TOUCH_FSM_INFO_PRINT_FORMAT "\n",
                   (uint64_t)0,
                   TOUCH_FSM_INFO_PRINT_ARGS(sensor->channel_list[i], f, &info));
        }
    }
#endif
    return ESP_OK;
}
