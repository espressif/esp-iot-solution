// Copyright 2020-2022 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <math.h>
#include <string.h>

#include <nvs_flash.h>

#include "hal_driver.h"
#include "lightbulb.h"

static const char *TAG = "lightbulb";

#if CONFIG_ENABLE_LIGHTBULB_DEBUG_LOG_OUTPUT
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

/**
 * @brief Resource Access Control
 *
 */
#define LIGHTBULB_MUTEX_TAKE(delay_ms)                  (xSemaphoreTake(s_lb_obj->mutex, delay_ms))
#define LIGHTBULB_MUTEX_GIVE()                          (xSemaphoreGive(s_lb_obj->mutex))

/**
 * @brief Lightbulb function check
 *
 */
#define CHECK_COLOR_CHANNEL_IS_SELECT()                 (s_lb_obj->cap.mode_mask & BIT(1) ? true : false)
#define CHECK_WHITE_CHANNEL_IS_SELECT()                 (s_lb_obj->cap.mode_mask & BIT(2) ? true : false)
#define CHECK_LOW_POWER_FUNC_IS_ENABLE()                ((s_lb_obj->cap.enable_lowpower && s_lb_obj->power_timer) ? true : false)
#define CHECK_AUTO_STATUS_STORAGE_FUNC_IS_ENABLE()      ((s_lb_obj->cap.enable_status_storage && s_lb_obj->storage_timer) ? true : false)
#define CHECK_WHITE_OUTPUT_REQ_MIXED()                  (s_lb_obj->cap.enable_mix_cct)

/**
 * @brief Lightbulb fade time calculate
 *
 */
#define CALCULATE_FADE_TIME()                           (s_lb_obj->cap.enable_fades ? s_lb_obj->cap.fades_ms : 0)


/**
 * @brief Key-value pair for nvs
 *
 */
#define LIGHTBULB_STORAGE_KEY                           ("lb_status")
#define LIGHTBULB_NAMESPACE                             ("lightbulb")

/**
 * @brief some default configuration
 *
 */
#define MAX_CCT_K                                       (7000)
#define MIN_CCT_K                                       (2200)
#define MAX_FADE_MS                                     (3000)
#define MIN_FADE_MS                                     (100)

typedef struct {
    lightbulb_status_t status;
    lightbulb_capability_t cap;
    lightbulb_power_limit_t power;
    lightbulb_cct_limit_t cct;
    TimerHandle_t power_timer;
    TimerHandle_t storage_timer;
    SemaphoreHandle_t mutex;
} lightbulb_obj_t;

static lightbulb_obj_t *s_lb_obj = NULL;

esp_err_t lightbulb_status_set_to_nvs(const lightbulb_status_t *value)
{
    LIGHTBULB_CHECK(value, "value is null", return ESP_ERR_INVALID_ARG);

    esp_err_t err = ESP_OK;
    nvs_handle handle = 0;

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
    nvs_handle handle = 0;
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
    nvs_handle handle = 0;

    err = nvs_open(LIGHTBULB_NAMESPACE, NVS_READWRITE, &handle);
    LIGHTBULB_CHECK(err == ESP_OK, "nvs open fail, reason code: %d", return err, err);

    err = nvs_erase_key(handle, LIGHTBULB_STORAGE_KEY);
    LIGHTBULB_CHECK(err == ESP_OK, "nvs erase fail, reason code: %d", return err, err);

    err = nvs_commit(handle);
    LIGHTBULB_CHECK(err == ESP_OK, "nvs commit fail, reason code: %d", return err, err);

    nvs_close(handle);

    return ESP_OK;
}

/**
 * @brief Convert CCT percentage to Kelvin
 * @note
 *      If you use the default maximum and minimum kelvin:
 *
 *      input       output
 *      0           2200k
 *      50          4600k
 *      100         7200k
 *
 * @param percentage range: 0-100
 * @return uint16_t
 *
 */
static uint16_t percentage_convert_to_kelvin(uint16_t percentage)
{
    float _percentage = (float)percentage / 100;
    uint16_t _kelvin = (_percentage * (s_lb_obj->cct.max_kelvin - s_lb_obj->cct.min_kelvin) + s_lb_obj->cct.max_kelvin);

    /* Convert to the nearest integer */
    _kelvin = (_kelvin / 100) * 100;
    return _kelvin;
}

/**
 * @brief Convert Kelvin to CCT percentage
 *  @note
 *      If you use the default maximum and minimum kelvin:
 *
 *      input       output
 *      2200k       0
 *      4600k       50
 *      7200k       100
 *
 * @param kelvin
 * @return uint8_t
 */
