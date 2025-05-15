/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_check.h"
#include "adc_battery_estimation.h"

static const char* TAG = "adc_battery_estimation";

typedef struct {
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t adc_cali_handle;
    adc_channel_t adc_channel;
    adc_unit_t adc_unit;
    bool is_adc_handle_owned;
    const battery_point_t *battery_points;
    size_t battery_points_count;
    adc_battery_charging_detect_cb_t charging_detect_cb;
    void *charging_detect_user_data;
    float last_capacity;          /*!< Last calculated battery capacity in percentage */
    bool is_first_read;           /*!< Flag for first capacity reading */
    bool last_charging_state;     /*!< Last charging state */
    float voltage_divider_ratio;  /*!< Voltage divider ratio */
    float filter_alpha;           /*!< Low-pass filter coefficient (0 < alpha < 1) */
} adc_battery_estimation_ctx_t;

// Helper function to calculate battery capacity based on voltage
static float calculate_battery_capacity(float voltage, const battery_point_t *points, size_t points_count)
{
    // Find the two points that bracket the current voltage
    size_t i;
    for (i = 0; i < points_count - 1; i++) {
        if (voltage >= points[i + 1].voltage && voltage <= points[i].voltage) {
            // Linear interpolation between the two points
            float voltage_range = points[i].voltage - points[i + 1].voltage;
            float capacity_range = points[i].capacity - points[i + 1].capacity;
            float voltage_offset = voltage - points[i + 1].voltage;

            return points[i + 1].capacity + (voltage_offset * capacity_range / voltage_range);
        }
    }

    // If voltage is outside the range, clamp to the nearest point
    if (voltage > points[0].voltage) {
        return points[0].capacity;
    } else {
        return points[points_count - 1].capacity;
    }
}

adc_battery_estimation_handle_t adc_battery_estimation_create(adc_battery_estimation_t *config)
{
    ESP_RETURN_ON_FALSE(config, NULL, TAG, "Config is NULL");

    adc_battery_estimation_ctx_t *ctx = (adc_battery_estimation_ctx_t *) calloc(1, sizeof(adc_battery_estimation_ctx_t));
    ESP_RETURN_ON_FALSE(ctx, NULL, TAG, "Failed to allocate memory for context");

    ctx->adc_channel = config->adc_channel;
    ctx->charging_detect_cb = config->charging_detect_cb;
    ctx->charging_detect_user_data = config->charging_detect_user_data;
    ctx->is_first_read = true;

    // Use default battery points if not provided
    if (config->battery_points == NULL || config->battery_points_count == 0) {
        ctx->battery_points = default_battery_points;
        ctx->battery_points_count = DEFAULT_POINTS_COUNT;
    } else {
        ctx->battery_points = config->battery_points;
        ctx->battery_points_count = config->battery_points_count;
    }

    // Use external ADC handle if provided
    if (config->external.adc_handle != NULL && config->external.adc_cali_handle != NULL) {
        ctx->adc_handle = config->external.adc_handle;
        ctx->adc_cali_handle = config->external.adc_cali_handle;
        ctx->is_adc_handle_owned = false;
    } else {
        // Create new ADC unit and channel
        adc_oneshot_unit_init_cfg_t init_cfg = {
            .unit_id = config->internal.adc_unit,
        };
        ESP_RETURN_ON_FALSE(adc_oneshot_new_unit(&init_cfg, &ctx->adc_handle) == ESP_OK, NULL, TAG, "Failed to create ADC unit");

        adc_oneshot_chan_cfg_t chan_cfg = {
            .atten = config->internal.adc_atten,
            .bitwidth = config->internal.adc_bitwidth,
        };
        ESP_RETURN_ON_FALSE(adc_oneshot_config_channel(ctx->adc_handle, ctx->adc_channel, &chan_cfg) == ESP_OK, NULL, TAG, "Failed to configure ADC channel");

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = config->internal.adc_unit,
            .chan = config->adc_channel,
            .atten = config->internal.adc_atten,
            .bitwidth = config->internal.adc_bitwidth,
        };
        ESP_RETURN_ON_FALSE(adc_cali_create_scheme_curve_fitting(&cali_config, &ctx->adc_cali_handle) == ESP_OK, NULL, TAG, "Failed to create ADC calibration scheme");
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = config->internal.adc_unit,
            .atten = config->internal.adc_atten,
            .bitwidth = config->internal.adc_bitwidth,
        };
        ESP_RETURN_ON_FALSE(adc_cali_create_scheme_line_fitting(&cali_config, &ctx->adc_cali_handle) == ESP_OK, NULL, TAG, "Failed to create ADC calibration scheme");
#endif
        ctx->is_adc_handle_owned = true;
    }

    // Validate voltage divider resistors
    if (config->upper_resistor <= 0.0f || config->lower_resistor <= 0.0f) {
        ESP_LOGE(TAG, "Invalid resistor values: upper_resistor=%.2f, lower_resistor=%.2f",
                 config->upper_resistor, config->lower_resistor);
        return NULL;
    }

    float total_resistance = config->upper_resistor + config->lower_resistor;
    if (total_resistance <= 0.0f) {
        ESP_LOGE(TAG, "Total resistance is zero or negative: %.2f", total_resistance);
        return NULL;
    }

    ctx->voltage_divider_ratio = config->lower_resistor / total_resistance;
    ctx->filter_alpha = CONFIG_BATTERY_CAPACITY_LPF_COEFFICIENT / 10.0f;

    return (adc_battery_estimation_handle_t) ctx;
}

