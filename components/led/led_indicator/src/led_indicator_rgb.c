/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "led_indicator_rgb.h"
#include "led_common.h"
#include "led_convert.h"
#include "led_indicator_blink_default.h"

#define TAG "led_rgb"

#define LED_RGB_CHECK(a, str, action, ...)                       \
    if (unlikely(!(a)))                                           \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        action;                                                   \
    }

typedef struct {
    bool is_active_level_high;     /*!< Set true if GPIO level is high when light is ON, otherwise false. */
    ledc_channel_t rgb_channel[3]; /*!< RGB Channels */
    uint32_t max_duty;             /*!< Max duty cycle from duty_resolution : 2^duty_resolution -1 */
    led_indicator_ihsv_t hsv;      /*!< HSV: H [0-360] - 9 bits, S [0-255] - 8 bits, V [0-255] - 8 bits*/
} led_rgb_t;

static esp_err_t led_indicator_rgb_init(void *param, void **ret_rgb)
{
    esp_err_t ret = ESP_OK;
    const led_indicator_rgb_config_t *cfg = (const led_indicator_rgb_config_t *)param;
    LED_RGB_CHECK(NULL != cfg, "param pointer invalid", return ESP_ERR_INVALID_ARG);
    led_rgb_t *rgb = calloc(1, sizeof(led_rgb_t));
    if (!cfg->timer_inited) {
        ledc_timer_config_t ledc_timer_cfg = LEDC_TIMER_CONFIG(cfg->timer_num);
        ret = ledc_timer_config(&ledc_timer_cfg);
        LED_RGB_CHECK(ESP_OK == ret, "LEDC timer config fail!", goto EXIT);
        rgb->max_duty = pow(2, ledc_timer_cfg.duty_resolution) - 1;
    }

    ledc_channel_config_t red_ch_cfg = LEDC_CHANNEL_CONFIG(cfg->timer_num, cfg->red_channel, cfg->red_gpio_num);
    red_ch_cfg.flags.output_invert = cfg->is_active_level_high ? false : true;
    ret = ledc_channel_config(&red_ch_cfg);
    LED_RGB_CHECK(ESP_OK == ret, "red ledc_channel_config fail!", goto EXIT);
    ledc_channel_config_t green_ch_cfg = LEDC_CHANNEL_CONFIG(cfg->timer_num, cfg->green_channel, cfg->green_gpio_num);
    green_ch_cfg.flags.output_invert = cfg->is_active_level_high ? false : true;
    ret = ledc_channel_config(&green_ch_cfg);
    LED_RGB_CHECK(ESP_OK == ret, "green ledc_channel_config fail!", goto EXIT);
    ledc_channel_config_t blue_ch_cfg = LEDC_CHANNEL_CONFIG(cfg->timer_num, cfg->blue_channel, cfg->blue_gpio_num);
    blue_ch_cfg.flags.output_invert = cfg->is_active_level_high ? false : true;
    ret = ledc_channel_config(&blue_ch_cfg);
    LED_RGB_CHECK(ESP_OK == ret, "blud ledc_channel_config fail!", goto EXIT);
    rgb->rgb_channel[0] = cfg->red_channel;
    rgb->rgb_channel[1] = cfg->green_channel;
    rgb->rgb_channel[2] = cfg->blue_channel;
    rgb->is_active_level_high = cfg->is_active_level_high;
    *ret_rgb = (void *)rgb;
    return ESP_OK;
EXIT:
    if (rgb) {
        free(rgb);
    }
    return ret;
}

static esp_err_t led_indicator_rgb_deinit(void *rgb_handle)
{
    LED_RGB_CHECK(NULL != rgb_handle, "rgb_handle pointer invalid", return ESP_ERR_INVALID_ARG);
    free(rgb_handle);
    return ESP_OK;
}

static esp_err_t led_indicator_rgb_set_duty(led_rgb_t *p_rgb, uint32_t rgb[])
{
    esp_err_t ret;
    for (int i = 0; i < 3; i++) {
        ret = ledc_set_duty(LEDC_MODE, p_rgb->rgb_channel[i], rgb[i]);
        LED_RGB_CHECK(ESP_OK == ret, "LEDC set duty error", return ret);
        ret = ledc_update_duty(LEDC_MODE, p_rgb->rgb_channel[i]);
        LED_RGB_CHECK(ESP_OK == ret, "LEDC update duty error", return ret);
    }
    return ESP_OK;
}

static esp_err_t led_indicator_rgb_set_on_off(void *rgb_handle, bool on_off)
{
    esp_err_t ret;
    LED_RGB_CHECK(NULL != rgb_handle, "rgb_handle pointer invalid", return ESP_ERR_INVALID_ARG);
    led_rgb_t *p_rgb = (led_rgb_t *)rgb_handle;

    if (on_off) {
        uint32_t rgb[3] = {0};
        led_indicator_hsv2rgb(p_rgb->hsv.value, &rgb[0], &rgb[1], &rgb[2]);

        for (int i = 0; i < 3; i++) {
            rgb[i] = rgb[i] * p_rgb->max_duty / UINT8_MAX;
        }

        ret = led_indicator_rgb_set_duty(p_rgb, rgb);
        LED_RGB_CHECK(ESP_OK == ret, "LEDC set duty error", return ret);
    } else {
        uint32_t rgb[3] = {0, 0, 0};
        ret = led_indicator_rgb_set_duty(p_rgb, rgb);
        LED_RGB_CHECK(ESP_OK == ret, "LEDC set duty error", return ret);
    }

    return ESP_OK;
}

