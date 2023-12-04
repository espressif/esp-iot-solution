/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
#define LIGHTBULB_MUTEX_TAKE(delay_ms)                  (xSemaphoreTakeRecursive(s_lb_obj->mutex, delay_ms))
#define LIGHTBULB_MUTEX_GIVE()                          (xSemaphoreGiveRecursive(s_lb_obj->mutex))

/**
 * @brief Lightbulb function check
 *
 */
#define CHECK_COLOR_CHANNEL_IS_SELECT()                 (s_lb_obj->cap.mode_mask & BIT(1) ? true : false)
#define CHECK_WHITE_CHANNEL_IS_SELECT()                 (s_lb_obj->cap.mode_mask & BIT(2) ? true : false)
#define CHECK_LOW_POWER_FUNC_IS_ENABLE()                ((s_lb_obj->cap.enable_lowpower && s_lb_obj->power_timer) ? true : false)
#define CHECK_AUTO_STATUS_STORAGE_FUNC_IS_ENABLE()      ((s_lb_obj->cap.enable_status_storage && s_lb_obj->storage_timer) ? true : false)
#define CHECK_WHITE_OUTPUT_REQ_MIXED()                  (s_lb_obj->cap.enable_mix_cct)
#define CHECK_AUTO_ON_FUNC_IS_ENABLE()                  ((!s_lb_obj->cap.disable_auto_on) ? true : false)
#define CHECK_EFFECT_TIMER_IS_ACTIVE()                  (s_lb_obj->effect_timer && (xTimerIsTimerActive(s_lb_obj->effect_timer) == pdTRUE))
#define CHECK_EFFECT_ALLOW_TO_BE_INTERRUPTED()          (s_lb_obj->effect_running_flag && (!s_lb_obj->effect_interrupt_forbidden_flag))
#define CHECK_EFFECT_IS_RUNNING()                       (s_lb_obj->effect_running_flag)

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
    lightbulb_cct_kelvin_range_t kelvin_range;
    TimerHandle_t power_timer;
    TimerHandle_t storage_timer;
    TimerHandle_t effect_timer;
    SemaphoreHandle_t mutex;
    bool effect_interrupt_forbidden_flag;
    bool effect_running_flag;
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
    uint16_t _kelvin = (_percentage * (s_lb_obj->kelvin_range.max - s_lb_obj->kelvin_range.min) + s_lb_obj->kelvin_range.min);

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
    if (kelvin > s_lb_obj->kelvin_range.max) {
        kelvin = s_lb_obj->kelvin_range.max;
    }
    if (kelvin < s_lb_obj->kelvin_range.min) {
        kelvin = s_lb_obj->kelvin_range.min;
    }

    return 100 * ((float)(kelvin - s_lb_obj->kelvin_range.min) / (s_lb_obj->kelvin_range.max - s_lb_obj->kelvin_range.min));
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
    hal_get_driver_feature(QUERY_MAX_INPUT_VALUE, &total);
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
    } else if (tmr == s_lb_obj->effect_timer) {
        lightbulb_basic_effect_stop();
        void(*user_cb)(void) = pvTimerGetTimerID(tmr);
        if (user_cb) {
            user_cb();
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
    ESP_LOGI(TAG, "lightbulb driver component version: %d.%d.%d", LIGHTBULB_DRIVER_VER_MAJOR, LIGHTBULB_DRIVER_VER_MINOR, LIGHTBULB_DRIVER_VER_PATCH);
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
        ESP_LOGI(TAG, "cct kelvin range param: ");
        ESP_LOGI(TAG, "     max cct: %d", s_lb_obj->kelvin_range.max);
        ESP_LOGI(TAG, "     min cct: %d", s_lb_obj->kelvin_range.min);
        ESP_LOGI(TAG, "cct: %d%%, %dK, brightness: %d", s_lb_obj->status.cct_percentage, percentage_convert_to_kelvin(s_lb_obj->status.cct_percentage), s_lb_obj->status.brightness);
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
    if (config->type != DRIVER_ESP_PWM && config->type != DRIVER_WS2812) {
        if (config->capability.enable_mix_cct != true) {
            config->capability.enable_mix_cct = true;
            ESP_LOGW(TAG, "The IIC dimming chip must enable CCT mix, rewrite the enable_mix_cct variable to true.");
        }
    }
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
        s_lb_obj->power.color_min_value = 1;
        s_lb_obj->power.white_min_brightness = 1;
        s_lb_obj->power.color_max_power = 300;
        s_lb_obj->power.white_max_power = 200;
    }

    // cct Limit check
    if (CHECK_WHITE_CHANNEL_IS_SELECT()) {
        if (config->kelvin_range) {
            memcpy(&s_lb_obj->kelvin_range, config->kelvin_range, sizeof(lightbulb_cct_kelvin_range_t));
            LIGHTBULB_CHECK(s_lb_obj->kelvin_range.max > s_lb_obj->kelvin_range.min, "CCT data error", goto EXIT)
        } else {
            s_lb_obj->kelvin_range.max = MAX_CCT_K;
            s_lb_obj->kelvin_range.min = MIN_CCT_K;
        }
    }

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

    print_func();

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
    LIGHTBULB_CHECK(percentage, "percentage is null", return ESP_ERR_INVALID_ARG);

    if (kelvin >= s_lb_obj->kelvin_range.min && kelvin <= s_lb_obj->kelvin_range.max) {
        ESP_LOGW(TAG, "kelvin out of range, will be forcibly converted");
    }

    *percentage = kelvin_convert_to_percentage(kelvin);

    return ESP_OK;
}

