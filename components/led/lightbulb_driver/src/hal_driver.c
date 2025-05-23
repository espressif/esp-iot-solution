/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
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

#if CONFIG_ENABLE_LIGHTBULB_DEBUG
#define PROBE_GPIO 4
#define FADE_DEBUG_LOG_OUTPUT 0

void create_gpio_probe(int gpio_num, int level)
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

void gpio_reverse(int gpio_num)
{
    static int level = 0;
    gpio_set_level(gpio_num, (level++) % 2);
}
#endif

#define CHANGE_RATE_MS                          (12)                    // Interval in milliseconds between each update during the lightbulb's fading transition.
#define FADE_CB_CHECK_MS                        (CHANGE_RATE_MS * 4)    // Maximum wait time in milliseconds when fade_cb is blocked.
#define HARDWARE_RETAIN_RATE_MS                 (CHANGE_RATE_MS - 2)    // Safety margin in milliseconds for hardware fade interface in PWM scheme.
//#define s_hal_obj->interface->driver_grayscale_level                          (256)                   // Maximum size for linear and gamma correction tables.
#define DEFAULT_CURVE_COE                       (1.0)                   // Default coefficient for gamma correction curve.
#define HAL_OUT_MAX_CHANNEL                     (5)                     // Maximum number of output channels in the Hardware Abstraction Layer (HAL).
#define ERROR_COUNT_THRESHOLD                   (6)                     // Threshold for errors in the lower interface.

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
    int s_err_count;
    bool use_hw_fade;
    bool enable_multi_ch_write;
    uint16_t *table_group;
    uint16_t table_size;
    // R G B C W
    float balance_coefficient[5];
    SemaphoreHandle_t fade_mutex;
#if FADE_TICKS_FROM_GPTIMER
    gptimer_handle_t fade_timer;
    TaskHandle_t notify_task;
    bool gptimer_is_active;
#else
    esp_timer_handle_t fade_timer;
#endif
} hal_context_t;

static hal_context_t *s_hal_obj = NULL;