static uint8_t kelvin_convert_to_percentage(uint16_t kelvin)
{
    if (kelvin > s_lb_obj->cct.max_kelvin) {
        kelvin = s_lb_obj->cct.max_kelvin;
    }
    if (kelvin < s_lb_obj->cct.min_kelvin) {
        kelvin = kelvin < s_lb_obj->cct.min_kelvin;
    }

    return 100 * ((float)(kelvin - s_lb_obj->cct.min_kelvin) / (kelvin > s_lb_obj->cct.max_kelvin - s_lb_obj->cct.max_kelvin));
}

/**
 * @brief Convert CCT and brightness to cold and warm
 * @attention White power was recalculated at the same time.
 * @note
 *
 *      input       output(white_max_power = 100)   output(white_max_power = 200)
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
 * @param cct range: 0-100
 * @param brightness range: 0-100
 * @param out_cold range: 0-255
 * @param out_warm range: 0-255
 *
 */
static void cct_and_brightness_convert_to_cold_and_warm(uint8_t cct, uint8_t brightness, uint16_t *out_cold, uint16_t *out_warm)
{
    // 1. Calculate the percentage of cold and warm
    float _warm = (100 - cct) / 100.0;
    float _cold = cct / 100.0;
    ESP_LOGD(TAG, "_warm:%f _cold:%f", _warm, _cold);

    // 2. According to the maximum power assign percentage
    float _warm_finally = _warm * 1.0;
    float _cold_finally = _cold * 1.0;
    float x = s_lb_obj->power.white_max_power / 100.0;
    float max_power = x * (255.0);
    ESP_LOGD(TAG, "x:%f, max_power:%f", x, max_power);
    for (int i = 0; i <= max_power; i++) {
        if (_warm_finally >= 255 || _cold_finally >= 255) {
            break;
        }
        _warm_finally = _warm * i;
        _cold_finally = _cold * i;
    }
    ESP_LOGD(TAG, "_warm_finally:%f _cold_finally:%f", _warm_finally, _cold_finally);

    // 3. Reassign based on brightness
    *out_warm = _warm_finally * (brightness / 100.0);
    *out_cold = _cold_finally * (brightness / 100.0);

    ESP_LOGD(TAG, "software cct: [input: %d %d], [output:%d %d]", cct, brightness, *out_cold, *out_warm);
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
static void process_color_power_limit(uint8_t r, uint8_t g, uint8_t b, uint16_t *out_r, uint16_t *out_g, uint16_t *out_b)
{
    if (r == 0 && g == 0 && b == 0) {
        *out_r = 0;
        *out_g = 0;
        *out_b = 0;
        return;
    }

    uint16_t total;
    uint16_t gamma_r;
    uint16_t gamma_g;
    uint16_t gamma_b;

    // 1. First we need to find the mapped value in the gamma table
    hal_get_driver_feature(QUERY_COLOR_BIT_DEPTH, &total);
    hal_get_gamma_value(r, g, b, &gamma_r, &gamma_g, &gamma_b);

    // 2. Second, we need to calculate the color distribution ratio of the rgb channel, and their ratio determines the final rendered color.
    float rgb_ratio_r = gamma_r * 1.0 / (total);
    float rgb_ratio_g = gamma_g * 1.0 / (total);
    float rgb_ratio_b = gamma_b * 1.0 / (total);
    ESP_LOGD(TAG, "rgb_ratio_r:%f rgb_ratio_g:%f rgb_ratio_b:%f total:%d", rgb_ratio_r, rgb_ratio_g, rgb_ratio_b, total);

    // 3. Next, we need to calculate the grayscale ratio, which provides a baseline value for subsequent power limiting.
    float grayscale_ratio_r = gamma_r / ((gamma_r + gamma_g + gamma_b) * 1.0);
    float grayscale_ratio_g = gamma_g / ((gamma_r + gamma_g + gamma_b) * 1.0);
    float grayscale_ratio_b = gamma_b / ((gamma_r + gamma_g + gamma_b) * 1.0);
    float baseline = MAX(grayscale_ratio_r, grayscale_ratio_g);
    baseline = MAX(baseline, grayscale_ratio_b);
    ESP_LOGD(TAG, "grayscale_ratio_r:%f grayscale_ratio_g:%f grayscale_ratio_b:%f baseline:%f", grayscale_ratio_r, grayscale_ratio_g, grayscale_ratio_b, baseline);

    // 4. Finally, we recalculate the final value based on the maximum allowed output power.
    float x = s_lb_obj->power.color_max_power / 100.0;
    float max_power = x * (total);
    float max_power_baseline = MIN(total, baseline * max_power);
    ESP_LOGD(TAG, "x:%f, max_power:%f max_power_baseline:%f", x, max_power, max_power_baseline);
    *out_r = max_power_baseline * rgb_ratio_r;
    *out_g = max_power_baseline * rgb_ratio_g;
    *out_b = max_power_baseline * rgb_ratio_b;

    ESP_LOGD(TAG, "setp_2 [input: %d %d %d], [output:%d %d %d]", r, g, b, *out_r, *out_g, *out_b);
}

/**
 * @brief Recalculate white power
 * @attention Please refer to `cct_and_brightness_convert_to_cold_and_warm`
 */
static void process_white_power_limit(uint8_t cold, uint8_t warm, uint8_t *out_cold, uint8_t *out_warm)
{
    *out_warm = (uint8_t)warm;
    *out_cold = (uint8_t)cold;
    return;
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
    }
}