esp_err_t lightbulb_percentage2kelvin(uint8_t percentage, uint16_t *kelvin)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(kelvin, "kelvin is null", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(percentage <= 100, "percentage out of range", return ESP_ERR_INVALID_ARG);

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
    return lightbulb_set_cctb(s_lb_obj->status.cct_percentage, brightness);
}

esp_err_t lightbulb_set_hsv(uint16_t hue, uint8_t saturation, uint8_t value)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(hue <= 360, "hue out of range: %d", return ESP_ERR_INVALID_ARG, hue);
    LIGHTBULB_CHECK(saturation <= 100, "saturation out of range: %d", return ESP_ERR_INVALID_ARG, saturation);
    LIGHTBULB_CHECK(value <= 100, "value out of range: %d", return ESP_ERR_INVALID_ARG, value);
    LIGHTBULB_CHECK(CHECK_COLOR_CHANNEL_IS_SELECT(), "color channel output is disable", return ESP_ERR_INVALID_STATE);
    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);

    esp_err_t err = ESP_OK;

    if (CHECK_AUTO_STATUS_STORAGE_FUNC_IS_ENABLE()) {
        xTimerReset(s_lb_obj->storage_timer, 0);
    }

    if (CHECK_EFFECT_IS_RUNNING() && CHECK_EFFECT_ALLOW_TO_BE_INTERRUPTED()) {
        ESP_LOGW(TAG, "The effect has stopped because the %s API is changing the lights.", __FUNCTION__);
        if (CHECK_EFFECT_TIMER_IS_ACTIVE()) {
            xTimerStop(s_lb_obj->effect_timer, 0);
        }
        s_lb_obj->effect_interrupt_forbidden_flag = false;
        s_lb_obj->effect_running_flag = false;
    } else if (CHECK_EFFECT_IS_RUNNING()) {
        ESP_LOGW(TAG, "The effect are not allowed to be interrupted, skip calling %s, just save this change.", __FUNCTION__);
        goto SAVE_ONLY;
    }

    if (s_lb_obj->status.on || CHECK_AUTO_ON_FUNC_IS_ENABLE()) {
        if (CHECK_LOW_POWER_FUNC_IS_ENABLE()) {
            xTimerStop(s_lb_obj->power_timer, 0);
        }

        uint16_t color_value[5] = { 0 };
        uint16_t fade_time = CALCULATE_FADE_TIME();
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

        s_lb_obj->status.on = true;
    } else {
        ESP_LOGW(TAG, "skip calling %s, just save this change.", __FUNCTION__);
    }

SAVE_ONLY:
    s_lb_obj->status.mode = WORK_COLOR;
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

