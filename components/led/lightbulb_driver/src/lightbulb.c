/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <string.h>

#include <nvs_flash.h>

#include "hal_driver.h"
#include "lightbulb.h"

static const char *TAG = "lightbulb";

/**
 * @brief Resource Access Control
 */
#define LB_MUTEX_TAKE(handle, delay_ms)                (xSemaphoreTakeRecursive((handle)->mutex, delay_ms))
#define LB_MUTEX_GIVE(handle)                          (xSemaphoreGiveRecursive((handle)->mutex))

/**
 * @brief Lightbulb function check
 */
#define IS_COLOR_CHANNEL_SELECTED(handle)              ((handle)->cap.led_beads >= LED_BEADS_3CH_RGB)
#define IS_WHITE_CHANNEL_SELECTED(handle)              ((handle)->cap.led_beads != LED_BEADS_3CH_RGB)
#define IS_LOW_POWER_FUNCTION_ENABLED(handle)          (((handle)->cap.enable_lowpower && (handle)->power_timer) ? true : false)
#define IS_AUTO_STATUS_STORAGE_ENABLED(handle)         (((handle)->cap.enable_status_storage && (handle)->storage_timer) ? true : false)
#define IS_WHITE_OUTPUT_HARDWARE_MIXED(handle)         ((handle)->cap.enable_hardware_cct)
#define IS_AUTO_ON_FUNCTION_ENABLED(handle)            ((!(handle)->cap.disable_auto_on) ? true : false)
#define IS_EFFECT_TIMER_ACTIVE(handle)                 ((handle)->effect_timer && (xTimerIsTimerActive((handle)->effect_timer) == pdTRUE))
#define IS_EFFECT_ALLOW_INTERRUPT(handle)              ((handle)->effect_flag.running && (handle)->effect_flag.allow_interrupt)
#define IS_EFFECT_RUNNING(handle)                      ((handle)->effect_flag.running)

/**
 * @brief Lightbulb fade time calculation
 */
#define CALCULATE_FADE_TIME(handle)                    ((handle)->cap.enable_fade ? (handle)->cap.fade_time_ms : 0)

/**
 * @brief NVS namespace for lightbulb status storage
 *
 */
#define LIGHTBULB_NAMESPACE                   ("lightbulb")

/**
 * @brief Default configuration values
 */
#define MAX_CCT_K           (7000)   // Maximum color temperature in Kelvin
#define MIN_CCT_K           (2200)   // Minimum color temperature in Kelvin
#define MAX_FADE_MS         (3000)   // Maximum fade time in milliseconds
#define MIN_FADE_MS         (100)    // Minimum fade time in milliseconds

struct lightbulb_t {
    lightbulb_status_t status;         // Record the current status of the lightbulb, e.g., on or off
    lightbulb_capability_t cap;        // Record the capabilities of the driver
    lightbulb_power_limit_t power;     // Record the power limit param of the lightbulb,
    TimerHandle_t power_timer;         // Timer handle related to the power management of the lightbulb
    TimerHandle_t storage_timer;       // Timer handle related to the storage status of the lightbulb
    TimerHandle_t effect_timer;        // Timer handle related to the flashing, fading
    void (*effect_user_cb)(void);      // User callback for effect auto-stop timer
    SemaphoreHandle_t mutex;           // For multi-thread protection

    // Structure containing pointers to gamma correction tables for color and white
    struct {
        uint16_t *color_gamma_table;     // Pointer to the color gamma correction table (for RGB)
        uint16_t *white_gamma_table;     // Pointer to the white gamma correction table (for CCT)
    } gamma_correction;

    // Structure containing flags related to effects
    struct {
        bool allow_interrupt : 1;      // Flag indicating if the effect can be interrupted
        bool running : 1;              // Flag indicating if the effect is currently running
    } effect_flag;

    // Structure for managing color temperature-related functionalities
    struct {
        uint16_t (*percentage_to_kelvin)(lightbulb_handle_t handle, uint8_t percentage); // Function pointer to convert percentage to Kelvin color temperature
        uint8_t (*kelvin_to_percentage)(lightbulb_handle_t handle, uint16_t kelvin);    // Function pointer to convert Kelvin color temperature to percentage
        lightbulb_cct_kelvin_range_t kelvin_range;           // Color temperature range
        lightbulb_cct_mapping_data_t *mix_table;             // Mapping table for mixing color temperatures
        int table_size;                                      // Size of the mapping table
    } cct_manager;
    struct {
        esp_err_t (*hsv_to_rgb)(lightbulb_handle_t handle, uint16_t hue, uint8_t saturation, uint8_t value, float *red, float *green, float *blue, float *cold, float *warm);
        lightbulb_color_mapping_data_t *mix_table;            // Mapping table for mixing color
        int table_size;                                      // Size of the mapping table
    } color_manager;

    char nvs_key[16];  // NVS key
    bool deiniting;

    // HAL context for lightbulb instance
    hal_context_t hal_ctx;
};

static lightbulb_handle_t lightbulb_init(hal_context_t hal_ctx, lightbulb_driver_t driver_type, lightbulb_config_t *config);
static esp_err_t lightbulb_hsv2rgb_adjusted(lightbulb_handle_t handle, uint16_t hue, uint8_t saturation, uint8_t value, float *red, float *green, float *blue, float *cold, float *warm);
static esp_err_t _lightbulb_hsv2rgb(lightbulb_handle_t handle, uint16_t hue, uint8_t saturation, uint8_t value, float *red, float *green, float *blue, float *cold, float *warm);

esp_err_t lightbulb_status_set_to_nvs(lightbulb_handle_t handle, const lightbulb_status_t *value)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(value, "value is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(handle->nvs_key[0] != '\0', "nvs_key not set", return ESP_ERR_INVALID_STATE);

    esp_err_t err = ESP_OK;
    nvs_handle_t nvs_handle = 0;

    err = nvs_open(LIGHTBULB_NAMESPACE, NVS_READWRITE, &nvs_handle);
    LIGHTBULB_CHECK(err == ESP_OK, "nvs open fail, reason code: %d", return err, err);

    err = nvs_set_blob(nvs_handle, handle->nvs_key, value, sizeof(lightbulb_status_t));
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        ESP_LOGE(TAG, "nvs set fail, reason code: %d", err);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        ESP_LOGE(TAG, "nvs commit fail");
        return err;
    }

    nvs_close(nvs_handle);

    return ESP_OK;
}

esp_err_t lightbulb_status_get_from_nvs(lightbulb_handle_t handle, lightbulb_status_t *value)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(value, "value is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(handle->nvs_key[0] != '\0', "nvs_key not set", return ESP_ERR_INVALID_STATE);

    esp_err_t err = ESP_OK;
    nvs_handle_t nvs_handle = 0;
    size_t req_len = sizeof(lightbulb_status_t);

    err = nvs_open(LIGHTBULB_NAMESPACE, NVS_READWRITE, &nvs_handle);
    LIGHTBULB_CHECK(err == ESP_OK, "nvs open fail, reason code: %d", return err, err);

    err = nvs_get_blob(nvs_handle, handle->nvs_key, value, &req_len);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        ESP_LOGE(TAG, "nvs get fail, reason code: %d", err);
        return err;
    }

    nvs_close(nvs_handle);

    return ESP_OK;
}

esp_err_t lightbulb_status_erase_nvs_storage(lightbulb_handle_t handle)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(handle->nvs_key[0] != '\0', "nvs_key not set", return ESP_ERR_INVALID_STATE);

    esp_err_t err = ESP_OK;
    nvs_handle_t nvs_handle = 0;

    err = nvs_open(LIGHTBULB_NAMESPACE, NVS_READWRITE, &nvs_handle);
    LIGHTBULB_CHECK(err == ESP_OK, "nvs open fail, reason code: %d", return err, err);

    err = nvs_erase_key(nvs_handle, handle->nvs_key);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        ESP_LOGE(TAG, "nvs erase fail, reason code: %d", err);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        ESP_LOGE(TAG, "nvs commit fail, reason code: %d", err);
        return err;
    }

    nvs_close(nvs_handle);

    return ESP_OK;
}

