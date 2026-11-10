/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <sys/lock.h>

#include <esp_log.h>
#include <esp_compiler.h>
#include <esp_idf_version.h>
#include <driver/ledc.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include <esp_rom_sys.h>
#else
#include <rom/ets_sys.h>
#endif

#include "driver_utils.h"
#include "pwm.h"

#if CONFIG_PM_ENABLE && CONFIG_IDF_TARGET_ESP32
#include "esp_pm.h"
#endif

static const char *TAG = "driver_pwm";

struct pwm_context_s {
    ledc_timer_config_t ledc_config;
    ledc_channel_t channels[PWM_CHANNEL_MAX];
#if CONFIG_PM_ENABLE && CONFIG_IDF_TARGET_ESP32
    esp_pm_lock_handle_t sleep_lock;
    esp_pm_lock_handle_t freq_lock;
    esp_pm_lock_handle_t cpu_lock;
    bool lock_is_take;
    bool create_done;
#endif
    uint8_t registered_channel_mask;
    uint8_t phase_delay_flag;
    bool hw_cct;
    uint32_t duty[PWM_CHANNEL_MAX];
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
    bool invert_level;
#endif
    uint32_t hpoint[5];
};

static _lock_t s_resource_lock;
static uint32_t s_channels;
static gpio_num_t s_channel_gpios[LEDC_CHANNEL_MAX];
static unsigned s_instances;
static struct {
    unsigned users;
    ledc_timer_config_t config;
} s_timers[LEDC_TIMER_MAX];

#if CONFIG_PM_ENABLE && CONFIG_IDF_TARGET_ESP32
static void power_control_enable_sleep(pwm_handle_t pwm_handle)
{
    if (pwm_handle->lock_is_take && pwm_handle->create_done) {
        ESP_LOGW(TAG, "release power lock, wifi enable sleep");
        esp_pm_lock_release(pwm_handle->freq_lock);
        esp_pm_lock_release(pwm_handle->cpu_lock);
        esp_pm_lock_release(pwm_handle->sleep_lock);
        pwm_handle->lock_is_take = false;
    }
}

static void power_control_disable_sleep(pwm_handle_t pwm_handle)
{
    if (!pwm_handle->lock_is_take && pwm_handle->create_done) {
        ESP_LOGW(TAG, "acquire power lock, disable sleep");
        esp_pm_lock_acquire(pwm_handle->freq_lock);
        esp_pm_lock_acquire(pwm_handle->cpu_lock);
        esp_pm_lock_acquire(pwm_handle->sleep_lock);
        pwm_handle->lock_is_take = true;
    }
}

static esp_err_t power_control_lock_create(pwm_handle_t pwm_handle)
{
    esp_err_t err = ESP_FAIL;
    err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "s_sleep_lock", &pwm_handle->sleep_lock);
    DRIVER_CHECK(err == ESP_OK, "create sleep lock fail", return err);

    err = esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "cpu_lock", &pwm_handle->cpu_lock);
    DRIVER_CHECK(err == ESP_OK, "create cpu lock fail", return err);

    err = esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "freq_lock", &pwm_handle->freq_lock);
    DRIVER_CHECK(err == ESP_OK, "create freq lock fail", return err);

    pwm_handle->create_done = true;
    return err;
}
#endif