static uint8_t get_channel_mask(lightbulb_works_mode_t cur_mode)
{
    uint8_t channel_mask = 0;
    uint8_t allow_all_output = false;
    hal_get_driver_feature(QUERY_IS_ALLOW_ALL_OUTPUT, &allow_all_output);
    if (cur_mode == WORK_COLOR) {
        channel_mask = SELECT_COLOR_CHANNEL;
        /* If the driver cannot output all channels at the same time, it needs to be unselected in the App layer, such as driver IC: SM2135 */
        if (CHECK_WHITE_CHANNEL_IS_SELECT() && allow_all_output == true) {
            channel_mask |= SELECT_WHITE_CHANNEL;
        }
    } else {
        channel_mask = SELECT_WHITE_CHANNEL;
        if (CHECK_COLOR_CHANNEL_IS_SELECT() && allow_all_output == true) {
            channel_mask |= SELECT_COLOR_CHANNEL;
        }
    }
    return channel_mask;
}

static void print_func(void)
{
    char *name;

    hal_get_driver_feature(QUERY_DRIVER_NAME, &name);
    ESP_LOGI(TAG, "---------------------------------------------------------------------");
    ESP_LOGI(TAG, "driver name: %s", name);
    ESP_LOGI(TAG, "low power control: %s", s_lb_obj->cap.enable_lowpower ? "enable" : "disable");
    ESP_LOGI(TAG, "status storage: %s", s_lb_obj->cap.enable_status_storage ? "enable" : "disable");
    ESP_LOGI(TAG, "status storage delay %d ms", s_lb_obj->cap.enable_status_storage == true ? s_lb_obj->cap.storage_delay_ms : 0);
    ESP_LOGI(TAG, "fade: %s", s_lb_obj->cap.enable_fades ? "enable" : "disable");
    ESP_LOGI(TAG, "fade %d ms", s_lb_obj->cap.enable_fades == true ? s_lb_obj->cap.fades_ms : 0);
    ESP_LOGI(TAG, "mode: %d", s_lb_obj->cap.mode_mask);
    if (CHECK_WHITE_CHANNEL_IS_SELECT()) {
        ESP_LOGI(TAG, "     white mode: enable");
    }
    if (CHECK_COLOR_CHANNEL_IS_SELECT()) {
        ESP_LOGI(TAG, "     color mode: enable");
    }
    ESP_LOGI(TAG, "mix cct: %s", s_lb_obj->cap.enable_mix_cct ? "enable" : "disable");
    ESP_LOGI(TAG, "sync change: %s", s_lb_obj->cap.sync_change_brightness_value ? "enable" : "disable");
    ESP_LOGI(TAG, "power limit param: ");
    ESP_LOGI(TAG, "     white max brightness: %d", s_lb_obj->power.white_max_brightness);
    ESP_LOGI(TAG, "     white min brightness: %d", s_lb_obj->power.white_min_brightness);
    ESP_LOGI(TAG, "     white max power: %d", s_lb_obj->power.white_max_power);
    ESP_LOGI(TAG, "     color max brightness: %d", s_lb_obj->power.color_max_value);
    ESP_LOGI(TAG, "     color min brightness: %d", s_lb_obj->power.color_min_value);
    ESP_LOGI(TAG, "     color max power: %d", s_lb_obj->power.color_max_power);
    if (CHECK_WHITE_CHANNEL_IS_SELECT()) {
        ESP_LOGI(TAG, "cct limit param: ");
        ESP_LOGI(TAG, "     max cct: %d", s_lb_obj->cct.max_kelvin);
        ESP_LOGI(TAG, "     min cct: %d\r\n", s_lb_obj->cct.min_kelvin);
    }
    ESP_LOGI(TAG, "hue: %d, saturation: %d, value: %d", s_lb_obj->status.hue, s_lb_obj->status.saturation, s_lb_obj->status.value);
    ESP_LOGI(TAG, "cct: %d, brightness: %d", s_lb_obj->status.cct, s_lb_obj->status.brightness);
    ESP_LOGI(TAG, "select works mode: %s, power status: %d", s_lb_obj->status.mode == WORK_COLOR ? "color" : "white", s_lb_obj->status.on);
    ESP_LOGI(TAG, "---------------------------------------------------------------------");
}

