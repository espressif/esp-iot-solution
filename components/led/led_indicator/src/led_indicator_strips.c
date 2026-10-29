/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include "esp_log.h"
#include "led_indicator_strips.h"
#include "led_strip.h"
#include "led_strip_types.h"
#include "led_convert.h"
#include "led_common.h"
#include "led_indicator_blink_default.h"

#define TAG "led_strips"

#define LED_STRIPS_CHECK(a, str, action, ...)                       \
    if (unlikely(!(a)))                                           \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        action;                                                   \
    }

typedef struct {
    led_strip_handle_t led_strip;
    uint32_t max_index;             /*!< Maximum LEDs in a single strip */
    led_indicator_ihsv_t ihsv;      /*!< IHSV: I [0-127] 7 bits -  H [0-360] - 9 bits, S [0-255] - 8 bits, V [0-255] - 8 bits*/
} led_strips_t;

static esp_err_t led_indicator_strips_init(void *param, void **ret_strips)
{
    esp_err_t ret = ESP_OK;
    const led_indicator_strips_config_t *cfg = (led_indicator_strips_config_t *)param;
    LED_STRIPS_CHECK(NULL != cfg, "param pointer invalid", return ESP_ERR_INVALID_ARG);
    LED_STRIPS_CHECK(cfg->led_strip_cfg.max_leds < MAX_INDEX, "The LED indicator supports a maximum of 126 LEDs", return ESP_ERR_INVALID_ARG);

    led_strips_t *p_strip = calloc(1, sizeof(led_strips_t));
    p_strip->max_index = cfg->led_strip_cfg.max_leds;
    LED_STRIPS_CHECK(NULL != p_strip, "calloc failed", return ESP_ERR_NO_MEM);
    switch (cfg->led_strip_driver) {
#if CONFIG_SOC_RMT_SUPPORTED
    case LED_STRIP_RMT: {
        ret = led_strip_new_rmt_device(&cfg->led_strip_cfg, &cfg->led_strip_rmt_cfg, &p_strip->led_strip);
        LED_STRIPS_CHECK(ret == ESP_OK, "Created LED strip object with RMT backend", goto fail);
        break;
    }
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    case LED_STRIP_SPI: {
        ret = led_strip_new_spi_device(&cfg->led_strip_cfg, &cfg->led_strip_spi_cfg, &p_strip->led_strip);
        LED_STRIPS_CHECK(ret == ESP_OK, "Created LED strip object with SPI backend", goto fail);
        break;
    }
#endif
    default:
        ret = ESP_ERR_INVALID_ARG;
        ESP_LOGE(TAG, "led strip driver type invalid");
        goto fail;
    }

    *ret_strips = (void *)p_strip;
    return ESP_OK;
fail:
    if (p_strip) {
        free(p_strip);
    }
    return ret;
}

static esp_err_t led_indicator_strips_deinit(void *strips)
{
    LED_STRIPS_CHECK(NULL != strips, "param pointer invalid", return ESP_ERR_INVALID_ARG);
    led_strips_t *p_strip = (led_strips_t *)strips;
    led_strip_handle_t led_strip = (led_strip_handle_t)p_strip->led_strip;
    esp_err_t err = led_strip_del(led_strip);
    LED_STRIPS_CHECK(err == ESP_OK, "Delete LED strip object", return err);
    free(p_strip);
    return ESP_OK;
}

