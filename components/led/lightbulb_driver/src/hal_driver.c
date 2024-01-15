/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <string.h>

#include <esp_log.h>
#include <esp_timer.h>

#include "hal_driver.h"
#include "lightbulb.h"

#ifdef CONFIG_USE_GPTIMER_GENERATE_TICKS
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#define FADE_TICKS_FROM_GPTIMER 1
#include "driver/gptimer.h"
#else
#warning The current IDF version does not support using the gptimer API
#endif
#endif

static const char *TAG = "hal_manage";

#if CONFIG_ENABLE_LIGHTBULB_DEBUG_LOG_OUTPUT
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define PROBE_GPIO 4
#define FADE_DEBUG_LOG_OUTPUT 0

static void create_gpio_probe(int gpio_num, int level)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL << gpio_num;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(gpio_num, level);
}

static void gpio_reverse(int gpio_num)
{
    static int level = 0;
    gpio_set_level(gpio_num, (level++) % 2);
}
#endif

#define CHANGE_RATE_MS                          (12)
#define FADE_CB_CHECK_MS                        (CHANGE_RATE_MS * 2)
#define HARDWARE_RETAIN_RATE_MS                 (CHANGE_RATE_MS)
#define MAX_TABLE_SIZE                          (256)
#define DEFAULT_GAMMA_CURVE                     (1.0)
#define HAL_OUT_MAX_CHANNEL                     (5)
#define ERROR_COUNT_THRESHOLD                   (1)

typedef esp_err_t (*x_init_t)(void *config, void(*hook_func)(void *));
typedef esp_err_t (*x_regist_channel_t)(int channel, int value);
typedef esp_err_t (*x_set_channel_t)(int channel, uint16_t value);
typedef esp_err_t (*x_set_rgb_channel_t)(uint16_t value_r, uint16_t value_g, uint16_t value_b);
typedef esp_err_t (*x_set_wy_or_cb_channel_t)(uint16_t value_w, uint16_t value_y);
typedef esp_err_t (*x_set_rgbwy_or_rgbct_channel_t)(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_w, uint16_t value_y);
typedef esp_err_t (*x_set_shutdown_t)(void);
typedef esp_err_t (*x_set_hw_fade_t)(int channel, uint16_t value, int fade_ms);
typedef esp_err_t (*x_set_init_mode_t)(bool set_wy_mode);
typedef esp_err_t (*x_deinit_t)(void);
typedef esp_err_t (*x_set_sleep_t)(bool enable_sleep);

typedef struct {
    lightbulb_driver_t type;
    const char *name;
    x_init_t init;
    x_set_channel_t set_channel;
    x_regist_channel_t regist_channel;
    x_set_rgb_channel_t set_rgb_channel;
    x_set_wy_or_cb_channel_t set_wy_or_ct_channel;
    x_set_rgbwy_or_rgbct_channel_t set_rgbwy_or_rgbct_channel;
    x_set_shutdown_t set_shutdown;
    x_deinit_t deinit;
    x_set_hw_fade_t set_hw_fade;
    x_set_init_mode_t set_init_mode;
    x_set_sleep_t set_sleep_status;
    uint32_t driver_grayscale_level;
    uint16_t hardware_allow_max_input_value;
    /* Supports all channels output at the same time */
    bool all_ch_allow_output;
    uint8_t channel_num;
} hal_obj_t;

typedef struct {
    float cur;
    float final;
    float step;
    float cycle;
    float num;
    float min;
} fade_data_t;

typedef struct {
    fade_data_t fade_data[HAL_OUT_MAX_CHANNEL];
    hal_obj_t *interface;
    bool use_hw_fade;
    bool use_balance;
    bool use_common_gamma_table;
    SemaphoreHandle_t fade_mutex;
#if FADE_TICKS_FROM_GPTIMER
    gptimer_handle_t fade_timer;
    TaskHandle_t notify_task;
    bool gptimer_is_active;
#else
    esp_timer_handle_t fade_timer;
#endif
} hal_context_t;

static uint16_t *s_rgb_gamma_table_group[4]     = { NULL };
uint16_t s_default_linear_table[MAX_TABLE_SIZE] = { 0 };
static float s_rgb_white_balance_coefficient[3] = { 1.0, 1.0, 1.0 };
static int s_err_count                          = 0;
static hardware_monitor_user_cb_t s_user_cb     = NULL;
static hal_context_t *s_hal_obj                 = NULL;