esp_err_t lightbulb_init(lightbulb_config_t *config)
{
    esp_err_t err = ESP_FAIL;
    LIGHTBULB_CHECK(config, "Config is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(config->type > DRIVER_SELECT_INVALID, "Invalid driver select", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(!s_lb_obj, "Already init done", return ESP_ERR_INVALID_STATE);

    s_lb_obj = calloc(1, sizeof(lightbulb_obj_t));
    LIGHTBULB_CHECK(s_lb_obj, "calloc fail", goto EXIT);

    s_lb_obj->mutex = xSemaphoreCreateMutex();
    LIGHTBULB_CHECK(s_lb_obj->mutex, "mutex create fail", goto EXIT);

    // hal configuration
    void *driver_conf = NULL;
#ifdef CONFIG_ENABLE_PWM_DRIVER
    if (config->type == DRIVER_ESP_PWM) {
        driver_conf = (void *) & (config->driver_conf.pwm);
    }
#endif
#ifdef CONFIG_ENABLE_SM2135E_DRIVER
    if (config->type == DRIVER_SM2135E) {
        driver_conf = (void *) & (config->driver_conf.sm2135e);
    }
#endif
#ifdef CONFIG_ENABLE_SM2135EH_DRIVER
    if (config->type == DRIVER_SM2135EH) {
        driver_conf = (void *) & (config->driver_conf.sm2135eh);
    }
#endif
#ifdef CONFIG_ENABLE_BP5758D_DRIVER
    if (config->type == DRIVER_BP5758D) {
        driver_conf = (void *) & (config->driver_conf.bp5758d);
    }
#endif
#ifdef CONFIG_ENABLE_SM2x35EGH_DRIVER
    if (config->type == DRIVER_SM2235EGH) {
        driver_conf = (void *) & (config->driver_conf.sm2235egh);
    }
    if (config->type == DRIVER_SM2335EGH) {
        driver_conf = (void *) & (config->driver_conf.sm2335egh);
    }
#endif
#ifdef CONFIG_ENABLE_WS2812_DRIVER
    if (config->type == DRIVER_WS2812) {
        driver_conf = (void *) & (config->driver_conf.ws2812);
        if (config->capability.mode_mask != COLOR_MODE) {
            config->capability.mode_mask = COLOR_MODE;
            ESP_LOGW(TAG, "ws2812 only supports color mode, rewrite the mode_mask variable to color.");
        }
    }
#endif
    hal_config_t hal_conf = {
        .type = config->type,
        .driver_data = driver_conf,
    };
    err = hal_output_init(&hal_conf, config->gamma_conf, (void *)&config->init_status.mode);
    LIGHTBULB_CHECK(err == ESP_OK, "hal init fail", goto EXIT);

    // Load init status
    memcpy(&s_lb_obj->status, &config->init_status, sizeof(lightbulb_status_t));
    memcpy(&s_lb_obj->cap, &config->capability, sizeof(lightbulb_capability_t));

    // Channel registration
    if (CHECK_COLOR_CHANNEL_IS_SELECT()) {
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
    if (CHECK_WHITE_CHANNEL_IS_SELECT()) {
        if (config->type == DRIVER_ESP_PWM) {
            hal_regist_channel(CHANNEL_ID_COLD_CCT_WHITE, config->io_conf.pwm_io.cold_cct);
            hal_regist_channel(CHANNEL_ID_WARM_BRIGHTNESS_YELLOW, config->io_conf.pwm_io.warm_brightness);
        } else {
            hal_regist_channel(CHANNEL_ID_COLD_CCT_WHITE, config->io_conf.iic_io.cold_white);
            hal_regist_channel(CHANNEL_ID_WARM_BRIGHTNESS_YELLOW, config->io_conf.iic_io.warm_yellow);
        }
    }

    // monitor cb
    if (config->capability.monitor_cb) {
        hal_register_monitor_cb(config->capability.monitor_cb);
    }

    // Fade check
    if (s_lb_obj->cap.enable_fades) {
        s_lb_obj->cap.fades_ms = MIN(MAX_FADE_MS, s_lb_obj->cap.fades_ms);
        s_lb_obj->cap.fades_ms = MAX(MIN_FADE_MS, s_lb_obj->cap.fades_ms);
    }

    // Low power check
    if (config->capability.enable_lowpower) {
        /* Make sure the fade is done and the flash operation is done, then enable light sleep */
        uint32_t time_ms = MAX(s_lb_obj->cap.fades_ms, s_lb_obj->cap.storage_delay_ms) + 1000;
        s_lb_obj->power_timer = xTimerCreate("power_timer", pdMS_TO_TICKS(time_ms), false, NULL, timercb);
        LIGHTBULB_CHECK(s_lb_obj->power_timer != NULL, "create timer fail", goto EXIT);
    }

    // Storage check
    if (config->capability.enable_status_storage) {
        s_lb_obj->cap.storage_delay_ms = MAX(s_lb_obj->cap.fades_ms, s_lb_obj->cap.storage_delay_ms) + 1000;
        s_lb_obj->storage_timer = xTimerCreate("storage_timer", pdMS_TO_TICKS(s_lb_obj->cap.storage_delay_ms), false, NULL, timercb);
        LIGHTBULB_CHECK(s_lb_obj->storage_timer != NULL, "create timer fail", goto EXIT);
    }

    // Power Limit check
    if (config->external_limit) {
        memcpy(&s_lb_obj->power, config->external_limit, sizeof(lightbulb_power_limit_t));
    } else {
        s_lb_obj->power.color_max_value = 100;
        s_lb_obj->power.white_max_brightness = 100;
        s_lb_obj->power.color_min_value = 10;
        s_lb_obj->power.white_min_brightness = 10;
        s_lb_obj->power.color_max_power = 100;
        s_lb_obj->power.white_max_power = 100;
    }

    // cct Limit check
    if (CHECK_WHITE_CHANNEL_IS_SELECT()) {
        if (config->cct_limit) {
            memcpy(&s_lb_obj->cct, config->cct_limit, sizeof(lightbulb_cct_limit_t));
            LIGHTBULB_CHECK(s_lb_obj->cct.max_kelvin > s_lb_obj->cct.min_kelvin, "CCT data error", goto EXIT)
        } else {
            s_lb_obj->cct.max_kelvin = MAX_CCT_K;
            s_lb_obj->cct.min_kelvin = MIN_CCT_K;
        }
    }

    print_func();

    // Output status according to init parameter
    if (s_lb_obj->status.on) {
        /* Fade can cause perceptible state changes when the system restarts abnormally, so we need to temporarily disable fade. */
        if (s_lb_obj->cap.enable_fades) {
            lightbulb_set_fades_function(false);
            lightbulb_set_switch(true);
            lightbulb_set_fades_function(true);
        } else {
            lightbulb_set_switch(true);
        }
    }

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
    free(s_lb_obj);
    s_lb_obj = NULL;

    return hal_output_deinit();
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
    LIGHTBULB_CHECK(percentage, "percentage is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(kelvin > s_lb_obj->cct.max_kelvin, "kelvin out of max range", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(kelvin < s_lb_obj->cct.min_kelvin, "kelvin out of min range", return ESP_ERR_INVALID_ARG);

    *percentage = kelvin_convert_to_percentage(kelvin);

    return ESP_OK;
}

esp_err_t lightbulb_percentage2kelvin(uint8_t percentage, uint16_t *kelvin)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(kelvin, "kelvin is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(percentage > 100, "percentage out of range", return ESP_ERR_INVALID_ARG);

    *kelvin = percentage_convert_to_kelvin(percentage);

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
    return lightbulb_set_cctb(s_lb_obj->status.cct, brightness);
}

esp_err_t lightbulb_set_hsv(uint16_t hue, uint8_t saturation, uint8_t value)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(hue <= 360, "hue out of range: %d", return ESP_ERR_INVALID_ARG, hue);
    LIGHTBULB_CHECK(saturation <= 100, "saturation out of range: %d", return ESP_ERR_INVALID_ARG, saturation);
    LIGHTBULB_CHECK(value <= 100, "value out of range: %d", return ESP_ERR_INVALID_ARG, value);
    LIGHTBULB_CHECK(CHECK_COLOR_CHANNEL_IS_SELECT(), "color channel output is disable", return ESP_ERR_INVALID_STATE);
    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);

    if (CHECK_LOW_POWER_FUNC_IS_ENABLE()) {
        xTimerStop(s_lb_obj->power_timer, 0);
    }

    if (CHECK_AUTO_STATUS_STORAGE_FUNC_IS_ENABLE()) {
        xTimerReset(s_lb_obj->storage_timer, 0);
    }

    esp_err_t err = ESP_OK;
    uint16_t color_value[5] = { 0 };
    uint32_t fade_time = CALCULATE_FADE_TIME();
    uint8_t channel_mask = get_channel_mask(WORK_COLOR);
    uint8_t _value = value;

    // 1. calculate value
    ESP_LOGI(TAG, "set [h:%d s:%d v:%d]", hue, saturation, value);
    _value = process_color_value_limit(value);

    // 2. convert to r g b
    lightbulb_hsv2rgb(hue, saturation, _value, (uint8_t *)&color_value[0], (uint8_t *)&color_value[1], (uint8_t *)&color_value[2]);
    ESP_LOGI(TAG, "8 bit color conversion value [r:%d g:%d b:%d]", color_value[0], color_value[1], color_value[2]);

    // 3. according to power, re-calculate
    process_color_power_limit(color_value[0], color_value[1], color_value[2], &color_value[0], &color_value[1], &color_value[2]);
    ESP_LOGI(TAG, "hal write value [r:%d g:%d b:%d], channel_mask:%d fade_ms:%d", color_value[0], color_value[1], color_value[2], channel_mask, fade_time);

    err = hal_set_channel_group(color_value, channel_mask, fade_time);
    LIGHTBULB_CHECK(err == ESP_OK, "set hal channel group fail", goto EXIT);

    s_lb_obj->status.mode = WORK_COLOR;
    s_lb_obj->status.on = true;
    s_lb_obj->status.hue = hue;
    s_lb_obj->status.saturation = saturation;
    s_lb_obj->status.value = value;

    if (s_lb_obj->cap.sync_change_brightness_value && CHECK_WHITE_CHANNEL_IS_SELECT()) {
        s_lb_obj->status.brightness = value;
    }

EXIT:
    LIGHTBULB_MUTEX_GIVE();
    return err;
}

esp_err_t lightbulb_set_cctb(uint8_t cct, uint8_t brightness)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(brightness <= 100, "brightness out of range: %d", return ESP_ERR_INVALID_ARG, brightness);
    if (cct > 100) {
        ESP_LOGW(TAG, "Current cct percentage value (%d) exceeds the max (100), convert to %d", cct, kelvin_convert_to_percentage(cct));
        cct = kelvin_convert_to_percentage(cct);
    }
    LIGHTBULB_CHECK(cct <= 100, "cct out of range: %d", return ESP_ERR_INVALID_ARG, cct);
    LIGHTBULB_CHECK(CHECK_WHITE_CHANNEL_IS_SELECT(), "white channel output is disable", return ESP_ERR_INVALID_STATE);
    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);

    if (CHECK_LOW_POWER_FUNC_IS_ENABLE()) {
        xTimerStop(s_lb_obj->power_timer, 0);
    }

    if (CHECK_AUTO_STATUS_STORAGE_FUNC_IS_ENABLE()) {
        xTimerReset(s_lb_obj->storage_timer, 0);
    }

    esp_err_t err = ESP_OK;
    uint16_t white_value[5] = { 0 };
    uint32_t fade_time = CALCULATE_FADE_TIME();
    uint8_t channel_mask = get_channel_mask(WORK_WHITE);
    uint8_t _brightness = brightness;

    ESP_LOGI(TAG, "set cct:%d brightness:%d", cct, brightness);
    // 1. calculate brightness
    ESP_LOGD(TAG, "input cct:%d brightness:%d", cct, brightness);
    _brightness = process_white_brightness_limit(_brightness);
    ESP_LOGD(TAG, "setp_1 output cct:%d brightness:%d", cct, _brightness);

    // 2. convert to cold warm
    if (CHECK_WHITE_OUTPUT_REQ_MIXED()) {
        cct_and_brightness_convert_to_cold_and_warm(cct, _brightness, &white_value[3], &white_value[4]);
        ESP_LOGI(TAG, "convert cold:%d warm:%d", white_value[3], white_value[4]);
        process_white_power_limit(white_value[3], white_value[4], (uint8_t *)&white_value[3], (uint8_t *)&white_value[4]);
    } else {
        white_value[3] = cct * 255 / 100;
        white_value[4] = _brightness * 255 / 100;
        ESP_LOGI(TAG, "convert cct:%d brightness:%d", white_value[3], white_value[4]);
    }
    ESP_LOGI(TAG, "hal write value [white1:%d white2:%d], channel_mask:%d fade_ms:%d", white_value[3], white_value[4], channel_mask, fade_time);

    err = hal_set_channel_group(white_value, channel_mask, fade_time);
    LIGHTBULB_CHECK(err == ESP_OK, "set hal channel group fail", goto EXIT);

    s_lb_obj->status.mode = WORK_WHITE;
    s_lb_obj->status.on = true;
    s_lb_obj->status.cct = cct;
    s_lb_obj->status.brightness = brightness;

    if (s_lb_obj->cap.sync_change_brightness_value && CHECK_COLOR_CHANNEL_IS_SELECT()) {
        s_lb_obj->status.value = brightness;
    }