static lightbulb_cct_mapping_data_t search_mapping_cct_data(lightbulb_handle_t handle, uint16_t cct)
{
    int low = 0;
    int high = handle->cct_manager.table_size - 1;
    int mid;
    lightbulb_cct_mapping_data_t result = {0};
    while (low <= high) {
        mid = low + (high - low) / 2;
        lightbulb_cct_mapping_data_t* current_entry = &handle->cct_manager.mix_table[mid];

        int compare;
        if (cct <= 100) {
            compare = current_entry->cct_percentage - cct;
        } else {
            compare = current_entry->cct_kelvin - cct;
        }

        if (compare == 0) {
            return *current_entry;
        } else if (compare < 0) {
            result = *current_entry;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return result;
}

/**
 * @brief Convert CCT percentage to Kelvin
 *
 * @note If you use the default maximum and minimum kelvin:
 *
 *      input       output
 *      0           2200k
 *      50          4600k
 *      100         7200k
 *
 * @param percentage Range: 0-100
 * @return uint16_t Converted Kelvin value
 */
static uint16_t standard_percentage_convert_to_kelvin(lightbulb_handle_t handle, uint8_t percentage)
{
    // Convert percentage to a floating-point value between 0 and 1
    float _percentage = (float)percentage / 100;

    // Calculate Kelvin value within the specified range
    uint16_t _kelvin = (_percentage * (handle->cct_manager.kelvin_range.max - handle->cct_manager.kelvin_range.min) + handle->cct_manager.kelvin_range.min);

    // Convert Kelvin value to the nearest multiple of 100 (round to the nearest integer)
    _kelvin = (_kelvin / 100) * 100;

    return _kelvin;
}

static uint16_t precise_percentage_convert_to_kelvin(lightbulb_handle_t handle, uint8_t percentage)
{
    // Convert percentage to a floating-point value between 0 and 1
    lightbulb_cct_mapping_data_t data;
    data = search_mapping_cct_data(handle, percentage);

    return data.cct_kelvin;
}

/**
 * @brief Convert Kelvin to CCT percentage
 *
 * @note If you use the default maximum and minimum kelvin:
 *
 *      input       output
 *      2200k       0
 *      4600k       50
 *      7200k       100
 *
 * @param kelvin Kelvin temperature
 * @return uint8_t Percentage value
 */
static uint8_t standard_kelvin_convert_to_percentage(lightbulb_handle_t handle, uint16_t kelvin)
{
    // Ensure the input kelvin value is within the specified range
    if (kelvin > handle->cct_manager.kelvin_range.max) {
        kelvin = handle->cct_manager.kelvin_range.max;
    }
    if (kelvin < handle->cct_manager.kelvin_range.min) {
        kelvin = handle->cct_manager.kelvin_range.min;
    }

    // Calculate the percentage value based on the Kelvin value within the range
    return 100 * ((float)(kelvin - handle->cct_manager.kelvin_range.min) / (handle->cct_manager.kelvin_range.max - handle->cct_manager.kelvin_range.min));
}

/**
 * @brief Convert Kelvin to CCT percentage
 *
 * @note If you use the default maximum and minimum kelvin:
 *
 *      input       output
 *      2200k       0
 *      4600k       50
 *      7200k       100
 *
 * @param kelvin Kelvin temperature
 * @return uint8_t Percentage value
 */
static uint8_t precise_kelvin_convert_to_percentage(lightbulb_handle_t handle, uint16_t kelvin)
{
    // Ensure the input kelvin value is within the specified range
    if (kelvin > handle->cct_manager.kelvin_range.max) {
        kelvin = handle->cct_manager.kelvin_range.max;
    }
    if (kelvin < handle->cct_manager.kelvin_range.min) {
        kelvin = handle->cct_manager.kelvin_range.min;
    }

    // Ensure the input kelvin value is within the specified range
    lightbulb_cct_mapping_data_t data;
    data = search_mapping_cct_data(handle, kelvin);

    // Calculate the percentage value based on the Kelvin value within the range
    return data.cct_percentage;
}

/**
 * @brief Convert CCT and brightness to cold and warm values and handle power limiting for white output.
 *
 * @note
 *
 *      input           output(multiple = 1.0)      output(multiple = 2.0)
 *      0,100                   0,255                           0,255
 *      50,100                  127,127                         255,255
 *      100,100                 255,0                           255,0
 *
 *      0,0                     0,0                             0,0
 *      0,50                    0,127                           0,127
 *      0,100                   0,255                           0,255
 *
 *      50,0                    0,0                             0,0
 *      50,50                   63,63                           127,127
 *      50,100                  127,127                         255,255
 *
 *      100,0                   0,0                             0,0
 *      100,50                  127,0                           127,0
 *      100,100                 255,0                           255,0
 *
 * @param led_beads Type of LED beads
 * @param multiple Power limiting factor (used when multiple channels share a maximum output value)
 * @param cct CCT value in the range 0-100
 * @param brightness Brightness value in the range 0-100
 * @param white_value Array to store the calculated cold and warm values for white output
 */
static esp_err_t cct_and_brightness_convert_and_power_limit(lightbulb_handle_t handle, lightbulb_led_beads_comb_t led_beads, float multiple, uint8_t cct, uint8_t brightness, uint16_t white_value[])
{
    uint16_t max_value = 0;
    hal_get_driver_feature(handle->hal_ctx, QUERY_MAX_INPUT_VALUE, &max_value);

    if (led_beads == LED_BEADS_1CH_C || led_beads == LED_BEADS_4CH_RGBC || led_beads == LED_BEADS_4CH_RGBCC) {
        uint16_t value = brightness * max_value / 100;
        hal_get_curve_table_value(handle->hal_ctx, value, &white_value[3]);
        if (led_beads == LED_BEADS_4CH_RGBCC) {
            hal_get_curve_table_value(handle->hal_ctx, value, &white_value[4]);
        }
    } else if (led_beads == LED_BEADS_1CH_W || led_beads == LED_BEADS_4CH_RGBW || led_beads == LED_BEADS_4CH_RGBWW) {
        uint16_t value = brightness * max_value / 100;
        hal_get_curve_table_value(handle->hal_ctx, value, &white_value[4]);
        if (led_beads == LED_BEADS_4CH_RGBWW) {
            hal_get_curve_table_value(handle->hal_ctx, value, &white_value[3]);
        }
    } else if ((led_beads == LED_BEADS_2CH_CW || led_beads == LED_BEADS_5CH_RGBCW) && IS_WHITE_OUTPUT_HARDWARE_MIXED(handle)) {
        uint16_t value1 = cct * max_value / 100;
        uint16_t value2 = brightness * max_value / 100;
        hal_get_curve_table_value(handle->hal_ctx, value1, &white_value[3]);
        hal_get_curve_table_value(handle->hal_ctx, value2, &white_value[4]);
    } else if (led_beads == LED_BEADS_2CH_CW || ((led_beads == LED_BEADS_5CH_RGBCW) && (handle->cap.enable_precise_cct_control == false))) {
        float max_power;
        float _c = cct / 100.0;
        float _w = (100 - cct) / 100.0;
        float baseline = MAX(_c, _w);

        max_power = MIN(max_value * multiple, max_value / baseline);
        _c = max_power * _c * (brightness / 100.0);
        _w = max_power * _w * (brightness / 100.0);
        hal_get_curve_table_value(handle->hal_ctx, _c, &white_value[3]);
        hal_get_curve_table_value(handle->hal_ctx, _w, &white_value[4]);
    } else {
        float max_power;
        lightbulb_cct_mapping_data_t data = search_mapping_cct_data(handle, cct);
        ESP_LOGD(TAG, "%f, %f, %f, %f, %f", data.rgbcw[0], data.rgbcw[1], data.rgbcw[2], data.rgbcw[3], data.rgbcw[4]);

        float baseline = MAX(data.rgbcw[0], data.rgbcw[1]);
        baseline = MAX(baseline, data.rgbcw[2]);
        baseline = MAX(baseline, data.rgbcw[3]);
        baseline = MAX(baseline, data.rgbcw[4]);
        max_power = MIN(max_value * multiple, max_value / baseline);
        ESP_LOGD(TAG, "%f, %d, %f", max_power, max_value, baseline);
        for (int i = 0; i < 5; i++) {
            float value = round(max_power * data.rgbcw[i] * (brightness / 100.0));
            hal_get_curve_table_value(handle->hal_ctx, (uint16_t)value, &white_value[i]);
        }
    }

#if CONFIG_ENABLE_LIGHTBULB_DEBUG
    uint16_t test_power =  white_value[0] + white_value[1] + white_value[2] + white_value[3] +  white_value[4];
    uint16_t limit_power = max_value * multiple * (brightness / 100.0);

    if (test_power > limit_power) {
        ESP_LOGE(TAG, "Power exceeds expected, current: %d, expected:%d", test_power, limit_power);
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

/**
 * @brief Recalculate value
 *
 * @note
 *      if max:100 min:10
 *
 *      input   output
 *      100      100
 *      80       82
 *      50       55
 *      11       19
 *      1        10
 *      0        0
 */
static uint8_t process_color_value_limit(lightbulb_handle_t handle, uint8_t value)
{
    if (value == 0) {
        return 0;
    }
    float percentage = value / 100.0;

    uint8_t result = (handle->power.color_max_value - handle->power.color_min_value) * percentage + handle->power.color_min_value;
    result = handle->gamma_correction.color_gamma_table[result];
    ESP_LOGD(TAG, "color_value convert input:%d output:%d", value, result);

    return result;
}

/**
 * @brief Recalculate brightness
 *
 * @note
 *      if max:100 min:10
 *
 *      input   output
 *      100      100
 *      80       82
 *      50       55
 *      11       19
 *      1        10
 *      0        0
 */
static uint8_t process_white_brightness_limit(lightbulb_handle_t handle, uint8_t brightness)
{
    if (brightness == 0) {
        return 0;
    }
    float percentage = brightness / 100.0;

    uint8_t result = (handle->power.white_max_brightness - handle->power.white_min_brightness) * percentage + handle->power.white_min_brightness;
    result = handle->gamma_correction.white_gamma_table[result];
    ESP_LOGD(TAG, "white_brightness_output input:%d output:%d", brightness, result);

    return result;
}

/**
 * @brief Recalculate color power
 * @attention 300% = 100% + 100% + 100% : Full power output on each channel. If single channel output is 3w then total output is 9w.
 *
 * lightbulb_power_limit_t limit = {
 *   .color_max_power = 100,
 *   .color_max_value = 100,
 *   .color_min_value = 10,
 *   .white_max_power = 100,
 *   .white_max_brightness = 100,
 *   .white_min_brightness = 10
 * };
 *
 * lightbulb_gamma_config_t Gamma = {
 *  .balance_coefficient = {1.0, 1.0, 1.0, 1.0, 1.0},
 *  .color_curve_coefficient = 2.0,
 *  .white_curve_coefficient = 2.0,
 * };
 *
 * @note
 *      input(hsv)          output(color_max_power = 100)       output(color_max_power = 200)       output(color_max_power = 300)
 *      0,100,100           255,0,0                             255,0,0                             255,0,0
 *      0,100,1             79,0,0                              79,0,0                              79,0,0
 *      60,100,100          127,127,0                           255,255,0                           255,255,0
 *      60,100,1            39,39,0                             79,79,0                             79,79,0
 *      0,50,100            127,63,63                           255,127,127                         255,127,127
 *      0,50,1              40,19,19                            79,38,38                            79,38,38
 *      60,50,100           102,102,50                          204,204,101                         254,254,126
 *      60,50,1             31,31,15                            63,63,30                            79,79,38
 *      0,0,100             85,85,85                            170,170,170                         255,255,255
 *      0,0,1               26,26,26                            52,52,52                            79,79,79
 */
static esp_err_t process_color_power_limit(lightbulb_handle_t handle, float multiple, float rgbcw[5], uint16_t value, uint16_t out[5])
{
    uint16_t max_value;
    hal_get_driver_feature(handle->hal_ctx, QUERY_MAX_INPUT_VALUE, &max_value);

    float scaled[5];
    float max_scale = 0.0f;
    float max_scale_limit = (float)max_value;

    for (int i = 0; i < 5; ++i) {
        scaled[i] = rgbcw[i] * multiple;
        if (scaled[i] > max_scale) {
            max_scale = scaled[i];
        }
    }

    float scale_factor = 1.0f;
    if (max_scale > 1.0f) {
        scale_factor = max_scale_limit / (max_scale * max_scale_limit);
    }

    for (int i = 0; i < 5; ++i) {
        float value_f = scaled[i] * scale_factor * max_value * (value / 100.0);
        hal_get_curve_table_value(handle->hal_ctx, (uint16_t)value_f, &out[i]);
    }

#if CONFIG_ENABLE_LIGHTBULB_DEBUG
    uint16_t test_power = out[0] + out[1] + out[2] + out[3] + out[4];
    uint16_t limit_power = max_value * multiple * (value / 100.0);

    if (test_power > limit_power) {
        ESP_LOGE(TAG, "Power exceeds expected, current: %d, expected:%d", test_power, limit_power);
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

static void timercb(TimerHandle_t tmr)
{
    // Get handle from timer ID
    lightbulb_handle_t handle = (lightbulb_handle_t)pvTimerGetTimerID(tmr);
    if (!handle || handle->deiniting) {
        return;
    }

    if (tmr == handle->power_timer) {
        hal_sleep_control(handle->hal_ctx, true);
    } else if (tmr == handle->storage_timer) {
        lightbulb_status_set_to_nvs(handle, &handle->status);
        if (handle->cap.storage_cb) {
            handle->cap.storage_cb(handle->status);
        }
    } else if (tmr == handle->effect_timer) {
        lightbulb_basic_effect_stop(handle);
        if (handle->effect_user_cb) {
            handle->effect_user_cb();
            handle->effect_user_cb = NULL;
        }
    }
}

static uint8_t get_channel_mask(lightbulb_led_beads_comb_t led_beads)
{
    uint8_t channel_mask = 0;

    switch (led_beads) {
    case LED_BEADS_1CH_C:
        // 0 1 0 0 0
        channel_mask = SELECT_COLD_CCT_WHITE_CHANNEL;
        break;
    case LED_BEADS_1CH_W:
        // 1 0 0 0 0
        channel_mask = SELECT_WARM_BRIGHTNESS_YELLOW_CHANNEL;
        break;
    case LED_BEADS_2CH_CW:
        // 1 1 0 0 0
        channel_mask = SELECT_WHITE_CHANNEL;
        break;
    case LED_BEADS_3CH_RGB:
        // 0 0 1 1 1
        channel_mask = SELECT_COLOR_CHANNEL;
        break;
    case LED_BEADS_4CH_RGBC:
        // 0 1 1 1 1
        channel_mask = ((SELECT_COLOR_CHANNEL) | (SELECT_COLD_CCT_WHITE_CHANNEL));
        break;
    case LED_BEADS_4CH_RGBCC:
        // 0 1 1 1 1
        channel_mask = ((SELECT_COLOR_CHANNEL) | (SELECT_COLD_CCT_WHITE_CHANNEL));
        break;
    case LED_BEADS_4CH_RGBWW:
        // 0 1 1 1 1
        channel_mask = ((SELECT_COLOR_CHANNEL) | (SELECT_WARM_BRIGHTNESS_YELLOW_CHANNEL));
        break;
    case LED_BEADS_4CH_RGBW:
        // 1 0 1 1 1
        channel_mask = ((SELECT_COLOR_CHANNEL) | (SELECT_WARM_BRIGHTNESS_YELLOW_CHANNEL));
        break;
    case LED_BEADS_5CH_RGBCW:
        // 1 1 1 1 1
        channel_mask = ((SELECT_COLOR_CHANNEL) | (SELECT_WHITE_CHANNEL));
        break;
    case LED_BEADS_5CH_RGBCC:
        // 1 1 1 1 1
        channel_mask = ((SELECT_COLOR_CHANNEL) | (SELECT_WHITE_CHANNEL));
        break;
    case LED_BEADS_5CH_RGBWW:
        // 1 1 1 1 1
        channel_mask = ((SELECT_COLOR_CHANNEL) | (SELECT_WHITE_CHANNEL));
        break;
    case LED_BEADS_5CH_RGBC:
        // 0 1 1 1 1
        channel_mask = ((SELECT_COLOR_CHANNEL) | (SELECT_COLD_CCT_WHITE_CHANNEL));
        break;
    case LED_BEADS_5CH_RGBW:
        // 1 0 1 1 1
        channel_mask = ((SELECT_COLOR_CHANNEL) | (SELECT_WARM_BRIGHTNESS_YELLOW_CHANNEL));
        break;
    default:
        break;
    }
    return channel_mask;
}

static bool cct_mix_table_data_check(lightbulb_handle_t handle)
{
    bool result = true;

    for (int i = 0; i < handle->cct_manager.table_size; i++) {
        float sum = 0;
        uint8_t flag = 0;
        for (int j = 0; j < 5; j ++) {
            sum += handle->cct_manager.mix_table[i].rgbcw[j];
        }
        if (i >= 1) {
            if (handle->cct_manager.mix_table[i].cct_percentage <= handle->cct_manager.mix_table[i - 1].cct_percentage) {
                flag |= 0x01;
            }
            if (handle->cct_manager.mix_table[i].cct_kelvin <= handle->cct_manager.mix_table[i - 1].cct_kelvin) {
                flag |= 0x02;
            }
        }

        if (!(fabs(sum - 1.0) < 0.001)) {
            result = false;
            ESP_LOGE(TAG, "%d%%: %dK, sum:%f ,sum is not equal to 1.0, please check", handle->cct_manager.mix_table[i].cct_percentage, handle->cct_manager.mix_table[i].cct_kelvin, sum);
        }
        if (flag) {
            result = false;
            ESP_LOGE(TAG, "%d%%: %dK, mix table data must be sorted from small to large", handle->cct_manager.mix_table[i].cct_percentage, handle->cct_manager.mix_table[i].cct_kelvin);
        }
    }

    return result;
}

static bool color_mix_table_data_check(void)
{
    bool result = true;
    //TODO

    return result;
}

static void print_func(lightbulb_handle_t handle, char *driver_details, char *driver_io)
{
    ESP_LOGI(TAG, "----------------------Lightbulb Driver Component-----------------------------");
    ESP_LOGI(TAG, "version: %d.%d.%d", LIGHTBULB_DRIVER_VER_MAJOR, LIGHTBULB_DRIVER_VER_MINOR, LIGHTBULB_DRIVER_VER_PATCH);
    ESP_LOGI(TAG, "%s", driver_details);
    ESP_LOGI(TAG, "%s", driver_io);
    ESP_LOGI(TAG, "low power control: %s", handle->cap.enable_lowpower ? "enable" : "disable");
    ESP_LOGI(TAG, "status storage: %s", handle->cap.enable_status_storage ? "enable" : "disable");
    ESP_LOGI(TAG, "status storage delay %d ms", handle->cap.enable_status_storage == true ? handle->cap.storage_delay_ms : 0);
    ESP_LOGI(TAG, "fade: %s", handle->cap.enable_fade ? "enable" : "disable");
    ESP_LOGI(TAG, "fade %d ms", handle->cap.enable_fade == true ? handle->cap.fade_time_ms : 0);
    ESP_LOGI(TAG, "led_beads: %d", handle->cap.led_beads);
    ESP_LOGI(TAG, "hardware cct: %s", handle->cap.enable_hardware_cct ? "Yes" : "No");
    ESP_LOGI(TAG, "precise cct control: %s", handle->cap.enable_precise_cct_control ?  "enable" : "disable");
    ESP_LOGI(TAG, "sync change: %s", handle->cap.sync_change_brightness_value ? "enable" : "disable");
    ESP_LOGI(TAG, "auto on: %s", handle->cap.disable_auto_on ? "disable" : "enable");

    if (IS_WHITE_CHANNEL_SELECTED(handle)) {
        ESP_LOGI(TAG, "     white mode: enable");
    }
    if (IS_COLOR_CHANNEL_SELECTED(handle)) {
        ESP_LOGI(TAG, "     color mode: enable");
    }
    ESP_LOGI(TAG, "sync change: %s", handle->cap.sync_change_brightness_value ? "enable" : "disable");
    ESP_LOGI(TAG, "power limit param: ");
    ESP_LOGI(TAG, "     white max brightness: %d", handle->power.white_max_brightness);
    ESP_LOGI(TAG, "     white min brightness: %d", handle->power.white_min_brightness);
    ESP_LOGI(TAG, "     white max power: %d", handle->power.white_max_power);
    ESP_LOGI(TAG, "     color max brightness: %d", handle->power.color_max_value);
    ESP_LOGI(TAG, "     color min brightness: %d", handle->power.color_min_value);
    ESP_LOGI(TAG, "     color max power: %d", handle->power.color_max_power);
    if (IS_WHITE_CHANNEL_SELECTED(handle)) {
        ESP_LOGI(TAG, "cct kelvin range param: ");
        ESP_LOGI(TAG, "     max cct: %d K", handle->cct_manager.kelvin_range.max);
        ESP_LOGI(TAG, "     min cct: %d K", handle->cct_manager.kelvin_range.min);
        for (int i = 0; i < handle->cct_manager.table_size && handle->cct_manager.table_size > 0; i++) {
            ESP_LOGI(TAG, "%d%%, %dK, %f %f %f %f %f", handle->cct_manager.mix_table[i].cct_percentage, handle->cct_manager.mix_table[i].cct_kelvin, handle->cct_manager.mix_table[i].rgbcw[0], handle->cct_manager.mix_table[i].rgbcw[1], handle->cct_manager.mix_table[i].rgbcw[2], handle->cct_manager.mix_table[i].rgbcw[3], handle->cct_manager.mix_table[i].rgbcw[4]);
        }
    }
    ESP_LOGI(TAG, "hue: %d, saturation: %d, value: %d", handle->status.hue, handle->status.saturation, handle->status.value);
    ESP_LOGI(TAG, "select works mode: %s, power status: %d", handle->status.mode == WORK_COLOR ? "color" : "white", handle->status.on);
    ESP_LOGI(TAG, "---------------------------------------------------------------------");
}

#ifdef CONFIG_ENABLE_PWM_DRIVER
lightbulb_handle_t lightbulb_new_pwm_device(lightbulb_config_t *config)
{
    LIGHTBULB_CHECK(config, "config is null", return NULL);

    hal_context_t hal_ctx = pwm_hal_output_init(&config->driver_conf.pwm, config->gamma_conf);
    if (!hal_ctx) {
        return NULL;
    }

    return lightbulb_init(hal_ctx, DRIVER_ESP_PWM, config);
}
#endif

#ifdef CONFIG_ENABLE_SM2182E_DRIVER
lightbulb_handle_t lightbulb_new_sm2182e_device(lightbulb_config_t *config)
{
    LIGHTBULB_CHECK(config, "config is null", return NULL);

    hal_context_t hal_ctx = sm2182e_hal_output_init(&config->driver_conf.sm2182e, config->gamma_conf);
    if (!hal_ctx) {
        return NULL;
    }

    return lightbulb_init(hal_ctx, DRIVER_SM2182E, config);
}
#endif

#ifdef CONFIG_ENABLE_SM2135EH_DRIVER
lightbulb_handle_t lightbulb_new_sm2135eh_device(lightbulb_config_t *config)
{
    LIGHTBULB_CHECK(config, "config is null", return NULL);

    hal_context_t hal_ctx = sm2135eh_hal_output_init(&config->driver_conf.sm2135eh, config->gamma_conf);
    if (!hal_ctx) {
        return NULL;
    }

    return lightbulb_init(hal_ctx, DRIVER_SM2135EH, config);
}
#endif

#ifdef CONFIG_ENABLE_SM2x35EGH_DRIVER
lightbulb_handle_t lightbulb_new_sm2x35egh_device(lightbulb_config_t *config)
{
    LIGHTBULB_CHECK(config, "config is null", return NULL);

    hal_context_t hal_ctx = sm2x35egh_hal_output_init(&config->driver_conf.sm2x35egh, config->gamma_conf);
    if (!hal_ctx) {
        return NULL;
    }

    return lightbulb_init(hal_ctx, DRIVER_SM2x35EGH, config);
}
#endif

#ifdef CONFIG_ENABLE_BP57x8D_DRIVER
lightbulb_handle_t lightbulb_new_bp57x8d_device(lightbulb_config_t *config)
{
    LIGHTBULB_CHECK(config, "config is null", return NULL);

    hal_context_t hal_ctx = bp57x8d_hal_output_init(&config->driver_conf.bp57x8d, config->gamma_conf);
    if (!hal_ctx) {
        return NULL;
    }

    return lightbulb_init(hal_ctx, DRIVER_BP57x8D, config);
}
#endif

#ifdef CONFIG_ENABLE_BP1658CJ_DRIVER
lightbulb_handle_t lightbulb_new_bp1658cj_device(lightbulb_config_t *config)
{
    LIGHTBULB_CHECK(config, "config is null", return NULL);

    hal_context_t hal_ctx = bp1658cj_hal_output_init(&config->driver_conf.bp1658cj, config->gamma_conf);
    if (!hal_ctx) {
        return NULL;
    }

    return lightbulb_init(hal_ctx, DRIVER_BP1658CJ, config);
}
#endif

#ifdef CONFIG_ENABLE_KP18058_DRIVER
lightbulb_handle_t lightbulb_new_kp18058_device(lightbulb_config_t *config)
{
    LIGHTBULB_CHECK(config, "config is null", return NULL);

    hal_context_t hal_ctx = kp18058_hal_output_init(&config->driver_conf.kp18058, config->gamma_conf);
    if (!hal_ctx) {
        return NULL;
    }

    return lightbulb_init(hal_ctx, DRIVER_KP18058, config);
}
#endif

#ifdef CONFIG_ENABLE_WS2812_DRIVER
lightbulb_handle_t lightbulb_new_ws2812_device(lightbulb_config_t *config)
{
    LIGHTBULB_CHECK(config, "config is null", return NULL);

    hal_context_t hal_ctx = ws2812_hal_output_init(&config->driver_conf.ws2812, config->gamma_conf);
    if (!hal_ctx) {
        return NULL;
    }

    return lightbulb_init(hal_ctx, DRIVER_WS2812, config);
}
#endif

#ifdef CONFIG_ENABLE_SM16825E_DRIVER
lightbulb_handle_t lightbulb_new_sm16825e_device(lightbulb_config_t *config)
{
    LIGHTBULB_CHECK(config, "config is null", return NULL);

    hal_context_t hal_ctx = sm16825e_hal_output_init(&config->driver_conf.sm16825e, config->gamma_conf);
    if (!hal_ctx) {
        return NULL;
    }

    return lightbulb_init(hal_ctx, DRIVER_SM16825E, config);
}
#endif

static lightbulb_handle_t lightbulb_init(hal_context_t hal_ctx, lightbulb_driver_t driver_type, lightbulb_config_t *config)
{
    LIGHTBULB_CHECK(driver_type > DRIVER_SELECT_INVALID && driver_type < DRIVER_SELECT_MAX, "invalid driver_type", goto CLEANUP_HAL);
    LIGHTBULB_CHECK(hal_ctx, "hal_ctx is null", return NULL);
    LIGHTBULB_CHECK(config, "Config is null", goto CLEANUP_HAL);
    LIGHTBULB_CHECK(config->capability.led_beads > LED_BEADS_INVALID && config->capability.led_beads < LED_BEADS_MAX, "Invalid led beads combination select", goto CLEANUP_HAL);

    lightbulb_handle_t handle = (lightbulb_handle_t)calloc(1, sizeof(struct lightbulb_t));
    LIGHTBULB_CHECK(handle, "calloc fail", goto CLEANUP_HAL);

    handle->hal_ctx = hal_ctx;

    handle->mutex = xSemaphoreCreateRecursiveMutex();
    LIGHTBULB_CHECK(handle->mutex, "mutex create fail", goto EXIT);

    // Driver details for logging
    char driver_details[224] = {0};
    char driver_io[64] = {0};
    sprintf(driver_details, "HAL context: %p", (void *)hal_ctx);
    sprintf(driver_io, "Configured via hal_regist_channel");

    // Config check
    if (config->capability.enable_hardware_cct == true && config->capability.enable_precise_cct_control == true) {
        ESP_LOGW(TAG, "The detection uses both hardware CCT and precision CCT control. Precision CCT control will be disable.");
        config->capability.enable_precise_cct_control = false;
    }

    if (config->capability.enable_precise_cct_control == true && config->capability.led_beads < LED_BEADS_5CH_RGBCW) {
        ESP_LOGW(TAG, "Only supports precise CCT control of 5-channel led bead combination. Precision CCT control will be disable.");
        config->capability.enable_precise_cct_control = false;
    }

    if ((config->capability.led_beads >= LED_BEADS_5CH_RGBCC) && (config->capability.enable_precise_cct_control == false)) {
        ESP_LOGW(TAG, "This led beads combination must enable precise color temperature control. Precision CCT control will be enable.");
        config->capability.enable_precise_cct_control = true;
    }

    // Load init status
    memcpy(&handle->status, &config->init_status, sizeof(lightbulb_status_t));
    memcpy(&handle->cap, &config->capability, sizeof(lightbulb_capability_t));

    // Check channel
    if (config->capability.led_beads >= LED_BEADS_3CH_RGB) {
        if (driver_type == DRIVER_ESP_PWM) {
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_RED, config->io_conf.pwm_io.red) == ESP_OK, "register red channel fail", goto EXIT);
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_GREEN, config->io_conf.pwm_io.green) == ESP_OK, "register green channel fail", goto EXIT);
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_BLUE, config->io_conf.pwm_io.blue) == ESP_OK, "register blue channel fail", goto EXIT);
        } else if (driver_type == DRIVER_WS2812) {
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_RED, 0) == ESP_OK, "register red channel fail", goto EXIT);
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_GREEN, 0) == ESP_OK, "register green channel fail", goto EXIT);
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_BLUE, 0) == ESP_OK, "register blue channel fail", goto EXIT);
        } else if (driver_type == DRIVER_SM16825E) {
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_RED, config->io_conf.sm16825e_io.red) == ESP_OK, "register red channel fail", goto EXIT);
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_GREEN, config->io_conf.sm16825e_io.green) == ESP_OK, "register green channel fail", goto EXIT);
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_BLUE, config->io_conf.sm16825e_io.blue) == ESP_OK, "register blue channel fail", goto EXIT);
        } else {
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_RED, config->io_conf.iic_io.red) == ESP_OK, "register red channel fail", goto EXIT);
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_GREEN, config->io_conf.iic_io.green) == ESP_OK, "register green channel fail", goto EXIT);
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_BLUE, config->io_conf.iic_io.blue) == ESP_OK, "register blue channel fail", goto EXIT);
        }
    }
    if (config->capability.led_beads == LED_BEADS_1CH_C || config->capability.led_beads == LED_BEADS_2CH_CW || config->capability.led_beads == LED_BEADS_4CH_RGBC || config->capability.led_beads == LED_BEADS_4CH_RGBCC
            || config->capability.led_beads == LED_BEADS_5CH_RGBCW || config->capability.led_beads == LED_BEADS_5CH_RGBC || config->capability.led_beads == LED_BEADS_5CH_RGBCC || config->capability.led_beads == LED_BEADS_5CH_RGBWW) {
        if (driver_type == DRIVER_ESP_PWM) {
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_COLD_CCT_WHITE, config->io_conf.pwm_io.cold_cct) == ESP_OK, "register cold channel fail", goto EXIT);
        } else if (driver_type == DRIVER_SM16825E) {
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_COLD_CCT_WHITE, config->io_conf.sm16825e_io.cold_white) == ESP_OK, "register cold channel fail", goto EXIT);
        } else {
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_COLD_CCT_WHITE, config->io_conf.iic_io.cold_white) == ESP_OK, "register cold channel fail", goto EXIT);
        }
    }
    if (config->capability.led_beads == LED_BEADS_1CH_W || config->capability.led_beads == LED_BEADS_2CH_CW || config->capability.led_beads == LED_BEADS_4CH_RGBW || config->capability.led_beads == LED_BEADS_4CH_RGBWW
            || config->capability.led_beads == LED_BEADS_5CH_RGBCW || config->capability.led_beads == LED_BEADS_5CH_RGBW || config->capability.led_beads == LED_BEADS_5CH_RGBCC || config->capability.led_beads == LED_BEADS_5CH_RGBWW) {
        if (driver_type == DRIVER_ESP_PWM) {
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_WARM_BRIGHTNESS_YELLOW, config->io_conf.pwm_io.warm_brightness) == ESP_OK, "register warm channel fail", goto EXIT);
        } else if (driver_type == DRIVER_SM16825E) {
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_WARM_BRIGHTNESS_YELLOW, config->io_conf.sm16825e_io.warm_yellow) == ESP_OK, "register warm channel fail", goto EXIT);
        } else {
            LIGHTBULB_CHECK(hal_regist_channel(hal_ctx, CHANNEL_ID_WARM_BRIGHTNESS_YELLOW, config->io_conf.iic_io.warm_yellow) == ESP_OK, "register warm channel fail", goto EXIT);
        }
    }

    // Initialize NVS storage configuration
    if (config->nvs_key && config->nvs_key[0] != '\0') {
        snprintf(handle->nvs_key, sizeof(handle->nvs_key), "%s", config->nvs_key);
    } else if (handle->cap.enable_status_storage) {
        LIGHTBULB_CHECK(false, "nvs_key must be explicitly set when enable_status_storage is true", goto EXIT);
    }

    // Check cct output mode
    if (IS_WHITE_CHANNEL_SELECTED(handle)) {
        if (handle->cap.enable_precise_cct_control) {
            handle->cct_manager.table_size = config->cct_mix_mode.precise.table_size;
            handle->cct_manager.kelvin_to_percentage = precise_kelvin_convert_to_percentage;
            handle->cct_manager.percentage_to_kelvin = precise_percentage_convert_to_kelvin;

            if (handle->cct_manager.table_size == 0) {
                if (handle->cap.led_beads == LED_BEADS_5CH_RGBC || handle->cap.led_beads == LED_BEADS_5CH_RGBW) {
                    ESP_LOGW(TAG, "The default color mix table will be used for this led combination.");

                    float default_rgbc_data[11][5] = {
                        {0.45,  0.45,   0.00,   0.10,   0.00},
                        {0.40,  0.40,   0.00,   0.20,   0.00},
                        {0.35,  0.35,   0.00,   0.30,   0.00},
                        {0.30,  0.30,   0.00,   0.40,   0.00},
                        {0.25,  0.25,   0.00,   0.50,   0.00},
                        {0.20,  0.20,   0.00,   0.60,   0.00},
                        {0.15,  0.15,   0.00,   0.70,   0.00},
                        {0.10,  0.10,   0.00,   0.80,   0.00},
                        {0.05,  0.05,   0.00,   0.90,   0.00},
                        {0.00,  0.00,   0.00,   1.00,   0.00},
                        {0.00,  0.00,   0.00,   1.00,   0.00},
                    };
                    float default_rgbw_data[11][5] = {
                        {0.00,  0.00,   0.00,   0.00,   1.00},
                        {0.05,  0.05,   0.00,   0.00,   0.90},
                        {0.05,  0.15,   0.00,   0.00,   0.80},
                        {0.05,  0.25,   0.00,   0.00,   0.70},
                        {0.05,  0.35,   0.10,   0.00,   0.50},
                        {0.05,  0.35,   0.25,   0.00,   0.35},
                        {0.05,  0.35,   0.30,   0.00,   0.30},
                        {0.05,  0.35,   0.35,   0.00,   0.25},
                        {0.10,  0.35,   0.35,   0.00,   0.20},
                        {0.10,  0.30,   0.50,   0.00,   0.10},
                        {0.10,  0.30,   0.50,   0.00,   0.10},
                    };
                    handle->cct_manager.kelvin_range.max = MAX_CCT_K;
                    handle->cct_manager.kelvin_range.min = MIN_CCT_K;
                    handle->cct_manager.table_size = 11;
                    handle->cct_manager.mix_table = calloc(handle->cct_manager.table_size, sizeof(lightbulb_cct_mapping_data_t));
                    LIGHTBULB_CHECK(handle->cct_manager.mix_table, "calloc fail", goto EXIT);
                    uint16_t interval_kelvin = (handle->cct_manager.kelvin_range.max - handle->cct_manager.kelvin_range.min) / 10;

                    for (int i = 0; i < handle->cct_manager.table_size; i++) {
                        lightbulb_cct_mapping_data_t unit = { 0 };

                        unit.cct_percentage = i * 10;
                        unit.cct_kelvin = ((interval_kelvin * i) / 100) * 100 + handle->cct_manager.kelvin_range.min;
                        if (handle->cap.led_beads == LED_BEADS_5CH_RGBC) {
                            memcpy(unit.rgbcw, &default_rgbc_data[i][0], sizeof(float) * 5);
                        } else {
                            memcpy(unit.rgbcw, &default_rgbw_data[i][0], sizeof(float) * 5);
                        }
                        memcpy(&handle->cct_manager.mix_table[i], &unit, sizeof(lightbulb_cct_mapping_data_t));
                    }
                } else {
                    ESP_LOGE(TAG, "This led combination does not have a default cct mix table, please income it in from outside.");
                    goto EXIT;
                }
            } else {
                handle->cct_manager.table_size = config->cct_mix_mode.precise.table_size;
                handle->cct_manager.mix_table = calloc(handle->cct_manager.table_size, sizeof(lightbulb_cct_mapping_data_t));
                LIGHTBULB_CHECK(handle->cct_manager.mix_table, "calloc fail", goto EXIT);

                handle->cct_manager.kelvin_range.min = config->cct_mix_mode.precise.table[0].cct_kelvin;
                handle->cct_manager.kelvin_range.max = config->cct_mix_mode.precise.table[config->cct_mix_mode.precise.table_size - 1].cct_kelvin;

                for (int i = 0; i < handle->cct_manager.table_size; i++) {
                    memcpy(&handle->cct_manager.mix_table[i], &config->cct_mix_mode.precise.table[i], sizeof(lightbulb_cct_mapping_data_t));
                }
            }
        } else {
            handle->cct_manager.kelvin_to_percentage = standard_kelvin_convert_to_percentage;
            handle->cct_manager.percentage_to_kelvin = standard_percentage_convert_to_kelvin;
            if ((config->cct_mix_mode.standard.kelvin_min >= config->cct_mix_mode.standard.kelvin_max) || (config->cct_mix_mode.standard.kelvin_min < 100) || (config->cct_mix_mode.standard.kelvin_max < 100)) {
                handle->cct_manager.kelvin_range.max = MAX_CCT_K;
                handle->cct_manager.kelvin_range.min = MIN_CCT_K;
                ESP_LOGW(TAG, "Kelvin value not set or is incorrect, default range (%dk - %dk) will be used", MIN_CCT_K, MAX_CCT_K);
            } else {
                handle->cct_manager.kelvin_range.max = config->cct_mix_mode.standard.kelvin_max;
                handle->cct_manager.kelvin_range.min = config->cct_mix_mode.standard.kelvin_min;
            }
        }
    }
    if (handle->cct_manager.table_size > 0) {
        bool result = cct_mix_table_data_check(handle);
        LIGHTBULB_CHECK(result, "mix table check fail", goto EXIT);
    }

    // Check color output mode
    if (IS_COLOR_CHANNEL_SELECTED(handle)) {
        if (handle->cap.enable_precise_color_control) {
            LIGHTBULB_CHECK(config->color_mix_mode.precise.table_size <= 24, "Currently, only < 24 color calibration points are supported", goto EXIT);

            handle->color_manager.hsv_to_rgb = lightbulb_hsv2rgb_adjusted;
            handle->color_manager.table_size = config->color_mix_mode.precise.table_size;
            handle->color_manager.mix_table = calloc(handle->color_manager.table_size, sizeof(lightbulb_color_mapping_data_t));
            LIGHTBULB_CHECK(handle->color_manager.mix_table, "calloc fail", goto EXIT);
            for (int i = 0; i < handle->color_manager.table_size; i++) {
                memcpy(&handle->color_manager.mix_table[i], &config->color_mix_mode.precise.table[i], sizeof(lightbulb_color_mapping_data_t));
            }
        } else {
            handle->color_manager.hsv_to_rgb = _lightbulb_hsv2rgb;
        }
    }
    if (handle->cap.enable_precise_color_control && handle->color_manager.table_size > 0) {
        bool result = color_mix_table_data_check();
        LIGHTBULB_CHECK(result, "mix table check fail", goto EXIT);
    }

    // init status update
    if (handle->cap.led_beads == LED_BEADS_1CH_C || handle->cap.led_beads == LED_BEADS_4CH_RGBC) {
        handle->status.cct_percentage = 100;
    } else if (handle->cap.led_beads == LED_BEADS_1CH_W || handle->cap.led_beads == LED_BEADS_4CH_RGBW) {
        handle->status.cct_percentage = 0;
    }

    // Fade check
    if (handle->cap.enable_fade) {
        handle->cap.fade_time_ms = MIN(MAX_FADE_MS, handle->cap.fade_time_ms);
        handle->cap.fade_time_ms = MAX(MIN_FADE_MS, handle->cap.fade_time_ms);
    }

    //Gamma table create
    float color_coe = 1.0;
    float white_coe = 1.0;
    if (config->gamma_conf) {
        color_coe = config->gamma_conf->color_curve_coefficient;
        white_coe = config->gamma_conf->white_curve_coefficient;
    }
    handle->gamma_correction.color_gamma_table = calloc(101, sizeof(uint16_t));
    LIGHTBULB_CHECK(handle->gamma_correction.color_gamma_table, "curve table buffer alloc fail", goto EXIT);
    handle->gamma_correction.white_gamma_table = calloc(101, sizeof(uint16_t));
    LIGHTBULB_CHECK(handle->gamma_correction.white_gamma_table, "curve table buffer alloc fail", goto EXIT);
    hal_gamma_table_create(handle->gamma_correction.color_gamma_table, 101, color_coe, 100);
    hal_gamma_table_create(handle->gamma_correction.white_gamma_table, 101, white_coe, 100);

    // Low power check
    if (config->capability.enable_lowpower) {
        /* Make sure the fade is done and the flash operation is done, then enable light sleep */
        uint32_t time_ms = MAX(handle->cap.fade_time_ms, handle->cap.storage_delay_ms) + 1000;
        handle->power_timer = xTimerCreate("power_timer", pdMS_TO_TICKS(time_ms), false, handle, timercb);
        LIGHTBULB_CHECK(handle->power_timer != NULL, "create timer fail", goto EXIT);
    }

    // Storage check
    if (config->capability.enable_status_storage) {
        handle->cap.storage_delay_ms = MAX(handle->cap.fade_time_ms, handle->cap.storage_delay_ms) + 1000;
        handle->storage_timer = xTimerCreate("storage_timer", pdMS_TO_TICKS(handle->cap.storage_delay_ms), false, handle, timercb);
        LIGHTBULB_CHECK(handle->storage_timer != NULL, "create timer fail", goto EXIT);
    }

    // Power Limit check
    if (config->external_limit) {
        memcpy(&handle->power, config->external_limit, sizeof(lightbulb_power_limit_t));
    } else {
        handle->power.color_max_value = 100;
        handle->power.white_max_brightness = 100;
        handle->power.color_min_value = 1;
        handle->power.white_min_brightness = 1;
        handle->power.color_max_power = 300;
        handle->power.white_max_power = 100;
    }

    // Output status according to init parameter
    if (handle->status.on) {
        /* Fade can cause perceptible state changes when the system restarts abnormally, so we need to temporarily disable fade. */
        if (handle->cap.enable_fade) {
            lightbulb_set_fades_function(handle, false);
            lightbulb_set_switch(handle, true);
            lightbulb_set_fades_function(handle, true);
        } else {
            lightbulb_set_switch(handle, true);
        }
    }

    print_func(handle, driver_details, driver_io);

    return handle;

