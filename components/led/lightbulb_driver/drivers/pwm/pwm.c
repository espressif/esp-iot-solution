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

#include <string.h>
#include <stdio.h>

#include <esp_log.h>
#include <esp_compiler.h>
#include <driver/ledc.h>

#include "pwm.h"

static const char *TAG = "driver_pwm";

#define PWM_CHECK(a, str, action, ...)                                      \
    if (unlikely(!(a))) {                                                   \
        ESP_LOGE(TAG, str, ##__VA_ARGS__);                                  \
        action;                                                             \
    }

#if CONFIG_ENABLE_DRIVER_DEBUG_LOG_OUTPUT
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

typedef struct {
    ledc_timer_config_t ledc_config;
    uint8_t registered_channel_mask;
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
    bool invert_level;
#endif
} pwm_handle_t;

static pwm_handle_t *s_pwm = NULL;

#if CONFIG_PM_ENABLE
#include "esp_pm.h"

static esp_pm_lock_handle_t s_sleep_lock = NULL;
static esp_pm_lock_handle_t s_freq_lock = NULL;
static esp_pm_lock_handle_t s_cpu_lock = NULL;
static bool s_lock_is_take = false;
static bool s_create_done = false;
static void power_control_enable_sleep(void)
{
    if (s_lock_is_take && s_create_done) {
        ESP_LOGW(TAG, "release power lock, wifi enable sleep");
        esp_pm_lock_release(s_freq_lock);
        esp_pm_lock_release(s_cpu_lock);
        esp_pm_lock_release(s_sleep_lock);
        s_lock_is_take = false;
    }
}

static void power_control_disable_sleep(void)
{
    if (!s_lock_is_take && s_create_done) {
        ESP_LOGW(TAG, "acquire power lock, disable sleep");
        esp_pm_lock_acquire(s_freq_lock);
        esp_pm_lock_acquire(s_cpu_lock);
        esp_pm_lock_acquire(s_sleep_lock);
        s_lock_is_take = true;
    }
}

static esp_err_t power_control_lock_create(void)
{
    esp_err_t err = ESP_FAIL;
    err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "s_sleep_lock", &s_sleep_lock);
    PWM_CHECK(err == ESP_OK, "create sleep lock fail", return err);

    err = esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "cpu_lock", &s_cpu_lock);
    PWM_CHECK(err == ESP_OK, "create cpu lock fail", return err);

    err = esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "freq_lock", &s_freq_lock);
    PWM_CHECK(err == ESP_OK, "create freq lock fail", return err);

    s_create_done = true;
    return err;
}
#endif

esp_err_t pwm_init(driver_pwm_t *config)
{
    esp_err_t err = ESP_OK;
    PWM_CHECK(config, "config is null", return ESP_ERR_INVALID_ARG);
    PWM_CHECK(!s_pwm, "already init done", return ESP_ERR_INVALID_ARG);

    s_pwm = calloc(1, sizeof(pwm_handle_t));
    PWM_CHECK(s_pwm, "alloc fail", return ESP_ERR_NO_MEM);

    s_pwm->ledc_config.timer_num = LEDC_TIMER_0;
    s_pwm->ledc_config.freq_hz = config->freq_hz;
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
    s_pwm->invert_level = config->invert_level;
#endif

#if CONFIG_IDF_TARGET_ESP32
    s_pwm->ledc_config.speed_mode = LEDC_HIGH_SPEED_MODE;
    s_pwm->ledc_config.duty_resolution = LEDC_TIMER_12_BIT;
    s_pwm->ledc_config.clk_cfg = LEDC_USE_APB_CLK;
#else
    s_pwm->ledc_config.speed_mode = LEDC_LOW_SPEED_MODE;
    s_pwm->ledc_config.duty_resolution = LEDC_TIMER_12_BIT;
    s_pwm->ledc_config.clk_cfg = LEDC_USE_XTAL_CLK;
#endif

    err = ledc_timer_config(&s_pwm->ledc_config);
    PWM_CHECK(err == ESP_OK, "LEDC timer config fail", goto EXIT);
    err = ledc_fade_func_install(ESP_INTR_FLAG_IRAM);
    PWM_CHECK(err == ESP_OK, "ledc_fade_func_install fail", goto EXIT);

#if CONFIG_PM_ENABLE
    err = power_control_lock_create();
    PWM_CHECK(err == ESP_OK, "power_control_lock_create fail", goto EXIT);
    power_control_disable_sleep();
#endif

    return ESP_OK;

EXIT:
    if (s_pwm) {
        free(s_pwm);
        s_pwm = NULL;
    }

    return err;
}

