/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include "driver/ledc.h"
#include "esp_log.h"
#include "led_ledc.h"

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
    ledc_mode_t speed_mode;         /*!< LEDC speed speed_mode, high-speed mode or low-speed mode */
    ledc_channel_t channel;         /*!< LEDC channel (0 - 7) */
} led_indicator_ledc_channel_t;

typedef struct {
    bool ledc_timer_is_init;                                      /*!< Avoid repeated init ledc_timer */
    uint8_t channel_num;                                          /*!< Number of LEDC channels already in use */
    uint32_t max_duty;                                            /*!< Max duty cycle from duty_resolution : 2^duty_resolution -1 */
    led_indicator_ledc_channel_t ledc_channel[LEDC_MAX_CHANNEL];  /*!< LEDC channel state */
} led_indicator_ledc_t;

static led_indicator_ledc_t *s_ledc = NULL;

esp_err_t led_indicator_ledc_init(void *param)
{
    esp_err_t ret = ESP_OK;
    uint32_t ch;
    const led_indicator_ledc_config_t *cfg = (led_indicator_ledc_config_t *)param;
    if (!s_ledc) {
        s_ledc = calloc(1, sizeof(led_indicator_ledc_t));
    }
    LED_LEDC_CHECK(NULL != s_ledc, "alloc fail", return ESP_ERR_NO_MEM);

    if ( !s_ledc->ledc_timer_is_init ) {
        ret = ledc_timer_config(cfg->ledc_timer_config);
        LED_LEDC_CHECK(ESP_OK == ret, "LEDC timer config fail!", goto EXIT);
        s_ledc->ledc_timer_is_init = true;
        s_ledc->max_duty = pow(2, cfg->ledc_timer_config->duty_resolution) - 1;
    }

    ch = cfg->ledc_channel_config->channel;
    LED_LEDC_CHECK(!s_ledc->ledc_channel[ch].is_init, "LEDC channel is already initialized!", goto EXIT);
    ret = ledc_channel_config(cfg->ledc_channel_config);
    LED_LEDC_CHECK(ESP_OK == ret, "ledc_channel_config fail!", goto EXIT);
    s_ledc->ledc_channel[ch].speed_mode = cfg->ledc_channel_config->speed_mode;
    s_ledc->ledc_channel[ch].channel = cfg->ledc_channel_config->channel;
    s_ledc->ledc_channel[ch].is_init = true;
    s_ledc->channel_num += 1;
    return ESP_OK;

EXIT:
    if (s_ledc) {
        free(s_ledc);
        s_ledc = NULL;
    }
    return ret;
}

esp_err_t led_indicator_ledc_deinit(void *channel)
{
    uint32_t ch = (uint32_t)channel;
    LED_LEDC_CHECK(s_ledc->ledc_channel[ch].is_init, "LEDC channel does't init", return ESP_FAIL);
    s_ledc->ledc_channel[ch].is_init = false;
    s_ledc->channel_num -= 1;
    if (s_ledc->channel_num <= 0 && s_ledc) {
        free(s_ledc);
        s_ledc = NULL;
    }

    return ESP_OK;
}

esp_err_t led_indicator_ledc_set_on_off(void *channel, bool on_off)
{
    esp_err_t ret;
    uint32_t ch = (uint32_t)channel;
    LED_LEDC_CHECK(s_ledc->ledc_channel[ch].is_init, "LEDC channel does't init", return ESP_FAIL);
    if (on_off) {
        ret = ledc_set_duty(s_ledc->ledc_channel[ch].speed_mode, s_ledc->ledc_channel[ch].channel, s_ledc->max_duty);
        LED_LEDC_CHECK(ESP_OK == ret, "LEDC set duty error", return ret);
    } else {
        ret = ledc_set_duty(s_ledc->ledc_channel[ch].speed_mode, s_ledc->ledc_channel[ch].channel, on_off);
        LED_LEDC_CHECK(ESP_OK == ret, "LEDC set duty error", return ret);
    }

    ret = ledc_update_duty(s_ledc->ledc_channel[ch].speed_mode, s_ledc->ledc_channel[ch].channel);
    LED_LEDC_CHECK(ESP_OK == ret, "LEDC update duty error", return ret);
    return ESP_OK;
}

esp_err_t led_indicator_ledc_set_brightness(void *channel, uint32_t brightness)
{
    esp_err_t ret;
    uint32_t ch = (uint32_t)channel;
    LED_LEDC_CHECK(s_ledc->ledc_channel[ch].is_init, "LEDC channel does't init", return ESP_FAIL);
    LED_LEDC_CHECK(s_ledc->max_duty > brightness, "brightness can't be larger than (2^max_duty - 1)", return ESP_FAIL);
    ret = ledc_set_duty(s_ledc->ledc_channel[ch].speed_mode, s_ledc->ledc_channel[ch].channel, brightness);
    LED_LEDC_CHECK(ESP_OK == ret, "LEDC set duty error", return ret);
    ret = ledc_update_duty(s_ledc->ledc_channel[ch].speed_mode, s_ledc->ledc_channel[ch].channel);
    LED_LEDC_CHECK(ESP_OK == ret, "LEDC update duty error", return ret);
    return ESP_OK;
}