static hal_obj_t s_hal_obj_group[]           = {
#ifdef CONFIG_ENABLE_PWM_DRIVER
    {
        .type = DRIVER_ESP_PWM,
        .name = "PWM",
        .driver_grayscale_level = 1 << 12,
                                    .channel_num = 5,
                                    .hardware_allow_max_input_value = (1 << 12),
                                    .all_ch_allow_output = true,
                                    .init = (x_init_t)pwm_init,
                                    .set_channel = (x_set_channel_t)pwm_set_channel,
                                    .regist_channel = (x_regist_channel_t)pwm_regist_channel,
                                    .set_shutdown = (x_set_shutdown_t)pwm_set_shutdown,
                                    .set_hw_fade = (x_set_hw_fade_t)pwm_set_hw_fade,
                                    .deinit = (x_deinit_t)pwm_deinit,
                                    .set_sleep_status = (x_set_sleep_t)pwm_set_sleep,
    },
#endif
#ifdef CONFIG_ENABLE_SM2135E_DRIVER
    {
        .type = DRIVER_SM2135E,
        .name = "SM2135E",
        .driver_grayscale_level = 1 << 8,
                                    .channel_num = 5,
                                    .hardware_allow_max_input_value = (1 << 8) - 1,
                                    .all_ch_allow_output = false,
                                    .init = (x_init_t)sm2135e_init,
                                    .set_channel = (x_set_channel_t)_sm2135e_set_channel,
                                    .regist_channel = (x_regist_channel_t)sm2135e_regist_channel,
                                    .set_shutdown = (x_set_shutdown_t)sm2135e_set_shutdown,
                                    .deinit = (x_deinit_t)sm2135e_deinit,
                                    .set_init_mode = (x_set_init_mode_t)sm2135e_set_output_mode,
    },
#endif
#ifdef CONFIG_ENABLE_SM2135EH_DRIVER
    {
        .type = DRIVER_SM2135EH,
        .name = "SM2135EH",
        .driver_grayscale_level = 1 << 8,
                                    .channel_num = 5,
                                    .hardware_allow_max_input_value = (1 << 8) - 1,
                                    .all_ch_allow_output = true,
                                    .init = (x_init_t)sm2135eh_init,
                                    .set_channel = (x_set_channel_t)_sm2135eh_set_channel,
                                    .regist_channel = (x_regist_channel_t)sm2135eh_regist_channel,
                                    .set_shutdown = (x_set_shutdown_t)sm2135eh_set_shutdown,
                                    .deinit = (x_deinit_t)sm2135eh_deinit,
                                    .set_sleep_status = (x_set_sleep_t)sm2135eh_set_standby_mode,
    },
#endif
#ifdef CONFIG_ENABLE_SM2x35EGH_DRIVER
    {
        .type = DRIVER_SM2235EGH,
        .name = "SM2235EGH",
        .driver_grayscale_level = 1 << 10,
                                    .channel_num = 5,
                                    .hardware_allow_max_input_value = (1 << 10) - 1,
                                    .all_ch_allow_output = true,
                                    .init = (x_init_t)sm2x35egh_init,
                                    .set_channel = (x_set_channel_t)sm2x35egh_set_channel,
                                    .regist_channel = (x_regist_channel_t)sm2x35egh_regist_channel,
                                    .set_shutdown = (x_set_shutdown_t)sm2x35egh_set_shutdown,
                                    .deinit = (x_deinit_t)sm2x35egh_deinit,
                                    .set_sleep_status = (x_set_sleep_t)sm2x35egh_set_standby_mode,
    },
#endif
#ifdef CONFIG_ENABLE_SM2x35EGH_DRIVER
    {
        .type = DRIVER_SM2335EGH,
        .name = "SM2335EGH",
        .driver_grayscale_level = 1 << 10,
                                    .channel_num = 5,
                                    .hardware_allow_max_input_value = (1 << 10) - 1,
                                    .all_ch_allow_output = true,
                                    .init = (x_init_t)sm2x35egh_init,
                                    .set_channel = (x_set_channel_t)sm2x35egh_set_channel,
                                    .regist_channel = (x_regist_channel_t)sm2x35egh_regist_channel,
                                    .set_shutdown = (x_set_shutdown_t)sm2x35egh_set_shutdown,
                                    .deinit = (x_deinit_t)sm2x35egh_deinit,
                                    .set_sleep_status = (x_set_sleep_t)sm2x35egh_set_standby_mode,
    },
#endif
#ifdef CONFIG_ENABLE_BP5758D_DRIVER
    {
        .type = DRIVER_BP5758D,
        .name = "BP5758D",
        .driver_grayscale_level = 1 << 10,
                                    .channel_num = 5,
                                    .hardware_allow_max_input_value = (1 << 10) - 1,
                                    .all_ch_allow_output = true,
                                    .init = (x_init_t)bp5758d_init,
                                    .set_channel = (x_set_channel_t)bp5758d_set_channel,
                                    .regist_channel = (x_regist_channel_t)bp5758d_regist_channel,
                                    .set_shutdown = (x_set_shutdown_t)bp5758d_set_shutdown,
                                    .deinit = (x_deinit_t)bp5758d_deinit,
                                    .set_sleep_status = (x_set_sleep_t)bp5758d_set_standby_mode,
    },
#endif
#ifdef CONFIG_ENABLE_BP1658CJ_DRIVER
    {
        .type = DRIVER_BP1658CJ,
        .name = "BP1658CJ",
        .driver_grayscale_level = 1 << 10,
                                    .channel_num = 5,
                                    .hardware_allow_max_input_value = (1 << 10) - 1,
                                    .all_ch_allow_output = true,
                                    .init = (x_init_t)bp1658cj_init,
                                    .set_channel = (x_set_channel_t)bp1658cj_set_channel,
                                    .regist_channel = (x_regist_channel_t)bp1658cj_regist_channel,
                                    .set_shutdown = (x_set_shutdown_t)bp1658cj_set_shutdown,
                                    .deinit = (x_deinit_t)bp1658cj_deinit,
                                    .set_sleep_status = (x_set_sleep_t)bp1658cj_set_sleep_mode,
    },
#endif
#ifdef CONFIG_ENABLE_KP18058_DRIVER
    {
        .type = DRIVER_KP18058,
        .name = "KP18058",
        .driver_grayscale_level = 1 << 10,
                                    .channel_num = 5,
                                    .hardware_allow_max_input_value = (1 << 10) - 1,
                                    .all_ch_allow_output = true,
                                    .init = (x_init_t)kp18058_init,
                                    .set_channel = (x_set_channel_t)kp18058_set_channel,
                                    .regist_channel = (x_regist_channel_t)kp18058_regist_channel,
                                    .set_shutdown = (x_set_shutdown_t)kp18058_set_shutdown,
                                    .deinit = (x_deinit_t)kp18058_deinit,
                                    .set_sleep_status = (x_set_sleep_t)kp18058_set_standby_mode,
    },
#endif
#ifdef CONFIG_ENABLE_WS2812_DRIVER
    {
        .type = DRIVER_WS2812,
        .name = "WS2812",
        .driver_grayscale_level = 1 << 8,
                                    .channel_num = 3,
                                    .hardware_allow_max_input_value = (1 << 8) - 1,
                                    .all_ch_allow_output = false,
                                    .init = (x_init_t)ws2812_init,
                                    .set_rgb_channel = (x_set_rgb_channel_t)_ws2812_set_rgb_channel,
                                    .deinit = (x_deinit_t)ws2812_deinit,
    },
#endif
    {
        .type = DRIVER_SELECT_MAX,
    }
};

#if FADE_TICKS_FROM_GPTIMER
static void fade_cb(void *priv);
static IRAM_ATTR bool on_timer_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    BaseType_t task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_hal_obj->notify_task, &task_woken);

    return task_woken == pdTRUE;
}

static void fade_tick_task(void *arg)
{
    while (true) {
        ulTaskNotifyTake(true, portMAX_DELAY);
        fade_cb(NULL);
    }
    vTaskDelete(NULL);
}
#endif

static float final_processing(uint8_t channel, uint16_t src_value)
{
    if (channel >= CHANNEL_ID_COLD_CCT_WHITE) {
        if (src_value >= MAX_TABLE_SIZE) {
            ESP_LOGE(TAG, "The data is not supported and will be truncated to 255");
            src_value = 255;
        }
        return s_default_linear_table[src_value];
    }

    /* Only handle RGB channels */
    float target_value = src_value;
    if (s_hal_obj->use_balance) {
        target_value = s_rgb_white_balance_coefficient[channel] * src_value;
    }

    return target_value;
}

static esp_err_t gamma_table_create(uint16_t *output_gamma_table, uint16_t table_size, float gamma_curve_coefficient, int32_t grayscale_level)
{
    float value_tmp = 0;

    /**
     * @brief curve formula: y=a*x^(1/gamma)
     * x ∈ (0, (table_size)/table_size - 1)
     * a = target color bit depth
     * gamma = gamma curve coefficient
     */
    for (int i = 0; i < table_size; i++) {
        value_tmp = (float)(i) / (table_size - 1);
        value_tmp = powf(value_tmp, 1.0f / gamma_curve_coefficient);
        value_tmp *= grayscale_level;
        output_gamma_table[i] = (uint16_t)value_tmp;
        ESP_LOGD(TAG, "index:%4d %4f  %4d", i, value_tmp, output_gamma_table[i]);
    };

    return ESP_OK;
}

static void force_stop_all_ch(void)
{
    s_hal_obj->fade_data[0].num = 0;
    s_hal_obj->fade_data[1].num = 0;
    s_hal_obj->fade_data[2].num = 0;
    s_hal_obj->fade_data[3].num = 0;
    s_hal_obj->fade_data[4].num = 0;

    s_hal_obj->fade_data[0].cycle = 0;
    s_hal_obj->fade_data[1].cycle = 0;
    s_hal_obj->fade_data[2].cycle = 0;
    s_hal_obj->fade_data[3].cycle = 0;
    s_hal_obj->fade_data[4].cycle = 0;
}