esp_err_t pwm_deinit()
{
    ledc_fade_func_uninstall();

    if (s_pwm) {
        free(s_pwm);
        s_pwm = NULL;
    }
    return ESP_OK;
}

esp_err_t pwm_regist_channel(pwm_channel_t channel, gpio_num_t gpio_num)
{
    esp_err_t err = ESP_OK;
    PWM_CHECK(s_pwm, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);

    // The LEDC will keep outputting during software reset, so we need to load the last value to make sure the lights don't flicker during the initial ledc channel.
    uint32_t duty = ledc_get_duty(s_pwm->ledc_config.speed_mode, channel);

    const ledc_channel_config_t ledc_ch_config = {
        .gpio_num = gpio_num,
        .channel = channel,
        .intr_type = LEDC_INTR_DISABLE,
        .speed_mode = s_pwm->ledc_config.speed_mode,
        .timer_sel = s_pwm->ledc_config.timer_num,
        .duty = duty,
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
        .flags.output_invert = s_pwm->invert_level,
#endif
    };
    err = ledc_channel_config(&ledc_ch_config);
    PWM_CHECK(err == ESP_OK, "channel config fail", return ESP_ERR_INVALID_STATE);
    ESP_LOGI(TAG, "channel:%d -> gpio_num:%d", channel, gpio_num);
    s_pwm->registered_channel_mask |= (1 << channel);

    return err;
}

esp_err_t pwm_set_channel(pwm_channel_t channel, uint16_t value)
{
    esp_err_t err = ESP_OK;
    PWM_CHECK(s_pwm, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);
    PWM_CHECK(s_pwm->registered_channel_mask & BIT(channel), "Channel not registered", return ESP_ERR_INVALID_STATE);
    PWM_CHECK((value <= (1 << s_pwm->ledc_config.duty_resolution)), "value out of range", return ESP_ERR_INVALID_ARG);

#if CONFIG_PM_ENABLE
    power_control_disable_sleep();
#endif

    err |= ledc_set_duty(s_pwm->ledc_config.speed_mode, channel, value);
    err |= ledc_update_duty(s_pwm->ledc_config.speed_mode, channel);

    return err;
}

esp_err_t pwm_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b)
{
    esp_err_t err = ESP_OK;
    PWM_CHECK(s_pwm, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);
    PWM_CHECK((s_pwm->registered_channel_mask & BIT(PWM_CHANNEL_R)) &&
              (s_pwm->registered_channel_mask & BIT(PWM_CHANNEL_G)) &&
              (s_pwm->registered_channel_mask & BIT(PWM_CHANNEL_B)), "RGB Channel not registered", return ESP_ERR_INVALID_STATE);
    PWM_CHECK((value_r <= (1 << s_pwm->ledc_config.duty_resolution)) &&
              (value_g <= (1 << s_pwm->ledc_config.duty_resolution)) &&
              (value_b <= (1 << s_pwm->ledc_config.duty_resolution)), "value out of range", return ESP_ERR_INVALID_ARG);

#if CONFIG_PM_ENABLE
    power_control_disable_sleep();
#endif

    //Must be set first
    err |= ledc_set_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_R, value_r);
    err |= ledc_set_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_G, value_g);
    err |= ledc_set_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_B, value_b);

    //Update later
    err |= ledc_update_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_R);
    err |= ledc_update_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_G);
    err |= ledc_update_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_B);

    return err;
}

esp_err_t pwm_set_cctb_or_cw_channel(uint16_t value_cct_c, uint16_t value_b_w)
{
    esp_err_t err = ESP_OK;
    PWM_CHECK(s_pwm, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);
    PWM_CHECK(s_pwm->registered_channel_mask & BIT(PWM_CHANNEL_CCT_COLD) &&
              s_pwm->registered_channel_mask & BIT(PWM_CHANNEL_BRIGHTNESS_WARM), "CCT/Brightness or cold/warm Channel not registered", return ESP_ERR_INVALID_STATE);
    PWM_CHECK((value_cct_c <= (1 << s_pwm->ledc_config.duty_resolution)) &&
              (value_b_w <= (1 << s_pwm->ledc_config.duty_resolution)), "value out of range", return ESP_ERR_INVALID_ARG);

#if CONFIG_PM_ENABLE
    power_control_disable_sleep();
#endif

    //Must be set first
    err |= ledc_set_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_CCT_COLD, value_cct_c);
    err |= ledc_set_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_BRIGHTNESS_WARM, value_b_w);

    //Update later
    err |= ledc_update_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_CCT_COLD);
    err |= ledc_update_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_BRIGHTNESS_WARM);

    return err;
}

