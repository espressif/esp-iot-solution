/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include "esp_log.h"
#include "led_strips.h"
#include "led_strip.h"
#include "led_strip_types.h"
#include "led_convert.h"

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

esp_err_t led_indicator_strips_init(void *param, void **ret_strips)
{
    esp_err_t ret = ESP_OK;
    const led_indicator_strips_config_t *cfg = (led_indicator_strips_config_t *)param;
    LED_STRIPS_CHECK(NULL != cfg, "param pointer invalid", return ESP_ERR_INVALID_ARG);
    LED_STRIPS_CHECK(cfg->led_strip_cfg.max_leds < MAX_INDEX, "The LED indicator supports a maximum of 126 LEDs", return ESP_ERR_INVALID_ARG);

    led_strips_t *p_strip = calloc(1, sizeof(led_strips_t));
    p_strip->max_index = cfg->led_strip_cfg.max_leds;
    LED_STRIPS_CHECK(NULL != p_strip, "calloc failed", return ESP_ERR_NO_MEM);
    switch (cfg->led_strip_driver) {
#if !CONFIG_IDF_TARGET_ESP32C2
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

esp_err_t led_indicator_strips_deinit(void *strips)
{
    LED_STRIPS_CHECK(NULL != strips, "param pointer invalid", return ESP_ERR_INVALID_ARG);
    led_strips_t *p_strip = (led_strips_t *)strips;
    led_strip_handle_t led_strip = (led_strip_handle_t)p_strip->led_strip;
    esp_err_t err = led_strip_del(led_strip);
    LED_STRIPS_CHECK(err == ESP_OK, "Delete LED strip object", return err);
    free(p_strip);
    return ESP_OK;
}

esp_err_t led_indicator_strips_set_on_off(void *strips, bool on_off)
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

esp_err_t led_indicator_strips_set_rgb(void *strips, uint32_t irgb_value)
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

esp_err_t led_indicator_strips_set_hsv(void *strips, uint32_t ihsv_value)
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

esp_err_t led_indicator_strips_set_brightness(void *strips, uint32_t ihsv)
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
