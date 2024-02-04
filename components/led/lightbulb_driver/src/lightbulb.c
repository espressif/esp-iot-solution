/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
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
#define LB_MUTEX_TAKE(delay_ms)                (xSemaphoreTakeRecursive(s_lb_obj->mutex, delay_ms))
#define LB_MUTEX_GIVE()                        (xSemaphoreGiveRecursive(s_lb_obj->mutex))

/**
 * @brief Lightbulb function check
 */
#define IS_COLOR_CHANNEL_SELECTED()           (s_lb_obj->cap.led_beads >= LED_BEADS_3CH_RGB)
#define IS_WHITE_CHANNEL_SELECTED()           (s_lb_obj->cap.led_beads != LED_BEADS_3CH_RGB)
#define IS_LOW_POWER_FUNCTION_ENABLED()       ((s_lb_obj->cap.enable_lowpower && s_lb_obj->power_timer) ? true : false)
#define IS_AUTO_STATUS_STORAGE_ENABLED()      ((s_lb_obj->cap.enable_status_storage && s_lb_obj->storage_timer) ? true : false)
#define IS_WHITE_OUTPUT_HARDWARE_MIXED()      (s_lb_obj->cap.enable_hardware_cct)
#define IS_AUTO_ON_FUNCTION_ENABLED()         ((!s_lb_obj->cap.disable_auto_on) ? true : false)
#define IS_EFFECT_TIMER_ACTIVE()              (s_lb_obj->effect_timer && (xTimerIsTimerActive(s_lb_obj->effect_timer) == pdTRUE))
#define IS_EFFECT_ALLOW_INTERRUPT()           (s_lb_obj->effect_flag.running && s_lb_obj->effect_flag.allow_interrupt)
#define IS_EFFECT_RUNNING()                   (s_lb_obj->effect_flag.running)

/**
 * @brief Lightbulb fade time calculation
 */
#define CALCULATE_FADE_TIME()                 (s_lb_obj->cap.enable_fade ? s_lb_obj->cap.fade_time_ms : 0)

/**
 * @brief
 *
 */
#define LIGHTBULB_STORAGE_KEY                 ("lb_status")
#define LIGHTBULB_NAMESPACE                   ("lightbulb")

/**
 * @brief Default configuration values
 */
#define MAX_CCT_K           (7000)   // Maximum color temperature in Kelvin
#define MIN_CCT_K           (2200)   // Minimum color temperature in Kelvin
#define MAX_FADE_MS         (3000)   // Maximum fade time in milliseconds
#define MIN_FADE_MS         (100)    // Minimum fade time in milliseconds

typedef struct {
    lightbulb_status_t status;         // Record the current status of the lightbulb, e.g., on or off
    lightbulb_capability_t cap;        // Record the capabilities of the driver
    lightbulb_power_limit_t power;     // Record the power limit param of the lightbulb,
    TimerHandle_t power_timer;         // Timer handle related to the power management of the lightbulb
    TimerHandle_t storage_timer;       // Timer handle related to the storage status of the lightbulb
    TimerHandle_t effect_timer;        // Timer handle related to the flashing, fading
    SemaphoreHandle_t mutex;           // For multi-thread protection

    // Structure containing flags related to effects
    struct {
        bool allow_interrupt : 1;      // Flag indicating if the effect can be interrupted
        bool running : 1;              // Flag indicating if the effect is currently running
    } effect_flag;

    // Structure for managing color temperature-related functionalities
    struct {
        uint16_t (*percentage_to_kelvin)(uint8_t percentage); // Function pointer to convert percentage to Kelvin color temperature
        uint8_t (*kelvin_to_percentage)(uint16_t kelvin);    // Function pointer to convert Kelvin color temperature to percentage
        lightbulb_cct_kelvin_range_t kelvin_range;           // Color temperature range
        lightbulb_cct_mapping_data_t *mix_table;             // Mapping table for mixing color temperatures
        int table_size;                                      // Size of the mapping table
    } cct_manager;
} lightbulb_obj_t;

static lightbulb_obj_t *s_lb_obj = NULL;

esp_err_t lightbulb_status_set_to_nvs(const lightbulb_status_t *value)
{
    LIGHTBULB_CHECK(value, "value is null", return ESP_ERR_INVALID_ARG);

    esp_err_t err = ESP_OK;
    nvs_handle_t handle = 0;

    err = nvs_open(LIGHTBULB_NAMESPACE, NVS_READWRITE, &handle);
    LIGHTBULB_CHECK(err == ESP_OK, "nvs open fail, reason code: %d", return err, err);

    err = nvs_set_blob(handle, LIGHTBULB_STORAGE_KEY, value, sizeof(lightbulb_status_t));
    LIGHTBULB_CHECK(err == ESP_OK, "nvs set fail, reason code: %d", return err, err);

    err = nvs_commit(handle);
    LIGHTBULB_CHECK(err == ESP_OK, "nvs commit fail", return err);

    nvs_close(handle);

    return ESP_OK;
}

esp_err_t lightbulb_status_get_from_nvs(lightbulb_status_t *value)
{
    LIGHTBULB_CHECK(value, "value is null", return ESP_ERR_INVALID_ARG);

    esp_err_t err = ESP_OK;
    nvs_handle_t handle = 0;
    size_t req_len = sizeof(lightbulb_status_t);

    err = nvs_open(LIGHTBULB_NAMESPACE, NVS_READWRITE, &handle);
    LIGHTBULB_CHECK(err == ESP_OK, "nvs open fail, reason code: %d", return err, err);

    err = nvs_get_blob(handle, LIGHTBULB_STORAGE_KEY, value, &req_len);
    LIGHTBULB_CHECK(err == ESP_OK, "nvs get fail, reason code: %d", return err, err);

    nvs_close(handle);

    return ESP_OK;
}

esp_err_t lightbulb_status_erase_nvs_storage(void)
{
    esp_err_t err = ESP_OK;
    nvs_handle_t handle = 0;

    err = nvs_open(LIGHTBULB_NAMESPACE, NVS_READWRITE, &handle);
    LIGHTBULB_CHECK(err == ESP_OK, "nvs open fail, reason code: %d", return err, err);

    err = nvs_erase_key(handle, LIGHTBULB_STORAGE_KEY);
    LIGHTBULB_CHECK(err == ESP_OK, "nvs erase fail, reason code: %d", return err, err);

    err = nvs_commit(handle);
    LIGHTBULB_CHECK(err == ESP_OK, "nvs commit fail, reason code: %d", return err, err);

    nvs_close(handle);

    return ESP_OK;
}