static hal_obj_t s_hal_obj_group[] = {
#ifdef CONFIG_ENABLE_PWM_DRIVER
    {
        .type = DRIVER_ESP_PWM,
        .name = "PWM",
        .driver_grayscale_level = (1 << 12),
        .channel_num = 5,
        .hardware_allow_max_input_value = (1 << 12),
        .init = (x_init_t)pwm_init,
        .set_channel = (x_set_channel_t)pwm_set_channel,
        .regist_channel = (x_regist_channel_t)pwm_regist_channel,
        .set_shutdown = (x_set_shutdown_t)pwm_set_shutdown,
        .set_hw_fade = (x_set_hw_fade_t)pwm_set_hw_fade,
        .deinit = (x_deinit_t)pwm_deinit,
        .set_sleep_status = (x_set_sleep_t)pwm_set_sleep,
    },
#endif
#ifdef CONFIG_ENABLE_SM2182E_DRIVER
    {
        .type = DRIVER_SM2182E,
        .name = "SM2182E",
        .driver_grayscale_level = (1 << 10),
        .channel_num = 5,
        .hardware_allow_max_input_value = (1 << 10) - 1,
        .init = (x_init_t)sm2182e_init,
        .set_wy_or_ct_channel = (x_set_wy_or_cb_channel_t)sm2182e_set_cw_channel,
        .regist_channel = (x_regist_channel_t)sm2182e_regist_channel,
        .set_shutdown = (x_set_shutdown_t)sm2182e_set_shutdown,
        .deinit = (x_deinit_t)sm2182e_deinit,
        .set_sleep_status = (x_set_sleep_t)sm2182e_set_standby_mode,
    },
#endif
#ifdef CONFIG_ENABLE_SM2135EH_DRIVER
    {
        .type = DRIVER_SM2135EH,
        .name = "SM2135EH",
        .driver_grayscale_level = (1 << 8),
        .channel_num = 5,
        .hardware_allow_max_input_value = (1 << 8) - 1,
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
        .type = DRIVER_SM2x35EGH,
        .name = "SM2235EGH",
        .driver_grayscale_level = (1 << 10),
        .channel_num = 5,
        .hardware_allow_max_input_value = (1 << 10) - 1,
        .init = (x_init_t)sm2x35egh_init,
        .set_channel = (x_set_channel_t)sm2x35egh_set_channel,
        .regist_channel = (x_regist_channel_t)sm2x35egh_regist_channel,
        .set_shutdown = (x_set_shutdown_t)sm2x35egh_set_shutdown,
        .deinit = (x_deinit_t)sm2x35egh_deinit,
        .set_sleep_status = (x_set_sleep_t)sm2x35egh_set_standby_mode,
    },
#endif
#ifdef CONFIG_ENABLE_BP57x8D_DRIVER
    {
        .type = DRIVER_BP57x8D,
        .name = "BP57x8D",
        .driver_grayscale_level = (1 << 10),
        .channel_num = 5,
        .hardware_allow_max_input_value = (1 << 10) - 1,
        .init = (x_init_t)bp57x8d_init,
        .set_channel = (x_set_channel_t)bp57x8d_set_channel,
        .regist_channel = (x_regist_channel_t)bp57x8d_regist_channel,
        .set_shutdown = (x_set_shutdown_t)bp57x8d_set_shutdown,
        .deinit = (x_deinit_t)bp57x8d_deinit,
        .set_sleep_status = (x_set_sleep_t)bp57x8d_set_standby_mode,
    },
#endif
#ifdef CONFIG_ENABLE_BP1658CJ_DRIVER
    {
        .type = DRIVER_BP1658CJ,
        .name = "BP1658CJ",
        .driver_grayscale_level = (1 << 10),
        .channel_num = 5,
        .hardware_allow_max_input_value = (1 << 10) - 1,
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
        .driver_grayscale_level = (1 << 10),
        .channel_num = 5,
        .hardware_allow_max_input_value = (1 << 10) - 1,
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
        .driver_grayscale_level = (1 << 8),
        .channel_num = 3,
        .hardware_allow_max_input_value = (1 << 8) - 1,
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
    return s_hal_obj->balance_coefficient[channel] * src_value;
}

esp_err_t hal_gamma_table_create(uint16_t *output_gamma_table, uint16_t table_size, float gamma_curve_coefficient, int32_t grayscale_level)
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
    if (s_hal_obj->table_group) {
        free(s_hal_obj->table_group);
        s_hal_obj->table_group = NULL;
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

#define WRITE_TO_HW(CH, VALUE)                                                                                      \
{                                                                                                                   \
    if(!s_hal_obj->enable_multi_ch_write) {                                                                         \
        if (s_hal_obj->use_hw_fade) {                                                                               \
            err |= s_hal_obj->interface->set_hw_fade(CH, s_hal_obj->fade_data[CH].cur, HARDWARE_RETAIN_RATE_MS);    \
        } else {                                                                                                    \
            err |= s_hal_obj->interface->set_channel(CH, s_hal_obj->fade_data[channel].cur);                        \
        }                                                                                                           \
   }                                                                                                                \
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
            s_hal_obj->s_err_count++;
        } else {
            s_hal_obj->s_err_count = 0;
        }
        if (s_hal_obj->s_err_count >= ERROR_COUNT_THRESHOLD) {
            s_hal_obj->s_err_count = 0;
            ESP_LOGE(TAG, "Hardware may be unresponsive, fade terminated");
            force_stop_all_ch();
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
                    WRITE_TO_HW(channel, s_hal_obj->fade_data[channel].cur);
#if FADE_DEBUG_LOG_OUTPUT
                    ESP_LOGW(TAG, "1.ch[%d]: cur:%f", channel, s_hal_obj->fade_data[channel].cur);
                    gpio_reverse(PROBE_GPIO);
#endif
                    // Update the final value of this channel, which may be the maximum value or the minimum value, depending on whether it is currently increasing or decreasing.
                } else {
                    WRITE_TO_HW(channel, s_hal_obj->fade_data[channel].cur);
#if FADE_DEBUG_LOG_OUTPUT
                    ESP_LOGW(TAG, "2..ch[%d]: cur:%f", channel, s_hal_obj->fade_data[channel].cur);
                    gpio_reverse(PROBE_GPIO);
#endif
                }
                // Because this channel does not need to perform fade, write the final value directly
            } else {
                WRITE_TO_HW(channel, s_hal_obj->fade_data[channel].cur);
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
            WRITE_TO_HW(channel, s_hal_obj->fade_data[channel].cur);
#if FADE_DEBUG_LOG_OUTPUT
            ESP_LOGW(TAG, "4....ch[%d]: cur:%f setp:%f fin:%f", channel, s_hal_obj->fade_data[channel].cur, s_hal_obj->fade_data[channel].step, s_hal_obj->fade_data[channel].final);
            gpio_reverse(PROBE_GPIO);
#endif
            // Here all channels complete the expected behavior.
        } else {
            idle_channel_num++;
        }
    }

    if (s_hal_obj->enable_multi_ch_write) {
        if (s_hal_obj->interface->set_rgb_channel) {
            s_hal_obj->interface->set_rgb_channel(s_hal_obj->fade_data[0].cur, s_hal_obj->fade_data[1].cur, s_hal_obj->fade_data[2].cur);
        } else if (s_hal_obj->interface->set_wy_or_ct_channel) {
            s_hal_obj->interface->set_wy_or_ct_channel(s_hal_obj->fade_data[3].cur, s_hal_obj->fade_data[4].cur);
        } else if (s_hal_obj->interface->set_rgbwy_or_rgbct_channel) {
            s_hal_obj->interface->set_rgbwy_or_rgbct_channel(s_hal_obj->fade_data[0].cur, s_hal_obj->fade_data[1].cur, s_hal_obj->fade_data[2].cur, s_hal_obj->fade_data[3].cur, s_hal_obj->fade_data[4].cur);
        }
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

esp_err_t hal_output_init(hal_config_t *config, lightbulb_gamma_config_t *gamma, void *priv_data)
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

    /**
     * @brief Differential configuration for different chips
     *
     */
    int table_size = s_hal_obj->interface->driver_grayscale_level;
    if (s_hal_obj->interface->type == DRIVER_ESP_PWM) {
#if CONFIG_PWM_ENABLE_HW_FADE
        s_hal_obj->use_hw_fade = true;
#endif
        // PWM
        // 10bit: 0~1024, size: 1024 + 1
        table_size += 1;

        // I2C Chip
        // 10bit: 0~1023, size: 1024
        //Nothing

        // WS2812 and SM2182E can only use multi-channel write
    } else if (s_hal_obj->interface->type == DRIVER_WS2812 || s_hal_obj->interface->type == DRIVER_SM2182E) {
        s_hal_obj->enable_multi_ch_write = true;
    }

    s_hal_obj->table_size = table_size;
    s_hal_obj->table_group = calloc(s_hal_obj->table_size, sizeof(uint16_t));
    LIGHTBULB_CHECK(s_hal_obj->table_group, "curve table buffer alloc fail", goto EXIT);

    //Currently only used as a mapping table, it will be used for fade to achieve curve sliding changes in the future
    float curve_coe = DEFAULT_CURVE_COE;
    hal_gamma_table_create(s_hal_obj->table_group, s_hal_obj->table_size, curve_coe, s_hal_obj->interface->hardware_allow_max_input_value);

    for (int i = 0; i < 5; i++) {
        float balance = gamma ? gamma->balance_coefficient[i] : 1.0;
        LIGHTBULB_CHECK(balance >= 0.0 && balance <= 1.0, "balance data error", goto EXIT);
        s_hal_obj->balance_coefficient[i] = balance;
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

    if ((fade_ms > CHANGE_RATE_MS * 2 * min_delta) && min_delta != 0) {
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

    if ((fade_ms > CHANGE_RATE_MS * 2 * min_delta) && min_delta != 0) {
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
    LIGHTBULB_CHECK(period_ms > CHANGE_RATE_MS * 2, "period_ms not allowed", return ESP_ERR_INVALID_ARG);

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
    LIGHTBULB_CHECK(period_ms > CHANGE_RATE_MS * 2, "period_ms not allowed", return ESP_ERR_INVALID_ARG);

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

        /*
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

    if (QUERY_MAX_INPUT_VALUE == type) {
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

esp_err_t hal_get_curve_table_value(uint16_t input, uint16_t *output)
{
    LIGHTBULB_CHECK(s_hal_obj, "init() must be called first", return ESP_ERR_INVALID_STATE);
    LIGHTBULB_CHECK(output, "out_data is null", return ESP_ERR_INVALID_STATE);

    *output = s_hal_obj->table_group[input];

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