EXIT:
    lightbulb_deinit(handle);
    return NULL;

CLEANUP_HAL:
    hal_output_deinit(hal_ctx);
    return NULL;
}

esp_err_t lightbulb_deinit(lightbulb_handle_t handle)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);

    handle->deiniting = true;
    handle->effect_user_cb = NULL;

    if (handle->power_timer) {
        vTimerSetTimerID(handle->power_timer, NULL);
        xTimerStop(handle->power_timer, portMAX_DELAY);
        xTimerDelete(handle->power_timer, portMAX_DELAY);
        handle->power_timer = NULL;
    }

    if (handle->storage_timer) {
        vTimerSetTimerID(handle->storage_timer, NULL);
        xTimerStop(handle->storage_timer, portMAX_DELAY);
        xTimerDelete(handle->storage_timer, portMAX_DELAY);
        handle->storage_timer = NULL;
    }

    if (handle->effect_timer) {
        vTimerSetTimerID(handle->effect_timer, NULL);
        xTimerStop(handle->effect_timer, portMAX_DELAY);
        xTimerDelete(handle->effect_timer, portMAX_DELAY);
        handle->effect_timer = NULL;
    }

    if (handle->mutex) {
        vSemaphoreDelete(handle->mutex);
        handle->mutex = NULL;
    }

    if (handle->cct_manager.mix_table) {
        free(handle->cct_manager.mix_table);
        handle->cct_manager.mix_table = NULL;
    }
    if (handle->color_manager.mix_table) {
        free(handle->color_manager.mix_table);
        handle->color_manager.mix_table = NULL;
    }

    if (handle->gamma_correction.color_gamma_table) {
        free(handle->gamma_correction.color_gamma_table);
        handle->gamma_correction.color_gamma_table = NULL;
    }
    if (handle->gamma_correction.white_gamma_table) {
        free(handle->gamma_correction.white_gamma_table);
        handle->gamma_correction.white_gamma_table = NULL;
    }

    if (handle->hal_ctx) {
        hal_output_deinit(handle->hal_ctx);
        handle->hal_ctx = NULL;
    }

    free(handle);

    return ESP_OK;
}