static lightbulb_cct_mapping_data_t search_mapping_cct_data(uint16_t cct)
{
    int low = 0;
    int high = s_lb_obj->cct_manager.table_size - 1;
    int mid;
    lightbulb_cct_mapping_data_t result;
    while (low <= high) {
        mid = low + (high - low) / 2;
        lightbulb_cct_mapping_data_t* current_entry = &s_lb_obj->cct_manager.mix_table[mid];

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
static uint16_t standard_percentage_convert_to_kelvin(uint8_t percentage)
{
    // Convert percentage to a floating-point value between 0 and 1
    float _percentage = (float)percentage / 100;

    // Calculate Kelvin value within the specified range
    uint16_t _kelvin = (_percentage * (s_lb_obj->cct_manager.kelvin_range.max - s_lb_obj->cct_manager.kelvin_range.min) + s_lb_obj->cct_manager.kelvin_range.min);

    // Convert Kelvin value to the nearest multiple of 100 (round to the nearest integer)
    _kelvin = (_kelvin / 100) * 100;

    return _kelvin;
}

static uint16_t precise_percentage_convert_to_kelvin(uint8_t percentage)
{
    // Convert percentage to a floating-point value between 0 and 1
    lightbulb_cct_mapping_data_t data;
    data = search_mapping_cct_data(percentage);

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
static uint8_t standard_kelvin_convert_to_percentage(uint16_t kelvin)
{
    // Ensure the input kelvin value is within the specified range
    if (kelvin > s_lb_obj->cct_manager.kelvin_range.max) {
        kelvin = s_lb_obj->cct_manager.kelvin_range.max;
    }
    if (kelvin < s_lb_obj->cct_manager.kelvin_range.min) {
        kelvin = s_lb_obj->cct_manager.kelvin_range.min;
    }

    // Calculate the percentage value based on the Kelvin value within the range
    return 100 * ((float)(kelvin - s_lb_obj->cct_manager.kelvin_range.min) / (s_lb_obj->cct_manager.kelvin_range.max - s_lb_obj->cct_manager.kelvin_range.min));
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
static uint8_t precise_kelvin_convert_to_percentage(uint16_t kelvin)
{
    // Ensure the input kelvin value is within the specified range
    lightbulb_cct_mapping_data_t data;
    data = search_mapping_cct_data(kelvin);

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
static void cct_and_brightness_convert_and_power_limit(lightbulb_led_beads_comb_t led_beads, float multiple, uint8_t cct, uint8_t brightness, uint16_t white_value[])
{
    if (led_beads == LED_BEADS_1CH_C || led_beads == LED_BEADS_4CH_RGBC || led_beads == LED_BEADS_4CH_RGBCC) {
        uint16_t value = brightness * 255 / 100;
        hal_get_linear_table_value((uint8_t)value, &white_value[3]);
        if (led_beads == LED_BEADS_4CH_RGBCC) {
            hal_get_linear_table_value((uint8_t)value, &white_value[4]);
        }
    } else if (led_beads == LED_BEADS_1CH_W || led_beads == LED_BEADS_4CH_RGBW || led_beads == LED_BEADS_4CH_RGBWW) {
        uint16_t value = brightness * 255 / 100;
        hal_get_linear_table_value((uint8_t)value, &white_value[4]);
        if (led_beads == LED_BEADS_4CH_RGBWW) {
            hal_get_linear_table_value((uint8_t)value, &white_value[3]);
        }
    } else if ((led_beads == LED_BEADS_2CH_CW || led_beads == LED_BEADS_5CH_RGBCW) && IS_WHITE_OUTPUT_HARDWARE_MIXED()) {
        uint16_t value1 = cct * 255 / 100;
        uint16_t value2 = brightness * 255 / 100;
        hal_get_linear_table_value((uint8_t)value1, &white_value[3]);
        hal_get_linear_table_value((uint8_t)value2, &white_value[4]);
    } else if (led_beads == LED_BEADS_2CH_CW || ((led_beads == LED_BEADS_5CH_RGBCW) && (s_lb_obj->cap.enable_precise_cct_control == false))) {
        uint16_t max_value = 255;
        float max_power;
        float _c = cct / 100.0;
        float _w = (100 - cct) / 100.0;

        float baseline = MAX(_c, _w);
        max_power = MIN(max_value * multiple, max_value / baseline);
        _c = max_power * _c * (brightness / 100.0);
        _w = max_power * _w * (brightness / 100.0);
        hal_get_linear_table_value((uint8_t)_c, &white_value[3]);
        hal_get_linear_table_value((uint8_t)_w, &white_value[4]);
    } else {
        uint16_t max_value;
        float max_power;
        lightbulb_cct_mapping_data_t data = search_mapping_cct_data(cct);
        ESP_LOGD(TAG, "%f, %f, %f, %f, %f", data.rgbcw[0], data.rgbcw[1], data.rgbcw[2], data.rgbcw[3], data.rgbcw[4]);

        hal_get_driver_feature(QUERY_MAX_INPUT_VALUE, &max_value);
        float baseline = MAX(data.rgbcw[0], data.rgbcw[1]);
        baseline = MAX(baseline, data.rgbcw[2]);
        baseline = MAX(baseline, data.rgbcw[3]);
        baseline = MAX(baseline, data.rgbcw[4]);
        max_power = MIN(max_value * multiple, max_value / baseline);
        ESP_LOGD(TAG, "%f, %d, %f", max_power, max_value, baseline);
        for (int i = 0; i < 5; i++) {
            white_value[i] = round(max_power * data.rgbcw[i] * (brightness / 100.0));
        }
    }
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
static uint8_t process_color_value_limit(uint8_t value)
{
    if (value == 0) {
        return 0;
    }
    float percentage = value / 100.0;

    uint8_t result = (s_lb_obj->power.color_max_value - s_lb_obj->power.color_min_value) * percentage + s_lb_obj->power.color_min_value;
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
static uint8_t process_white_brightness_limit(uint8_t brightness)
{
    if (brightness == 0) {
        return 0;
    }
    float percentage = brightness / 100.0;

    uint8_t result = (s_lb_obj->power.white_max_brightness - s_lb_obj->power.white_min_brightness) * percentage + s_lb_obj->power.white_min_brightness;
    ESP_LOGD(TAG, "white_brightness_output input:%d output:%d", brightness, result);
    return result;
}

/**
 * @brief Recalculate color power
 * @attention 300% = 100% + 100% + 100% : Full power output on each channel. If single channel output is 3w then total output is 9w.
 * @note
 *      input           output(color_max_power = 100)       output(color_max_power = 200)       output(color_max_power = 300)
 *      255,255,0       127,127,0                           255,255,0                           255,255,0
 *      127,127,0       63,63,0                             127,127,0                           127,127,0
 *      63,63,0         31,31,0                             63,63,0                             63,63,0
 *      255,255,255     85,85,85                            170,170,170                         255,255,255
 *      127,127,127     42,42,42                            84,84,84                            127,127,127
 *      63,63,63        21,21,21                            42,42,42                            63,63,63
 *
 */
static void process_color_power_limit(float multiple, uint8_t r, uint8_t g, uint8_t b, uint16_t *out_r, uint16_t *out_g, uint16_t *out_b)
{
    if (r == 0 && g == 0 && b == 0) {
        *out_r = 0;
        *out_g = 0;
        *out_b = 0;
        return;
    }

    uint16_t max_value;
    uint16_t _r;
    uint16_t _g;
    uint16_t _b;

    // 1. Map the input RGB values to the gamma table to obtain the corresponding mapped values
    hal_get_driver_feature(QUERY_MAX_INPUT_VALUE, &max_value);
    hal_get_curve_table_value(r, &_r);
    hal_get_curve_table_value(g, &_g);
    hal_get_curve_table_value(b, &_b);

    // 2. Calculate the color distribution ratios for the RGB channels
    float ratio_r = _r * 1.0 / (max_value);
    float ratio_g = _g * 1.0 / (max_value);
    float ratio_b = _b * 1.0 / (max_value);
    ESP_LOGD(TAG, "ratio_r:%f ratio_g:%f rgb_ratio_b:%f max_value:%d", ratio_r, ratio_g, ratio_b, max_value);

    // 3. Calculate the grayscale ratios for the RGB channels (baseline values for power limiting)
    float grayscale_r = _r / ((_r + _g + _b) * 1.0);
    float grayscale_g = _g / ((_r + _g + _b) * 1.0);
    float grayscale_b = _b / ((_r + _g + _b) * 1.0);
    float baseline = MAX(grayscale_r, grayscale_g);
    baseline = MAX(baseline,  grayscale_b);
    ESP_LOGD(TAG, "grayscale_r:%f grayscale_g:%f grayscale_b:%f baseline:%f", grayscale_r, grayscale_g, grayscale_b, baseline);

    // 4. Calculate the maximum allowed output power based on the maximum input value and power limiting factor
    float max_power = multiple * (max_value);
    max_power = MIN(max_value, baseline * max_power);
    ESP_LOGD(TAG, "multiple:%f, max_power:%f ", multiple, max_power);

    // 5. Recalculate the final output values based on the maximum allowed output power and color distribution ratios
    *out_r = max_power * ratio_r;
    *out_g = max_power * ratio_g;
    *out_b = max_power * ratio_b;

    ESP_LOGD(TAG, "[input: %d %d %d], [output:%d %d %d]", r, g, b, *out_r, *out_g, *out_b);
}

static void timercb(TimerHandle_t tmr)
{
    if (tmr == s_lb_obj->power_timer) {
        hal_sleep_control(true);
    } else if (tmr == s_lb_obj->storage_timer) {
        lightbulb_status_set_to_nvs(&s_lb_obj->status);
        if (s_lb_obj->cap.storage_cb) {
            s_lb_obj->cap.storage_cb(s_lb_obj->status);
        }
    } else if (tmr == s_lb_obj->effect_timer) {
        lightbulb_basic_effect_stop();
        void(*user_cb)(void) = pvTimerGetTimerID(tmr);
        if (user_cb) {
            user_cb();
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

static bool mix_table_data_check(void)
{
    bool result = true;

    for (int i = 0; i < s_lb_obj->cct_manager.table_size; i++) {
        float sum = 0;
        uint8_t flag = 0;
        for (int j = 0; j < 5; j ++) {
            sum += s_lb_obj->cct_manager.mix_table[i].rgbcw[j];
        }
        if (i >= 1) {
            if (s_lb_obj->cct_manager.mix_table[i].cct_percentage <= s_lb_obj->cct_manager.mix_table[i - 1].cct_percentage) {
                flag |= 0x01;
            }
            if (s_lb_obj->cct_manager.mix_table[i].cct_kelvin <= s_lb_obj->cct_manager.mix_table[i - 1].cct_kelvin) {
                flag |= 0x02;
            }
        }

        if (!(fabs(sum - 1.0) < 0.001)) {
            result = false;
            ESP_LOGE(TAG, "%d%%: %dK, sum:%f ,sum is not equal to 1.0, please check", s_lb_obj->cct_manager.mix_table[i].cct_percentage, s_lb_obj->cct_manager.mix_table[i].cct_kelvin, sum);
        }
        if (flag) {
            result = false;
            ESP_LOGE(TAG, "%d%%: %dK, mix table data must be sorted from small to large", s_lb_obj->cct_manager.mix_table[i].cct_percentage, s_lb_obj->cct_manager.mix_table[i].cct_kelvin);
        }
    }

    return result;
}

static void print_func(void *priv)
{
    char *name;

    hal_get_driver_feature(QUERY_DRIVER_NAME, &name);
    ESP_LOGI(TAG, "---------------------------------------------------------------------");
    ESP_LOGI(TAG, "lightbulb driver component version: %d.%d.%d", LIGHTBULB_DRIVER_VER_MAJOR, LIGHTBULB_DRIVER_VER_MINOR, LIGHTBULB_DRIVER_VER_PATCH);
    ESP_LOGI(TAG, "driver name: %s", name);
    ESP_LOGI(TAG, "low power control: %s", s_lb_obj->cap.enable_lowpower ? "enable" : "disable");
    ESP_LOGI(TAG, "status storage: %s", s_lb_obj->cap.enable_status_storage ? "enable" : "disable");
    ESP_LOGI(TAG, "status storage delay %d ms", s_lb_obj->cap.enable_status_storage == true ? s_lb_obj->cap.storage_delay_ms : 0);
    ESP_LOGI(TAG, "fade: %s", s_lb_obj->cap.enable_fade ? "enable" : "disable");
    ESP_LOGI(TAG, "fade %d ms", s_lb_obj->cap.enable_fade == true ? s_lb_obj->cap.fade_time_ms : 0);
    ESP_LOGI(TAG, "led_beads: %d", s_lb_obj->cap.led_beads);
    ESP_LOGI(TAG, "hardware cct: %s", s_lb_obj->cap.enable_hardware_cct ? "Yes" : "No");
    ESP_LOGI(TAG, "precise cct control: %s", s_lb_obj->cap.enable_precise_cct_control ? "disable" : "enable");
    ESP_LOGI(TAG, "sync change: %s", s_lb_obj->cap.sync_change_brightness_value ? "enable" : "disable");
    ESP_LOGI(TAG, "auto on: %s", s_lb_obj->cap.disable_auto_on ? "disable" : "enable");

    if (IS_WHITE_CHANNEL_SELECTED()) {
        ESP_LOGI(TAG, "     white mode: enable");
    }
    if (IS_COLOR_CHANNEL_SELECTED()) {
        ESP_LOGI(TAG, "     color mode: enable");
    }
    ESP_LOGI(TAG, "sync change: %s", s_lb_obj->cap.sync_change_brightness_value ? "enable" : "disable");
    ESP_LOGI(TAG, "power limit param: ");
    ESP_LOGI(TAG, "     white max brightness: %d", s_lb_obj->power.white_max_brightness);
    ESP_LOGI(TAG, "     white min brightness: %d", s_lb_obj->power.white_min_brightness);
    ESP_LOGI(TAG, "     white max power: %d", s_lb_obj->power.white_max_power);
    ESP_LOGI(TAG, "     color max brightness: %d", s_lb_obj->power.color_max_value);
    ESP_LOGI(TAG, "     color min brightness: %d", s_lb_obj->power.color_min_value);
    ESP_LOGI(TAG, "     color max power: %d", s_lb_obj->power.color_max_power);
    if (IS_WHITE_CHANNEL_SELECTED()) {
        ESP_LOGI(TAG, "cct kelvin range param: ");
        ESP_LOGI(TAG, "     max cct: %d K", s_lb_obj->cct_manager.kelvin_range.max);
        ESP_LOGI(TAG, "     min cct: %d K", s_lb_obj->cct_manager.kelvin_range.min);
        for (int i = 0; i < s_lb_obj->cct_manager.table_size && s_lb_obj->cct_manager.table_size > 0; i++) {
            ESP_LOGI(TAG, "%d%%, %dK, %f %f %f %f %f", s_lb_obj->cct_manager.mix_table[i].cct_percentage, s_lb_obj->cct_manager.mix_table[i].cct_kelvin, s_lb_obj->cct_manager.mix_table[i].rgbcw[0], s_lb_obj->cct_manager.mix_table[i].rgbcw[1], s_lb_obj->cct_manager.mix_table[i].rgbcw[2], s_lb_obj->cct_manager.mix_table[i].rgbcw[3], s_lb_obj->cct_manager.mix_table[i].rgbcw[4]);
        }
    }
    ESP_LOGI(TAG, "hue: %d, saturation: %d, value: %d", s_lb_obj->status.hue, s_lb_obj->status.saturation, s_lb_obj->status.value);
    ESP_LOGI(TAG, "select works mode: %s, power status: %d", s_lb_obj->status.mode == WORK_COLOR ? "color" : "white", s_lb_obj->status.on);
    ESP_LOGI(TAG, "---------------------------------------------------------------------");
}

esp_err_t lightbulb_init(lightbulb_config_t *config)
{
    esp_err_t err = ESP_FAIL;
    LIGHTBULB_CHECK(config, "Config is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(config->type > DRIVER_SELECT_INVALID, "Invalid driver select", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(config->type != DRIVER_SM2135E, "This version no longer supports SM2135E chip. Please switch to v0.5.2", return ESP_ERR_INVALID_STATE);
    LIGHTBULB_CHECK(config->capability.led_beads > LED_BEADS_INVALID && config->capability.led_beads < LED_BEADS_MAX, "Invalid led beads combination select", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(!s_lb_obj, "Already init done", return ESP_ERR_INVALID_STATE);

    s_lb_obj = calloc(1, sizeof(lightbulb_obj_t));
    LIGHTBULB_CHECK(s_lb_obj, "calloc fail", goto EXIT);

    s_lb_obj->mutex = xSemaphoreCreateRecursiveMutex();
    LIGHTBULB_CHECK(s_lb_obj->mutex, "mutex create fail", goto EXIT);

    // hal configuration
    void *driver_conf = NULL;
#ifdef CONFIG_ENABLE_PWM_DRIVER
    if (config->type == DRIVER_ESP_PWM) {
        driver_conf = (void *) & (config->driver_conf.pwm);
    }
#endif
#ifdef CONFIG_ENABLE_SM2135EH_DRIVER
    if (config->type == DRIVER_SM2135EH) {
        driver_conf = (void *) & (config->driver_conf.sm2135eh);
    }
#endif
#ifdef CONFIG_ENABLE_BP57x8D_DRIVER
    if (config->type == DRIVER_BP57x8D) {
        driver_conf = (void *) & (config->driver_conf.bp57x8d);
    }
#endif
#ifdef CONFIG_ENABLE_BP1658CJ_DRIVER
    if (config->type == DRIVER_BP1658CJ) {
        driver_conf = (void *) & (config->driver_conf.bp1658cj);
    }
#endif
#ifdef CONFIG_ENABLE_KP18058_DRIVER
    if (config->type == DRIVER_KP18058) {
        driver_conf = (void *) & (config->driver_conf.kp18058);
    }
#endif
#ifdef CONFIG_ENABLE_SM2x35EGH_DRIVER
    if (config->type == DRIVER_SM2x35EGH) {
        driver_conf = (void *) & (config->driver_conf.sm2x35egh);
    }
#endif
#ifdef CONFIG_ENABLE_WS2812_DRIVER
    if (config->type == DRIVER_WS2812) {
        driver_conf = (void *) & (config->driver_conf.ws2812);
    }
#endif
    // Config check
    if (config->type != DRIVER_ESP_PWM && config->type != DRIVER_WS2812) {
        if (config->capability.enable_hardware_cct == true) {
            config->capability.enable_hardware_cct = false;
            ESP_LOGW(TAG, "The IIC dimming chip must enable CCT mix, rewrite the enable_hardware_cct variable to false.");
        }
    }

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

    //Init HAL
    hal_config_t hal_conf = {
        .type = config->type,
        .driver_data = driver_conf,
    };
    err = hal_output_init(&hal_conf, config->gamma_conf, (void *)&config->init_status.mode);
    LIGHTBULB_CHECK(err == ESP_OK, "hal init fail", goto EXIT);

    // Load init status
    memcpy(&s_lb_obj->status, &config->init_status, sizeof(lightbulb_status_t));
    memcpy(&s_lb_obj->cap, &config->capability, sizeof(lightbulb_capability_t));

    // Check channel
    if (s_lb_obj->cap.led_beads == LED_BEADS_1CH_C || s_lb_obj->cap.led_beads == LED_BEADS_2CH_CW || s_lb_obj->cap.led_beads == LED_BEADS_4CH_RGBC || s_lb_obj->cap.led_beads == LED_BEADS_4CH_RGBCC
            || s_lb_obj->cap.led_beads == LED_BEADS_5CH_RGBCW || s_lb_obj->cap.led_beads == LED_BEADS_5CH_RGBC || s_lb_obj->cap.led_beads == LED_BEADS_5CH_RGBCC || s_lb_obj->cap.led_beads == LED_BEADS_5CH_RGBWW) {
        if (config->type == DRIVER_ESP_PWM) {
            hal_regist_channel(CHANNEL_ID_COLD_CCT_WHITE, config->io_conf.pwm_io.cold_cct);
        } else {
            hal_regist_channel(CHANNEL_ID_COLD_CCT_WHITE, config->io_conf.iic_io.cold_white);
        }
    }
    if (s_lb_obj->cap.led_beads == LED_BEADS_1CH_W || s_lb_obj->cap.led_beads == LED_BEADS_2CH_CW || s_lb_obj->cap.led_beads == LED_BEADS_4CH_RGBW || s_lb_obj->cap.led_beads == LED_BEADS_4CH_RGBWW
            || s_lb_obj->cap.led_beads == LED_BEADS_5CH_RGBCW || s_lb_obj->cap.led_beads == LED_BEADS_5CH_RGBW || s_lb_obj->cap.led_beads == LED_BEADS_5CH_RGBCC || s_lb_obj->cap.led_beads == LED_BEADS_5CH_RGBWW) {
        if (config->type == DRIVER_ESP_PWM) {
            hal_regist_channel(CHANNEL_ID_WARM_BRIGHTNESS_YELLOW, config->io_conf.pwm_io.warm_brightness);
        } else {
            hal_regist_channel(CHANNEL_ID_WARM_BRIGHTNESS_YELLOW, config->io_conf.iic_io.warm_yellow);
        }
    }

    if (s_lb_obj->cap.led_beads >= LED_BEADS_3CH_RGB) {
        if (config->type == DRIVER_ESP_PWM) {
            hal_regist_channel(CHANNEL_ID_RED, config->io_conf.pwm_io.red);
            hal_regist_channel(CHANNEL_ID_GREEN, config->io_conf.pwm_io.green);
            hal_regist_channel(CHANNEL_ID_BLUE, config->io_conf.pwm_io.blue);
        } else {
            hal_regist_channel(CHANNEL_ID_RED, config->io_conf.iic_io.red);
            hal_regist_channel(CHANNEL_ID_GREEN, config->io_conf.iic_io.green);
            hal_regist_channel(CHANNEL_ID_BLUE, config->io_conf.iic_io.blue);
        }
    }

    // Check cct output mode
    if (IS_WHITE_CHANNEL_SELECTED()) {
        if (s_lb_obj->cap.enable_precise_cct_control) {
            s_lb_obj->cct_manager.table_size = config->cct_mix_mode.precise.table_size;
            s_lb_obj->cct_manager.kelvin_to_percentage = precise_kelvin_convert_to_percentage;
            s_lb_obj->cct_manager.percentage_to_kelvin = precise_percentage_convert_to_kelvin;

            if (s_lb_obj->cct_manager.table_size == 0) {
                if (s_lb_obj->cap.led_beads == LED_BEADS_5CH_RGBC || s_lb_obj->cap.led_beads == LED_BEADS_5CH_RGBW) {
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
                    s_lb_obj->cct_manager.kelvin_range.max = MAX_CCT_K;
                    s_lb_obj->cct_manager.kelvin_range.min = MIN_CCT_K;
                    s_lb_obj->cct_manager.table_size = 11;
                    s_lb_obj->cct_manager.mix_table = calloc(s_lb_obj->cct_manager.table_size, sizeof(lightbulb_cct_mapping_data_t));
                    LIGHTBULB_CHECK(s_lb_obj->cct_manager.mix_table, "calloc fail", goto EXIT);
                    uint16_t interval_kelvin = (s_lb_obj->cct_manager.kelvin_range.max - s_lb_obj->cct_manager.kelvin_range.min) / 10;

                    for (int i = 0; i < s_lb_obj->cct_manager.table_size; i++) {
                        lightbulb_cct_mapping_data_t unit = { 0 };

                        unit.cct_percentage = i * 10;
                        unit.cct_kelvin = ((interval_kelvin * i) / 100) * 100 + s_lb_obj->cct_manager.kelvin_range.min;
                        if (s_lb_obj->cap.led_beads == LED_BEADS_5CH_RGBC) {
                            memcpy(unit.rgbcw, &default_rgbc_data[i][0], sizeof(float) * 5);
                        } else {
                            memcpy(unit.rgbcw, &default_rgbw_data[i][0], sizeof(float) * 5);
                        }
                        memcpy(&s_lb_obj->cct_manager.mix_table[i], &unit, sizeof(lightbulb_cct_mapping_data_t));
                    }
                } else {
                    ESP_LOGE(TAG, "This led combination does not have a default cct mix table, please income it in from outside.");
                    err = ESP_ERR_NOT_SUPPORTED;
                    goto EXIT;
                }
            } else {
                s_lb_obj->cct_manager.table_size = config->cct_mix_mode.precise.table_size;
                s_lb_obj->cct_manager.mix_table = calloc(s_lb_obj->cct_manager.table_size, sizeof(lightbulb_cct_mapping_data_t));
                LIGHTBULB_CHECK(s_lb_obj->cct_manager.mix_table, "calloc fail", goto EXIT);

                for (int i = 0; i < s_lb_obj->cct_manager.table_size; i++) {
                    memcpy(&s_lb_obj->cct_manager.mix_table[i], &config->cct_mix_mode.precise.table[i], sizeof(lightbulb_cct_mapping_data_t));
                }
            }
        } else {
            s_lb_obj->cct_manager.kelvin_to_percentage = standard_kelvin_convert_to_percentage;
            s_lb_obj->cct_manager.percentage_to_kelvin = standard_percentage_convert_to_kelvin;
            if ((config->cct_mix_mode.standard.kelvin_min >= config->cct_mix_mode.standard.kelvin_max) || (config->cct_mix_mode.standard.kelvin_min < 100) || (config->cct_mix_mode.standard.kelvin_max < 100)) {
                s_lb_obj->cct_manager.kelvin_range.max = MAX_CCT_K;
                s_lb_obj->cct_manager.kelvin_range.min = MIN_CCT_K;
                ESP_LOGW(TAG, "Kelvin value not set or is incorrect, default range (%dk - %dk) will be used", MIN_CCT_K, MAX_CCT_K);
            } else {
                s_lb_obj->cct_manager.kelvin_range.max = config->cct_mix_mode.standard.kelvin_max;
                s_lb_obj->cct_manager.kelvin_range.min = config->cct_mix_mode.standard.kelvin_min;
            }
        }
    }
    if (s_lb_obj->cct_manager.table_size > 0) {
        bool result = mix_table_data_check();
        err = ESP_ERR_INVALID_ARG;
        LIGHTBULB_CHECK(result, "mix table check fail", goto EXIT);
    }

    // init status update
    if (s_lb_obj->cap.led_beads == LED_BEADS_1CH_C || s_lb_obj->cap.led_beads == LED_BEADS_4CH_RGBC) {
        s_lb_obj->status.cct_percentage = 100;
    } else if (s_lb_obj->cap.led_beads == LED_BEADS_1CH_W || s_lb_obj->cap.led_beads == LED_BEADS_4CH_RGBW) {
        s_lb_obj->status.cct_percentage = 0;
    }

    // Fade check
    if (s_lb_obj->cap.enable_fade) {
        s_lb_obj->cap.fade_time_ms = MIN(MAX_FADE_MS, s_lb_obj->cap.fade_time_ms);
        s_lb_obj->cap.fade_time_ms = MAX(MIN_FADE_MS, s_lb_obj->cap.fade_time_ms);
    }

    // Low power check
    if (config->capability.enable_lowpower) {
        /* Make sure the fade is done and the flash operation is done, then enable light sleep */
        uint32_t time_ms = MAX(s_lb_obj->cap.fade_time_ms, s_lb_obj->cap.storage_delay_ms) + 1000;
        s_lb_obj->power_timer = xTimerCreate("power_timer", pdMS_TO_TICKS(time_ms), false, NULL, timercb);
        LIGHTBULB_CHECK(s_lb_obj->power_timer != NULL, "create timer fail", goto EXIT);
    }

    // Storage check
    if (config->capability.enable_status_storage) {
        s_lb_obj->cap.storage_delay_ms = MAX(s_lb_obj->cap.fade_time_ms, s_lb_obj->cap.storage_delay_ms) + 1000;
        s_lb_obj->storage_timer = xTimerCreate("storage_timer", pdMS_TO_TICKS(s_lb_obj->cap.storage_delay_ms), false, NULL, timercb);
        LIGHTBULB_CHECK(s_lb_obj->storage_timer != NULL, "create timer fail", goto EXIT);
    }

    // Power Limit check
    if (config->external_limit) {
        memcpy(&s_lb_obj->power, config->external_limit, sizeof(lightbulb_power_limit_t));
    } else {
        s_lb_obj->power.color_max_value = 100;
        s_lb_obj->power.white_max_brightness = 100;
        s_lb_obj->power.color_min_value = 1;
        s_lb_obj->power.white_min_brightness = 1;
        s_lb_obj->power.color_max_power = 300;
        s_lb_obj->power.white_max_power = 100;
    }

    // Output status according to init parameter
    if (s_lb_obj->status.on) {
        /* Fade can cause perceptible state changes when the system restarts abnormally, so we need to temporarily disable fade. */
        if (s_lb_obj->cap.enable_fade) {
            lightbulb_set_fades_function(false);
            lightbulb_set_switch(true);
            lightbulb_set_fades_function(true);
        } else {
            lightbulb_set_switch(true);
        }
    }

    print_func(config);

    return ESP_OK;

EXIT:
    lightbulb_deinit();

    return err;
}

esp_err_t lightbulb_deinit(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "deinit fail", return ESP_ERR_INVALID_STATE);

    if (s_lb_obj->power_timer) {
        xTimerStop(s_lb_obj->power_timer, 0);
        xTimerDelete(s_lb_obj->power_timer, 0);
        s_lb_obj->power_timer = NULL;
    }

    if (s_lb_obj->storage_timer) {
        xTimerStop(s_lb_obj->storage_timer, 0);
        xTimerDelete(s_lb_obj->storage_timer, 0);
        s_lb_obj->storage_timer = NULL;
    }

    if (s_lb_obj->effect_timer) {
        xTimerStop(s_lb_obj->effect_timer, 0);
        xTimerDelete(s_lb_obj->effect_timer, 0);
        s_lb_obj->effect_timer = NULL;
    }

    if (s_lb_obj->cct_manager.mix_table) {
        free(s_lb_obj->cct_manager.mix_table);
        s_lb_obj->cct_manager.mix_table = NULL;
    }

    free(s_lb_obj);
    s_lb_obj = NULL;

    return hal_output_deinit();
}

esp_err_t lightbulb_set_xyy(float x, float y, float Y)
{
    LIGHTBULB_CHECK(x <= 1.0, "x out of range", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(y <= 1.0, "y out of range", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(Y <= 100.0, "Y out of range", return ESP_ERR_INVALID_ARG);

    uint8_t r, g, b;
    uint16_t h;
    uint8_t s, v;

    lightbulb_xyy2rgb(x, y, Y, &r, &g, &b);
    lightbulb_rgb2hsv(r, g, b, &h, &s, &v);

    return lightbulb_set_hsv(h, s, v);
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

esp_err_t lightbulb_kelvin2percentage(uint16_t kelvin, uint8_t *percentage)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(s_lb_obj->cct_manager.kelvin_to_percentage, "No conversion function was registered because the this led combination does not support CCT.", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(percentage, "percentage is null", return ESP_ERR_INVALID_ARG);

    if (kelvin >= s_lb_obj->cct_manager.kelvin_range.min && kelvin <= s_lb_obj->cct_manager.kelvin_range.max) {
        ESP_LOGW(TAG, "kelvin out of range, will be forcibly converted");
    }

    *percentage = s_lb_obj->cct_manager.kelvin_to_percentage(kelvin);

    return ESP_OK;
}

esp_err_t lightbulb_percentage2kelvin(uint8_t percentage, uint16_t *kelvin)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(s_lb_obj->cct_manager.percentage_to_kelvin, "No conversion function was registered because the this led combination does not support CCT.", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(kelvin, "kelvin is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(percentage <= 100, "percentage out of range", return ESP_ERR_INVALID_ARG);

    *kelvin = s_lb_obj->cct_manager.percentage_to_kelvin(percentage);

    return ESP_OK;
}

esp_err_t lightbulb_set_hue(uint16_t hue)
{
    return lightbulb_set_hsv(hue, s_lb_obj->status.saturation, s_lb_obj->status.value);
}

esp_err_t lightbulb_set_saturation(uint8_t saturation)
{
    return lightbulb_set_hsv(s_lb_obj->status.hue, saturation, s_lb_obj->status.value);
}

esp_err_t lightbulb_set_value(uint8_t value)
{
    return lightbulb_set_hsv(s_lb_obj->status.hue, s_lb_obj->status.saturation, value);
}

esp_err_t lightbulb_set_cct(uint16_t cct)
{
    return lightbulb_set_cctb(cct, s_lb_obj->status.brightness);
}

esp_err_t lightbulb_set_brightness(uint8_t brightness)
{
    return lightbulb_set_cctb(s_lb_obj->status.cct_percentage, brightness);
}

esp_err_t lightbulb_set_hsv(uint16_t hue, uint8_t saturation, uint8_t value)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(hue <= 360, "hue out of range: %d", return ESP_ERR_INVALID_ARG, hue);
    LIGHTBULB_CHECK(saturation <= 100, "saturation out of range: %d", return ESP_ERR_INVALID_ARG, saturation);
    LIGHTBULB_CHECK(value <= 100, "value out of range: %d", return ESP_ERR_INVALID_ARG, value);
    LIGHTBULB_CHECK(IS_COLOR_CHANNEL_SELECTED(), "color channel output is disable", return ESP_ERR_INVALID_STATE);
    LB_MUTEX_TAKE(portMAX_DELAY);

    esp_err_t err = ESP_OK;

    if (IS_AUTO_STATUS_STORAGE_ENABLED()) {
        xTimerReset(s_lb_obj->storage_timer, 0);
    }

    if (IS_EFFECT_RUNNING() && IS_EFFECT_ALLOW_INTERRUPT()) {
        if (IS_EFFECT_TIMER_ACTIVE()) {
            xTimerStop(s_lb_obj->effect_timer, 0);
        }
        s_lb_obj->effect_flag.running = false;
    } else if (IS_EFFECT_RUNNING()) {
        ESP_LOGW(TAG, "Executing the effect, and does not allow interruption, skip writing, only save");
        goto SAVE_ONLY;
    }

    if (s_lb_obj->status.on || IS_AUTO_ON_FUNCTION_ENABLED()) {
        if (IS_LOW_POWER_FUNCTION_ENABLED()) {
            xTimerStop(s_lb_obj->power_timer, 0);
        }

        uint16_t color_value[5] = { 0 };
        uint16_t fade_time = CALCULATE_FADE_TIME();
        uint8_t channel_mask = get_channel_mask(s_lb_obj->cap.led_beads);
        uint8_t _value = value;

        // 1. calculate value
        ESP_LOGI(TAG, "set [h:%d s:%d v:%d]", hue, saturation, value);
        _value = process_color_value_limit(value);

        // 2. convert to r g b
        lightbulb_hsv2rgb(hue, saturation, _value, (uint8_t *)&color_value[0], (uint8_t *)&color_value[1], (uint8_t *)&color_value[2]);
        ESP_LOGI(TAG, "8 bit color conversion value [r:%d g:%d b:%d]", color_value[0], color_value[1], color_value[2]);

        // 3. according to power, re-calculate
        process_color_power_limit(s_lb_obj->power.color_max_power / 100.0, color_value[0], color_value[1], color_value[2], &color_value[0], &color_value[1], &color_value[2]);
        ESP_LOGI(TAG, "hal write value [r:%d g:%d b:%d], channel_mask:%d fade_ms:%d", color_value[0], color_value[1], color_value[2], channel_mask, fade_time);

        err = hal_set_channel_group(color_value, channel_mask, fade_time);
        LIGHTBULB_CHECK(err == ESP_OK, "set hal channel group fail", goto EXIT);

        s_lb_obj->status.on = true;
    } else {
        ESP_LOGW(TAG, "Skip writing, because power is not turned on or auto-on is disable");
    }

SAVE_ONLY:
    s_lb_obj->status.mode = WORK_COLOR;
    s_lb_obj->status.hue = hue;
    s_lb_obj->status.saturation = saturation;
    s_lb_obj->status.value = value;

    if (s_lb_obj->cap.sync_change_brightness_value && IS_WHITE_CHANNEL_SELECTED()) {
        s_lb_obj->status.brightness = value;
    }

EXIT:
    LB_MUTEX_GIVE();
    return err;
}

esp_err_t lightbulb_set_cctb(uint16_t cct, uint8_t brightness)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED(), "white channel output is disable", return ESP_ERR_INVALID_STATE);
    LIGHTBULB_CHECK(brightness <= 100, "brightness out of range: %d", return ESP_ERR_INVALID_ARG, brightness);
    LIGHTBULB_CHECK((cct >= s_lb_obj->cct_manager.kelvin_range.min && cct <= s_lb_obj->cct_manager.kelvin_range.max) || ((cct <= 100)), "CCT(%d) is not in valid range and is converted by an internal function", NULL, cct);

    if (cct > 100) {
        ESP_LOGW(TAG, "will convert kelvin to percentage, %dK -> %d%%", cct, s_lb_obj->cct_manager.kelvin_to_percentage(cct));
        cct = s_lb_obj->cct_manager.kelvin_to_percentage(cct);
    }
    if (s_lb_obj->cap.led_beads == LED_BEADS_1CH_C || s_lb_obj->cap.led_beads == LED_BEADS_4CH_RGBC) {
        if (cct != 100) {
            cct = 100;
            ESP_LOGW(TAG, "The current led beads can only be set to cold white");
        }
    } else if (s_lb_obj->cap.led_beads == LED_BEADS_1CH_W || s_lb_obj->cap.led_beads == LED_BEADS_4CH_RGBW) {
        if (cct != 0) {
            cct = 0;
            ESP_LOGW(TAG, "The current led beads can only be set to warm white");
        }
    }
    LIGHTBULB_CHECK(cct <= 100, "cct out of range: %d", return ESP_ERR_INVALID_ARG, cct);
    LB_MUTEX_TAKE(portMAX_DELAY);

    esp_err_t err = ESP_OK;

    if (IS_AUTO_STATUS_STORAGE_ENABLED()) {
        xTimerReset(s_lb_obj->storage_timer, 0);
    }

    if (IS_EFFECT_RUNNING() && IS_EFFECT_ALLOW_INTERRUPT()) {
        if (IS_EFFECT_TIMER_ACTIVE()) {
            xTimerStop(s_lb_obj->effect_timer, 0);
        }
        s_lb_obj->effect_flag.running = false;
    } else if (IS_EFFECT_RUNNING()) {
        ESP_LOGW(TAG, "Executing the effect, and does not allow interruption, skip writing, only save");
        goto SAVE_ONLY;
    }

    if (s_lb_obj->status.on || IS_AUTO_ON_FUNCTION_ENABLED()) {
        if (IS_LOW_POWER_FUNCTION_ENABLED()) {
            xTimerStop(s_lb_obj->power_timer, 0);
        }

        uint16_t white_value[5] = { 0 };
        uint16_t fade_time = CALCULATE_FADE_TIME();
        uint8_t channel_mask = get_channel_mask(s_lb_obj->cap.led_beads);
        uint8_t _brightness = brightness;

        ESP_LOGI(TAG, "set cct:%d, brightness:%d", cct, brightness);
        // 1. calculate brightness
        _brightness = process_white_brightness_limit(_brightness);

        // 2. convert to cold warm
        cct_and_brightness_convert_and_power_limit(s_lb_obj->cap.led_beads, s_lb_obj->power.white_max_power / 100.0, cct, _brightness, white_value);
        ESP_LOGI(TAG, "hal write value [r:%d g:%d b:%d c:%d w:%d], channel_mask:%d fade_ms:%d", white_value[0], white_value[1], white_value[2], white_value[3], white_value[4], channel_mask, fade_time);

        err = hal_set_channel_group(white_value, channel_mask, fade_time);
        LIGHTBULB_CHECK(err == ESP_OK, "set hal channel group fail", goto EXIT);

        s_lb_obj->status.on = true;
    } else {
        ESP_LOGW(TAG, "skip calling %s, just save this change.", __FUNCTION__);
    }

SAVE_ONLY:
    s_lb_obj->status.mode = WORK_WHITE;
    s_lb_obj->status.cct_percentage = cct;
    s_lb_obj->status.brightness = brightness;

    if (s_lb_obj->cap.sync_change_brightness_value && IS_COLOR_CHANNEL_SELECTED()) {
        s_lb_obj->status.value = brightness;
    }

EXIT:
    LB_MUTEX_GIVE();
    return err;
}

esp_err_t lightbulb_set_switch(bool status)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    esp_err_t err = ESP_OK;
    uint16_t fade_time = CALCULATE_FADE_TIME();

    if (!status) {
        LB_MUTEX_TAKE(portMAX_DELAY);
        if (IS_LOW_POWER_FUNCTION_ENABLED()) {
            xTimerReset(s_lb_obj->power_timer, 0);
        }
        if (IS_AUTO_STATUS_STORAGE_ENABLED()) {
            xTimerReset(s_lb_obj->storage_timer, 0);
        }
        if (IS_EFFECT_RUNNING() && IS_EFFECT_ALLOW_INTERRUPT()) {
            if (IS_EFFECT_TIMER_ACTIVE()) {
                xTimerStop(s_lb_obj->effect_timer, 0);
            }
            s_lb_obj->effect_flag.running = false;
        } else if (IS_EFFECT_RUNNING()) {
            ESP_LOGW(TAG, "Executing the effect, and does not allow interruption, skip writing, only save on/off status to off");
            s_lb_obj->status.on = false;
            LB_MUTEX_GIVE();
            return ESP_FAIL;
        }
        s_lb_obj->status.on = false;

        uint16_t value[5] = { 0 };
        uint8_t channel_mask = get_channel_mask(s_lb_obj->cap.led_beads);

        /**
         * When the hardware CCT is enabled, the CCT channel will not change and only the brightness channel will be turned off.
         *
         */
        if (IS_WHITE_OUTPUT_HARDWARE_MIXED()) {
            channel_mask &= (SELECT_COLOR_CHANNEL | SELECT_WARM_BRIGHTNESS_YELLOW_CHANNEL);
            err = hal_set_channel_group(value, channel_mask, fade_time);
        } else {
            err = hal_set_channel_group(value, channel_mask, fade_time);
        }

        ESP_LOGD(TAG, "hal write value [r:0 g:0 b:0 c:0 w:0], power off, channel_mask:%d fade_ms:%d", channel_mask, fade_time);
        LB_MUTEX_GIVE();
    } else {
        LB_MUTEX_TAKE(portMAX_DELAY);
        switch (s_lb_obj->status.mode) {
        case WORK_COLOR:
            s_lb_obj->status.on = true;
            s_lb_obj->status.value = (s_lb_obj->status.value) ? s_lb_obj->status.value : 100;
            err = lightbulb_set_hsv(s_lb_obj->status.hue, s_lb_obj->status.saturation, s_lb_obj->status.value);
            break;

        case WORK_WHITE:
            s_lb_obj->status.on = true;
            s_lb_obj->status.brightness = (s_lb_obj->status.brightness) ? s_lb_obj->status.brightness : 100;
            err = lightbulb_set_cctb(s_lb_obj->status.cct_percentage, s_lb_obj->status.brightness);
            break;

        default:
            ESP_LOGW(TAG, "This operation is not supported");
            break;
        }
        LB_MUTEX_GIVE();
    }

    return err;
}

int16_t lightbulb_get_hue(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(IS_COLOR_CHANNEL_SELECTED(), "color channel output is disable", return -1);

    LB_MUTEX_TAKE(portMAX_DELAY);
    int16_t result = s_lb_obj->status.hue;
    LB_MUTEX_GIVE();

    return result;
}

int8_t lightbulb_get_saturation(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return -1);
    LIGHTBULB_CHECK(IS_COLOR_CHANNEL_SELECTED(), "color channel output is disable", return -1);

    LB_MUTEX_TAKE(portMAX_DELAY);
    int8_t result = s_lb_obj->status.saturation;
    LB_MUTEX_GIVE();

    return result;
}

int8_t lightbulb_get_value(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return -1);
    LIGHTBULB_CHECK(IS_COLOR_CHANNEL_SELECTED(), "color channel output is disable", return -1);

    LB_MUTEX_TAKE(portMAX_DELAY);
    int8_t result = s_lb_obj->status.value;
    LB_MUTEX_GIVE();

    return result;
}

int8_t lightbulb_get_cct_percentage(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return -1);
    LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED(), "white channel output is disable", return -1);

    LB_MUTEX_TAKE(portMAX_DELAY);
    int8_t result = s_lb_obj->status.cct_percentage;
    LB_MUTEX_GIVE();

    return result;
}

int16_t lightbulb_get_cct_kelvin(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return -1);
    LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED(), "white channel output is disable", return -1);

    LB_MUTEX_TAKE(portMAX_DELAY);
    int16_t result = s_lb_obj->cct_manager.percentage_to_kelvin(s_lb_obj->status.cct_percentage);
    LB_MUTEX_GIVE();

    return result;
}

int8_t lightbulb_get_brightness(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return -1);
    LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED(), "white channel output is disable", return -1);

    LB_MUTEX_TAKE(portMAX_DELAY);
    int8_t result = s_lb_obj->status.brightness;
    LB_MUTEX_GIVE();

    return result;
}

esp_err_t lightbulb_get_all_detail(lightbulb_status_t *status)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(status, "status is null", return ESP_FAIL);
    LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED() || IS_COLOR_CHANNEL_SELECTED(), "white or color channel output is disable", return false);

    LB_MUTEX_TAKE(portMAX_DELAY);
    memcpy(status, &s_lb_obj->status, sizeof(lightbulb_status_t));
    LB_MUTEX_GIVE();

    return ESP_OK;
}

bool lightbulb_get_switch(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED() || IS_COLOR_CHANNEL_SELECTED(), "white or color channel output is disable", return false);

    LB_MUTEX_TAKE(portMAX_DELAY);
    bool result = s_lb_obj->status.on;
    LB_MUTEX_GIVE();

    return result;
}