static void pwm_recompute_hpoints(pwm_handle_t pwm_handle, uint32_t *eff_duty)
{
    uint32_t period = 1 << pwm_handle->ledc_config.duty_resolution;
    uint32_t total = 0;
    uint32_t acc = 0;
    for (int ch = 0; ch < PWM_CHANNEL_MAX; ch++) {
        eff_duty[ch] = pwm_handle->duty[ch];
        if ((pwm_handle->registered_channel_mask & BIT(ch)) && !(pwm_handle->hw_cct && ch == PWM_CHANNEL_CCT_COLD)) {
            total += pwm_handle->duty[ch];
        }
    }
    if (total > period) {
        uint32_t used = 0;
        int max_ch = -1;
        for (int ch = 0; ch < PWM_CHANNEL_MAX; ch++) {
            if ((pwm_handle->registered_channel_mask & BIT(ch)) && !(pwm_handle->hw_cct && ch == PWM_CHANNEL_CCT_COLD)) {
                eff_duty[ch] = (uint32_t)(((uint64_t)pwm_handle->duty[ch] * period) / total);
                if (pwm_handle->duty[ch] && !eff_duty[ch]) {
                    eff_duty[ch] = 1; /* A requested channel must never be turned off by truncation. */
                }
                used += eff_duty[ch];
                if (max_ch < 0 || eff_duty[ch] >= eff_duty[max_ch]) {
                    max_ch = ch;
                }
            }
        }
        if (max_ch >= 0) {
            /* Absorb the rounding difference in the largest channel. */
            eff_duty[max_ch] += period - used;
        }
    }
    for (int ch = 0; ch < PWM_CHANNEL_MAX; ch++) {
        if (pwm_handle->registered_channel_mask & BIT(ch)) {
            if (pwm_handle->hw_cct && ch == PWM_CHANNEL_CCT_COLD) {
                pwm_handle->hpoint[ch] = 0;
                continue;
            }
            pwm_handle->hpoint[ch] = acc % period;
            acc += eff_duty[ch];
        }
    }
}

static esp_err_t pwm_commit_dynamic(pwm_handle_t pwm_handle)
{
    uint32_t eff_duty[PWM_CHANNEL_MAX];
    pwm_recompute_hpoints(pwm_handle, eff_duty);
    esp_err_t err = ESP_OK;
    for (int ch = 0; ch < PWM_CHANNEL_MAX; ch++) {
        if (pwm_handle->registered_channel_mask & BIT(ch)) {
            err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[ch],
                                             eff_duty[ch], pwm_handle->hpoint[ch]);
        }
    }
    for (int ch = 0; ch < PWM_CHANNEL_MAX; ch++) {
        if (pwm_handle->registered_channel_mask & BIT(ch)) {
            err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[ch]);
        }
    }
    return err;
}