esp_err_t lightbulb_set_cctb(uint16_t cct, uint8_t brightness)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_CHECK(brightness <= 100, "brightness out of range: %d", return ESP_ERR_INVALID_ARG, brightness);
    if (cct > 100) {
        LIGHTBULB_CHECK(cct >= s_lb_obj->kelvin_range.min && cct <= s_lb_obj->kelvin_range.max, "cct out of range: %d", NULL, cct);
        ESP_LOGW(TAG, "will convert kelvin to percentage, %dK -> %d%%", cct, kelvin_convert_to_percentage(cct));
        cct = kelvin_convert_to_percentage(cct);
    }
    LIGHTBULB_CHECK(cct <= 100, "cct out of range: %d", return ESP_ERR_INVALID_ARG, cct);
    LIGHTBULB_CHECK(CHECK_WHITE_CHANNEL_IS_SELECT(), "white channel output is disable", return ESP_ERR_INVALID_STATE);
    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);

    esp_err_t err = ESP_OK;

    if (CHECK_AUTO_STATUS_STORAGE_FUNC_IS_ENABLE()) {
        xTimerReset(s_lb_obj->storage_timer, 0);
    }

    if (CHECK_EFFECT_IS_RUNNING() && CHECK_EFFECT_ALLOW_TO_BE_INTERRUPTED()) {
        ESP_LOGW(TAG, "The effect has stopped because the %s API is changing the lights.", __FUNCTION__);
        if (CHECK_EFFECT_TIMER_IS_ACTIVE()) {
            xTimerStop(s_lb_obj->effect_timer, 0);
        }
        s_lb_obj->effect_interrupt_forbidden_flag = false;
        s_lb_obj->effect_running_flag = false;
    } else if (CHECK_EFFECT_IS_RUNNING()) {
        ESP_LOGW(TAG, "The effect are not allowed to be interrupted, skip calling %s, just save this change.", __FUNCTION__);
        goto SAVE_ONLY;
    }

    if (s_lb_obj->status.on || CHECK_AUTO_ON_FUNC_IS_ENABLE()) {
        if (CHECK_LOW_POWER_FUNC_IS_ENABLE()) {
            xTimerStop(s_lb_obj->power_timer, 0);
        }

        uint16_t white_value[5] = { 0 };
        uint16_t fade_time = CALCULATE_FADE_TIME();
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

        s_lb_obj->status.on = true;
    } else {
        ESP_LOGW(TAG, "skip calling %s, just save this change.", __FUNCTION__);
    }

SAVE_ONLY:
    s_lb_obj->status.mode = WORK_WHITE;
    s_lb_obj->status.cct_percentage = cct;
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
    uint32_t fade_time = CALCULATE_FADE_TIME();
    ESP_LOGI(TAG, "%s will update on/off status: %s -> %s", __FUNCTION__, s_lb_obj->status.on ? "on" : "off", status ? "on" : "off");

    if (!status) {
        LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
        if (CHECK_LOW_POWER_FUNC_IS_ENABLE()) {
            xTimerReset(s_lb_obj->power_timer, 0);
        }
        if (CHECK_AUTO_STATUS_STORAGE_FUNC_IS_ENABLE()) {
            xTimerReset(s_lb_obj->storage_timer, 0);
        }
        if (CHECK_EFFECT_IS_RUNNING() && CHECK_EFFECT_ALLOW_TO_BE_INTERRUPTED()) {
            ESP_LOGW(TAG, "The effect has stopped because the %s API is changing the lights.", __FUNCTION__);
            if (CHECK_EFFECT_TIMER_IS_ACTIVE()) {
                xTimerStop(s_lb_obj->effect_timer, 0);
            }
            s_lb_obj->effect_interrupt_forbidden_flag = false;
            s_lb_obj->effect_running_flag = false;
        } else if (CHECK_EFFECT_IS_RUNNING()) {
            ESP_LOGW(TAG, "The effect are not allowed to be interrupted, skip calling %s, just save this change.", __FUNCTION__);
            s_lb_obj->status.on = false;
            LIGHTBULB_MUTEX_GIVE();
            return ESP_FAIL;
        }
        s_lb_obj->status.on = false;

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
        LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
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
        LIGHTBULB_MUTEX_GIVE();
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
    int8_t result = s_lb_obj->status.cct_percentage;
    LIGHTBULB_MUTEX_GIVE();

    return result;
}

int16_t lightbulb_get_cct_kelvin(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return -1);
    LIGHTBULB_CHECK(CHECK_WHITE_CHANNEL_IS_SELECT(), "white channel output is disable", return -1);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    int16_t result = percentage_convert_to_kelvin(s_lb_obj->status.cct_percentage);
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

esp_err_t lightbulb_update_status_variable(lightbulb_status_t *new_status, bool trigger)
{
    LIGHTBULB_CHECK(new_status, "new_status is null", return ESP_FAIL);
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    esp_err_t err = ESP_OK;

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    memcpy(&s_lb_obj->status, new_status, sizeof(lightbulb_status_t));
    if (trigger) {
        err = lightbulb_set_switch(s_lb_obj->status.on);
    }
    LIGHTBULB_MUTEX_GIVE();

    return err;
}