static void cleanup(void)
{
    if (s_rgb_gamma_table_group[0]) {
        free(s_rgb_gamma_table_group[0]);
        s_rgb_gamma_table_group[0] = NULL;
    }
    if (s_rgb_gamma_table_group[1]) {
        free(s_rgb_gamma_table_group[1]);
        s_rgb_gamma_table_group[1] = NULL;
    }
    if (s_rgb_gamma_table_group[2]) {
        free(s_rgb_gamma_table_group[2]);
        s_rgb_gamma_table_group[2] = NULL;
    }
    if (s_rgb_gamma_table_group[3]) {
        free(s_rgb_gamma_table_group[3]);
        s_rgb_gamma_table_group[3] = NULL;
    }
    if (s_hal_obj->fade_mutex) {
        vSemaphoreDelete(s_hal_obj->fade_mutex);
        s_hal_obj->fade_mutex = NULL;
    }
#ifdef FADE_TICKS_FROM_GPTIMER
    if (s_hal_obj->fade_timer) {
        gptimer_disable(s_hal_obj->fade_timer);
        gptimer_del_timer(s_hal_obj->fade_timer);
    }
    if (s_hal_obj->notify_task) {
        vTaskDelete(s_hal_obj->notify_task);
    }
#else
    if (s_hal_obj->fade_timer) {
        esp_timer_delete(s_hal_obj->fade_timer);
        s_hal_obj->fade_timer = NULL;
    }
#endif
    if (s_hal_obj) {
        free(s_hal_obj);
        s_hal_obj = NULL;
    }
}

/**
 * @brief fade processing logic
 *
 * @note
 *
 * fade_data[channel].num -> Fade cycle, This value is related to CHANGE_RATE_MS and fade_ms time
 * fade_data[channel].step -> Fade step, < 0 indicates decrement, otherwise increment. The actual meaning is delta.
 * fade_data[channel].cycle -> This value is used for the actions.
 *
 * fade_data[channel].cur -> Current value
 * fade_data[channel].final -> Final value
 * fade_data[channel].min -> Minimum value
 * Final, min, cur are used to define a set of ranges, which will allow grayscale changes in arbitrary ranges, not from 0% to 100%.
 *
 *
 */
static void fade_cb(void *priv)
{
    esp_err_t err = ESP_OK;
    if (xSemaphoreTake(s_hal_obj->fade_mutex, 0) == pdFALSE) {
        return;
    }
    int idle_channel_num = 0;
    // 1. Check all channels
    for (int channel = 0; channel < s_hal_obj->interface->channel_num; channel++) {
        if (err != ESP_OK) {
            err = ESP_OK;
            s_err_count++;
        } else {
            s_err_count = 0;
        }
        if (s_err_count >= ERROR_COUNT_THRESHOLD) {
            s_err_count = 0;
            bool stop_flag = false;

            if (s_user_cb) {
                stop_flag = s_user_cb();
            }
            if (stop_flag == true) {
                force_stop_all_ch();
#ifdef FADE_TICKS_FROM_GPTIMER
                if (s_hal_obj->gptimer_is_active) {
                    s_hal_obj->gptimer_is_active = false;
                    gptimer_stop(s_hal_obj->fade_timer);
                }
#else
                esp_timer_stop(s_hal_obj->fade_timer);
#endif
                ESP_LOGE(TAG, "Hardware may be unresponsive, fade terminated");
            }
            xSemaphoreGive(s_hal_obj->fade_mutex);
            return;
        }

        // If this channel needs to be updated
        if (s_hal_obj->fade_data[channel].num > 0) {
            s_hal_obj->fade_data[channel].num--;

            // If this channel need to perform fade
            if (s_hal_obj->fade_data[channel].step) {
                s_hal_obj->fade_data[channel].cur = s_hal_obj->fade_data[channel].cur + s_hal_obj->fade_data[channel].step;

                // Range check
                if (s_hal_obj->fade_data[channel].cur > s_hal_obj->fade_data[channel].final && s_hal_obj->fade_data[channel].cycle) {
                    s_hal_obj->fade_data[channel].cur = s_hal_obj->fade_data[channel].final;
                }
                if (s_hal_obj->fade_data[channel].cur < s_hal_obj->fade_data[channel].min && s_hal_obj->fade_data[channel].cycle) {
                    s_hal_obj->fade_data[channel].cur = s_hal_obj->fade_data[channel].min;
                }

                // If this channel is not the last step of the fade
                if (s_hal_obj->fade_data[channel].num != 0) {
                    if (s_hal_obj->use_hw_fade && s_hal_obj->interface->type == DRIVER_ESP_PWM) {
                        err |= s_hal_obj->interface->set_hw_fade(channel, s_hal_obj->fade_data[channel].cur, HARDWARE_RETAIN_RATE_MS - 2);
                    } else if (s_hal_obj->interface->type != DRIVER_WS2812) {
                        err |= s_hal_obj->interface->set_channel(channel, s_hal_obj->fade_data[channel].cur);
                    } else {
                        //Nothing
                    }
#if FADE_DEBUG_LOG_OUTPUT
                    ESP_LOGW(TAG, "1.ch[%d]: cur:%f", channel, s_hal_obj->fade_data[channel].cur);
                    gpio_reverse(PROBE_GPIO);
#endif
                    // Update the final value of this channel, which may be the maximum value or the minimum value, depending on whether it is currently increasing or decreasing.
                } else {
                    s_hal_obj->fade_data[channel].cur = s_hal_obj->fade_data[channel].cycle && s_hal_obj->fade_data[channel].step < 0 ? s_hal_obj->fade_data[channel].min : s_hal_obj->fade_data[channel].final;
                    if (s_hal_obj->use_hw_fade && s_hal_obj->interface->type == DRIVER_ESP_PWM) {
                        err |= s_hal_obj->interface->set_hw_fade(channel, s_hal_obj->fade_data[channel].cur, HARDWARE_RETAIN_RATE_MS - 2);
                    } else if (s_hal_obj->interface->type != DRIVER_WS2812) {
                        err |= s_hal_obj->interface->set_channel(channel, s_hal_obj->fade_data[channel].cur);
                    } else {
                        //Nothing
                    }
#if FADE_DEBUG_LOG_OUTPUT
                    ESP_LOGW(TAG, "2..ch[%d]: cur:%f", channel, s_hal_obj->fade_data[channel].cur);
                    gpio_reverse(PROBE_GPIO);
#endif
                }
                // Because this channel does not need to perform fade, write the final value directly
            } else {
                if (s_hal_obj->use_hw_fade && s_hal_obj->interface->type == DRIVER_ESP_PWM) {
                    err |= s_hal_obj->interface->set_hw_fade(channel, s_hal_obj->fade_data[channel].cur, HARDWARE_RETAIN_RATE_MS - 2);
                } else if (s_hal_obj->interface->type != DRIVER_WS2812) {
                    err |= s_hal_obj->interface->set_channel(channel, s_hal_obj->fade_data[channel].cur);
                } else {
                    //Nothing
                }
#if FADE_DEBUG_LOG_OUTPUT
                ESP_LOGW(TAG, "3...ch[%d]: cur:%f", channel, s_hal_obj->fade_data[channel].cur);
                gpio_reverse(PROBE_GPIO);
#endif
            }
            // If this channel finishes updating, `fade_data[channel].num` will be less than 1, need to check if auto loop is needed again.
        } else if (s_hal_obj->fade_data[channel].cycle) {
            s_hal_obj->fade_data[channel].num = s_hal_obj->fade_data[channel].cycle - 1;

            // Set the value that needs to be updated in the next cycle
            if (s_hal_obj->fade_data[channel].step) {
                s_hal_obj->fade_data[channel].step *= -1;
                s_hal_obj->fade_data[channel].cur += s_hal_obj->fade_data[channel].step;
            } else {
                s_hal_obj->fade_data[channel].cur = (s_hal_obj->fade_data[channel].cur == s_hal_obj->fade_data[channel].final) ? s_hal_obj->fade_data[channel].min : s_hal_obj->fade_data[channel].final;
            }
            if (s_hal_obj->use_hw_fade && s_hal_obj->interface->type == DRIVER_ESP_PWM) {
                err |= s_hal_obj->interface->set_hw_fade(channel, s_hal_obj->fade_data[channel].cur, HARDWARE_RETAIN_RATE_MS - 2);
            } else if (s_hal_obj->interface->type != DRIVER_WS2812) {
                err |= s_hal_obj->interface->set_channel(channel, s_hal_obj->fade_data[channel].cur);
            } else {
                //Nothing
            }
#if FADE_DEBUG_LOG_OUTPUT
            ESP_LOGW(TAG, "4....ch[%d]: cur:%f setp:%f fin:%f", channel, s_hal_obj->fade_data[channel].cur, s_hal_obj->fade_data[channel].step, s_hal_obj->fade_data[channel].final);
            gpio_reverse(PROBE_GPIO);
#endif
            // Here all channels complete the expected behavior.
        } else {
            idle_channel_num++;
        }
    }

    // Enable multi-channel set only for ws2812 driver
    if (s_hal_obj->interface->type == DRIVER_WS2812) {
        s_hal_obj->interface->set_rgb_channel(s_hal_obj->fade_data[0].cur, s_hal_obj->fade_data[1].cur, s_hal_obj->fade_data[2].cur);
    }
#ifdef FADE_TICKS_FROM_GPTIMER
    if (idle_channel_num >= s_hal_obj->interface->channel_num) {
        if (s_hal_obj->gptimer_is_active) {
            s_hal_obj->gptimer_is_active = false;
            gptimer_stop(s_hal_obj->fade_timer);
        }
    }
#else
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    if (idle_channel_num >= s_hal_obj->interface->channel_num && esp_timer_is_active(s_hal_obj->fade_timer)) {
        esp_timer_stop(s_hal_obj->fade_timer);
    }
#else
    if (idle_channel_num >= s_hal_obj->interface->channel_num) {
        esp_timer_stop(s_hal_obj->fade_timer);
    }
#endif
#endif
    xSemaphoreGive(s_hal_obj->fade_mutex);
}