esp_err_t adc_battery_estimation_destroy(adc_battery_estimation_handle_t handle)
{
    esp_err_t ret = ESP_OK;
    if (handle == NULL) {
        return ESP_OK;
    }

    adc_battery_estimation_ctx_t *ctx = (adc_battery_estimation_ctx_t *) handle;
    if (ctx->is_adc_handle_owned) {
        printf("delete internal adc unit\n");
        // Delete ADC unit and calibration scheme if owned
        ret = adc_oneshot_del_unit(ctx->adc_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete ADC unit: %s", esp_err_to_name(ret));
            return ret;
        }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        ret = adc_cali_delete_scheme_curve_fitting(ctx->adc_cali_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete ADC calibration scheme: %s", esp_err_to_name(ret));
            return ret;
        }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        ret = adc_cali_delete_scheme_line_fitting(ctx->adc_cali_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete ADC calibration scheme: %s", esp_err_to_name(ret));
            return ret;
        }
#endif
    }
    free(ctx);
    return ESP_OK;
}

esp_err_t adc_battery_estimation_get_capacity(adc_battery_estimation_handle_t handle, float *capacity)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(handle && capacity, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    adc_battery_estimation_ctx_t *ctx = (adc_battery_estimation_ctx_t *) handle;
    bool is_charging = false;

    // Check charging state if callback is provided
    if (ctx->charging_detect_cb) {
        is_charging = ctx->charging_detect_cb(ctx->charging_detect_user_data);
    }

    // Get ADC reading via filtering
    int vol[CONFIG_ADC_FILTER_WINDOW_SIZE] = {0};
    int avg = 0, std_vol = 0, filtered_vol = 0, filtered_result = 0, filtered_count = 0;
    for (int i = 0; i < CONFIG_ADC_FILTER_WINDOW_SIZE; i++) {
        int adc_raw = 0;
        ret = adc_oneshot_read(ctx->adc_handle, ctx->adc_channel, &adc_raw);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read ADC: %s", esp_err_to_name(ret));
            return ret;
        }

        ret = adc_cali_raw_to_voltage(ctx->adc_cali_handle, adc_raw, &vol[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to convert ADC raw to voltage: %s", esp_err_to_name(ret));
            return ret;
        }
        avg += vol[i];
    }
    avg /= CONFIG_ADC_FILTER_WINDOW_SIZE;
    filtered_result = avg;

    for (int i = 0; i < CONFIG_ADC_FILTER_WINDOW_SIZE; i++) {
        std_vol += (vol[i] - avg) * (vol[i] - avg);
    }
    std_vol = (int)sqrt(std_vol / (CONFIG_ADC_FILTER_WINDOW_SIZE));

    for (int i = 0; i < CONFIG_ADC_FILTER_WINDOW_SIZE; i++) {
        if (abs(vol[i] - avg) < std_vol) {
            filtered_vol += vol[i];
            filtered_count++;
        }
    }

    if (filtered_count > 0) {
        filtered_result = filtered_vol / filtered_count;
    }

    // Convert ADC voltage (mV) to battery voltage (V)
    float battery_voltage = (float)filtered_result / 1000.0f / ctx->voltage_divider_ratio;
    ESP_LOGD(TAG, "Battery voltage: %.2fV", battery_voltage);

    // Calculate battery capacity based on voltage
    float current_capacity = calculate_battery_capacity(battery_voltage, ctx->battery_points, ctx->battery_points_count);

    // Apply low-pass filter and handle capacity monotonicity
    if (!ctx->is_first_read) {
        // Apply low-pass filter
        float filtered_capacity = ctx->filter_alpha * current_capacity + (1.0f - ctx->filter_alpha) * ctx->last_capacity;

        if (is_charging) {
            // In charging state, capacity should not decrease
            if (filtered_capacity < ctx->last_capacity) {
                ESP_LOGD(TAG, "Capacity decreased in charging state (%.1f%% -> %.1f%%), keeping previous value",
                         ctx->last_capacity, filtered_capacity);
                filtered_capacity = ctx->last_capacity;
            }
        } else {
            // In discharging state, capacity should not increase
            if (filtered_capacity > ctx->last_capacity) {
                ESP_LOGD(TAG, "Capacity increased in discharging state (%.1f%% -> %.1f%%), keeping previous value",
                         ctx->last_capacity, filtered_capacity);
                filtered_capacity = ctx->last_capacity;
            }
        }
        current_capacity = filtered_capacity;
    } else {
        // First reading, just store it
        ctx->is_first_read = false;
    }

    // Update last capacity and charging state
    ctx->last_capacity = current_capacity;
    ctx->last_charging_state = is_charging;
    *capacity = current_capacity;

    ESP_LOGD(TAG, "Battery capacity: %.1f%%", *capacity);
    return ESP_OK;
}

esp_err_t adc_battery_estimation_get_charging_state(adc_battery_estimation_handle_t handle, bool *is_charging)
{
    ESP_RETURN_ON_FALSE(handle && is_charging, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    adc_battery_estimation_ctx_t *ctx = (adc_battery_estimation_ctx_t *) handle;
    if (ctx->charging_detect_cb) {
        *is_charging = ctx->charging_detect_cb(ctx->charging_detect_user_data);
    } else {
        ESP_LOGE(TAG, "Charging detection callback is not set");
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}
