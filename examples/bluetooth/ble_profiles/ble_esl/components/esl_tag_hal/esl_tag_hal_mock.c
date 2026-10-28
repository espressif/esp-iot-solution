/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esl_tag_hal.h"

#include <inttypes.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esl_image_codec.h"
#include "esp_log.h"
#include "esp_check.h"

#if CONFIG_ESL_TAG_LED_BACKEND_WS2812
#include "led_strip.h"
#if CONFIG_ESL_TAG_WS2812_BACKEND_SPI
#include "led_strip_spi.h"
#include "driver/spi_master.h"
#endif
#endif

static const char *TAG = "tag_hal";

#if CONFIG_ESL_TAG_LED_BACKEND_GPIO && CONFIG_ESL_TAG_LED_GPIO >= 0
static bool s_led_gpio_ready;
#endif

#if CONFIG_ESL_TAG_LED_BACKEND_WS2812
static led_strip_handle_t s_ws2812;
#endif

static uint8_t esl_color_level_to_u8(uint8_t level)
{
    if (level > 3) {
        level = 3;
    }
    return (uint8_t)((level * 255) / 3);
}

static uint8_t esl_apply_brightness(uint8_t component, uint8_t brightness)
{
    static const uint8_t scale[] = { 64, 128, 191, 255 };
    if (brightness > 3) {
        brightness = 3;
    }
    return (uint8_t)((uint16_t)component * scale[brightness] / 255);
}

static esp_err_t led_gpio_init(void)
{
#if CONFIG_ESL_TAG_LED_BACKEND_GPIO && CONFIG_ESL_TAG_LED_GPIO >= 0
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << CONFIG_ESL_TAG_LED_GPIO,
                             .mode = GPIO_MODE_OUTPUT,
                             .pull_up_en = GPIO_PULLUP_DISABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io);
    if (ret != ESP_OK) {
        return ret;
    }

    gpio_set_level(CONFIG_ESL_TAG_LED_GPIO, CONFIG_ESL_TAG_LED_ACTIVE_LEVEL ? 0 : 1);
    s_led_gpio_ready = true;
    ESP_LOGI(TAG, "Discrete LED GPIO %d ready (active_level=%d)",
             CONFIG_ESL_TAG_LED_GPIO, CONFIG_ESL_TAG_LED_ACTIVE_LEVEL);
#endif
    return ESP_OK;
}

static esp_err_t ws2812_init(void)
{
#if CONFIG_ESL_TAG_LED_BACKEND_WS2812
    led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_ESL_TAG_WS2812_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };
    esp_err_t ret;
    const char *backend = "unknown";

#if CONFIG_ESL_TAG_WS2812_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 0,
        .flags.with_dma = false,
    };
    ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_ws2812);
    backend = "RMT";
#elif CONFIG_ESL_TAG_WS2812_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .clk_src = SPI_CLK_SRC_DEFAULT,
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ret = led_strip_new_spi_device(&strip_config, &spi_config, &s_ws2812);
    backend = "SPI";
#else
#error "No WS2812 backend selected"
#endif
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 init failed on GPIO %d (%s): %s",
                 CONFIG_ESL_TAG_WS2812_GPIO, backend, esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(led_strip_clear(s_ws2812));
    ESP_LOGI(TAG, "WS2812 onboard LED ready on GPIO %d (%s)", CONFIG_ESL_TAG_WS2812_GPIO, backend);
#endif
    return ESP_OK;
}

static void led_gpio_set_on(bool on)
{
#if CONFIG_ESL_TAG_LED_BACKEND_GPIO && CONFIG_ESL_TAG_LED_GPIO >= 0
    if (!s_led_gpio_ready) {
        return;
    }
    int level = on ? CONFIG_ESL_TAG_LED_ACTIVE_LEVEL : !CONFIG_ESL_TAG_LED_ACTIVE_LEVEL;
    gpio_set_level(CONFIG_ESL_TAG_LED_GPIO, level);
#else
    (void)on;
#endif
}

