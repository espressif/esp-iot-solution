/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "led_indicator_ledc.h"
#include "led_common.h"
#include "led_indicator_blink_default.h"

#define TAG "led_ledc"

#define LED_LEDC_CHECK(a, str, action, ...)                       \
    if (unlikely(!(a)))                                           \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        action;                                                   \
    }

#define LEDC_MAX_CHANNEL 8

typedef struct {
    bool is_init;                   /*!< Is the channel being used */
    bool is_active_level_high;      /*!< Set true if GPIO level is high when light is ON, otherwise false. */
    ledc_channel_t channel;         /*!< LEDC channel (0 - 7) */

} led_indicator_ledc_channel_t;

typedef struct {
    uint8_t channel_num;                                          /*!< Number of LEDC channels already in use */
    uint32_t max_duty;                                            /*!< Max duty cycle from duty_resolution : 2^duty_resolution -1 */
    led_indicator_ledc_channel_t ledc_channel[LEDC_MAX_CHANNEL];  /*!< LEDC channel state */
} led_indicator_ledc_t;

static led_indicator_ledc_t *s_ledc = NULL;

static esp_err_t led_indicator_ledc_init(void *param)
{
    esp_err_t ret = ESP_OK;
    const led_indicator_ledc_config_t *cfg = (led_indicator_ledc_config_t *)param;
    if (!s_ledc) {
        s_ledc = calloc(1, sizeof(led_indicator_ledc_t));
    }
    LED_LEDC_CHECK(NULL != s_ledc, "alloc fail", return ESP_ERR_NO_MEM);

    if (!cfg->timer_inited) {
        ledc_timer_config_t ledc_timer_cfg = LEDC_TIMER_CONFIG(cfg->timer_num);
        ret = ledc_timer_config(&ledc_timer_cfg);
        LED_LEDC_CHECK(ESP_OK == ret, "LEDC timer config fail!", goto EXIT);
        s_ledc->max_duty = pow(2, ledc_timer_cfg.duty_resolution) - 1;
    }

    ledc_channel_t ch = cfg->channel;
    LED_LEDC_CHECK(!s_ledc->ledc_channel[ch].is_init, "LEDC channel is already initialized!", goto EXIT);
    ledc_channel_config_t ledc_ch_cfg = LEDC_CHANNEL_CONFIG(cfg->timer_num, cfg->channel, cfg->gpio_num);
    ledc_ch_cfg.flags.output_invert = cfg->is_active_level_high ? false : true;
    ret = ledc_channel_config(&ledc_ch_cfg);
    LED_LEDC_CHECK(ESP_OK == ret, "ledc_channel_config fail!", goto EXIT);
    s_ledc->ledc_channel[ch].channel = ch;
    s_ledc->ledc_channel[ch].is_init = true;
    s_ledc->ledc_channel[ch].is_active_level_high = cfg->is_active_level_high;
    s_ledc->channel_num += 1;
    return ESP_OK;

EXIT:
    if (s_ledc) {
        free(s_ledc);
        s_ledc = NULL;
    }
    return ret;
}

static esp_err_t led_indicator_ledc_deinit(void *channel)
{
    uint32_t ch = (uint32_t)channel;
    LED_LEDC_CHECK(NULL != s_ledc, "LEDC is not initialized", return ESP_FAIL);
    LED_LEDC_CHECK(s_ledc->ledc_channel[ch].is_init, "LEDC channel doesn't init", return ESP_FAIL);
    s_ledc->ledc_channel[ch].is_init = false;
    s_ledc->channel_num -= 1;
    if (s_ledc->channel_num <= 0 && s_ledc) {
        free(s_ledc);
        s_ledc = NULL;
    }

    return ESP_OK;
}

static esp_err_t led_indicator_ledc_set_on_off(void *channel, bool on_off)
{
    esp_err_t ret;
    uint32_t ch = (uint32_t)channel;
    LED_LEDC_CHECK(s_ledc->ledc_channel[ch].is_init, "LEDC channel doesn't init", return ESP_FAIL);
    uint32_t duty = on_off ? s_ledc->max_duty : 0;

    ret = ledc_set_duty(LEDC_MODE, s_ledc->ledc_channel[ch].channel, duty);
    LED_LEDC_CHECK(ESP_OK == ret, "LEDC set duty error", return ret);
    ret = ledc_update_duty(LEDC_MODE, s_ledc->ledc_channel[ch].channel);
    LED_LEDC_CHECK(ESP_OK == ret, "LEDC update duty error", return ret);
    return ESP_OK;
}