esp_err_t pwm_init(driver_pwm_t *config, void(*hook_func)(void *, void *), void *user_data, pwm_handle_t *out_handle)
{
    esp_err_t err = ESP_OK;
    DRIVER_CHECK(out_handle, "out_handle is null", return ESP_ERR_INVALID_ARG);
    *out_handle = NULL;
    DRIVER_CHECK(config && config->freq_hz, "invalid config", return ESP_ERR_INVALID_ARG);
    DRIVER_CHECK(config->phase_delay.flag == 0 ||
                 config->phase_delay.flag == PWM_RGB_CHANNEL_PHASE_DELAY_FLAG ||
                 config->phase_delay.flag == PWM_CW_CHANNEL_PHASE_DELAY_FLAG ||
                 config->phase_delay.flag == PWM_RGBCW_CHANNEL_PHASE_DELAY_FLAG ||
                 config->phase_delay.flag == PWM_CHANNEL_COMPLEMENTARY_OUTPUT_FLAG,
                 "unsupported phase_delay flag", return ESP_ERR_INVALID_ARG);

    pwm_handle_t pwm_handle = calloc(1, sizeof(*pwm_handle));
    DRIVER_CHECK(pwm_handle, "alloc fail", return ESP_ERR_NO_MEM);
    _lock_acquire(&s_resource_lock);

    int timer = -1;
    bool timer_acquired = false;
    bool fade_installed = false;
    pwm_handle->ledc_config.freq_hz = config->freq_hz;
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
    pwm_handle->invert_level = config->invert_level;
#endif

#if CONFIG_IDF_TARGET_ESP32
    pwm_handle->ledc_config.speed_mode = LEDC_HIGH_SPEED_MODE;
    pwm_handle->ledc_config.clk_cfg = LEDC_USE_APB_CLK;
#else
    pwm_handle->ledc_config.speed_mode = LEDC_LOW_SPEED_MODE;
    pwm_handle->ledc_config.clk_cfg = LEDC_USE_XTAL_CLK;
#endif
    for (int i = 0; i < LEDC_TIMER_MAX; i++) {
        if (s_timers[i].users && s_timers[i].config.freq_hz == config->freq_hz) {
            timer = i;
            pwm_handle->ledc_config = s_timers[i].config;
            break;
        }
    }
    if (timer < 0) {
        for (int i = 0; i < LEDC_TIMER_MAX; i++) {
            if (!s_timers[i].users) {
                timer = i;
                break;
            }
        }
        if (timer < 0) {
            err = ESP_ERR_NOT_FOUND;
            goto EXIT;
        }
        pwm_handle->ledc_config.timer_num = timer;
        uint32_t preset_bit = LEDC_TIMER_12_BIT;
        for (pwm_handle->ledc_config.duty_resolution = preset_bit; pwm_handle->ledc_config.duty_resolution >= LEDC_TIMER_10_BIT; pwm_handle->ledc_config.duty_resolution--) {
            err = ledc_timer_config(&pwm_handle->ledc_config);
            if (err == ESP_OK) {
                if (preset_bit != pwm_handle->ledc_config.duty_resolution) {
                    ESP_LOGW(TAG, "Updated resolution to %d bit", pwm_handle->ledc_config.duty_resolution);
                }
                break;
            }
        }
        DRIVER_CHECK(err == ESP_OK, "LEDC timer config fail, please reduce the frequency", goto EXIT);
        s_timers[timer].config = pwm_handle->ledc_config;
    }
    s_timers[timer].users++;
    timer_acquired = true;

    pwm_handle->phase_delay_flag = config->phase_delay.flag;
    pwm_handle->hw_cct = config->hw_cct;
    uint32_t grayscale_level = 1 << pwm_handle->ledc_config.duty_resolution;
    if (pwm_handle->phase_delay_flag == PWM_CHANNEL_COMPLEMENTARY_OUTPUT_FLAG) {
        /* Dynamic mode uses the full PWM range. */
        goto INIT_COMMON;
    }
    if (config->phase_delay.flag & PWM_RGB_CHANNEL_PHASE_DELAY_FLAG) {
        grayscale_level /= 3;
        for (int i = 0; i < 3; i++) {
            pwm_handle->hpoint[i] = grayscale_level * i;
        }
    } else if (config->phase_delay.flag & PWM_CW_CHANNEL_PHASE_DELAY_FLAG) {
        grayscale_level /= 2;
        uint8_t cw_start_ch = 3;
        for (int i = 0; i < 2; i++) {
            pwm_handle->hpoint[cw_start_ch + i] = grayscale_level * i;
        }
    } else if (config->phase_delay.flag & PWM_RGBCW_CHANNEL_PHASE_DELAY_FLAG) {
        grayscale_level /= 5;
        for (int i = 0; i < 5; i++) {
            pwm_handle->hpoint[i] = grayscale_level * i;
        }
    } else {
        /* Nothing */
    }
INIT_COMMON:

    err = s_instances ? ESP_OK : ledc_fade_func_install(ESP_INTR_FLAG_IRAM);
    DRIVER_CHECK(err == ESP_OK, "ledc_fade_func_install fail", goto EXIT);
    fade_installed = (s_instances == 0);

    /**
     * Since LEDC uses the XTAL clock source for configuration, there is no need to create a power lock to keep PWM output during in sleep mode.
     * Additionally, this update does not apply to the ESP32.
     *
     */
#if CONFIG_PM_ENABLE && CONFIG_IDF_TARGET_ESP32
    err = power_control_lock_create(pwm_handle);
    DRIVER_CHECK(err == ESP_OK, "power_control_lock_create fail", goto EXIT);
    power_control_disable_sleep(pwm_handle);
#endif

    s_instances++;
    *out_handle = pwm_handle;
    _lock_release(&s_resource_lock);
    if (hook_func) {
        hook_func(user_data, (void *)(uintptr_t)grayscale_level);
    }
    return ESP_OK;

EXIT:
#if CONFIG_PM_ENABLE && CONFIG_IDF_TARGET_ESP32
    if (pwm_handle->sleep_lock) {
        esp_pm_lock_delete(pwm_handle->sleep_lock);
    }
    if (pwm_handle->cpu_lock) {
        esp_pm_lock_delete(pwm_handle->cpu_lock);
    }
    if (pwm_handle->freq_lock) {
        esp_pm_lock_delete(pwm_handle->freq_lock);
    }
#endif
    if (fade_installed) {
        ledc_fade_func_uninstall();
    }
    if (timer_acquired && --s_timers[timer].users == 0) {
        /* Pause first; ledc_timer_del rejects a timer that is still running. */
        ledc_timer_pause(pwm_handle->ledc_config.speed_mode, pwm_handle->ledc_config.timer_num);
    }
    if (s_instances == 0) {
        for (timer = 0; timer < LEDC_TIMER_MAX; timer++) {
            if (s_timers[timer].config.freq_hz) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
                s_timers[timer].config.deconfigure = true;
                ledc_timer_config(&s_timers[timer].config);
#endif
            }
        }
        memset(s_timers, 0, sizeof(s_timers));
    }
    free(pwm_handle);
    _lock_release(&s_resource_lock);
    return err;
}

