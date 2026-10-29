/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sdkconfig.h>
#include <esp_log.h>
#include <iot_button.h>
#include <button_gpio.h>
#include <esp_weaver.h>

#include <led_indicator.h>
#include <led_convert.h>
#ifdef CONFIG_LED_TYPE_RGB
#include <led_indicator_rgb.h>
#elif defined(CONFIG_LED_TYPE_WS2812)
#include <led_indicator_strips.h>
#endif

#include "app_driver.h"
#include "app_light.h"

static const char *TAG = "app_driver";

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0

static uint16_t g_hue = DEFAULT_HUE;
static uint16_t g_saturation = DEFAULT_SATURATION;
static uint16_t g_value = DEFAULT_BRIGHTNESS;
static bool g_power = DEFAULT_POWER;

/* LED Indicator handle */
static led_indicator_handle_t g_led_indicator = NULL;

static esp_err_t set_led_hsv(uint16_t hue, uint16_t saturation, uint16_t brightness)
{
    /* Convert HSV (0-360, 0-100%, 0-100%) to led_indicator format (0-359, 0-255, 0-255) */
    uint16_t hue_359 = hue % 360;
    uint16_t sat_255 = (saturation * 255) / 100;
    uint16_t val_255 = (brightness * 255) / 100;
    return led_indicator_set_hsv(g_led_indicator, SET_IHSV(MAX_INDEX, hue_359, sat_255, val_255));
}

static esp_err_t drive_led(void)
{
#ifdef CONFIG_LED_TYPE_NONE
    return ESP_OK;
#endif
    if (!g_led_indicator) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!g_power) {
        return led_indicator_set_brightness(g_led_indicator, 0);
    }
    return set_led_hsv(g_hue, g_saturation, g_value);
}

esp_err_t app_light_set_led(uint16_t hue, uint16_t saturation, uint16_t brightness)
{
    g_hue = hue;
    g_saturation = saturation;
    g_value = brightness;
    return drive_led();
}

esp_err_t app_light_set_power(bool power)
{
    g_power = power;
    return drive_led();
}

static esp_err_t app_light_init(void)
{
#ifdef CONFIG_LED_TYPE_RGB
    /* Use RGB LED (3-pin RGB LED) */
    led_indicator_rgb_config_t rgb_config = {
        .timer_inited = false,
        .timer_num = LEDC_TIMER_0,
        .red_gpio_num = CONFIG_RGB_LED_RED_GPIO,
        .green_gpio_num = CONFIG_RGB_LED_GREEN_GPIO,
        .blue_gpio_num = CONFIG_RGB_LED_BLUE_GPIO,
        .red_channel = LEDC_CHANNEL_0,
        .green_channel = LEDC_CHANNEL_1,
        .blue_channel = LEDC_CHANNEL_2,
    };
#ifdef CONFIG_RGB_LED_ACTIVE_LEVEL_HIGH
    rgb_config.is_active_level_high = true;
#else
    rgb_config.is_active_level_high = false;
#endif

    led_indicator_config_t config = {
        .blink_lists = NULL,
        .blink_list_num = 0,
    };

    esp_err_t err = led_indicator_new_rgb_device(&config, &rgb_config, &g_led_indicator);
#elif defined(CONFIG_LED_TYPE_WS2812)
    /* Use LED Strip (WS2812) */
    led_indicator_strips_config_t strips_config = {
        .led_strip_cfg = {
            .strip_gpio_num = CONFIG_WS2812_LED_GPIO,
            .max_leds = CONFIG_WS2812_LED_COUNT,
            .led_model = LED_MODEL_WS2812,
            .flags.invert_out = false,
        },
#if CONFIG_SOC_RMT_SUPPORTED
        .led_strip_driver = LED_STRIP_RMT,
        .led_strip_rmt_cfg = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = 10 * 1000 * 1000, /* 10 MHz */
            .flags.with_dma = false,
        },
#else
        /* Use SPI backend for chips without RMT (ESP32-C2, ESP32-C61, etc.) */
        .led_strip_driver = LED_STRIP_SPI,
        .led_strip_spi_cfg = {
            .clk_src = SPI_CLK_SRC_DEFAULT,
            .spi_bus = SPI2_HOST,
            .flags.with_dma = false,
        },
#endif
    };

    led_indicator_config_t config = {
        .blink_lists = NULL,
        .blink_list_num = 0,
    };

    esp_err_t err = led_indicator_new_strips_device(&config, &strips_config, &g_led_indicator);
#else
    /* No LED hardware - g_led_indicator remains NULL */
    esp_err_t err = ESP_OK;
#endif

    if (err != ESP_OK) {
        return err;
    }

    return drive_led();
}

esp_err_t app_light_set_brightness(uint16_t brightness)
{
    g_value = brightness;
    return drive_led();
}

esp_err_t app_light_set_hue(uint16_t hue)
{
    g_hue = hue;
    return drive_led();
}

esp_err_t app_light_set_saturation(uint16_t saturation)
{
    g_saturation = saturation;
    return drive_led();
}

static void push_btn_cb(void *arg, void *data)
{
    app_light_set_power(!g_power);
    esp_weaver_param_t *power_param = app_light_get_param_by_type(ESP_WEAVER_PARAM_POWER);
    if (power_param) {
        esp_weaver_param_update_and_report(power_param, esp_weaver_bool(g_power));
    }
}

void app_driver_init(void)
{
    esp_err_t rc = app_light_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "app_light_init failed: %s (0x%x)", esp_err_to_name(rc), rc);
    }
    button_config_t btn_cfg = {
        .long_press_time = 0,  /* Use default */
        .short_press_time = 0, /* Use default */
    };
    button_gpio_config_t gpio_cfg = {
        .gpio_num = BUTTON_GPIO,
        .active_level = BUTTON_ACTIVE_LEVEL,
        .enable_power_save = false,
    };
    button_handle_t btn_handle = NULL;
    rc = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn_handle);
    if (rc != ESP_OK || !btn_handle) {
        ESP_LOGW(TAG, "Failed to create button device: %s", esp_err_to_name(rc));
        return;
    }
    /* Register a callback for a button single click event */
    rc = iot_button_register_cb(btn_handle, BUTTON_SINGLE_CLICK, NULL, push_btn_cb, NULL);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register button callback: %s", esp_err_to_name(rc));
    }
}