esp_err_t lightbulb_set_fades_function(bool is_enable)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LB_MUTEX_TAKE(portMAX_DELAY);
    if (is_enable) {
        s_lb_obj->cap.enable_fade = true;
    } else {
        s_lb_obj->cap.enable_fade = false;
    }
    LB_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t lightbulb_set_storage_function(bool is_enable)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LB_MUTEX_TAKE(portMAX_DELAY);
    if (is_enable) {
        s_lb_obj->cap.enable_status_storage = true;
    } else {
        s_lb_obj->cap.enable_status_storage = false;
    }
    LB_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t lightbulb_set_fade_time(uint32_t fade_time_ms)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LB_MUTEX_TAKE(portMAX_DELAY);
    s_lb_obj->cap.fade_time_ms = fade_time_ms;
    LB_MUTEX_GIVE();

    return ESP_OK;
}

bool lightbulb_get_fades_function_status(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LB_MUTEX_TAKE(portMAX_DELAY);
    bool result = s_lb_obj->cap.enable_fade;
    LB_MUTEX_GIVE();

    return result;
}

lightbulb_works_mode_t lightbulb_get_mode(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LB_MUTEX_TAKE(portMAX_DELAY);
    lightbulb_works_mode_t result = WORK_INVALID;
    if (IS_COLOR_CHANNEL_SELECTED() || IS_WHITE_CHANNEL_SELECTED()) {
        result = s_lb_obj->status.mode;
    }
    LB_MUTEX_GIVE();

    return result;
}