esp_err_t pwm_deinit(pwm_handle_t pwm_handle)
{
    esp_err_t err = ESP_OK;
    DRIVER_CHECK(pwm_handle, "invalid handle", return ESP_ERR_INVALID_ARG);
    _lock_acquire(&s_resource_lock);
    for (int ch = 0; ch < PWM_CHANNEL_MAX; ch++) {
        if (pwm_handle->registered_channel_mask & BIT(ch)) {
            /* This waits for an outstanding hardware fade before recycling the channel. */
            err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[ch], 0, 0);
            err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[ch]);
            err |= ledc_stop(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[ch], 0);
            /* Detach LEDC but actively hold the inactive level; reset_pin enables pull-up. */
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
            err |= gpio_set_level(s_channel_gpios[pwm_handle->channels[ch]], pwm_handle->invert_level);
#else
            err |= gpio_set_level(s_channel_gpios[pwm_handle->channels[ch]], 0);
#endif
            err |= gpio_set_direction(s_channel_gpios[pwm_handle->channels[ch]], GPIO_MODE_OUTPUT);
            err |= gpio_set_pull_mode(s_channel_gpios[pwm_handle->channels[ch]], GPIO_FLOATING);
            s_channels &= ~BIT(pwm_handle->channels[ch]);
        }
    }
#if CONFIG_PM_ENABLE && CONFIG_IDF_TARGET_ESP32
    power_control_enable_sleep(pwm_handle);
    err |= esp_pm_lock_delete(pwm_handle->sleep_lock);
    err |= esp_pm_lock_delete(pwm_handle->cpu_lock);
    err |= esp_pm_lock_delete(pwm_handle->freq_lock);
#endif
    if (--s_timers[pwm_handle->ledc_config.timer_num].users == 0) {
        /* Defer deconfiguration: IDF timer deletion can disable the global
         * sleep clock even while another timer still needs it.
         * Low-speed LEDC copies the new duty into duty_rd on timer overflow.
         * Wait two periods so that copy happens before the timer is paused. */
        if (pwm_handle->ledc_config.freq_hz) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
            esp_rom_delay_us((2000000 / pwm_handle->ledc_config.freq_hz) + 50);
#else
            ets_delay_us((2000000 / pwm_handle->ledc_config.freq_hz) + 50);
#endif
        }
        err |= ledc_timer_pause(pwm_handle->ledc_config.speed_mode, pwm_handle->ledc_config.timer_num);
    }
    if (--s_instances == 0) {
        for (int timer = 0; timer < LEDC_TIMER_MAX; timer++) {
            if (s_timers[timer].config.freq_hz) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
                s_timers[timer].config.deconfigure = true;
                err |= ledc_timer_config(&s_timers[timer].config);
#endif
            }
        }
        memset(s_timers, 0, sizeof(s_timers));
        ledc_fade_func_uninstall();
    }
    free(pwm_handle);
    _lock_release(&s_resource_lock);
    return err;
}