EXIT:
    LIGHTBULB_MUTEX_GIVE();
    return err;
}

esp_err_t lightbulb_set_switch(bool status)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    esp_err_t err = ESP_OK;

    s_lb_obj->status.on = status;
    uint32_t fade_time = CALCULATE_FADE_TIME();

    if (!s_lb_obj->status.on) {
        LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
        if (CHECK_LOW_POWER_FUNC_IS_ENABLE()) {
            xTimerReset(s_lb_obj->power_timer, 0);
        }
        if (CHECK_AUTO_STATUS_STORAGE_FUNC_IS_ENABLE()) {
            xTimerReset(s_lb_obj->storage_timer, 0);
        }
        if (CHECK_COLOR_CHANNEL_IS_SELECT() && (s_lb_obj->status.mode == WORK_COLOR)) {
            uint16_t value[5] = { 0 };
            uint8_t channel_mask = get_channel_mask(WORK_COLOR);
            hal_set_channel_group(value, channel_mask, fade_time);
        }
        if (CHECK_WHITE_CHANNEL_IS_SELECT() && (s_lb_obj->status.mode == WORK_WHITE)) {
            if (CHECK_WHITE_OUTPUT_REQ_MIXED()) {
                uint16_t value[5] = { 0 };
                uint8_t channel_mask = get_channel_mask(WORK_WHITE);
                hal_set_channel_group(value, channel_mask, fade_time);
            } else {
                err = hal_set_channel(CHANNEL_ID_WARM_BRIGHTNESS_YELLOW, 0, fade_time);
            }
        }
        LIGHTBULB_MUTEX_GIVE();
    } else {
        switch (s_lb_obj->status.mode) {
        case WORK_COLOR:
            LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
            s_lb_obj->status.value = (s_lb_obj->status.value) ? s_lb_obj->status.value : 100;
            LIGHTBULB_MUTEX_GIVE();
            err = lightbulb_set_hsv(s_lb_obj->status.hue, s_lb_obj->status.saturation, s_lb_obj->status.value);
            break;

        case WORK_WHITE:
            LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
            s_lb_obj->status.brightness = (s_lb_obj->status.brightness) ? s_lb_obj->status.brightness : 100;
            LIGHTBULB_MUTEX_GIVE();
            err = lightbulb_set_cctb(s_lb_obj->status.cct, s_lb_obj->status.brightness);
            break;

        default:
            ESP_LOGW(TAG, "This operation is not supported");
            break;
        }
    }

    return err;
}

