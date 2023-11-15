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
#include "driver/ledc.h"
#include "esp_log.h"
#include "led_strips.h"
#include "led_strip.h"
#include "led_strip_types.h"

#define TAG "led_strips"

#define LED_RGB_CHECK(a, str, action, ...)                       \
    if (unlikely(!(a)))                                           \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        action;                                                   \
    }

typedef struct {
    led_strip_handle_t led_strip;
    uint32_t max_index;             /*!< Maximum LEDs in a single strip */
    uint32_t ihsv;                  /*!< IHSV: I [0-127] 7 bits -  H [0-360] - 9 bits, S [0-255] - 8 bits, V [0-255] - 8 bits*/
} led_strips_t;

uint32_t led_indicator_strips_rgb2hsv(uint32_t rgb_value)
{
    uint8_t r = (rgb_value >> 16) & 0xFF;
    uint8_t g = (rgb_value >> 8) & 0xFF;
    uint8_t b = rgb_value & 0xFF;
    uint16_t h, s, v;

    uint8_t minRGB, maxRGB;
    uint8_t delta;

    minRGB = r < g ? (r < b ? r : b) : (g < b ? g : b);
    maxRGB = r > g ? (r > b ? r : b) : (g > b ? g : b);

    v = maxRGB;
    delta = maxRGB - minRGB;

    if (delta == 0) {
        h = 0;
        s = 0;
    } else {
        s = delta * 255 / maxRGB;

        if (r == maxRGB) {
            h = (60 * (g - b) / delta + 360) % 360;
        } else if (g == maxRGB) {
            h = (60 * (b - r) / delta + 120);
        } else {
            h = (60 * (r-g) / delta + 240);
        }
    }
    return (h << 16) | (s << 8) | v;
}