esp_err_t pwm_regist_channel(pwm_handle_t pwm_handle, pwm_channel_t channel, gpio_num_t gpio_num)
{
    esp_err_t err = ESP_OK;
    DRIVER_CHECK(pwm_handle, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);

    DRIVER_CHECK(channel >= 0 && channel < PWM_CHANNEL_MAX && GPIO_IS_VALID_OUTPUT_GPIO(gpio_num),
                 "invalid channel or GPIO", return ESP_ERR_INVALID_ARG);
    DRIVER_CHECK(!(pwm_handle->registered_channel_mask & BIT(channel)), "channel already registered", return ESP_ERR_INVALID_STATE);
    _lock_acquire(&s_resource_lock);
    for (int i = 0; i < LEDC_CHANNEL_MAX; i++) {
        if ((s_channels & BIT(i)) && s_channel_gpios[i] == gpio_num) {
            _lock_release(&s_resource_lock);
            return ESP_ERR_INVALID_STATE;
        }
    }
    int physical = ((int)channel < LEDC_CHANNEL_MAX && !(s_channels & BIT(channel))) ? channel : -1;
    for (int i = 0; physical < 0 && i < LEDC_CHANNEL_MAX; i++) {
        if (!(s_channels & BIT(i))) {
            physical = i;
            break;
        }
    }
    if (physical < 0) {
        _lock_release(&s_resource_lock);
        return ESP_ERR_NOT_FOUND;
    }
    pwm_handle->channels[channel] = physical;
    // The LEDC will keep outputting during software reset, so we need to load the last value to make sure the lights don't flicker during the initial ledc channel.
    uint32_t duty = ledc_get_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[channel]);

    const ledc_channel_config_t ledc_ch_config = {
        .gpio_num = gpio_num,
        .channel = physical,
        .intr_type = LEDC_INTR_DISABLE,
        .speed_mode = pwm_handle->ledc_config.speed_mode,
        .timer_sel = pwm_handle->ledc_config.timer_num,
        .duty = duty,
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)) && !CONFIG_IDF_TARGET_ESP32
        /* Keep the XTAL clock and GPIO output active during Light-sleep. */
        .sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE,
#endif
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
        .flags.output_invert = pwm_handle->invert_level,
#endif
    };
    err = ledc_channel_config(&ledc_ch_config);
    DRIVER_CHECK(err == ESP_OK, "channel config fail", _lock_release(&s_resource_lock); return ESP_ERR_INVALID_STATE);
    ESP_LOGD(TAG, "channel:%d -> gpio_num:%d", channel, gpio_num);
    pwm_handle->duty[channel] = duty;
    pwm_handle->registered_channel_mask |= (1 << channel);

    s_channel_gpios[physical] = gpio_num;
    s_channels |= BIT(physical);
    _lock_release(&s_resource_lock);

    if (pwm_handle->phase_delay_flag == PWM_CHANNEL_COMPLEMENTARY_OUTPUT_FLAG) {
        /* ledc_channel_config() resets hpoint to 0, re-commit to restore complementary phasing. */
        return pwm_commit_dynamic(pwm_handle);
    }

    return err;
}

esp_err_t pwm_set_channel(pwm_handle_t pwm_handle, pwm_channel_t channel, uint16_t value)
{
    esp_err_t err = ESP_OK;
    DRIVER_CHECK(pwm_handle, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK(channel >= 0 && channel < PWM_CHANNEL_MAX, "invalid channel", return ESP_ERR_INVALID_ARG);
    DRIVER_CHECK(pwm_handle->registered_channel_mask & BIT(channel), "Channel %d not registered", return ESP_ERR_INVALID_STATE, channel);
    DRIVER_CHECK((value <= (1 << pwm_handle->ledc_config.duty_resolution)), "value out of range", return ESP_ERR_INVALID_ARG);

#if CONFIG_PM_ENABLE && CONFIG_IDF_TARGET_ESP32
    power_control_disable_sleep(pwm_handle);
#endif
    if (pwm_handle->phase_delay_flag == PWM_CHANNEL_COMPLEMENTARY_OUTPUT_FLAG) {
        pwm_handle->duty[channel] = value;
        return pwm_commit_dynamic(pwm_handle);
    }

    err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[channel], value, pwm_handle->hpoint[channel]);
    err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[channel]);

    return err;
}