static void driver_default_hook_func(void *ctx)
{
    if (s_hal_obj->interface->type == DRIVER_ESP_PWM) {
        uint32_t grayscale_level = (uint32_t) ctx;
        s_hal_obj->interface->driver_grayscale_level = grayscale_level;
        s_hal_obj->interface->hardware_allow_max_input_value = grayscale_level;
    }
}

esp_err_t hal_output_init(hal_config_t *config, lightbulb_gamma_data_t *gamma, void *priv_data)
{
    esp_err_t err = ESP_FAIL;
    LIGHTBULB_CHECK(config, "config is null", return ESP_FAIL);
    LIGHTBULB_CHECK(!s_hal_obj, "already init done", return ESP_ERR_INVALID_STATE);

    s_hal_obj = calloc(1, sizeof(hal_context_t));
    LIGHTBULB_CHECK(s_hal_obj, "alloc fail", return ESP_ERR_NO_MEM);

    s_hal_obj->fade_mutex = xSemaphoreCreateBinary();
    LIGHTBULB_CHECK(s_hal_obj->fade_mutex, "mutex alloc fail", return ESP_ERR_NO_MEM);
    xSemaphoreGive(s_hal_obj->fade_mutex);

    for (int i = 0; i < DRIVER_SELECT_MAX; i++) {
        if (config->type == s_hal_obj_group[i].type) {
            s_hal_obj->interface = &(s_hal_obj_group[i]);
            break;
        } else if (s_hal_obj_group[i].type == DRIVER_SELECT_MAX) {
            break;
        }
    }
    LIGHTBULB_CHECK(s_hal_obj->interface, "Unable to find the corresponding driver function", goto EXIT);

    err = s_hal_obj->interface->init(config->driver_data, driver_default_hook_func);
    LIGHTBULB_CHECK(err == ESP_OK, "driver init fail", goto EXIT);

    if (gamma && gamma->table != NULL) {
        ESP_LOGW(TAG, "Use custom gamma table");
        if (gamma->table->table_size != 256) {
            ESP_LOGW(TAG, "Unsupported gamma table length");
            goto EXIT;
        }
        s_rgb_gamma_table_group[0] = calloc(MAX_TABLE_SIZE, sizeof(uint16_t));
        LIGHTBULB_CHECK(s_rgb_gamma_table_group[0], "red channel gamma table buffer alloc fail", goto EXIT);
        memcpy(s_rgb_gamma_table_group[0], gamma->table->custom_table[0], MAX_TABLE_SIZE);

        s_rgb_gamma_table_group[1] = calloc(MAX_TABLE_SIZE, sizeof(uint16_t));
        LIGHTBULB_CHECK(s_rgb_gamma_table_group[1], "green channel gamma table buffer alloc fail", goto EXIT);
        memcpy(s_rgb_gamma_table_group[1], gamma->table->custom_table[1], MAX_TABLE_SIZE);

        s_rgb_gamma_table_group[2] = calloc(MAX_TABLE_SIZE, sizeof(uint16_t));
        LIGHTBULB_CHECK(s_rgb_gamma_table_group[2], "blue channel gamma table buffer alloc fail", goto EXIT);
        memcpy(s_rgb_gamma_table_group[2], gamma->table->custom_table[2], MAX_TABLE_SIZE);

    } else if (gamma) {
        ESP_LOGW(TAG, "Generate gamma table with external parameter");
        if ((gamma->r_curve_coe == gamma->g_curve_coe) && (gamma->g_curve_coe == gamma->b_curve_coe)) {
            s_rgb_gamma_table_group[3] = calloc(MAX_TABLE_SIZE, sizeof(uint16_t));
            LIGHTBULB_CHECK(s_rgb_gamma_table_group[3], "common gamma table buffer alloc fail", goto EXIT);
            gamma_table_create(s_rgb_gamma_table_group[3], MAX_TABLE_SIZE, gamma->r_curve_coe, s_hal_obj->interface->driver_grayscale_level);
            s_hal_obj->use_common_gamma_table = true;
        } else {
            // R
            s_rgb_gamma_table_group[0] = calloc(MAX_TABLE_SIZE, sizeof(uint16_t));
            LIGHTBULB_CHECK(s_rgb_gamma_table_group[0], "red channel gamma table buffer alloc fail", goto EXIT);
            gamma_table_create(s_rgb_gamma_table_group[0], MAX_TABLE_SIZE, gamma->r_curve_coe, s_hal_obj->interface->driver_grayscale_level);
            // G
            s_rgb_gamma_table_group[1] = calloc(MAX_TABLE_SIZE, sizeof(uint16_t));
            LIGHTBULB_CHECK(s_rgb_gamma_table_group[1], "green channel gamma table buffer alloc fail", goto EXIT);
            gamma_table_create(s_rgb_gamma_table_group[1], MAX_TABLE_SIZE, gamma->g_curve_coe, s_hal_obj->interface->driver_grayscale_level);
            // B
            s_rgb_gamma_table_group[2] = calloc(MAX_TABLE_SIZE, sizeof(uint16_t));
            LIGHTBULB_CHECK(s_rgb_gamma_table_group[2], "blue channel gamma table buffer alloc fail", goto EXIT);
            gamma_table_create(s_rgb_gamma_table_group[2], MAX_TABLE_SIZE, gamma->b_curve_coe, s_hal_obj->interface->driver_grayscale_level);
        }
    } else {
        ESP_LOGW(TAG, "Generate table with default parameters");
        s_rgb_gamma_table_group[3] = calloc(MAX_TABLE_SIZE, sizeof(uint16_t));
        LIGHTBULB_CHECK(s_rgb_gamma_table_group[3], "common gamma table buffer alloc fail", goto EXIT);
        gamma_table_create(s_rgb_gamma_table_group[3], MAX_TABLE_SIZE, DEFAULT_GAMMA_CURVE, s_hal_obj->interface->driver_grayscale_level);
        s_hal_obj->use_common_gamma_table = true;
    }

    if (gamma && gamma->balance) {
        s_hal_obj->use_balance = true;
        s_rgb_white_balance_coefficient[0] = gamma->balance->r_balance_coe;
        s_rgb_white_balance_coefficient[1] = gamma->balance->g_balance_coe;
        s_rgb_white_balance_coefficient[2] = gamma->balance->b_balance_coe;
    }

    gamma_table_create(s_default_linear_table, MAX_TABLE_SIZE, 1.0, s_hal_obj->interface->driver_grayscale_level);
    s_default_linear_table[255] = s_hal_obj->interface->hardware_allow_max_input_value;

    if (s_hal_obj->use_common_gamma_table) {
        s_rgb_gamma_table_group[3][255] = s_hal_obj->interface->hardware_allow_max_input_value;
    } else {
        s_rgb_gamma_table_group[0][255] = s_hal_obj->interface->hardware_allow_max_input_value;
        s_rgb_gamma_table_group[1][255] = s_hal_obj->interface->hardware_allow_max_input_value;
        s_rgb_gamma_table_group[2][255] = s_hal_obj->interface->hardware_allow_max_input_value;
    }

    /**
     * @brief Differential configuration for different chips
     *
     */
    if (s_hal_obj->interface->type == DRIVER_SM2135E) {
        lightbulb_works_mode_t init_mode = *(lightbulb_works_mode_t *)(priv_data);
        bool wy_mode = (init_mode == WORK_COLOR) ? false : true;
        err = s_hal_obj->interface->set_init_mode(wy_mode);
        LIGHTBULB_CHECK(err == ESP_OK, "init mode fail", goto EXIT);
    } else if (s_hal_obj->interface->type == DRIVER_ESP_PWM) {
#if CONFIG_PWM_ENABLE_HW_FADE
        s_hal_obj->use_hw_fade = true;
#endif
    }

#ifdef FADE_TICKS_FROM_GPTIMER
    xTaskCreate(fade_tick_task, "fade_tick_task", CONFIG_LB_NOTIFY_TASK_STACK, NULL, CONFIG_LB_NOTIFY_TASK_PRIORITY, &s_hal_obj->notify_task);
    LIGHTBULB_CHECK(s_hal_obj->notify_task, "notify task create fail", goto EXIT);

    gptimer_clock_source_t clk;
#if CONFIG_IDF_TARGET_ESP32
    clk = GPTIMER_CLK_SRC_APB;
#if CONFIG_PM_ENABLE
    ESP_LOGW(TAG, "This clock source will be affected by the DFS of the power management");
#endif
#else
    clk = GPTIMER_CLK_SRC_XTAL;
#endif
    gptimer_config_t timer_config = {
        .clk_src = clk,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick = 1us
    };
    gptimer_new_timer(&timer_config, &s_hal_obj->fade_timer);

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = CHANGE_RATE_MS * 1000,
        .flags.auto_reload_on_alarm = true,
    };
    gptimer_event_callbacks_t cbs = {
        .on_alarm = on_timer_alarm_cb,
    };
    gptimer_register_event_callbacks(s_hal_obj->fade_timer, &cbs, NULL);
    gptimer_set_alarm_action(s_hal_obj->fade_timer, &alarm_config);
    gptimer_enable(s_hal_obj->fade_timer);