void led_indicator_strips_hsv2rgb(uint32_t hsv, uint32_t *r, uint32_t *g, uint32_t *b)
{
    uint8_t h = (hsv >> 16) & 0x1FF;
    uint8_t s = (hsv >> 8) & 0xFF;
    uint16_t v = hsv & 0xFF;

    uint8_t rgb_max = v;
    uint8_t rgb_min = rgb_max * (255 - s) / 255.0f;

    uint8_t i = h / 60;
    uint8_t diff = h % 60;

    // RGB adjustment amount by hue
    uint8_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

esp_err_t led_indicator_strips_init(void *param, void **ret_strips)
{
    esp_err_t ret = ESP_OK;
    const led_indicator_strips_config_t *cfg = (led_indicator_strips_config_t *)param;
    LED_RGB_CHECK(NULL != cfg, "param pointer invalid", return ESP_ERR_INVALID_ARG);
    LED_RGB_CHECK(cfg->led_strip_cfg.max_leds < MAX_INDEX, "The LED indicator supports a maximum of 126 LEDs", return ESP_ERR_INVALID_ARG);

    led_strips_t *p_strip = calloc(1, sizeof(led_strips_t));
    p_strip->max_index = cfg->led_strip_cfg.max_leds;
    LED_RGB_CHECK(NULL != p_strip, "calloc failed", return ESP_ERR_NO_MEM);
    switch (cfg->led_strip_driver) {
#if !CONFIG_IDF_TARGET_ESP32C2
    case LED_STRIP_RMT: {
        ret = led_strip_new_rmt_device(&cfg->led_strip_cfg, &cfg->led_strip_rmt_cfg, &p_strip->led_strip);
        LED_RGB_CHECK(ret == ESP_OK, "Created LED strip object with RMT backend", goto fail);
        break;
    }
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    case LED_STRIP_SPI: {
        ret = led_strip_new_spi_device(&cfg->led_strip_cfg, &cfg->led_strip_spi_cfg, &p_strip->led_strip);
        LED_RGB_CHECK(ret == ESP_OK, "Created LED strip object with SPI backend", goto fail);
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
    LED_RGB_CHECK(NULL != strips, "param pointer invalid", return ESP_ERR_INVALID_ARG);
    led_strips_t *p_strip = (led_strips_t *)strips;
    led_strip_handle_t led_strip = (led_strip_handle_t)p_strip->led_strip;
    esp_err_t err = led_strip_del(led_strip);
    LED_RGB_CHECK(err == ESP_OK, "Delete LED strip object", return err);
    free(p_strip);
    return ESP_OK;
}

esp_err_t led_indicator_strips_set_on_off(void *strips, bool on_off)
{
    led_strips_t *p_strip = (led_strips_t *)strips;
    uint8_t i,s,v;
    uint16_t h;
    uint32_t iihsv_value = p_strip->ihsv;
    SET_BRIGHTNESS(iihsv_value, on_off ? MAX_BRIGHTNESS : 0);
    i = GET_INDEX(iihsv_value);
    h = GET_HUE(iihsv_value);
    s = GET_SATURATION(iihsv_value);
    v = GET_BRIGHTNESS(iihsv_value);
    esp_err_t err = ESP_OK;
    if (i == MAX_INDEX) {
        for (int j = 0; j < p_strip->max_index; j++) {
            err |= led_strip_set_pixel_hsv(p_strip->led_strip, j, h, s, v);
            if (err != ESP_OK) {
                return err;
            }
        }
    } else {
        err |= led_strip_set_pixel_hsv(p_strip->led_strip, i, h, s, v);
    }

    err |= led_strip_refresh(p_strip->led_strip);

    if (err != ESP_OK) {
        return err;
    }
    p_strip->ihsv = iihsv_value;
    return ESP_OK;
}

esp_err_t led_indicator_strips_set_rgb(void *strips, uint32_t irgb_value)
{
    led_strips_t *p_strip = (led_strips_t *)strips;
    uint8_t i,r,g,b;
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
    p_strip->ihsv = led_indicator_strips_rgb2hsv(irgb_value);
    SET_INDEX(p_strip->ihsv, i);
    return ESP_OK;
}

esp_err_t led_indicator_strips_set_hsv(void *strips, uint32_t iihsv_value)
{
    led_strips_t *p_strip = (led_strips_t *)strips;
    uint8_t i,s,v;
    uint16_t h;
    i = GET_INDEX(iihsv_value);
    h = GET_HUE(iihsv_value);
    s = GET_SATURATION(iihsv_value);
    v = GET_BRIGHTNESS(iihsv_value);

    esp_err_t err = ESP_OK;
    if (i == MAX_INDEX) {
        for (int j = 0; j < p_strip->max_index; j++) {
            err |= led_strip_set_pixel_hsv(p_strip->led_strip, j, h, s, v);
            if (err != ESP_OK) {
                return err;
            }
        }
    } else {
        err |= led_strip_set_pixel_hsv(p_strip->led_strip, i, h, s, v);
    }

    err |= led_strip_refresh(p_strip->led_strip);
    if (err != ESP_OK) {
        return err;
    }

    p_strip->ihsv = iihsv_value;
    return ESP_OK;
}

esp_err_t led_indicator_strips_set_brightness(void *strips, uint32_t ihsv)
{
    led_strips_t *p_strip = (led_strips_t *)strips;
    uint8_t i,s,v;
    uint16_t h;
    uint32_t ihsv_value;
    ihsv_value = p_strip->ihsv;
    SET_BRIGHTNESS(ihsv_value, GET_BRIGHTNESS(ihsv));
    SET_INDEX(ihsv_value, GET_INDEX(ihsv));
    i = GET_INDEX(ihsv_value);
    h = GET_HUE(ihsv_value);
    s = GET_SATURATION(ihsv_value);
    v = GET_BRIGHTNESS(ihsv_value);

    esp_err_t err = ESP_OK;
    if (i == MAX_INDEX) {
        for (int j = 0; j < p_strip->max_index; j++) {
            err |= led_strip_set_pixel_hsv(p_strip->led_strip, j, h, s, v);
            if (err != ESP_OK) {
                return err;
            }
        }
    } else {
        err |= led_strip_set_pixel_hsv(p_strip->led_strip, i, h, s, v);
    }

    err |= led_strip_refresh(p_strip->led_strip);
    if (err != ESP_OK) {
        return err;
    }

    p_strip->ihsv = ihsv_value;
    return ESP_OK;
}