esp_err_t pwm_set_rgb_channel(pwm_handle_t pwm_handle, uint16_t value_r, uint16_t value_g, uint16_t value_b)
{
    esp_err_t err = ESP_OK;
    DRIVER_CHECK(pwm_handle, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK((pwm_handle->registered_channel_mask & BIT(PWM_CHANNEL_R)) &&
                 (pwm_handle->registered_channel_mask & BIT(PWM_CHANNEL_G)) &&
                 (pwm_handle->registered_channel_mask & BIT(PWM_CHANNEL_B)), "RGB Channel not registered", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK((value_r <= (1 << pwm_handle->ledc_config.duty_resolution)) &&
                 (value_g <= (1 << pwm_handle->ledc_config.duty_resolution)) &&
                 (value_b <= (1 << pwm_handle->ledc_config.duty_resolution)), "value out of range", return ESP_ERR_INVALID_ARG);

#if CONFIG_PM_ENABLE && CONFIG_IDF_TARGET_ESP32
    power_control_disable_sleep(pwm_handle);
#endif
    if (pwm_handle->phase_delay_flag == PWM_CHANNEL_COMPLEMENTARY_OUTPUT_FLAG) {
        pwm_handle->duty[PWM_CHANNEL_R] = value_r;
        pwm_handle->duty[PWM_CHANNEL_G] = value_g;
        pwm_handle->duty[PWM_CHANNEL_B] = value_b;
        return pwm_commit_dynamic(pwm_handle);
    }

    //Must be set first
    err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_R], value_r, pwm_handle->hpoint[PWM_CHANNEL_R]);
    err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_G], value_g, pwm_handle->hpoint[PWM_CHANNEL_G]);
    err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_B], value_b, pwm_handle->hpoint[PWM_CHANNEL_B]);

    //Update later
    err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_R]);
    err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_G]);
    err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_B]);

    return err;
}

esp_err_t pwm_set_cctb_or_cw_channel(pwm_handle_t pwm_handle, uint16_t value_cct_c, uint16_t value_b_w)
{
    esp_err_t err = ESP_OK;
    DRIVER_CHECK(pwm_handle, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK(pwm_handle->registered_channel_mask & BIT(PWM_CHANNEL_CCT_COLD) &&
                 pwm_handle->registered_channel_mask & BIT(PWM_CHANNEL_BRIGHTNESS_WARM), "CCT/Brightness or cold/warm Channel not registered", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK((value_cct_c <= (1 << pwm_handle->ledc_config.duty_resolution)) &&
                 (value_b_w <= (1 << pwm_handle->ledc_config.duty_resolution)), "value out of range", return ESP_ERR_INVALID_ARG);

#if CONFIG_PM_ENABLE && CONFIG_IDF_TARGET_ESP32
    power_control_disable_sleep(pwm_handle);
#endif
    if (pwm_handle->phase_delay_flag == PWM_CHANNEL_COMPLEMENTARY_OUTPUT_FLAG) {
        pwm_handle->duty[PWM_CHANNEL_CCT_COLD] = value_cct_c;
        pwm_handle->duty[PWM_CHANNEL_BRIGHTNESS_WARM] = value_b_w;
        return pwm_commit_dynamic(pwm_handle);
    }

    //Must be set first
    err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_CCT_COLD], value_cct_c, pwm_handle->hpoint[PWM_CHANNEL_CCT_COLD]);
    err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_BRIGHTNESS_WARM], value_b_w, pwm_handle->hpoint[PWM_CHANNEL_BRIGHTNESS_WARM]);

    //Update later
    err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_CCT_COLD]);
    err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_BRIGHTNESS_WARM]);

    return err;
}