#else
    esp_timer_create_args_t timer_conf = {
        .callback = fade_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "fade_cb",
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
        .skip_unhandled_events = true,
#endif
    };
    err = esp_timer_create(&timer_conf, &s_hal_obj->fade_timer);
    LIGHTBULB_CHECK(err == ESP_OK, "esp_timer_create fail", goto EXIT);
#endif

    return ESP_OK;

EXIT:
    cleanup();

    return err;
}

esp_err_t hal_output_deinit()
{
    esp_err_t err = ESP_OK;
    LIGHTBULB_CHECK(s_hal_obj, "init() must be called first", return ESP_ERR_INVALID_STATE);

#ifdef FADE_TICKS_FROM_GPTIMER
    if (s_hal_obj->gptimer_is_active) {
        s_hal_obj->gptimer_is_active = false;
        gptimer_stop(s_hal_obj->fade_timer);
    }
#else
    esp_timer_stop(s_hal_obj->fade_timer);
#endif

    if (s_hal_obj->interface->set_shutdown) {
        err |= s_hal_obj->interface->set_shutdown();
    }

    err |= s_hal_obj->interface->deinit();

    cleanup();

    return err;
}

esp_err_t hal_regist_channel(int channel, gpio_num_t gpio_num)
{
    esp_err_t err = ESP_OK;
    LIGHTBULB_CHECK(s_hal_obj, "init() must be called first", return ESP_ERR_INVALID_STATE);

    if (s_hal_obj->interface->regist_channel) {
        err = s_hal_obj->interface->regist_channel(channel, gpio_num);
    }
    return err;
}