static esp_err_t led_indicator_ledc_set_brightness(void *channel, uint32_t brightness)
{
    esp_err_t ret;
    uint32_t ch = (uint32_t)channel;
    LED_LEDC_CHECK(s_ledc->ledc_channel[ch].is_init, "LEDC channel doesn't init", return ESP_FAIL);
    LED_LEDC_CHECK(brightness <= UINT8_MAX, "brightness can't be larger than UINT8_MAX", return ESP_FAIL);
    ret = ledc_set_duty(LEDC_MODE, s_ledc->ledc_channel[ch].channel, brightness * s_ledc->max_duty / UINT8_MAX);
    LED_LEDC_CHECK(ESP_OK == ret, "LEDC set duty error", return ret);
    ret = ledc_update_duty(LEDC_MODE, s_ledc->ledc_channel[ch].channel);
    LED_LEDC_CHECK(ESP_OK == ret, "LEDC update duty error", return ret);
    return ESP_OK;
}

esp_err_t led_indicator_new_ledc_device(const led_indicator_config_t *led_config, const led_indicator_ledc_config_t *ledc_cfg, led_indicator_handle_t *handle)
{
    esp_err_t ret = ESP_OK;
    bool if_blink_default_list = false;

    ESP_LOGI(TAG, "LED Indicator Version: %d.%d.%d", LED_INDICATOR_VER_MAJOR, LED_INDICATOR_VER_MINOR, LED_INDICATOR_VER_PATCH);
    LED_INDICATOR_CHECK(ledc_cfg != NULL, "invalid config pointer", return ESP_ERR_INVALID_ARG);
    LED_INDICATOR_CHECK(GPIO_IS_VALID_GPIO(ledc_cfg->gpio_num), "invalid GPIO number", return ESP_ERR_INVALID_ARG);
    LED_INDICATOR_CHECK(ledc_cfg->timer_num < LEDC_TIMER_MAX, "invalid LEDC timer", return ESP_ERR_INVALID_ARG);
    LED_INDICATOR_CHECK(ledc_cfg->channel < LEDC_CHANNEL_MAX, "invalid LEDC channel", return ESP_ERR_INVALID_ARG);

    _led_indicator_com_config_t com_cfg = {0};
    _led_indicator_t *p_led_indicator = NULL;

    ret = led_indicator_ledc_init((void *)ledc_cfg);
    LED_INDICATOR_CHECK(ESP_OK == ret, "LEDC mode init failed", return ESP_FAIL);
    com_cfg.hardware_data = (void *)ledc_cfg->channel;
    com_cfg.hal_indicator_set_on_off = led_indicator_ledc_set_on_off;
    com_cfg.hal_indicator_deinit = led_indicator_ledc_deinit;
    com_cfg.hal_indicator_set_brightness = led_indicator_ledc_set_brightness;
    com_cfg.duty_resolution = LED_DUTY_8_BIT;

    if (led_config->blink_lists == NULL) {
        ESP_LOGI(TAG, "blink_lists is null, use default blink list");
        com_cfg.blink_lists = default_led_indicator_blink_lists;
        com_cfg.blink_list_num = DEFAULT_BLINK_LIST_NUM;
        if_blink_default_list = true;
    } else {
        com_cfg.blink_lists = led_config->blink_lists;
        com_cfg.blink_list_num = led_config->blink_list_num;
    }

    p_led_indicator = _led_indicator_create_com(&com_cfg);

    LED_INDICATOR_CHECK(NULL != p_led_indicator, "LED indicator create failed", return ESP_FAIL);
    _led_indicator_add_node(p_led_indicator);
    ESP_LOGI(TAG, "Indicator create successfully. type:LEDC mode, hardware_data:%p, blink_lists:%s", p_led_indicator->hardware_data, if_blink_default_list ? "default" : "custom");
    *handle = (led_indicator_handle_t)p_led_indicator;
    return ESP_OK;
}
