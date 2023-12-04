/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <esp_log.h>
#include <driver/spi_master.h>
#include <hal/spi_hal.h>

#include "ws2812.h"

static const char *TAG = "ws2812";

#define WS2812_SPI_SPEED_HZ     (2400 * 1000)
#define WS2812_LED_BUF          (9)
#define WS2812_LED_BUF_BIT      (WS2812_LED_BUF * 8)

typedef struct {
    spi_device_handle_t spi_handle;
    uint16_t led_num;
    uint8_t *buf;
    uint32_t buf_size;
} ws2812_handle_t;

static ws2812_handle_t *s_ws2812 = NULL;

#define WS2812_CHECK(a, str, action, ...)                                   \
    if (unlikely(!(a))) {                                                   \
        ESP_LOGE(TAG, str, ##__VA_ARGS__);                                  \
        action;                                                             \
    }

#if CONFIG_ENABLE_DRIVER_DEBUG_LOG_OUTPUT
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

static esp_err_t _write(uint8_t *buf, size_t size)
{
    spi_transaction_t t = { 0 };

    t.length = size;
    t.tx_buffer = buf;
    t.rx_buffer = NULL;

    return spi_device_transmit(s_ws2812->spi_handle, &t);;
}

static void set_data_bit(uint8_t data, uint8_t *buf)
{
    *(buf + 2) |= data & BIT(0) ? BIT(2) | BIT(1) : BIT(2);
    *(buf + 2) |= data & BIT(1) ? BIT(5) | BIT(4) : BIT(5);
    *(buf + 2) |= data & BIT(2) ? BIT(7) : 0x00;
    *(buf + 1) |= data & BIT(2) ? BIT(0) : BIT(0);
    *(buf + 1) |= data & BIT(3) ? BIT(3) | BIT(2) : BIT(3);
    *(buf + 1) |= data & BIT(4) ? BIT(6) | BIT(5) : BIT(6);
    *(buf + 0) |= data & BIT(5) ? BIT(1) | BIT(0) : BIT(1);
    *(buf + 0) |= data & BIT(6) ? BIT(4) | BIT(3) : BIT(4);
    *(buf + 0) |= data & BIT(7) ? BIT(7) | BIT(6) : BIT(7);
}

static void generate_data(uint32_t index, uint8_t red, uint8_t green, uint8_t blue, uint8_t *out_buf)
{
    memset(out_buf + (index) * WS2812_LED_BUF, 0, WS2812_LED_BUF);
    set_data_bit(green, out_buf + (index) * WS2812_LED_BUF);
    set_data_bit(red, out_buf + (index) * WS2812_LED_BUF + 3);
    set_data_bit(blue, out_buf + (index) * WS2812_LED_BUF + 6);
}

static void cleanup(void)
{
    if (s_ws2812->spi_handle) {
        spi_bus_remove_device(s_ws2812->spi_handle);
        spi_bus_free(SPI2_HOST);
        s_ws2812->spi_handle = NULL;
    }

    if (s_ws2812->buf) {
        free(s_ws2812->buf);
        s_ws2812->buf = NULL;
    }

    if (s_ws2812) {
        free(s_ws2812);
        s_ws2812 = NULL;
    }
}

esp_err_t ws2812_init(driver_ws2812_t *config, void(*hook_func)(void *))
{
    esp_err_t err = ESP_OK;
    WS2812_CHECK(config, "config is null", return ESP_ERR_INVALID_ARG);
    WS2812_CHECK(!s_ws2812, "already init done", return ESP_ERR_INVALID_ARG);

    s_ws2812 = calloc(1, sizeof(ws2812_handle_t));
    WS2812_CHECK(err == ESP_OK, "alloc fail", return ESP_ERR_NO_MEM);
    s_ws2812->led_num = config->led_num;
    s_ws2812->buf_size = s_ws2812->led_num * WS2812_LED_BUF;

    s_ws2812->buf = heap_caps_malloc(s_ws2812->buf_size, MALLOC_CAP_DMA);
    memset(s_ws2812->buf, 0, s_ws2812->buf_size);

    err = gpio_set_drive_capability(config->ctrl_io, GPIO_DRIVE_CAP_3);
    WS2812_CHECK(err == ESP_OK, "set drive capability fail", return err);

    spi_bus_config_t buscfg = {
        .mosi_io_num = config->ctrl_io,
        .miso_io_num = -1,
        .sclk_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = config->led_num * WS2812_LED_BUF_BIT,
    };

    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = WS2812_SPI_SPEED_HZ,
        .duty_cycle_pos = 128,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
    };

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
#else
    int dma_chan = SPI2_HOST;
    err = spi_bus_initialize(SPI2_HOST, &buscfg, dma_chan);
#endif
    WS2812_CHECK(err == ESP_OK, "spi_bus_initialize error", return err);

#if !CONFIG_IDF_TARGET_ESP32
    spi_dev_t *hw = spi_periph_signal[SPI2_HOST].hw;
    hw->ctrl.d_pol = 0;
#endif
    err = spi_bus_add_device(SPI2_HOST, &devcfg, &s_ws2812->spi_handle);
    WS2812_CHECK(err == ESP_OK, "spi_bus_add_device error", goto EXIT);

    err = _write(s_ws2812->buf, s_ws2812->buf_size);
    WS2812_CHECK(err == ESP_OK, "set init data fail", return err);

    return ESP_OK;

EXIT:
    cleanup();

    return err;
}

esp_err_t ws2812_deinit(void)
{
    WS2812_CHECK(s_ws2812, "not init", return ESP_ERR_INVALID_ARG);

    cleanup();
    return ESP_OK;
}

esp_err_t ws2812_set_rgb_channel(uint8_t value_r, uint8_t value_g, uint8_t value_b)
{
    WS2812_CHECK(s_ws2812, "ws2812b_init() must be called first", return ESP_ERR_INVALID_STATE);

    for (int i = 0; i < s_ws2812->led_num; i++) {
        generate_data(i, value_r, value_g, value_b, s_ws2812->buf);
    }

    return _write(s_ws2812->buf, s_ws2812->led_num * WS2812_LED_BUF_BIT);
}