esp_err_t hal_set_channel(int channel, uint16_t value, uint16_t fade_ms)
{
    LIGHTBULB_CHECK(s_hal_obj, "init() must be called first", return ESP_ERR_INVALID_STATE);

#ifdef FADE_TICKS_FROM_GPTIMER
    if (s_hal_obj->gptimer_is_active) {
        s_hal_obj->gptimer_is_active = false;
        gptimer_stop(s_hal_obj->fade_timer);
    }
#else
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    // 1. Stop all fade_cb operations
    if (esp_timer_is_active(s_hal_obj->fade_timer)) {
        esp_timer_stop(s_hal_obj->fade_timer);
    }
#else
    esp_timer_stop(s_hal_obj->fade_timer);
#endif
#endif

    LIGHTBULB_CHECK(xSemaphoreTake(s_hal_obj->fade_mutex, pdMS_TO_TICKS(FADE_CB_CHECK_MS)) == pdTRUE, "Can't get mutex", return ESP_ERR_INVALID_STATE);

#ifdef CONFIG_ENABLE_DITHERING_CHECK
    // Allows to reduce fade time to increase resolution to avoid dithering
    uint32_t min_delta = UINT32_MAX;
    uint32_t max_valve = 0;

    fade_data_t data = { 0 };
    data = s_hal_obj->fade_data [channel];
    data.final = final_processing(channel, value);
    if (fabsf(data.final - data.cur) > 0) {
        min_delta = MIN(min_delta, fabsf(data.final - data.cur));
    }
    if (data.cur > max_valve) {
        max_valve = data.cur;
    }
    if (data.final > max_valve) {
        max_valve = data.final;
    }

    if (fade_ms > CHANGE_RATE_MS * 2 * min_delta) {
        fade_ms = min_delta * CHANGE_RATE_MS * 2;
        if (max_valve < 12) {
            fade_ms = fade_ms / 2;
        }
    }
#endif

    // 2. Get the current value of fade_data
    fade_data_t fade_data = s_hal_obj->fade_data[channel];

    // 3. Process the final value (e.g. with white balance calibration)
    fade_data.final = final_processing(channel, value);

    // 4. Count of calls to fade_cb function
    if (fade_ms < CHANGE_RATE_MS) {
        fade_data.num = 1;
    } else {
        fade_data.num = fade_ms / CHANGE_RATE_MS;
    }
    if (fabsf(fade_data.cur - fade_data.final) == 0) {
        fade_data.num = 1;
    }

    // 5. Count the step value required on each call to fade_ms
    fade_data.step = fabsf(fade_data.cur - fade_data.final) / fade_data.num;
    if (fade_data.cur > fade_data.final) {
        fade_data.step *= -1;
    }

    // 6. Fill parameters
    s_hal_obj->fade_data[channel].cur = fade_data.cur;
    s_hal_obj->fade_data[channel].final = fade_data.final;
    s_hal_obj->fade_data[channel].num = fade_data.num;
    s_hal_obj->fade_data[channel].step = fade_data.step;
    s_hal_obj->fade_data[channel].cur = fade_data.cur;
    s_hal_obj->fade_data[channel].cycle = 0; /* only for actions */
    s_hal_obj->fade_data[channel].min = 0; /* only for actions */
    xSemaphoreGive(s_hal_obj->fade_mutex);

    // 7. We need to execute a fade_cb immediately
    fade_cb(NULL);
#ifdef FADE_TICKS_FROM_GPTIMER
    if (gptimer_start(s_hal_obj->fade_timer) == ESP_OK) {
        s_hal_obj->gptimer_is_active = true;
    }
#else
    esp_timer_start_periodic(s_hal_obj->fade_timer, 1000 * CHANGE_RATE_MS);
#endif

    ESP_LOGD(TAG, "set channel:[%d] value:%d fade_ms:%d cur:%f final:%f step:%f num:%f", channel, value, fade_ms, fade_data.cur, fade_data.final, fade_data.step, fade_data.num);
    return ESP_OK;
}

esp_err_t hal_set_channel_group(uint16_t value[], uint8_t channel_mask, uint16_t fade_ms)
{
    LIGHTBULB_CHECK(s_hal_obj, "init() must be called first", return ESP_ERR_INVALID_STATE);

#ifdef FADE_TICKS_FROM_GPTIMER
    if (s_hal_obj->gptimer_is_active) {
        s_hal_obj->gptimer_is_active = false;
        gptimer_stop(s_hal_obj->fade_timer);
    }
#else
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    // 1. Stop all fade_cb operations
    if (esp_timer_is_active(s_hal_obj->fade_timer)) {
        esp_timer_stop(s_hal_obj->fade_timer);
    }
#else
    esp_timer_stop(s_hal_obj->fade_timer);
#endif
#endif

    LIGHTBULB_CHECK(xSemaphoreTake(s_hal_obj->fade_mutex, pdMS_TO_TICKS(FADE_CB_CHECK_MS)) == pdTRUE, "Can't get mutex", return ESP_ERR_INVALID_STATE);
    bool need_timer = false;

#ifdef CONFIG_ENABLE_DITHERING_CHECK
    // Allows to reduce fade time to increase resolution to avoid dithering
    uint32_t min_delta = UINT32_MAX;
    uint32_t max_valve = 0;
    for (int channel = 0; channel < HAL_OUT_MAX_CHANNEL; channel++) {
        fade_data_t fade_data [HAL_OUT_MAX_CHANNEL] = { 0 };
        fade_data [channel] = s_hal_obj->fade_data [channel];
        fade_data [channel].final = final_processing(channel, value [channel]);
        if (fabsf(fade_data [channel].final - fade_data [channel].cur) > 0) {
            min_delta = MIN(min_delta, fabsf(fade_data [channel].final - fade_data [channel].cur));
        }
        if (fade_data [channel].cur > max_valve) {
            max_valve = fade_data [channel].cur;
        }
        if (fade_data [channel].final > max_valve) {
            max_valve = fade_data [channel].final;
        }
    }

    if (fade_ms > CHANGE_RATE_MS * 2 * min_delta) {
        fade_ms = min_delta * CHANGE_RATE_MS * 2;
        if (max_valve < 12) {
            fade_ms = fade_ms / 2;
        }
    }
#endif

    // 2. loop update channels through mask bits
    fade_data_t fade_data[HAL_OUT_MAX_CHANNEL] = { 0 };
    for (int channel = 0; channel < s_hal_obj->interface->channel_num; channel++) {
        // 2.1 Unselected channels are skipped directly
        if ((channel_mask & BIT(channel)) == 0) {
            continue;
        }

        // 2.2 Get the current value of fade_data
        fade_data[channel] = s_hal_obj->fade_data[channel];

        // 2.3 Process the final value (e.g. with white balance calibration)
        fade_data[channel].final = final_processing(channel, value[channel]);

        // 2.4 Count of calls to fade_cb function
        if (fade_ms < CHANGE_RATE_MS) {
            fade_data[channel].num = 1;
        } else {
            fade_data[channel].num = fade_ms / CHANGE_RATE_MS;
        }
        if (fabsf(fade_data[channel].cur - fade_data[channel].final) == 0) {
            fade_data[channel].num = 1;
        }

        // 2.5 Count the step value required on each call to fade_ms
        fade_data[channel].step = fabsf(fade_data[channel].cur - fade_data[channel].final) / fade_data[channel].num;
        if (fade_data[channel].cur > fade_data[channel].final) {
            fade_data[channel].step *= -1;
        }

        // 2.6 Fill in other parameters
        fade_data[channel].cycle = 0; /* only for actions */
        fade_data[channel].min = 0; /* only for actions */

        // 2.7 If any channel nun > 1 then need to enable timer
        if (fade_data[channel].num > 1) {
            need_timer = true;
        }
        ESP_LOGD(TAG, "set group:[%d] value:%d fade_ms:%d cur:%f final:%f step:%f num:%f", channel, value[channel], fade_ms, fade_data[channel].cur, fade_data[channel].final, fade_data[channel].step, fade_data[channel].num);
    }
    memcpy(s_hal_obj->fade_data, fade_data, sizeof(fade_data));
    xSemaphoreGive(s_hal_obj->fade_mutex);

    // 3. We need to execute a fade_cb immediately, if need_timer is true then enable the timer to complete the fade operation
    fade_cb(NULL);
#ifdef FADE_TICKS_FROM_GPTIMER
    if (need_timer) {
        if (gptimer_start(s_hal_obj->fade_timer) == ESP_OK) {
            s_hal_obj->gptimer_is_active = true;
        }
    }
#else
    if (need_timer) {
        esp_timer_start_periodic(s_hal_obj->fade_timer, 1000 * CHANGE_RATE_MS);
    }
#endif
    return ESP_OK;
}