esp_err_t pwm_set_rgbcctb_or_rgbcw_channel(pwm_handle_t pwm_handle, uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_cct_c,
                                           uint16_t value_b_w)
{
    esp_err_t err = ESP_OK;
    DRIVER_CHECK(pwm_handle, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK(pwm_handle->registered_channel_mask & BIT(PWM_CHANNEL_R) &&
                 pwm_handle->registered_channel_mask & BIT(PWM_CHANNEL_G) &&
                 pwm_handle->registered_channel_mask & BIT(PWM_CHANNEL_B), "RGB Channel not registered", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK(pwm_handle->registered_channel_mask & BIT(PWM_CHANNEL_CCT_COLD) &&
                 pwm_handle->registered_channel_mask & BIT(PWM_CHANNEL_BRIGHTNESS_WARM), "CCT/Brightness or cold/warm Channel not registered", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK((value_r <= (1 << pwm_handle->ledc_config.duty_resolution)) &&
                 (value_g <= (1 << pwm_handle->ledc_config.duty_resolution)) &&
                 (value_b <= (1 << pwm_handle->ledc_config.duty_resolution)), "value out of range", return ESP_ERR_INVALID_ARG);
    DRIVER_CHECK((value_cct_c <= (1 << pwm_handle->ledc_config.duty_resolution)) &&
                 (value_b_w <= (1 << pwm_handle->ledc_config.duty_resolution)), "value out of range", return ESP_ERR_INVALID_ARG);

#if CONFIG_PM_ENABLE && CONFIG_IDF_TARGET_ESP32
    power_control_disable_sleep(pwm_handle);
#endif
    if (pwm_handle->phase_delay_flag == PWM_CHANNEL_COMPLEMENTARY_OUTPUT_FLAG) {
        pwm_handle->duty[PWM_CHANNEL_R] = value_r;
        pwm_handle->duty[PWM_CHANNEL_G] = value_g;
        pwm_handle->duty[PWM_CHANNEL_B] = value_b;
        pwm_handle->duty[PWM_CHANNEL_CCT_COLD] = value_cct_c;
        pwm_handle->duty[PWM_CHANNEL_BRIGHTNESS_WARM] = value_b_w;
        return pwm_commit_dynamic(pwm_handle);
    }

    //Must be set first
    err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_R], value_r, pwm_handle->hpoint[PWM_CHANNEL_R]);
    err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_G], value_g, pwm_handle->hpoint[PWM_CHANNEL_G]);
    err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_B], value_b, pwm_handle->hpoint[PWM_CHANNEL_B]);
    err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_CCT_COLD], value_cct_c, pwm_handle->hpoint[PWM_CHANNEL_CCT_COLD]);
    err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_BRIGHTNESS_WARM], value_b_w, pwm_handle->hpoint[PWM_CHANNEL_BRIGHTNESS_WARM]);

    //Update later
    err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_R]);
    err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_G]);
    err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_B]);
    err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_CCT_COLD]);
    err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[PWM_CHANNEL_BRIGHTNESS_WARM]);

    return err;
}