esp_err_t lightbulb_set_xyy(lightbulb_handle_t handle, float x, float y, float Y)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(x <= 1.0, "x out of range", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(y <= 1.0, "y out of range", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(Y <= 100.0, "Y out of range", return ESP_ERR_INVALID_ARG);

    uint8_t r, g, b;
    uint16_t h;
    uint8_t s, v;

    lightbulb_xyy2rgb(x, y, Y, &r, &g, &b);
    lightbulb_rgb2hsv(r, g, b, &h, &s, &v);

    return lightbulb_set_hsv(handle, h, s, v);
}

esp_err_t lightbulb_xyy2rgb(float x, float y, float Y, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    LIGHTBULB_CHECK(x <= 1.0, "x out of range", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(y <= 1.0, "y out of range", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(Y <= 100.0, "Y out of range", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_CHECK(red, "red is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(green, "green is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(blue, "blue is null", return ESP_ERR_INVALID_ARG);

    float _x = x;
    float _y = y;
    float _z = 1.0f - _x - _y;
    float _X, _Y, _Z;
    float _r, _g, _b;

    // Calculate XYZ values
    _Y = Y / 100.0;
    _X = (_Y / _y) * _x;
    _Z = (_Y / _y) * _z;

    // X, Y and Z input refer to a D65/2° standard illuminant.
    // sR, sG and sB (standard RGB) output range = 0 ÷ 255
    // convert XYZ to RGB - CIE XYZ to sRGB
    _r = (_X * 3.2410f) - (_Y * 1.5374f) - (_Z * 0.4986f);
    _g = -(_X * 0.9692f) + (_Y * 1.8760f) + (_Z * 0.0416f);
    _b = (_X * 0.0556f) - (_Y * 0.2040f) + (_Z * 1.0570f);

    // apply gamma 2.2 correction
    _r = (_r <= 0.00304f ? 12.92f * _r : (1.055f) * pow(_r, (1.0f / 2.4f)) - 0.055f);
    _g = (_g <= 0.00304f ? 12.92f * _g : (1.055f) * pow(_g, (1.0f / 2.4f)) - 0.055f);
    _b = (_b <= 0.00304f ? 12.92f * _b : (1.055f) * pow(_b, (1.0f / 2.4f)) - 0.055f);

    // Round off
    _r = MIN(1.0, _r);
    _r = MAX(0, _r);

    _g = MIN(1.0, _g);
    _g = MAX(0, _g);

    _b = MIN(1.0, _b);
    _b = MAX(0, _b);

    *red = (uint8_t)(_r * 255 + 0.5);
    *green = (uint8_t)(_g * 255 + 0.5);
    *blue = (uint8_t)(_b * 255 + 0.5);

    return ESP_OK;
}

esp_err_t lightbulb_rgb2xyy(uint8_t red, uint8_t green, uint8_t blue, float *x, float *y, float *Y)
{
    LIGHTBULB_CHECK(Y, "Y is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(x, "x is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(y, "y is null", return ESP_ERR_INVALID_ARG);

    float _red = red / 255.0;
    float _green = green / 255.0;
    float _blue = blue / 255.0;

    if (_red > 0.04045) {
        _red = pow((_red + 0.055) / 1.055, 2.4);
    } else {
        _red = _red / 12.92;
    }

    if (_green > 0.04045) {
        _green = pow((_green + 0.055) / 1.055, 2.4);
    } else {
        _green = _green / 12.92;
    }

    if (_blue > 0.04045) {
        _blue = pow((_blue + 0.055) / 1.055, 2.4);
    } else {
        _blue = _blue / 12.92;
    }

    _red = _red * 100;
    _green = _green * 100;
    _blue = _blue * 100;

    float _X = _red * 0.4124 + _green * 0.3576 + _blue * 0.1805;
    float _Y = _red * 0.2126 + _green * 0.7152 + _blue * 0.0722;
    float _Z = _red * 0.0193 + _green * 0.1192 + _blue * 0.9505;

    *Y = _Y;
    *x = _X / (_X + _Y + _Z);
    *y = _Y / (_X + _Y + _Z);

    return ESP_OK;
}

static float interpolate(float start, float end, float ratio)
{
    return start + (end - start) * ratio;
}

static esp_err_t lightbulb_hsv2rgb_adjusted(lightbulb_handle_t handle, uint16_t hue, uint8_t saturation, uint8_t value, float *red, float *green, float *blue, float *cold, float *warm)
{
    LIGHTBULB_CHECK(hue <= 360, "hue out of range", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(saturation <= 100, "saturation out of range", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(value <= 100, "value out of range", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_CHECK(red, "red is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(green, "green is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(blue, "blue is null", return ESP_ERR_INVALID_ARG);

    lightbulb_color_mapping_data_t *table = handle->color_manager.mix_table;
    size_t table_size = handle->color_manager.table_size;

    int lower_index = -1;
    int upper_index = -1;

    for (size_t i = 0; i < table_size; i++) {
        if (table[i].hue == hue) {
            lower_index = i;
            upper_index = i;
            break;
        } else if (table[i].hue < hue) {
            lower_index = i;
        } else if (table[i].hue > hue) {
            upper_index = i;
            break;
        }
    }

    if (lower_index == -1) {
        lower_index = table_size - 1;
    }
    if (upper_index == -1) {
        upper_index = 0;
    }

    float ratio = 0.0;
    if (lower_index != upper_index) {
        uint16_t lower_hue = table[lower_index].hue;
        uint16_t upper_hue = table[upper_index].hue;

        if (lower_hue > upper_hue) {
            upper_hue += 360;
        }

        if (hue < lower_hue) {
            hue += 360;
        }

        ratio = (float)(hue - lower_hue) / (float)(upper_hue - lower_hue);
    }

    float interpolated_rgbcw[5];

    for (int i = 0; i < 5; i++) {
        float lower_value_100 = table[lower_index].rgbcw_100[i];
        float upper_value_100 = table[upper_index].rgbcw_100[i];
        float lower_value_50 = table[lower_index].rgbcw_50[i];
        float upper_value_50 = table[upper_index].rgbcw_50[i];
        float lower_value_0 = table[lower_index].rgbcw_0[i];
        float upper_value_0 = table[upper_index].rgbcw_0[i];

        if (table[lower_index].hue > table[upper_index].hue) {
            upper_value_100 = table[upper_index].rgbcw_100[i] + (table[upper_index].rgbcw_100[i] - table[lower_index].rgbcw_100[i]);
            upper_value_50 = table[upper_index].rgbcw_50[i] + (table[upper_index].rgbcw_50[i] - table[lower_index].rgbcw_50[i]);
            upper_value_0 = table[upper_index].rgbcw_0[i] + (table[upper_index].rgbcw_0[i] - table[lower_index].rgbcw_0[i]);
        }

        float value_100 = interpolate(lower_value_100, upper_value_100, ratio);
        float value_50 = interpolate(lower_value_50, upper_value_50, ratio);
        float value_0 = interpolate(lower_value_0, upper_value_0, ratio);

        if (saturation == 100) {
            interpolated_rgbcw[i] = value_100;
        } else if (saturation == 50) {
            interpolated_rgbcw[i] = value_50;
        } else if (saturation == 0) {
            interpolated_rgbcw[i] = value_0;
        } else if (saturation < 50) {
            interpolated_rgbcw[i] = interpolate(value_50, value_0, (100 - (float)saturation) / 100);
        } else {
            interpolated_rgbcw[i] = interpolate(value_100, value_50, (100 - (float)saturation) / 100.0);
        }
    }

    *red = interpolated_rgbcw[0];
    *green = interpolated_rgbcw[1];
    *blue = interpolated_rgbcw[2];
    *cold = interpolated_rgbcw[3];
    *warm = interpolated_rgbcw[4];

    return ESP_OK;
}

static esp_err_t _lightbulb_hsv2rgb(lightbulb_handle_t handle, uint16_t hue, uint8_t saturation, uint8_t value, float *red, float *green, float *blue, float *cold, float *warm)
{
    uint8_t _red = 0;
    uint8_t _green = 0;
    uint8_t _blue = 0;
    lightbulb_hsv2rgb(hue, saturation, value, &_red, &_green, &_blue);

    ESP_LOGI(TAG, "Convert 8 bit value [r:%d g:%d b:%d]", _red, _green, _blue);

    if (value == 0) {
        *red = 0;
        *green = 0;
        *blue = 0;
    } else {
        *red = _red / 255.0;
        *green = _green / 255.0;
        *blue = _blue / 255.0;
        float total = *red + *green + *blue;

        *red = *red / total;
        *green = *green / total;
        *blue = *blue / total;
    }

    *cold = 0;
    *warm = 0;

    return ESP_OK;
}

esp_err_t lightbulb_hsv2rgb(uint16_t hue, uint8_t saturation, uint8_t value, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    LIGHTBULB_CHECK(hue <= 360, "hue out of range", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(saturation <= 100, "saturation out of range", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(value <= 100, "value out of range", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_CHECK(red, "red is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(green, "green is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(blue, "blue is null", return ESP_ERR_INVALID_ARG);

    hue = hue % 360;
    uint16_t hi = (hue / 60) % 6;
    uint16_t F = 100 * hue / 60 - 100 * hi;
    uint16_t P = value * (100 - saturation) / 100;
    uint16_t Q = value * (10000 - F * saturation) / 10000;
    uint16_t T = value * (10000 - saturation * (100 - F)) / 10000;

    switch (hi) {
    case 0:
        *red = value;
        *green = T;
        *blue = P;
        break;

    case 1:
        *red = Q;
        *green = value;
        *blue = P;
        break;

    case 2:
        *red = P;
        *green = value;
        *blue = T;
        break;

    case 3:
        *red = P;
        *green = Q;
        *blue = value;
        break;

    case 4:
        *red = T;
        *green = P;
        *blue = value;
        break;

    case 5:
        *red = value;
        *green = P;
        *blue = Q;
        break;

    default:
        return ESP_FAIL;
    }

    *red = *red * 255 / 100;
    *green = *green * 255 / 100;
    *blue = *blue * 255 / 100;

    return ESP_OK;
}

esp_err_t lightbulb_rgb2hsv(uint16_t red, uint16_t green, uint16_t blue, uint16_t *hue, uint8_t *saturation, uint8_t *value)
{
    LIGHTBULB_CHECK(hue, "h is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(saturation, "s is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(value, "v is null", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_CHECK(red <= 255, "red out of range", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(green <= 255, "green out of range", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(blue <= 255, "blue out of range", return ESP_ERR_INVALID_ARG);

    float _hue, _saturation, _value;
    float m_max = MAX(red, MAX(green, blue));
    float m_min = MIN(red, MIN(green, blue));
    float m_delta = m_max - m_min;

    _value = m_max / 255.0;

    if (m_delta == 0) {
        _hue = 0;
        _saturation = 0;
    } else {
        _saturation = m_delta / m_max;

        if (red == m_max) {
            _hue = (green - blue) / m_delta;
        } else if (green == m_max) {
            _hue = 2 + (blue - red) / m_delta;
        } else {
            _hue = 4 + (red - green) / m_delta;
        }

        _hue = _hue * 60;

        if (_hue < 0) {
            _hue = _hue + 360;
        }
    }

    *hue = (int)(_hue + 0.5);
    *saturation = (int)(_saturation * 100 + 0.5);
    *value = (int)(_value * 100 + 0.5);

    return ESP_OK;
}

esp_err_t lightbulb_kelvin2percentage(lightbulb_handle_t handle, uint16_t kelvin, uint8_t *percentage)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(handle->cct_manager.kelvin_to_percentage, "No conversion function was registered because the this led combination does not support CCT.", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(percentage, "percentage is null", return ESP_ERR_INVALID_ARG);

    if (kelvin < handle->cct_manager.kelvin_range.min || kelvin > handle->cct_manager.kelvin_range.max) {
        ESP_LOGW(TAG, "kelvin out of range, will be forcibly converted");
    }

    *percentage = handle->cct_manager.kelvin_to_percentage(handle, kelvin);

    return ESP_OK;
}

esp_err_t lightbulb_percentage2kelvin(lightbulb_handle_t handle, uint8_t percentage, uint16_t *kelvin)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(handle->cct_manager.percentage_to_kelvin, "No conversion function was registered because the this led combination does not support CCT.", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(kelvin, "kelvin is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(percentage <= 100, "percentage out of range", return ESP_ERR_INVALID_ARG);

    *kelvin = handle->cct_manager.percentage_to_kelvin(handle, percentage);

    return ESP_OK;
}

esp_err_t lightbulb_set_hue(lightbulb_handle_t handle, uint16_t hue)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    return lightbulb_set_hsv(handle, hue, handle->status.saturation, handle->status.value);
}

esp_err_t lightbulb_set_saturation(lightbulb_handle_t handle, uint8_t saturation)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    return lightbulb_set_hsv(handle, handle->status.hue, saturation, handle->status.value);
}

esp_err_t lightbulb_set_value(lightbulb_handle_t handle, uint8_t value)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    return lightbulb_set_hsv(handle, handle->status.hue, handle->status.saturation, value);
}

esp_err_t lightbulb_set_cct(lightbulb_handle_t handle, uint16_t cct)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    return lightbulb_set_cctb(handle, cct, handle->status.brightness);
}

esp_err_t lightbulb_set_brightness(lightbulb_handle_t handle, uint8_t brightness)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    return lightbulb_set_cctb(handle, handle->status.cct_percentage, brightness);
}

esp_err_t lightbulb_set_hsv(lightbulb_handle_t handle, uint16_t hue, uint8_t saturation, uint8_t value)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(hue <= 360, "hue out of range: %d", return ESP_ERR_INVALID_ARG, hue);
    LIGHTBULB_CHECK(saturation <= 100, "saturation out of range: %d", return ESP_ERR_INVALID_ARG, saturation);
    LIGHTBULB_CHECK(value <= 100, "value out of range: %d", return ESP_ERR_INVALID_ARG, value);
    LIGHTBULB_CHECK(IS_COLOR_CHANNEL_SELECTED(handle), "color channel output is disable", return ESP_ERR_INVALID_STATE);
    LB_MUTEX_TAKE(handle, portMAX_DELAY);

    esp_err_t err = ESP_OK;

    if (IS_AUTO_STATUS_STORAGE_ENABLED(handle)) {
        xTimerReset(handle->storage_timer, 0);
    }

    if (IS_EFFECT_RUNNING(handle) && IS_EFFECT_ALLOW_INTERRUPT(handle)) {
        if (IS_EFFECT_TIMER_ACTIVE(handle)) {
            xTimerStop(handle->effect_timer, 0);
        }
        handle->effect_flag.running = false;
    } else if (IS_EFFECT_RUNNING(handle)) {
        ESP_LOGW(TAG, "Executing the effect, and does not allow interruption, skip writing, only save");
        goto SAVE_ONLY;
    }

    if (handle->status.on || IS_AUTO_ON_FUNCTION_ENABLED(handle)) {
        if (IS_LOW_POWER_FUNCTION_ENABLED(handle)) {
            xTimerStop(handle->power_timer, 0);
        }

        uint16_t color_value[5] = { 0 };
        float color_param[5] = { 0 };
        uint16_t fade_time = CALCULATE_FADE_TIME(handle);
        uint8_t channel_mask = get_channel_mask(handle->cap.led_beads);
        uint8_t _value = value;

        // 1. calculate value
        ESP_LOGI(TAG, "set [h:%d s:%d v:%d]", hue, saturation, value);
        _value = process_color_value_limit(handle, value);

        // 2. convert to r g b
        handle->color_manager.hsv_to_rgb(handle, hue, saturation, _value, &color_param[0], &color_param[1], &color_param[2], &color_param[3], &color_param[4]);
        ESP_LOGI(TAG, "Convert write value [r:%0.2f%% g:%0.2f%% b:%0.2f%% c:%0.2f%% w:%0.2f%%]", color_param[0] * 100, color_param[1] * 100, color_param[2] * 100, color_param[3] * 100, color_param[4] * 100);

        // 3. Redistribute power
        err |= process_color_power_limit(handle, handle->power.color_max_power / 100.0, color_param, _value, color_value);
        ESP_LOGI(TAG, "hal write value [r:%d g:%d b:%d c:%d w:%d], channel_mask:%d fade_ms:%d", color_value[0], color_value[1], color_value[2], color_value[3], color_value[4], channel_mask, fade_time);

        err |= hal_set_channel_group(handle->hal_ctx, color_value, channel_mask, fade_time);
        LIGHTBULB_CHECK(err == ESP_OK, "set hal channel group fail", goto EXIT);

        handle->status.on = true;
    } else {
        ESP_LOGW(TAG, "Skip writing, because power is not turned on or auto-on is disable");
    }

SAVE_ONLY:
    handle->status.mode = WORK_COLOR;
    handle->status.hue = hue;
    handle->status.saturation = saturation;
    handle->status.value = value;

    if (handle->cap.sync_change_brightness_value && IS_WHITE_CHANNEL_SELECTED(handle)) {
        handle->status.brightness = value;
    }

EXIT:
    LB_MUTEX_GIVE(handle);
    return err;
}

esp_err_t lightbulb_set_channel_group(lightbulb_handle_t handle, uint16_t r_ch, uint16_t g_ch, uint16_t b_ch, uint16_t c_ch, uint16_t w_ch)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    ESP_LOGE(TAG, "Note: This API can only be used in debug/test mode.");

    esp_err_t err;
    uint16_t color_value[5] = { 0 };
    uint16_t fade_time = CALCULATE_FADE_TIME(handle);
    uint8_t channel_mask = 0xff;

    color_value[0] = r_ch;
    color_value[1] = g_ch;
    color_value[2] = b_ch;
    color_value[3] = c_ch;
    color_value[4] = w_ch;

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    err = hal_set_channel_group(handle->hal_ctx, color_value, channel_mask, fade_time);
    LIGHTBULB_CHECK(err == ESP_OK, "set hal channel group fail", goto EXIT);

EXIT:
    LB_MUTEX_GIVE(handle);
    return err;
}

esp_err_t lightbulb_set_rgb(lightbulb_handle_t handle, uint8_t r, uint8_t g, uint8_t b)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    ESP_LOGE(TAG, "Note: This API can only be used in debug/test mode.");

    esp_err_t err;
    uint16_t color_value[5] = { 0 };
    uint16_t fade_time = CALCULATE_FADE_TIME(handle);
    uint8_t channel_mask = get_channel_mask(handle->cap.led_beads);

    uint16_t max_value;
    hal_get_driver_feature(handle->hal_ctx, QUERY_MAX_INPUT_VALUE, &max_value);

    color_value[0] = r;
    color_value[1] = g;
    color_value[2] = b;
    ESP_LOGI(TAG, "8 bit color conversion value [r:%d g:%d b:%d]", color_value[0], color_value[1], color_value[2]);

    color_value[0] = (float)r / 255 * max_value;
    color_value[1] = (float)g / 255 * max_value;
    color_value[2] = (float)b / 255 * max_value;
    ESP_LOGI(TAG, "hal write value [r:%d g:%d b:%d], channel_mask:%d fade_ms:%d", color_value[0], color_value[1], color_value[2], channel_mask, fade_time);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    err = hal_set_channel_group(handle->hal_ctx, color_value, channel_mask, fade_time);
    LIGHTBULB_CHECK(err == ESP_OK, "set hal channel group fail", goto EXIT);

EXIT:
    LB_MUTEX_GIVE(handle);
    return err;
}

esp_err_t lightbulb_set_cctb(lightbulb_handle_t handle, uint16_t cct, uint8_t brightness)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED(handle), "white channel output is disable", return ESP_ERR_INVALID_STATE);
    LIGHTBULB_CHECK(brightness <= 100, "brightness out of range: %d", return ESP_ERR_INVALID_ARG, brightness);
    LIGHTBULB_CHECK((cct >= handle->cct_manager.kelvin_range.min && cct <= handle->cct_manager.kelvin_range.max) || ((cct <= 100)), "CCT(%d) is not in valid range and is converted by an internal function", NULL, cct);

    if (cct > 100) {
        ESP_LOGW(TAG, "will convert kelvin to percentage, %dK -> %d%%", cct, handle->cct_manager.kelvin_to_percentage(handle, cct));
        cct = handle->cct_manager.kelvin_to_percentage(handle, cct);
    }
    if (handle->cap.led_beads == LED_BEADS_1CH_C || handle->cap.led_beads == LED_BEADS_4CH_RGBC) {
        if (cct != 100) {
            cct = 100;
            ESP_LOGW(TAG, "The current led beads can only be set to cold white");
        }
    } else if (handle->cap.led_beads == LED_BEADS_1CH_W || handle->cap.led_beads == LED_BEADS_4CH_RGBW) {
        if (cct != 0) {
            cct = 0;
            ESP_LOGW(TAG, "The current led beads can only be set to warm white");
        }
    }
    LIGHTBULB_CHECK(cct <= 100, "cct out of range: %d", return ESP_ERR_INVALID_ARG, cct);
    LB_MUTEX_TAKE(handle, portMAX_DELAY);

    esp_err_t err = ESP_OK;

    if (IS_AUTO_STATUS_STORAGE_ENABLED(handle)) {
        xTimerReset(handle->storage_timer, 0);
    }

    if (IS_EFFECT_RUNNING(handle) && IS_EFFECT_ALLOW_INTERRUPT(handle)) {
        if (IS_EFFECT_TIMER_ACTIVE(handle)) {
            xTimerStop(handle->effect_timer, 0);
        }
        handle->effect_flag.running = false;
    } else if (IS_EFFECT_RUNNING(handle)) {
        ESP_LOGW(TAG, "Executing the effect, and does not allow interruption, skip writing, only save");
        goto SAVE_ONLY;
    }

    if (handle->status.on || IS_AUTO_ON_FUNCTION_ENABLED(handle)) {
        if (IS_LOW_POWER_FUNCTION_ENABLED(handle)) {
            xTimerStop(handle->power_timer, 0);
        }

        uint16_t white_value[5] = { 0 };
        uint16_t fade_time = CALCULATE_FADE_TIME(handle);
        uint8_t channel_mask = get_channel_mask(handle->cap.led_beads);
        uint8_t _brightness = brightness;

        ESP_LOGI(TAG, "set cct:%d, brightness:%d", cct, brightness);
        // 1. calculate brightness
        _brightness = process_white_brightness_limit(handle, _brightness);

        // 2. convert to cold warm and redistribute power
        err |= cct_and_brightness_convert_and_power_limit(handle, handle->cap.led_beads, handle->power.white_max_power / 100.0, cct, _brightness, white_value);
        ESP_LOGI(TAG, "hal write value [r:%d g:%d b:%d c:%d w:%d], channel_mask:%d fade_ms:%d", white_value[0], white_value[1], white_value[2], white_value[3], white_value[4], channel_mask, fade_time);

        err |= hal_set_channel_group(handle->hal_ctx, white_value, channel_mask, fade_time);
        LIGHTBULB_CHECK(err == ESP_OK, "set hal channel group fail", goto EXIT);

        handle->status.on = true;
    } else {
        ESP_LOGW(TAG, "skip calling %s, just save this change.", __FUNCTION__);
    }

SAVE_ONLY:
    handle->status.mode = WORK_WHITE;
    handle->status.cct_percentage = cct;
    handle->status.brightness = brightness;

    if (handle->cap.sync_change_brightness_value && IS_COLOR_CHANNEL_SELECTED(handle)) {
        handle->status.value = brightness;
    }

EXIT:
    LB_MUTEX_GIVE(handle);
    return err;
}

esp_err_t lightbulb_set_switch(lightbulb_handle_t handle, bool status)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);

    esp_err_t err = ESP_OK;
    uint16_t fade_time = CALCULATE_FADE_TIME(handle);

    if (!status) {
        LB_MUTEX_TAKE(handle, portMAX_DELAY);
        if (IS_LOW_POWER_FUNCTION_ENABLED(handle)) {
            xTimerReset(handle->power_timer, 0);
        }
        if (IS_AUTO_STATUS_STORAGE_ENABLED(handle)) {
            xTimerReset(handle->storage_timer, 0);
        }
        if (IS_EFFECT_RUNNING(handle) && IS_EFFECT_ALLOW_INTERRUPT(handle)) {
            if (IS_EFFECT_TIMER_ACTIVE(handle)) {
                xTimerStop(handle->effect_timer, 0);
            }
            handle->effect_flag.running = false;
        } else if (IS_EFFECT_RUNNING(handle)) {
            ESP_LOGW(TAG, "Executing the effect, and does not allow interruption, skip writing, only save on/off status to off");
            handle->status.on = false;
            LB_MUTEX_GIVE(handle);
            return ESP_FAIL;
        }
        handle->status.on = false;

        uint16_t value[5] = { 0 };
        uint8_t channel_mask = get_channel_mask(handle->cap.led_beads);

        /**
         * When the hardware CCT is enabled, the CCT channel will not change and only the brightness channel will be turned off.
         *
         */
        if (IS_WHITE_OUTPUT_HARDWARE_MIXED(handle)) {
            channel_mask &= (SELECT_COLOR_CHANNEL | SELECT_WARM_BRIGHTNESS_YELLOW_CHANNEL);
            err = hal_set_channel_group(handle->hal_ctx, value, channel_mask, fade_time);
        } else {
            err = hal_set_channel_group(handle->hal_ctx, value, channel_mask, fade_time);
        }

        ESP_LOGD(TAG, "hal write value [r:0 g:0 b:0 c:0 w:0], power off, channel_mask:%d fade_ms:%d", channel_mask, fade_time);
        LB_MUTEX_GIVE(handle);
    } else {
        LB_MUTEX_TAKE(handle, portMAX_DELAY);
        switch (handle->status.mode) {
        case WORK_COLOR:
            handle->status.on = true;
            handle->status.value = (handle->status.value) ? handle->status.value : 100;
            err = lightbulb_set_hsv(handle, handle->status.hue, handle->status.saturation, handle->status.value);
            break;

        case WORK_WHITE:
            handle->status.on = true;
            handle->status.brightness = (handle->status.brightness) ? handle->status.brightness : 100;
            err = lightbulb_set_cctb(handle, handle->status.cct_percentage, handle->status.brightness);
            break;

        default:
            ESP_LOGW(TAG, "This operation is not supported");
            break;
        }
        LB_MUTEX_GIVE(handle);
    }

    return err;
}

int16_t lightbulb_get_hue(lightbulb_handle_t handle)
{
    LIGHTBULB_CHECK(handle, "handle is null", return -1);
    LIGHTBULB_CHECK(IS_COLOR_CHANNEL_SELECTED(handle), "color channel output is disable", return -1);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    int16_t result = handle->status.hue;
    LB_MUTEX_GIVE(handle);

    return result;
}

int8_t lightbulb_get_saturation(lightbulb_handle_t handle)
{
    LIGHTBULB_CHECK(handle, "handle is null", return -1);
    LIGHTBULB_CHECK(IS_COLOR_CHANNEL_SELECTED(handle), "color channel output is disable", return -1);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    int8_t result = handle->status.saturation;
    LB_MUTEX_GIVE(handle);

    return result;
}

int8_t lightbulb_get_value(lightbulb_handle_t handle)
{
    LIGHTBULB_CHECK(handle, "handle is null", return -1);
    LIGHTBULB_CHECK(IS_COLOR_CHANNEL_SELECTED(handle), "color channel output is disable", return -1);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    int8_t result = handle->status.value;
    LB_MUTEX_GIVE(handle);

    return result;
}

int8_t lightbulb_get_cct_percentage(lightbulb_handle_t handle)
{
    LIGHTBULB_CHECK(handle, "handle is null", return -1);
    LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED(handle), "white channel output is disable", return -1);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    int8_t result = handle->status.cct_percentage;
    LB_MUTEX_GIVE(handle);

    return result;
}

int16_t lightbulb_get_cct_kelvin(lightbulb_handle_t handle)
{
    LIGHTBULB_CHECK(handle, "handle is null", return -1);
    LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED(handle), "white channel output is disable", return -1);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    int16_t result = handle->cct_manager.percentage_to_kelvin(handle, handle->status.cct_percentage);
    LB_MUTEX_GIVE(handle);

    return result;
}

int8_t lightbulb_get_brightness(lightbulb_handle_t handle)
{
    LIGHTBULB_CHECK(handle, "handle is null", return -1);
    LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED(handle), "white channel output is disable", return -1);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    int8_t result = handle->status.brightness;
    LB_MUTEX_GIVE(handle);

    return result;
}

esp_err_t lightbulb_get_all_detail(lightbulb_handle_t handle, lightbulb_status_t *status)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(status, "status is null", return ESP_FAIL);
    LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED(handle) || IS_COLOR_CHANNEL_SELECTED(handle), "white or color channel output is disable", return false);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    memcpy(status, &handle->status, sizeof(lightbulb_status_t));
    LB_MUTEX_GIVE(handle);

    return ESP_OK;
}

bool lightbulb_get_switch(lightbulb_handle_t handle)
{
    LIGHTBULB_CHECK(handle, "handle is null", return false);
    LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED(handle) || IS_COLOR_CHANNEL_SELECTED(handle), "white or color channel output is disable", return false);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    bool result = handle->status.on;
    LB_MUTEX_GIVE(handle);

    return result;
}

esp_err_t lightbulb_set_fades_function(lightbulb_handle_t handle, bool is_enable)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    if (is_enable) {
        handle->cap.enable_fade = true;
    } else {
        handle->cap.enable_fade = false;
    }
    LB_MUTEX_GIVE(handle);

    return ESP_OK;
}