static esp_err_t led_indicator_rgb_set_rgb(void *rgb_handle, uint32_t rgb_value)
{
    esp_err_t ret;
    led_rgb_t *p_rgb = (led_rgb_t *)rgb_handle;
    uint32_t rgb[3];
    rgb[0] = GET_RED(rgb_value) * p_rgb->max_duty / UINT8_MAX;
    rgb[1] = GET_GREEN(rgb_value) * p_rgb->max_duty / UINT8_MAX;
    rgb[2] = GET_BLUE(rgb_value) * p_rgb->max_duty / UINT8_MAX;

    ret = led_indicator_rgb_set_duty(p_rgb, rgb);
    LED_RGB_CHECK(ESP_OK == ret, "LEDC set duty error", return ret);
    p_rgb->hsv.value = led_indicator_rgb2hsv(rgb_value);
    return ESP_OK;
}

static esp_err_t led_indicator_rgb_set_hsv(void *rgb_handle, uint32_t hsv_value)
{
    esp_err_t ret;
    led_rgb_t *p_rgb = (led_rgb_t *)rgb_handle;
    uint32_t rgb[3];
    led_indicator_hsv2rgb(hsv_value, &rgb[0], &rgb[1], &rgb[2]);
    for (int i = 0; i < 3; i++) {
        rgb[i] = rgb[i] * p_rgb->max_duty / UINT8_MAX;
    }

    ret = led_indicator_rgb_set_duty(p_rgb, rgb);
    LED_RGB_CHECK(ESP_OK == ret, "LEDC set duty error", return ret);
    p_rgb->hsv.value = hsv_value;
    return ESP_OK;
}

static esp_err_t led_indicator_rgb_set_brightness(void *rgb_handle, uint32_t brightness)
{
    esp_err_t ret;
    led_rgb_t *p_rgb = (led_rgb_t *)rgb_handle;
    uint32_t rgb[3];
    p_rgb->hsv.v = brightness;
    led_indicator_hsv2rgb(p_rgb->hsv.value, &rgb[0], &rgb[1], &rgb[2]);
    for (int i = 0; i < 3; i++) {
        rgb[i] = rgb[i] * p_rgb->max_duty / UINT8_MAX;
    }

    ret = led_indicator_rgb_set_duty(p_rgb, rgb);
    LED_RGB_CHECK(ESP_OK == ret, "LEDC set duty error", return ret);
    return ESP_OK;
}

esp_err_t led_indicator_new_rgb_device(const led_indicator_config_t *led_config, const led_indicator_rgb_config_t *rgb_cfg, led_indicator_handle_t *handle)
{
    esp_err_t ret = ESP_OK;
    bool if_blink_default_list = false;

    ESP_LOGI(TAG, "LED Indicator Version: %d.%d.%d", LED_INDICATOR_VER_MAJOR, LED_INDICATOR_VER_MINOR, LED_INDICATOR_VER_PATCH);
    LED_INDICATOR_CHECK(rgb_cfg != NULL, "invalid config pointer", return ESP_ERR_INVALID_ARG);
    LED_INDICATOR_CHECK(rgb_cfg->timer_num < LEDC_TIMER_MAX, "invalid timer number", return ESP_ERR_INVALID_ARG);
    LED_INDICATOR_CHECK(GPIO_IS_VALID_GPIO(rgb_cfg->red_gpio_num), "invalid red GPIO number", return ESP_ERR_INVALID_ARG);
    LED_INDICATOR_CHECK(GPIO_IS_VALID_GPIO(rgb_cfg->green_gpio_num), "invalid green GPIO number", return ESP_ERR_INVALID_ARG);
    LED_INDICATOR_CHECK(GPIO_IS_VALID_GPIO(rgb_cfg->blue_gpio_num), "invalid blue GPIO number", return ESP_ERR_INVALID_ARG);
    LED_INDICATOR_CHECK(rgb_cfg->red_channel < LEDC_CHANNEL_MAX, "invalid red LEDC channel", return ESP_ERR_INVALID_ARG);
    LED_INDICATOR_CHECK(rgb_cfg->green_channel < LEDC_CHANNEL_MAX, "invalid green LEDC channel", return ESP_ERR_INVALID_ARG);
    LED_INDICATOR_CHECK(rgb_cfg->blue_channel < LEDC_CHANNEL_MAX, "invalid blue LEDC channel", return ESP_ERR_INVALID_ARG);

    _led_indicator_com_config_t com_cfg = {0};
    _led_indicator_t *p_led_indicator = NULL;

    void *hardware_data = NULL;
    ret = led_indicator_rgb_init((void *)rgb_cfg, &hardware_data);
    LED_INDICATOR_CHECK(ESP_OK == ret, "RGB mode init failed", return ESP_FAIL);
    com_cfg.hardware_data = hardware_data;
    com_cfg.hal_indicator_set_on_off = led_indicator_rgb_set_on_off;
    com_cfg.hal_indicator_deinit = led_indicator_rgb_deinit;
    com_cfg.hal_indicator_set_brightness = led_indicator_rgb_set_brightness;
    com_cfg.hal_indicator_set_rgb = led_indicator_rgb_set_rgb;
    com_cfg.hal_indicator_set_hsv = led_indicator_rgb_set_hsv;
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
    ESP_LOGI(TAG, "Indicator create successfully. type:LED RGB mode, hardware_data:%p, blink_lists:%s", p_led_indicator->hardware_data, if_blink_default_list ? "default" : "custom");
    *handle = (led_indicator_handle_t)p_led_indicator;
    return ESP_OK;
}