esp_err_t pwm_apply_registered(pwm_handle_t pwm_handle, uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_cct_c, uint16_t value_b_w)
{
    DRIVER_CHECK(pwm_handle, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);
    if (pwm_handle->phase_delay_flag != PWM_CHANNEL_COMPLEMENTARY_OUTPUT_FLAG) {
        /* Keep the original public path for the legacy/application-layer caller. */
        return pwm_set_rgbcctb_or_rgbcw_channel(pwm_handle, value_r, value_g, value_b,
                                                value_cct_c, value_b_w);
    }
    const uint16_t values[PWM_CHANNEL_MAX] = {value_r, value_g, value_b, value_cct_c, value_b_w};
    for (int ch = 0; ch < PWM_CHANNEL_MAX; ch++) {
        if (pwm_handle->registered_channel_mask & BIT(ch)) {
            DRIVER_CHECK((values[ch] <= (1 << pwm_handle->ledc_config.duty_resolution)), "value out of range", return ESP_ERR_INVALID_ARG);
        }
    }
    for (int ch = 0; ch < PWM_CHANNEL_MAX; ch++) {
        if (pwm_handle->registered_channel_mask & BIT(ch)) {
            pwm_handle->duty[ch] = values[ch];
        }
    }

#if CONFIG_PM_ENABLE && CONFIG_IDF_TARGET_ESP32
    power_control_disable_sleep(pwm_handle);
#endif
    return pwm_commit_dynamic(pwm_handle);
}

esp_err_t pwm_set_shutdown(pwm_handle_t pwm_handle)
{
    esp_err_t err = ESP_OK;
    DRIVER_CHECK(pwm_handle, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);

#if CONFIG_PM_ENABLE && CONFIG_IDF_TARGET_ESP32
    power_control_disable_sleep(pwm_handle);
#endif
    if (pwm_handle->phase_delay_flag == PWM_CHANNEL_COMPLEMENTARY_OUTPUT_FLAG) {
        memset(pwm_handle->duty, 0, sizeof(pwm_handle->duty));
        return pwm_commit_dynamic(pwm_handle);
    }

    for (int i = 0; i < PWM_CHANNEL_MAX; i++) {
        if (pwm_handle->registered_channel_mask & BIT(i)) {
            err |= ledc_set_duty_with_hpoint(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[i], 0, pwm_handle->hpoint[i]);
            err |= ledc_update_duty(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[i]);
        }
    }
    return err;
}

esp_err_t pwm_set_hw_fade(pwm_handle_t pwm_handle, pwm_channel_t channel, uint16_t value, int fade_ms)
{
    DRIVER_CHECK(pwm_handle, "pwm_init() must be called first", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK(channel >= 0 && channel < PWM_CHANNEL_MAX, "invalid channel", return ESP_ERR_INVALID_ARG);
    if (pwm_handle->phase_delay_flag == PWM_CHANNEL_COMPLEMENTARY_OUTPUT_FLAG) {
        /* Hardware fade only ramps the duty and cannot update the phase (hpoint),
         * so it is incompatible with dynamic complementary output. */
        return ESP_ERR_NOT_SUPPORTED;
    }
    DRIVER_CHECK(pwm_handle->registered_channel_mask & BIT(channel), "Channel %d not registered", return ESP_ERR_INVALID_STATE, channel);
    DRIVER_CHECK(value <= ((1 << pwm_handle->ledc_config.duty_resolution)), "value out of range", return ESP_ERR_INVALID_ARG);

#if CONFIG_PM_ENABLE && CONFIG_IDF_TARGET_ESP32
    power_control_disable_sleep(pwm_handle);
#endif
    if (0 != pwm_handle->hpoint[1]) {
        ESP_LOGE(TAG, "Unable to set hpoint during hardware fade, disable CONFIG_PWM_ENABLE_HW_FADE feature or call other set API");
        abort();
    }
    return ledc_set_fade_time_and_start(pwm_handle->ledc_config.speed_mode, pwm_handle->channels[channel], value, fade_ms, LEDC_FADE_NO_WAIT);
}

esp_err_t pwm_set_sleep(pwm_handle_t pwm_handle, bool is_enable)
{
    DRIVER_CHECK(pwm_handle, "invalid handle", return ESP_ERR_INVALID_STATE);
#if CONFIG_PM_ENABLE
#if CONFIG_IDF_TARGET_ESP32
    if (is_enable) {
        power_control_enable_sleep(pwm_handle);
    } else {
        power_control_disable_sleep(pwm_handle);
    }
#endif
    return ESP_OK;
#else
    ESP_LOGE(TAG, "You need to enable power management via menuconfig");
    return ESP_FAIL;
#endif
}