int16_t lightbulb_get_hue(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(CHECK_COLOR_CHANNEL_IS_SELECT(), "color channel output is disable", return -1);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    int16_t result = s_lb_obj->status.hue;
    LIGHTBULB_MUTEX_GIVE();

    return result;
}

int8_t lightbulb_get_saturation(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return -1);
    LIGHTBULB_CHECK(CHECK_COLOR_CHANNEL_IS_SELECT(), "color channel output is disable", return -1);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    int8_t result = s_lb_obj->status.saturation;
    LIGHTBULB_MUTEX_GIVE();

    return result;
}

int8_t lightbulb_get_value(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return -1);
    LIGHTBULB_CHECK(CHECK_COLOR_CHANNEL_IS_SELECT(), "color channel output is disable", return -1);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    int8_t result = s_lb_obj->status.value;
    LIGHTBULB_MUTEX_GIVE();

    return result;
}

int8_t lightbulb_get_cct_percentage(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return -1);
    LIGHTBULB_CHECK(CHECK_WHITE_CHANNEL_IS_SELECT(), "white channel output is disable", return -1);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    int8_t result = s_lb_obj->status.cct;
    LIGHTBULB_MUTEX_GIVE();

    return result;
}

int8_t lightbulb_get_brightness(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return -1);
    LIGHTBULB_CHECK(CHECK_WHITE_CHANNEL_IS_SELECT(), "white channel output is disable", return -1);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    int8_t result = s_lb_obj->status.brightness;
    LIGHTBULB_MUTEX_GIVE();

    return result;
}