esp_err_t pwm_set_rgbcctb_or_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_cct_c, uint16_t value_b_w)
{
    esp_err_t err = ESP_OK;
    PWM_CHECK(s_pwm, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);
    PWM_CHECK(s_pwm->registered_channel_mask & BIT(PWM_CHANNEL_R) &&
              s_pwm->registered_channel_mask & BIT(PWM_CHANNEL_G) &&
              s_pwm->registered_channel_mask & BIT(PWM_CHANNEL_B), "RGB Channel not registered", return ESP_ERR_INVALID_STATE);
    PWM_CHECK(s_pwm->registered_channel_mask & BIT(PWM_CHANNEL_CCT_COLD) &&
              s_pwm->registered_channel_mask & BIT(PWM_CHANNEL_BRIGHTNESS_WARM), "CCT/Brightness or cold/warm Channel not registered", return ESP_ERR_INVALID_STATE);
    PWM_CHECK((value_r <= (1 << s_pwm->ledc_config.duty_resolution)) &&
              (value_g <= (1 << s_pwm->ledc_config.duty_resolution)) &&
              (value_b <= (1 << s_pwm->ledc_config.duty_resolution)), "value out of range", return ESP_ERR_INVALID_ARG);
    PWM_CHECK((value_cct_c <= (1 << s_pwm->ledc_config.duty_resolution)) &&
              (value_b_w <= (1 << s_pwm->ledc_config.duty_resolution)), "value out of range", return ESP_ERR_INVALID_ARG);

#if CONFIG_PM_ENABLE
    power_control_disable_sleep();
#endif

    //Must be set first
    err |= ledc_set_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_R, value_r);
    err |= ledc_set_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_G, value_g);
    err |= ledc_set_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_B, value_b);
    err |= ledc_set_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_CCT_COLD, value_cct_c);
    err |= ledc_set_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_BRIGHTNESS_WARM, value_b_w);

    //Update later
    err |= ledc_update_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_R);
    err |= ledc_update_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_G);
    err |= ledc_update_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_B);
    err |= ledc_update_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_CCT_COLD);
    err |= ledc_update_duty(s_pwm->ledc_config.speed_mode, PWM_CHANNEL_BRIGHTNESS_WARM);

    return err;
}

esp_err_t pwm_set_shutdown(void)
{
    esp_err_t err = ESP_OK;
    PWM_CHECK(s_pwm, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);

#if CONFIG_PM_ENABLE
    power_control_disable_sleep();
#endif

    for (int i = 0; i < PWM_CHANNEL_MAX; i++) {
        if (s_pwm->registered_channel_mask & BIT(i)) {
            err |= ledc_set_duty(s_pwm->ledc_config.speed_mode, i, 0);
            err |= ledc_update_duty(s_pwm->ledc_config.speed_mode, i);
        }
    }
    return err;
}

esp_err_t pwm_set_hw_fade(pwm_channel_t channel, uint16_t value, int fade_ms)
{
    PWM_CHECK(s_pwm, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);
    PWM_CHECK(s_pwm->registered_channel_mask & BIT(channel), "Channel not registered", return ESP_ERR_INVALID_STATE);
    PWM_CHECK(value <= ((1 << s_pwm->ledc_config.duty_resolution)), "value out of range", return ESP_ERR_INVALID_ARG);

#if CONFIG_PM_ENABLE
    power_control_disable_sleep();
#endif

    return ledc_set_fade_time_and_start(s_pwm->ledc_config.speed_mode, channel, value, fade_ms, LEDC_FADE_NO_WAIT);
}

esp_err_t pwm_set_sleep(bool is_enable)
{
#if CONFIG_PM_ENABLE
    if (is_enable) {
        power_control_enable_sleep();
    } else {
        power_control_disable_sleep();
    }
    return ESP_OK;
#else
    ESP_LOGE(TAG, "You need to enable power management via menuconfig");
    return ESP_FAIL;
#endif
}