esp_err_t hal_start_channel_action(int channel, uint16_t value_min, uint16_t value_max, uint16_t period_ms, bool fade_flag)
{
    LIGHTBULB_CHECK(s_hal_obj, "init() must be called first", return ESP_ERR_INVALID_STATE);
    LIGHTBULB_CHECK((period_ms > CHANGE_RATE_MS * 2) || (period_ms == 0), "period_ms not allowed", return ESP_ERR_INVALID_ARG);

#ifdef FADE_TICKS_FROM_GPTIMER
    if (s_hal_obj->gptimer_is_active) {
        s_hal_obj->gptimer_is_active = false;
        gptimer_stop(s_hal_obj->fade_timer);
    }
#else
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    // 1. Stop all fade_cb operations
    if (esp_timer_is_active(s_hal_obj->fade_timer)) {
        esp_timer_stop(s_hal_obj->fade_timer);
    }
#else
    esp_timer_stop(s_hal_obj->fade_timer);
#endif
#endif

    LIGHTBULB_CHECK(xSemaphoreTake(s_hal_obj->fade_mutex, pdMS_TO_TICKS(FADE_CB_CHECK_MS)) == pdTRUE, "Can't get mutex", return ESP_ERR_INVALID_STATE);

    // 2. Get the current value of fade_data
    fade_data_t fade_data = s_hal_obj->fade_data[channel];

    // 3. Process the final value (e.g. with white balance calibration).
    fade_data.min = final_processing(channel, value_min);
    fade_data.final = final_processing(channel, value_max);
    // start actions from current value
    float cur = s_hal_obj->fade_data[channel].cur;

    /**
     * -0.1 is used to handle a specific scenario. When multiple channels are involved in the action, and the flag is set to 0,
     * if the current value (cur) of any channel is equal to the final value (fin), then the direction of change for that channel will be forcibly set to decreasing.
     * This may result in asynchronous changes across the channels, where some channels will increase while others will decrease.
     * To avoid this situation, we can simply use "final - 1" instead.
     *
     */
    cur = MIN(fade_data.final - 0.1, cur);
    cur = MAX(fade_data.min, cur);
    fade_data.cur = cur;

    // 4. Count the number of cycles. If cycle > 0, the timer will not stop
    fade_data.cycle = period_ms / 2 / CHANGE_RATE_MS;

    // 5. Count of calls to fade_cb function.
    // There is no need to consider the case where fade_data.cur and fade_data.final are equal, because fade_data.num will be updated again in fade_cb
    fade_data.num = (fade_flag) ? period_ms / 2 / CHANGE_RATE_MS : 0;

    // 6. Count the step value required on each call to fade_ms. The default is increment
    fade_data.step = (fade_flag) ? (fade_data.final - fade_data.min) / fade_data.num * 1 : 0;
    // TODO Decrease
    // fade_data.step = (fade_flag) ? (fade_data.final - fade_data.min) / fade_data.num * -1 : 0;

    // 7. Fill in other parameters
    s_hal_obj->fade_data[channel].cur = fade_data.cur;
    s_hal_obj->fade_data[channel].final = fade_data.final;
    s_hal_obj->fade_data[channel].num = fade_data.num;
    s_hal_obj->fade_data[channel].min = fade_data.min;
    s_hal_obj->fade_data[channel].step = fade_data.step;
    s_hal_obj->fade_data[channel].cycle = fade_data.cycle;
    xSemaphoreGive(s_hal_obj->fade_mutex);

    // 8. Actions need to be periodic, directly enabled
    fade_cb(NULL);
#ifdef FADE_TICKS_FROM_GPTIMER
    if (gptimer_start(s_hal_obj->fade_timer) == ESP_OK) {
        s_hal_obj->gptimer_is_active = true;
    }
#else
    esp_timer_start_periodic(s_hal_obj->fade_timer, 1000 * CHANGE_RATE_MS);
#endif

    ESP_LOGD(TAG, "start action:[%d] value:%d period_ms:%d cur:%f final:%f step:%f num:%f cycle:%f", channel, value_min, period_ms, fade_data.cur, fade_data.final, fade_data.step, fade_data.num, fade_data.cycle);
    return ESP_OK;
}

esp_err_t hal_start_channel_group_action(uint16_t value_min[], uint16_t value_max[], uint8_t channel_mask, uint16_t period_ms, bool fade_flag)
{
    LIGHTBULB_CHECK(s_hal_obj, "init() must be called first", return ESP_ERR_INVALID_STATE);
    LIGHTBULB_CHECK((period_ms > CHANGE_RATE_MS * 2) || (period_ms == 0), "period_ms not allowed", return ESP_ERR_INVALID_ARG);

#ifdef FADE_TICKS_FROM_GPTIMER
    if (s_hal_obj->gptimer_is_active) {
        s_hal_obj->gptimer_is_active = false;
        gptimer_stop(s_hal_obj->fade_timer);
    }
#else
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    // 1. Stop all fade_cb operations
    if (esp_timer_is_active(s_hal_obj->fade_timer)) {
        esp_timer_stop(s_hal_obj->fade_timer);
    }
#else
    esp_timer_stop(s_hal_obj->fade_timer);
#endif
#endif

    LIGHTBULB_CHECK(xSemaphoreTake(s_hal_obj->fade_mutex, pdMS_TO_TICKS(FADE_CB_CHECK_MS)) == pdTRUE, "Can't get mutex", return ESP_ERR_INVALID_STATE);

    // 2. loop update channels through mask bits
    fade_data_t fade_data[HAL_OUT_MAX_CHANNEL] = { 0 };
    for (int channel = 0; channel < s_hal_obj->interface->channel_num; channel++) {
        // 2.1 Unselected channels are skipped directly
        if ((channel_mask & BIT(channel)) == 0) {
            continue;
        }

        // 2.2 Get the current value of fade_data
        fade_data[channel] = s_hal_obj->fade_data[channel];

        // 2.3 Process the final value (e.g. with white balance calibration).
        fade_data[channel].min = final_processing(channel, value_min[channel]);
        fade_data[channel].final = final_processing(channel, value_max[channel]);
        float cur = s_hal_obj->fade_data[channel].cur;

        /**
         * -0.1 is used to handle a specific scenario. When multiple channels are involved in the action, and the flag is set to 0,
         * if the current value (cur) of any channel is equal to the final value (fin), then the direction of change for that channel will be forcibly set to decreasing.
         * This may result in asynchronous changes across the channels, where some channels will increase while others will decrease.
         * To avoid this situation, we can simply use "final - 1" instead.
         *
         */
        cur = MIN(fade_data[channel].final - 0.1, cur);
        cur = MAX(fade_data[channel].min, cur);
        fade_data[channel].cur = cur;

        // 4. Count the number of cycles. If cycle > 0, the timer will not stop
        fade_data[channel].cycle = period_ms / 2 / CHANGE_RATE_MS;

        // 5. Count of calls to fade_cb function.
        // There is no need to consider the case where fade_data.cur and fade_data.final are equal, because fade_data.num will be updated again in fade_cb
        fade_data[channel].num = (fade_flag) ? period_ms / 2 / CHANGE_RATE_MS : 0;

        // 6. Count the step value required on each call to fade_ms. The default is increment
        fade_data[channel].step = (fade_flag) ? (fade_data[channel].final - fade_data[channel].min) / fade_data[channel].num * 1 : 0;
        // TODO Decrease
        // fade_data[channel].step = (fade_flag) ? (fade_data[channel].final - fade_data[channel].min) / fade_data[channel].num * -1 : 0;

        ESP_LOGD(TAG, "start group action:[%d] value_min:%d value_max:%d period_ms:%d cur:%f final:%f step:%f num:%f cycle:%f", channel, value_min[channel], value_max[channel], period_ms, fade_data[channel].cur, fade_data[channel].final, fade_data[channel].step, fade_data[channel].num, fade_data[channel].cycle);
    };
    memcpy(s_hal_obj->fade_data, fade_data, sizeof(fade_data));
    xSemaphoreGive(s_hal_obj->fade_mutex);

    // 8. Actions need to be periodic, directly enabled
    fade_cb(NULL);
#ifdef FADE_TICKS_FROM_GPTIMER
    if (gptimer_start(s_hal_obj->fade_timer) == ESP_OK) {
        s_hal_obj->gptimer_is_active = true;
    }
#else
    esp_timer_start_periodic(s_hal_obj->fade_timer, 1000 * CHANGE_RATE_MS);
#endif

    return ESP_OK;
}