esp_err_t lightbulb_set_storage_function(lightbulb_handle_t handle, bool is_enable)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    if (is_enable) {
        handle->cap.enable_status_storage = true;
    } else {
        handle->cap.enable_status_storage = false;
    }
    LB_MUTEX_GIVE(handle);

    return ESP_OK;
}

esp_err_t lightbulb_set_fade_time(lightbulb_handle_t handle, uint32_t fade_time_ms)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    handle->cap.fade_time_ms = fade_time_ms;
    LB_MUTEX_GIVE(handle);

    return ESP_OK;
}

bool lightbulb_get_fades_function_status(lightbulb_handle_t handle)
{
    LIGHTBULB_CHECK(handle, "handle is null", return false);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    bool result = handle->cap.enable_fade;
    LB_MUTEX_GIVE(handle);

    return result;
}

lightbulb_works_mode_t lightbulb_get_mode(lightbulb_handle_t handle)
{
    LIGHTBULB_CHECK(handle, "handle is null", return WORK_INVALID);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    lightbulb_works_mode_t result = WORK_INVALID;
    if (IS_COLOR_CHANNEL_SELECTED(handle) || IS_WHITE_CHANNEL_SELECTED(handle)) {
        result = handle->status.mode;
    }
    LB_MUTEX_GIVE(handle);

    return result;
}

