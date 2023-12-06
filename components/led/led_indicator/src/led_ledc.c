/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include "driver/ledc.h"
#include "esp_log.h"
#include "led_ledc.h"
#include "led_common.h"

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

esp_err_t led_indicator_ledc_init(void *param)
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

esp_err_t led_indicator_ledc_deinit(void *channel)
{
    uint32_t ch = (uint32_t)channel;
    LED_LEDC_CHECK(NULL != s_ledc, "LEDC is not initialized", return ESP_FAIL);
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
    uint32_t duty = on_off ? s_ledc->max_duty : 0;
    if (!s_ledc->ledc_channel[ch].is_active_level_high) {
        duty = s_ledc->max_duty - duty;
    }

    ret = ledc_set_duty(LEDC_MODE, s_ledc->ledc_channel[ch].channel, duty);
    LED_LEDC_CHECK(ESP_OK == ret, "LEDC set duty error", return ret);
    ret = ledc_update_duty(LEDC_MODE, s_ledc->ledc_channel[ch].channel);
    LED_LEDC_CHECK(ESP_OK == ret, "LEDC update duty error", return ret);
    return ESP_OK;
}

esp_err_t led_indicator_ledc_set_brightness(void *channel, uint32_t brightness)
{
    esp_err_t ret;
    uint32_t ch = (uint32_t)channel;
    LED_LEDC_CHECK(s_ledc->ledc_channel[ch].is_init, "LEDC channel does't init", return ESP_FAIL);
    LED_LEDC_CHECK(brightness <= UINT8_MAX, "brightness can't be larger than UINT8_MAX", return ESP_FAIL);
    brightness = s_ledc->ledc_channel[ch].is_active_level_high ? brightness : (UINT8_MAX - brightness);
    ret = ledc_set_duty(LEDC_MODE, s_ledc->ledc_channel[ch].channel, brightness * s_ledc->max_duty / UINT8_MAX);
    LED_LEDC_CHECK(ESP_OK == ret, "LEDC set duty error", return ret);
    ret = ledc_update_duty(LEDC_MODE, s_ledc->ledc_channel[ch].channel);
    LED_LEDC_CHECK(ESP_OK == ret, "LEDC update duty error", return ret);
    return ESP_OK;
}