esp_err_t lightbulb_get_hsv(uint16_t *hue, uint8_t *saturation, uint8_t *value)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(CHECK_COLOR_CHANNEL_IS_SELECT(), "color channel output is disable", return -1);
    LIGHTBULB_CHECK(hue, "h is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(saturation, "s is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(value, "v is null", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    *hue = s_lb_obj->status.hue;
    *saturation = s_lb_obj->status.saturation;
    *value = s_lb_obj->status.value;
    LIGHTBULB_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t lightbulb_get_ctb(uint8_t *cct, uint8_t *brightness)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(CHECK_WHITE_CHANNEL_IS_SELECT(), "white channel output is disable", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(cct, "cct is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(brightness, "brightness is null", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    *cct = s_lb_obj->status.cct;
    *brightness = s_lb_obj->status.brightness;
    LIGHTBULB_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t lightbulb_get_all_detail(lightbulb_status_t *status)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(status, "status is null", return ESP_FAIL);
    LIGHTBULB_CHECK(CHECK_WHITE_CHANNEL_IS_SELECT() || CHECK_COLOR_CHANNEL_IS_SELECT(), "white or color channel output is disable", return false);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    memcpy(status, &s_lb_obj->status, sizeof(lightbulb_status_t));
    LIGHTBULB_MUTEX_GIVE();

    return ESP_OK;
}

bool lightbulb_get_switch(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(CHECK_WHITE_CHANNEL_IS_SELECT() || CHECK_COLOR_CHANNEL_IS_SELECT(), "white or color channel output is disable", return false);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    bool result = s_lb_obj->status.on;
    LIGHTBULB_MUTEX_GIVE();

    return result;
}

esp_err_t lightbulb_set_fades_function(bool is_enable)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    if (is_enable) {
        s_lb_obj->cap.enable_fades = true;
    } else {
        s_lb_obj->cap.enable_fades = false;
    }
    LIGHTBULB_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t lightbulb_set_storage_function(bool is_enable)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    if (is_enable) {
        s_lb_obj->cap.enable_status_storage = true;
    } else {
        s_lb_obj->cap.enable_status_storage = false;
    }
    LIGHTBULB_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t lightbulb_set_fade_time(uint32_t fades_ms)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    s_lb_obj->cap.fades_ms = fades_ms;
    LIGHTBULB_MUTEX_GIVE();

    return ESP_OK;
}

bool lightbulb_get_fades_function_status(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    bool result = s_lb_obj->cap.enable_fades;
    LIGHTBULB_MUTEX_GIVE();

    return result;
}

lightbulb_works_mode_t lightbulb_get_mode(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    lightbulb_works_mode_t result = WORK_NONE;
    if (CHECK_COLOR_CHANNEL_IS_SELECT() || CHECK_WHITE_CHANNEL_IS_SELECT()) {
        result = s_lb_obj->status.mode;
    }
    LIGHTBULB_MUTEX_GIVE();

    return result;
}

esp_err_t lightbulb_basic_effect_start(lightbulb_effect_config_t *config)
{
    esp_err_t err = ESP_OK;
    LIGHTBULB_CHECK(config, "config is null", return ESP_FAIL);
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);

    bool flag = ((config->effect_type == EFFECT_BREATH) ? true : false);

    if (CHECK_LOW_POWER_FUNC_IS_ENABLE()) {
        xTimerStop(s_lb_obj->power_timer, 0);
    }

    if (config->mode == WORK_COLOR) {
        LIGHTBULB_CHECK(CHECK_COLOR_CHANNEL_IS_SELECT(), "color channel output is disable", return ESP_ERR_INVALID_STATE);
        uint16_t color_value_max[5] = { 0 };
        uint16_t color_value_min[5] = { 0 };
        uint8_t channel_mask = get_channel_mask(WORK_COLOR);

        color_value_max[0] = config->red * config->max_brightness / 100;
        color_value_max[1] = config->green * config->max_brightness / 100;
        color_value_max[2] = config->blue * config->max_brightness / 100;
        err |= hal_get_gamma_value(color_value_max[0], color_value_max[1], color_value_max[2], &color_value_max[0], &color_value_max[1], &color_value_max[2]);

        color_value_min[0] = config->red * config->min_brightness / 100;
        color_value_min[1] = config->green * config->min_brightness / 100;
        color_value_min[2] = config->blue * config->min_brightness / 100;
        hal_get_gamma_value(color_value_min[0], color_value_min[1], color_value_min[2], &color_value_min[0], &color_value_min[1], &color_value_min[2]);

        err |= hal_start_channel_group_action(color_value_min, color_value_max, channel_mask, config->effect_cycle_ms, flag);

    } else if (config->mode == WORK_WHITE) {
        LIGHTBULB_CHECK(CHECK_WHITE_CHANNEL_IS_SELECT(), "white channel output is disable", return ESP_ERR_INVALID_ARG);
        uint16_t white_value_max[5] = { 0 };
        uint16_t white_value_min[5] = { 0 };
        uint8_t channel_mask = get_channel_mask(WORK_WHITE);

        if (CHECK_WHITE_OUTPUT_REQ_MIXED()) {
            cct_and_brightness_convert_to_cold_and_warm(config->cct, config->max_brightness, &white_value_max[3], &white_value_max[4]);
            cct_and_brightness_convert_to_cold_and_warm(config->cct, config->min_brightness, &white_value_min[3], &white_value_min[4]);
            err |= hal_start_channel_group_action(white_value_min, white_value_max, channel_mask, config->effect_cycle_ms, flag);
        } else {
            white_value_max[4] = config->max_brightness * 255 / 100;
            white_value_min[4] = config->min_brightness * 255 / 100;
            err = hal_set_channel(CHANNEL_ID_COLD_CCT_WHITE, config->cct * 255 / 100, 0);
            err = hal_start_channel_action(CHANNEL_ID_WARM_BRIGHTNESS_YELLOW, white_value_min[4], white_value_max[4], config->effect_cycle_ms, flag);
        }
    } else {
        err = ESP_ERR_NOT_SUPPORTED;
    }
    LIGHTBULB_MUTEX_GIVE();

    return err;
}

esp_err_t lightbulb_basic_effect_stop(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    esp_err_t err = ESP_FAIL;
    uint8_t channel_mask = 0;
    if (CHECK_COLOR_CHANNEL_IS_SELECT()) {
        channel_mask |= (SELECT_COLOR_CHANNEL);
    }

    if (CHECK_WHITE_CHANNEL_IS_SELECT()) {
        channel_mask |= (SELECT_WHITE_CHANNEL);
    }
    err = hal_stop_channel_action(channel_mask);
    LIGHTBULB_MUTEX_GIVE();

    return err;
}

esp_err_t lightbulb_basic_effect_stop_and_restore(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    bool is_on = s_lb_obj->status.on;
    LIGHTBULB_MUTEX_GIVE();

    return lightbulb_set_switch(is_on);
}