esp_err_t hal_stop_channel_action(uint8_t channel_mask)
{
    LIGHTBULB_CHECK(s_hal_obj, "init() must be called first", return ESP_ERR_INVALID_STATE);

#ifdef FADE_TICKS_FROM_GPTIMER
    if (s_hal_obj->gptimer_is_active) {
        s_hal_obj->gptimer_is_active = false;
        gptimer_stop(s_hal_obj->fade_timer);
    }
#else
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    // 1. Stop all fade_cb operations
    if (esp_timer_is_active(s_hal_obj->fade_timer)) {
        esp_timer_stop(s_hal_obj->fade_timer);
    }
#else
    esp_timer_stop(s_hal_obj->fade_timer);
#endif
#endif

    LIGHTBULB_CHECK(xSemaphoreTake(s_hal_obj->fade_mutex, pdMS_TO_TICKS(FADE_CB_CHECK_MS)) == pdTRUE, "Can't get mutex", return ESP_ERR_INVALID_STATE);

    // 2. loop update channels through mask bits
    fade_data_t fade_data[HAL_OUT_MAX_CHANNEL] = { 0 };
    for (int channel = 0; channel < s_hal_obj->interface->channel_num; channel++) {
        // 2.1 Unselected channels are skipped directly
        if ((channel_mask & BIT(channel)) == 0) {
            continue;
        }

        // 2.2 Just set the cycle
        fade_data[channel] = s_hal_obj->fade_data[channel];
        fade_data[channel].cycle = 0;
        ESP_LOGD(TAG, "stop action:[%d]", channel);
    };
    memcpy(s_hal_obj->fade_data, fade_data, sizeof(fade_data));
    xSemaphoreGive(s_hal_obj->fade_mutex);

    fade_cb(NULL);
#ifdef FADE_TICKS_FROM_GPTIMER
    if (gptimer_start(s_hal_obj->fade_timer) == ESP_OK) {
        s_hal_obj->gptimer_is_active = true;
    }
#else
    esp_timer_start_periodic(s_hal_obj->fade_timer, 1000 * CHANGE_RATE_MS);
#endif
    return ESP_OK;
}

esp_err_t hal_get_driver_feature(hal_feature_query_list_t type, void *out_data)
{
    LIGHTBULB_CHECK(s_hal_obj, "init() must be called first", return ESP_ERR_INVALID_STATE);
    LIGHTBULB_CHECK(out_data, "out_data is null", return ESP_ERR_INVALID_STATE);

    if (QUERY_IS_ALLOW_ALL_OUTPUT == type) {
        bool *_out_data = (bool *)out_data;
        *_out_data = (bool *)s_hal_obj->interface->all_ch_allow_output;
    } else if (QUERY_MAX_INPUT_VALUE == type) {
        uint16_t *_out_data = (uint16_t *)out_data;
        *_out_data = s_hal_obj->interface->hardware_allow_max_input_value;
    } else if (QUERY_GRAYSCALE_LEVEL == type) {
        uint32_t *_out_data = (uint32_t *)out_data;
        *_out_data = s_hal_obj->interface->driver_grayscale_level;
    } else if (QUERY_DRIVER_NAME == type) {
        char **_out_data = (char **)out_data;
        *_out_data = (char *)s_hal_obj->interface->name;
    } else {
        ESP_LOGE(TAG, "feature query(%d) not support", type);
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

esp_err_t hal_get_gamma_value(uint8_t r, uint8_t g, uint8_t b, uint16_t *out_r, uint16_t *out_g, uint16_t *out_b)
{
    LIGHTBULB_CHECK(s_hal_obj, "init() must be called first", return ESP_ERR_INVALID_STATE);
    LIGHTBULB_CHECK(out_r != NULL || out_g != NULL || out_b != NULL, "out_data is null", return ESP_ERR_INVALID_STATE);

    if (s_hal_obj->use_common_gamma_table) {
        *out_r = s_rgb_gamma_table_group[3][r];
        *out_g = s_rgb_gamma_table_group[3][g];
        *out_b = s_rgb_gamma_table_group[3][b];

        ESP_LOGD(TAG, "common gamma_value input:[%d %d %d] output:[%d %d %d]", r, g, b, *out_r, *out_g, *out_b);
        return ESP_OK;
    }

    *out_r = s_rgb_gamma_table_group[0][r];
    *out_g = s_rgb_gamma_table_group[1][g];
    *out_b = s_rgb_gamma_table_group[2][b];

    ESP_LOGD(TAG, " custom or external gamma_value input:[%d %d %d] output:[%d %d %d]", r, g, b, *out_r, *out_g, *out_b);
    return ESP_OK;
}

esp_err_t hal_get_linear_function_value(uint8_t input, uint16_t *output)
{
    LIGHTBULB_CHECK(s_hal_obj, "init() must be called first", return ESP_ERR_INVALID_STATE);
    LIGHTBULB_CHECK(output != NULL, "out_data is null", return ESP_ERR_INVALID_STATE);

    *output = s_default_linear_table[input];

    ESP_LOGD(TAG, "linear_function_value input:[%d] output:[%d]", input, *output);
    return ESP_OK;
}

esp_err_t hal_register_monitor_cb(hardware_monitor_user_cb_t cb)
{
    LIGHTBULB_CHECK(s_hal_obj, "init() must be called first", return ESP_ERR_INVALID_STATE);
    s_user_cb = cb;

    return ESP_OK;
}

esp_err_t hal_sleep_control(bool enable_sleep)
{
    LIGHTBULB_CHECK(s_hal_obj, "init() must be called first", return ESP_ERR_INVALID_STATE);

    if (s_hal_obj->interface->set_sleep_status) {
        s_hal_obj->interface->set_sleep_status(enable_sleep);
    } else {
        ESP_LOGW(TAG, "%s does not register sleep control functions", s_hal_obj->interface->name);
    }

    return ESP_OK;
}