static esp_err_t led_indicator_strips_set_on_off(void *strips, bool on_off)
{
    led_strips_t *p_strip = (led_strips_t *)strips;
    p_strip->ihsv.v = on_off ? MAX_BRIGHTNESS : 0;
    esp_err_t err = ESP_OK;
    if (p_strip->ihsv.i == MAX_INDEX) {
        for (int j = 0; j < p_strip->max_index; j++) {
            err |= led_strip_set_pixel_hsv(p_strip->led_strip, j, p_strip->ihsv.h, p_strip->ihsv.s, p_strip->ihsv.v);
            if (err != ESP_OK) {
                return err;
            }
        }
    } else {
        err |= led_strip_set_pixel_hsv(p_strip->led_strip, p_strip->ihsv.i, p_strip->ihsv.h, p_strip->ihsv.s, p_strip->ihsv.v);
    }

    err |= led_strip_refresh(p_strip->led_strip);

    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

static esp_err_t led_indicator_strips_set_rgb(void *strips, uint32_t irgb_value)
{
    led_strips_t *p_strip = (led_strips_t *)strips;
    uint8_t i, r, g, b;
    i = GET_INDEX(irgb_value);
    r = GET_RED(irgb_value);
    g = GET_GREEN(irgb_value);
    b = GET_BLUE(irgb_value);

    esp_err_t err = ESP_OK;
    if (i == MAX_INDEX) {
        for (int j = 0; j < p_strip->max_index; j++) {
            err |= led_strip_set_pixel(p_strip->led_strip, j, r, g, b);
            if (err != ESP_OK) {
                return err;
            }
        }
    } else {
        err |= led_strip_set_pixel(p_strip->led_strip, i, r, g, b);
    }

    err |= led_strip_refresh(p_strip->led_strip);

    if (err != ESP_OK) {
        return err;
    }
    p_strip->ihsv.value = led_indicator_rgb2hsv(irgb_value);
    p_strip->ihsv.i = i;
    return ESP_OK;
}

static esp_err_t led_indicator_strips_set_hsv(void *strips, uint32_t ihsv_value)
{
    led_strips_t *p_strip = (led_strips_t *)strips;
    p_strip->ihsv.value = ihsv_value;

    esp_err_t err = ESP_OK;
    if (p_strip->ihsv.i == MAX_INDEX) {
        for (int j = 0; j < p_strip->max_index; j++) {
            err |= led_strip_set_pixel_hsv(p_strip->led_strip, j, p_strip->ihsv.h, p_strip->ihsv.s, p_strip->ihsv.v);
            if (err != ESP_OK) {
                return err;
            }
        }
    } else {
        err |= led_strip_set_pixel_hsv(p_strip->led_strip, p_strip->ihsv.i, p_strip->ihsv.h, p_strip->ihsv.s, p_strip->ihsv.v);
    }

    err |= led_strip_refresh(p_strip->led_strip);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

static esp_err_t led_indicator_strips_set_brightness(void *strips, uint32_t ihsv)
{
    led_strips_t *p_strip = (led_strips_t *)strips;
    p_strip->ihsv.i = GET_INDEX(ihsv);
    p_strip->ihsv.v = GET_BRIGHTNESS(ihsv);

    esp_err_t err = ESP_OK;
    if (p_strip->ihsv.i == MAX_INDEX) {
        for (int j = 0; j < p_strip->max_index; j++) {
            err |= led_strip_set_pixel_hsv(p_strip->led_strip, j, p_strip->ihsv.h, p_strip->ihsv.s, p_strip->ihsv.v);
            if (err != ESP_OK) {
                return err;
            }
        }
    } else {
        err |= led_strip_set_pixel_hsv(p_strip->led_strip, p_strip->ihsv.i, p_strip->ihsv.h, p_strip->ihsv.s, p_strip->ihsv.v);
    }

    err |= led_strip_refresh(p_strip->led_strip);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t led_indicator_new_strips_device(const led_indicator_config_t *led_config, const led_indicator_strips_config_t *strips_cfg, led_indicator_handle_t *handle)
{
    esp_err_t ret = ESP_OK;
    bool if_blink_default_list = false;

    ESP_LOGI(TAG, "LED Indicator Version: %d.%d.%d", LED_INDICATOR_VER_MAJOR, LED_INDICATOR_VER_MINOR, LED_INDICATOR_VER_PATCH);
    LED_INDICATOR_CHECK(strips_cfg != NULL, "invalid config pointer", return ESP_ERR_INVALID_ARG);
    _led_indicator_com_config_t com_cfg = {0};
    _led_indicator_t *p_led_indicator = NULL;

    void *hardware_data = NULL;
    ret = led_indicator_strips_init((void *)strips_cfg, &hardware_data);
    LED_INDICATOR_CHECK(ESP_OK == ret, "LED rgb init failed", return ESP_FAIL);
    com_cfg.hardware_data = hardware_data;
    com_cfg.hal_indicator_set_on_off = led_indicator_strips_set_on_off;
    com_cfg.hal_indicator_deinit = led_indicator_strips_deinit;
    com_cfg.hal_indicator_set_brightness = led_indicator_strips_set_brightness;
    com_cfg.hal_indicator_set_rgb = led_indicator_strips_set_rgb;
    com_cfg.hal_indicator_set_hsv = led_indicator_strips_set_hsv;
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
    ESP_LOGI(TAG, "Indicator create successfully. type:LED Strips mode, hardware_data:%p, blink_lists:%s", p_led_indicator->hardware_data, if_blink_default_list ? "default" : "custom");
    *handle = (led_indicator_handle_t)p_led_indicator;
    return ESP_OK;
}