esp_err_t lightbulb_update_status(lightbulb_status_t *new_status, bool trigger)
{
    LIGHTBULB_CHECK(new_status, "new_status is null", return ESP_FAIL);
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LB_MUTEX_TAKE(portMAX_DELAY);
    esp_err_t err = ESP_OK;

    memcpy(&s_lb_obj->status, new_status, sizeof(lightbulb_status_t));

    if (trigger) {
        err = lightbulb_set_switch(s_lb_obj->status.on);
    }
    LB_MUTEX_GIVE();

    return err;
}

esp_err_t lightbulb_basic_effect_start(lightbulb_effect_config_t *config)
{
    esp_err_t err = ESP_ERR_INVALID_STATE;
    LIGHTBULB_CHECK(config, "config is null", return ESP_FAIL);
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LB_MUTEX_TAKE(portMAX_DELAY);

    bool flag = ((config->effect_type == EFFECT_BREATH) ? true : false);

    if (IS_LOW_POWER_FUNCTION_ENABLED()) {
        xTimerStop(s_lb_obj->power_timer, 0);
    }

    if (IS_EFFECT_TIMER_ACTIVE()) {
        xTimerStop(s_lb_obj->effect_timer, 0);
    }

    if (config->mode == WORK_COLOR) {
        LIGHTBULB_CHECK(IS_COLOR_CHANNEL_SELECTED(), "color channel output is disable", goto EXIT);
        uint16_t color_value_max[5] = { 0 };
        uint16_t color_value_min[5] = { 0 };
        uint8_t channel_mask = get_channel_mask(s_lb_obj->cap.led_beads);
        err = ESP_OK;

        process_color_power_limit(s_lb_obj->power.color_max_power / 100.0, config->red * config->max_brightness / 100.0, config->green * config->max_brightness / 100.0, config->blue * config->max_brightness / 100.0, &color_value_max[0], &color_value_max[1], &color_value_max[2]);
        process_color_power_limit(s_lb_obj->power.color_max_power / 100.0, config->red * config->min_brightness / 100.0, config->green * config->min_brightness / 100.0, config->blue * config->min_brightness / 100.0, &color_value_min[0], &color_value_min[1], &color_value_min[2]);

        err |= hal_start_channel_group_action(color_value_min, color_value_max, channel_mask, config->effect_cycle_ms, flag);

    } else if (config->mode == WORK_WHITE) {
        LIGHTBULB_CHECK(IS_WHITE_CHANNEL_SELECTED(), "white channel output is disable", goto EXIT);
        if (config->cct > 100) {
            LIGHTBULB_CHECK(config->cct >= s_lb_obj->cct_manager.kelvin_range.min && config->cct <= s_lb_obj->cct_manager.kelvin_range.max, "cct kelvin out of range: %d", goto EXIT, config->cct);
            ESP_LOGW(TAG, "will convert kelvin to percentage, %dK -> %d%%", config->cct, s_lb_obj->cct_manager.kelvin_to_percentage(config->cct));
            config->cct = s_lb_obj->cct_manager.kelvin_to_percentage(config->cct);
        }
        uint16_t white_value_max[5] = { 0 };
        uint16_t white_value_min[5] = { 0 };
        uint8_t channel_mask = get_channel_mask(s_lb_obj->cap.led_beads);
        err = ESP_OK;

        cct_and_brightness_convert_and_power_limit(s_lb_obj->cap.led_beads, s_lb_obj->power.white_max_power / 100.0, config->cct, config->max_brightness, white_value_max);
        cct_and_brightness_convert_and_power_limit(s_lb_obj->cap.led_beads, s_lb_obj->power.white_max_power / 100.0, config->cct, config->min_brightness, white_value_min);

        err |= hal_start_channel_group_action(white_value_min, white_value_max, channel_mask, config->effect_cycle_ms, flag);
    } else {
        err = ESP_ERR_NOT_SUPPORTED;
    }

    if (err == ESP_OK) {
        s_lb_obj->effect_flag.allow_interrupt = !config->interrupt_forbidden;
        s_lb_obj->effect_flag.running = true;
        if (config->total_ms > 0) {
            if (!s_lb_obj->effect_timer) {
                s_lb_obj->effect_timer = xTimerCreate("effect_timer", pdMS_TO_TICKS(config->total_ms), false, NULL, timercb);
                LIGHTBULB_CHECK(s_lb_obj->effect_timer, "create timer fail", goto EXIT);
            } else {
                xTimerChangePeriod(s_lb_obj->effect_timer, pdMS_TO_TICKS(config->total_ms), 0);
            }
            if (config->user_cb) {
                vTimerSetTimerID(s_lb_obj->effect_timer, config->user_cb);
            } else {
                vTimerSetTimerID(s_lb_obj->effect_timer, NULL);
            }

            if (xTimerStart(s_lb_obj->effect_timer, 0) != pdPASS) {
                ESP_LOGW(TAG, "The auto-stop timer start fail, the effect will continue executing, but will not stop automatically.");
                err = ESP_FAIL;
            } else {
                ESP_LOGI(TAG, "The auto-stop timer will trigger after %d ms.", config->total_ms);
            }
        } else {
            ESP_LOGI(TAG, "The auto-stop timer is not running, the effect will keep running.");
        }
        ESP_LOGI(TAG, "effect config: \r\n"
                 "\teffect type: %d\r\n"
                 "\tmode: %d\r\n"
                 "\tred:%d\r\n"
                 "\tgreen:%d\r\n"
                 "\tblue:%d\r\n"
                 "\tcct:%d\r\n"
                 "\tmin_brightness:%d\r\n"
                 "\tmax_brightness:%d\r\n"
                 "\teffect_cycle_ms:%d\r\n"
                 "\ttotal_ms:%d\r\n"
                 "\tinterrupt_forbidden:%d", config->effect_type, config->mode, config->red, config->green, config->blue,
                 config->cct, config->min_brightness, config->max_brightness, config->effect_cycle_ms, config->total_ms, config->interrupt_forbidden);
        ESP_LOGI(TAG, "This effect will %s to be interrupted", s_lb_obj->effect_flag.allow_interrupt ? "allow" : "not be allowed");
    }

EXIT:
    LB_MUTEX_GIVE();
    return err;
}

esp_err_t lightbulb_basic_effect_stop(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LB_MUTEX_TAKE(portMAX_DELAY);
    esp_err_t err = ESP_FAIL;
    uint8_t channel_mask = 0;
    if (IS_COLOR_CHANNEL_SELECTED()) {
        channel_mask |= (SELECT_COLOR_CHANNEL);
    }

    if (IS_WHITE_CHANNEL_SELECTED()) {
        channel_mask |= (SELECT_WHITE_CHANNEL);
    }
    err = hal_stop_channel_action(channel_mask);
    ESP_LOGI(TAG, "Stop effect");

    s_lb_obj->effect_flag.running = false;
    LB_MUTEX_GIVE();

    return err;
}

esp_err_t lightbulb_basic_effect_stop_and_restore(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LB_MUTEX_TAKE(portMAX_DELAY);
    bool is_on = s_lb_obj->status.on;
    s_lb_obj->effect_flag.running = false;
    LB_MUTEX_GIVE();

    return lightbulb_set_switch(is_on);
}