esp_err_t lightbulb_get_power_limit(lightbulb_handle_t handle, lightbulb_power_limit_t *power_limit)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(power_limit, "power_limit is null", return ESP_FAIL);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    memcpy(power_limit, &handle->power, sizeof(lightbulb_power_limit_t));
    LB_MUTEX_GIVE(handle);

    return ESP_OK;
}

esp_err_t lightbulb_update_status(lightbulb_handle_t handle, lightbulb_status_t *new_status, bool trigger)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(new_status, "new_status is null", return ESP_FAIL);
    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    esp_err_t err = ESP_OK;

    memcpy(&handle->status, new_status, sizeof(lightbulb_status_t));

    if (trigger) {
        err = lightbulb_set_switch(handle, handle->status.on);
    }
    LB_MUTEX_GIVE(handle);

    return err;
}

esp_err_t lightbulb_basic_effect_start(lightbulb_handle_t handle, lightbulb_effect_config_t *config)
{
    esp_err_t err = ESP_ERR_INVALID_STATE;
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(config, "config is null", return ESP_FAIL);
    LB_MUTEX_TAKE(handle, portMAX_DELAY);

    bool flag = ((config->effect_type == EFFECT_BREATH) ? true : false);

    if (IS_LOW_POWER_FUNCTION_ENABLED(handle)) {
        xTimerStop(handle->power_timer, 0);
    }

    if (IS_EFFECT_TIMER_ACTIVE(handle)) {
        xTimerStop(handle->effect_timer, 0);
    }

    if (config->mode == WORK_COLOR) {
        LIGHTBULB_CHECK(IS_COLOR_CHANNEL_SELECTED(handle), "color channel output is disable", goto EXIT);
        uint16_t color_value_max[5] = { 0 };
        float color_param_value_max[5] = { 0 };
        uint16_t color_value_min[5] = { 0 };
        float color_param_value_min[5] = { 0 };
        uint8_t channel_mask = get_channel_mask(handle->cap.led_beads);
        err = ESP_OK;
        handle->color_manager.hsv_to_rgb(handle, config->hue, config->saturation, config->max_value_brightness, &color_param_value_max[0], &color_param_value_max[1], &color_param_value_max[2], &color_param_value_max[3], &color_param_value_max[4]);
        handle->color_manager.hsv_to_rgb(handle, config->hue, config->saturation, config->min_value_brightness, &color_param_value_min[0], &color_param_value_min[1], &color_param_value_min[2], &color_param_value_min[3], &color_param_value_min[4]);

        process_color_power_limit(handle, handle->power.color_max_power / 100.0, color_param_value_max, config->max_value_brightness, color_value_max);
        process_color_power_limit(handle, handle->power.color_max_power / 100.0, color_param_value_min, config->min_value_brightness, color_value_min);

        err |= hal_start_channel_group_action(handle->hal_ctx, color_value_min, color_value_max, channel_mask, config->effect_cycle_ms, flag);

    } else if (config->mode == WORK_WHITE) {
        LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED(handle), "white channel output is disable", goto EXIT);
        if (config->cct > 100) {
            LIGHTBULB_CHECK(config->cct >= handle->cct_manager.kelvin_range.min && config->cct <= handle->cct_manager.kelvin_range.max, "cct kelvin out of range: %d", goto EXIT, config->cct);
            ESP_LOGW(TAG, "will convert kelvin to percentage, %dK -> %d%%", config->cct, handle->cct_manager.kelvin_to_percentage(handle, config->cct));
            config->cct = handle->cct_manager.kelvin_to_percentage(handle, config->cct);
        }
        uint16_t white_value_max[5] = { 0 };
        uint16_t white_value_min[5] = { 0 };
        uint8_t channel_mask = get_channel_mask(handle->cap.led_beads);
        err = ESP_OK;

        cct_and_brightness_convert_and_power_limit(handle, handle->cap.led_beads, handle->power.white_max_power / 100.0, config->cct, config->max_value_brightness, white_value_max);
        cct_and_brightness_convert_and_power_limit(handle, handle->cap.led_beads, handle->power.white_max_power / 100.0, config->cct, config->min_value_brightness, white_value_min);

        err |= hal_start_channel_group_action(handle->hal_ctx, white_value_min, white_value_max, channel_mask, config->effect_cycle_ms, flag);
    } else {
        err = ESP_ERR_NOT_SUPPORTED;
    }

    if (err == ESP_OK) {
        handle->effect_flag.allow_interrupt = !config->interrupt_forbidden;
        handle->effect_flag.running = true;
        if (config->total_ms > 0) {
            handle->effect_user_cb = config->user_cb;
            if (!handle->effect_timer) {
                handle->effect_timer = xTimerCreate("effect_timer", pdMS_TO_TICKS(config->total_ms), false, handle, timercb);
                if (!handle->effect_timer) {
                    ESP_LOGE(TAG, "create timer fail");
                    handle->effect_flag.running = false;
                    handle->effect_user_cb = NULL;
                    err = ESP_FAIL;
                    goto EXIT;
                }
            } else {
                xTimerChangePeriod(handle->effect_timer, pdMS_TO_TICKS(config->total_ms), 0);
            }
            vTimerSetTimerID(handle->effect_timer, handle);

            if (xTimerStart(handle->effect_timer, 0) != pdPASS) {
                ESP_LOGW(TAG, "The auto-stop timer start fail, the effect will continue executing, but will not stop automatically.");
                err = ESP_FAIL;
            } else {
                ESP_LOGI(TAG, "The auto-stop timer will trigger after %d ms.", config->total_ms);
            }
        } else {
            handle->effect_user_cb = NULL;
            ESP_LOGI(TAG, "The auto-stop timer is not running, the effect will keep running.");
        }
        ESP_LOGI(TAG, "effect config: \r\n"
                 "\teffect type: %d\r\n"
                 "\tmode: %d\r\n"
                 "\thue:%d\r\n"
                 "\tsaturation:%d\r\n"
                 "\tcct:%d\r\n"
                 "\tmin_brightness:%d\r\n"
                 "\tmax_brightness:%d\r\n"
                 "\teffect_cycle_ms:%d\r\n"
                 "\ttotal_ms:%d\r\n"
                 "\tinterrupt_forbidden:%d", config->effect_type, config->mode, config->hue, config->saturation,
                 config->cct, config->min_value_brightness, config->max_value_brightness, config->effect_cycle_ms, config->total_ms, config->interrupt_forbidden);
        ESP_LOGI(TAG, "This effect will %s to be interrupted", handle->effect_flag.allow_interrupt ? "allow" : "not be allowed");
    }

EXIT:
    LB_MUTEX_GIVE(handle);
    return err;
}

esp_err_t lightbulb_basic_effect_stop(lightbulb_handle_t handle)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    esp_err_t err = ESP_FAIL;
    uint8_t channel_mask = 0;
    if (IS_COLOR_CHANNEL_SELECTED(handle)) {
        channel_mask |= (SELECT_COLOR_CHANNEL);
    }

    if (IS_WHITE_CHANNEL_SELECTED(handle)) {
        channel_mask |= (SELECT_WHITE_CHANNEL);
    }
    err = hal_stop_channel_action(handle->hal_ctx, channel_mask);
    ESP_LOGI(TAG, "Stop effect");

    handle->effect_flag.running = false;
    LB_MUTEX_GIVE(handle);

    return err;
}

esp_err_t lightbulb_basic_effect_stop_and_restore(lightbulb_handle_t handle)
{
    LIGHTBULB_CHECK(handle, "handle is null", return ESP_ERR_INVALID_ARG);

    LB_MUTEX_TAKE(handle, portMAX_DELAY);
    bool is_on = handle->status.on;
    handle->effect_flag.running = false;
    LB_MUTEX_GIVE(handle);

    return lightbulb_set_switch(handle, is_on);
}