static void ws2812_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
#if CONFIG_ESL_TAG_LED_BACKEND_WS2812
    if (s_ws2812 == NULL) {
        return;
    }
    if (red == 0 && green == 0 && blue == 0) {
        ESP_ERROR_CHECK(led_strip_clear(s_ws2812));
        return;
    }
    ESP_ERROR_CHECK(led_strip_set_pixel(s_ws2812, 0, red, green, blue));
    ESP_ERROR_CHECK(led_strip_refresh(s_ws2812));
#else
    (void)red;
    (void)green;
    (void)blue;
#endif
}

#if CONFIG_ESL_TAG_DISPLAY_BACKEND_EPAPER
esp_err_t esl_tag_display_epaper_init(void);
#endif

esp_err_t esl_tag_hal_init(void)
{
#if CONFIG_ESL_TAG_DISPLAY_BACKEND_EPAPER
    ESP_LOGI(TAG, "Tag HAL initialized (e-paper backend: SSD1680)");
    ESP_RETURN_ON_ERROR(esl_tag_display_epaper_init(), TAG, "epaper display init failed");
#else
    ESP_LOGI(TAG, "Tag HAL initialized (e-paper backend: mock)");
#endif
    ESP_RETURN_ON_ERROR(led_gpio_init(), TAG, "GPIO LED init failed");
    ESP_RETURN_ON_ERROR(ws2812_init(), TAG, "WS2812 init failed");
    return ESP_OK;
}

#if CONFIG_ESL_TAG_DISPLAY_BACKEND_MOCK

esp_err_t esl_tag_display_show(uint8_t display_index, uint8_t image_index)
{
    ESP_LOGI(TAG, "[mock display] show display=%u image=%u", display_index, image_index);
    return ESP_OK;
}

esp_err_t esl_tag_display_refresh(uint8_t display_index)
{
    ESP_LOGI(TAG, "[mock display] refresh display=%u", display_index);
    return ESP_OK;
}
#endif /* CONFIG_ESL_TAG_DISPLAY_BACKEND_MOCK */

esp_err_t esl_tag_image_write(uint8_t image_index, uint32_t offset, const uint8_t *data, uint32_t len)
{
    ESP_LOGI(TAG, "Image write: index=%u offset=%" PRIu32 " len=%" PRIu32,
             image_index, offset, len);

    if (data != NULL && len > 0) {
        esl_image_hex_dump(TAG, image_index, offset, data, len, 16);
    }

    /* Pixel persistence is handled by esl_tag_image_store on write-complete. */
    (void)offset;
    return ESP_OK;
}

esp_err_t esl_tag_led_apply(uint8_t led_index, const esl_led_control_params_t *params)
{
    if (params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bool any_color = (params->color_red | params->color_green | params->color_blue) != 0;
    bool on = !params->is_off && any_color;

    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    if (on) {
        red = esl_apply_brightness(esl_color_level_to_u8(params->color_red), params->brightness);
        green = esl_apply_brightness(esl_color_level_to_u8(params->color_green), params->brightness);
        blue = esl_apply_brightness(esl_color_level_to_u8(params->color_blue), params->brightness);
    }

    ESP_LOGI(TAG, "[led] index=%u off=%d on=%d rgb=%u/%u/%u -> %u/%u/%u",
             led_index, params->is_off, on,
             params->color_red, params->color_green, params->color_blue,
             red, green, blue);

#if CONFIG_ESL_TAG_LED_BACKEND_WS2812
    ws2812_set_rgb(red, green, blue);
#elif CONFIG_ESL_TAG_LED_BACKEND_GPIO
    led_gpio_set_on(on);
    ESP_LOGI(TAG, "[led] gpio %d level=%d",
             CONFIG_ESL_TAG_LED_GPIO, gpio_get_level(CONFIG_ESL_TAG_LED_GPIO));
#endif

    return ESP_OK;
}

esp_err_t esl_tag_sensor_read(uint8_t sensor_index, uint8_t *buf, size_t buf_size, size_t *out_len)
{
    if (buf == NULL || buf_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t temp_value = 2500;
    buf[0] = (uint8_t)(temp_value & 0xFF);
    buf[1] = (uint8_t)((temp_value >> 8) & 0xFF);
    if (out_len != NULL) {
        *out_len = 2;
    }

    ESP_LOGI(TAG, "[mock sensor] index=%u value=25.0C", sensor_index);
    return ESP_OK;
}