esp_err_t lightbulb_basic_effect_start(lightbulb_effect_config_t *config)
{
    esp_err_t err = ESP_ERR_INVALID_STATE;
    LIGHTBULB_CHECK(config, "config is null", return ESP_FAIL);
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);
    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);

    bool flag = ((config->effect_type == EFFECT_BREATH) ? true : false);

    if (CHECK_LOW_POWER_FUNC_IS_ENABLE()) {
        xTimerStop(s_lb_obj->power_timer, 0);
    }

    if (CHECK_EFFECT_TIMER_IS_ACTIVE()) {
        xTimerStop(s_lb_obj->effect_timer, 0);
    }

    if (config->mode == WORK_COLOR) {
        LIGHTBULB_CHECK(CHECK_COLOR_CHANNEL_IS_SELECT(), "color channel output is disable", goto EXIT);
        uint16_t color_value_max[5] = { 0 };
        uint16_t color_value_min[5] = { 0 };
        uint8_t channel_mask = get_channel_mask(WORK_COLOR);
        err = ESP_OK;

        color_value_max[0] = config->red * config->max_brightness / 100;
        color_value_max[1] = config->green * config->max_brightness / 100;
        color_value_max[2] = config->blue * config->max_brightness / 100;
        hal_get_gamma_value(color_value_max[0], color_value_max[1], color_value_max[2], &color_value_max[0], &color_value_max[1], &color_value_max[2]);

        color_value_min[0] = config->red * config->min_brightness / 100;
        color_value_min[1] = config->green * config->min_brightness / 100;
        color_value_min[2] = config->blue * config->min_brightness / 100;
        hal_get_gamma_value(color_value_min[0], color_value_min[1], color_value_min[2], &color_value_min[0], &color_value_min[1], &color_value_min[2]);

        err |= hal_start_channel_group_action(color_value_min, color_value_max, channel_mask, config->effect_cycle_ms, flag);

    } else if (config->mode == WORK_WHITE) {
        LIGHTBULB_CHECK(CHECK_WHITE_CHANNEL_IS_SELECT(), "white channel output is disable", goto EXIT);
        if (config->cct > 100) {
            LIGHTBULB_CHECK(config->cct >= s_lb_obj->kelvin_range.min && config->cct <= s_lb_obj->kelvin_range.max, "cct kelvin out of range: %d", goto EXIT, config->cct);
            ESP_LOGW(TAG, "will convert kelvin to percentage, %dK -> %d%%", config->cct, kelvin_convert_to_percentage(config->cct));
            config->cct = kelvin_convert_to_percentage(config->cct);
        }
        uint16_t white_value_max[5] = { 0 };
        uint16_t white_value_min[5] = { 0 };
        uint8_t channel_mask = get_channel_mask(WORK_WHITE);
        err = ESP_OK;

        if (CHECK_WHITE_OUTPUT_REQ_MIXED()) {
            cct_and_brightness_convert_to_cold_and_warm(config->cct, config->max_brightness, &white_value_max[3], &white_value_max[4]);
            cct_and_brightness_convert_to_cold_and_warm(config->cct, config->min_brightness, &white_value_min[3], &white_value_min[4]);
            err |= hal_start_channel_group_action(white_value_min, white_value_max, channel_mask, config->effect_cycle_ms, flag);
        } else {
            white_value_max[4] = config->max_brightness * 255 / 100;
            white_value_min[4] = config->min_brightness * 255 / 100;
            err |= hal_set_channel(CHANNEL_ID_COLD_CCT_WHITE, config->cct * 255 / 100, 0);
            err |= hal_start_channel_action(CHANNEL_ID_WARM_BRIGHTNESS_YELLOW, white_value_min[4], white_value_max[4], config->effect_cycle_ms, flag);
        }
    } else {
        err = ESP_ERR_NOT_SUPPORTED;
    }

    if (err == ESP_OK) {
        s_lb_obj->effect_interrupt_forbidden_flag = config->interrupt_forbidden;
        s_lb_obj->effect_running_flag = true;
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
            xTimerStart(s_lb_obj->effect_timer, 0);
            ESP_LOGI(TAG, "The auto-stop timer will trigger after %d ms.", config->total_ms);
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
        ESP_LOGI(TAG, "This effect will %s to be interrupted", s_lb_obj->effect_interrupt_forbidden_flag ? "not be allowed" : "allow");
    }

EXIT:
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
    s_lb_obj->effect_interrupt_forbidden_flag = false;
    s_lb_obj->effect_running_flag = false;
    LIGHTBULB_MUTEX_GIVE();

    return err;
}

esp_err_t lightbulb_basic_effect_stop_and_restore(void)
{
    LIGHTBULB_CHECK(s_lb_obj, "not init", return ESP_ERR_INVALID_ARG);

    LIGHTBULB_MUTEX_TAKE(portMAX_DELAY);
    bool is_on = s_lb_obj->status.on;
    s_lb_obj->effect_interrupt_forbidden_flag = false;
    s_lb_obj->effect_running_flag = false;
    LIGHTBULB_MUTEX_GIVE();

    return lightbulb_set_switch(is_on);
}
