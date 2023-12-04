/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include <esp_log.h>

#include "iic.h"
#include "bp5758d.h"

static const char *TAG = "bp5758d";

#define BP5758D_CHECK(a, str, action, ...)                                  \
    if (unlikely(!(a))) {                                                   \
        ESP_LOGE(TAG, str, ##__VA_ARGS__);                                  \
        action;                                                             \
    }

#if CONFIG_ENABLE_DRIVER_DEBUG_LOG_OUTPUT
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

#define INVALID_ADDR        0xFF
#define IIC_BASE_UNIT_HZ    1000
#define BP5758D_MAX_PIN     5

/**
 * BP5758D register start address - Byte0
 */
#define BASE_ADDR           0x80

/* B[3:0] */
#define BIT_OUT_SELECT      0x00
#define BIT_OUT1_CURRENT    0x01
#define BIT_OUT2_CURRENT    0x02
#define BIT_OUT3_CURRENT    0x03
#define BIT_OUT4_CURRENT    0x04
#define BIT_OUT5_CURRENT    0x05
#define BIT_OUT1_GRAYSCALE  0x06
#define BIT_OUT2_GRAYSCALE  0x08
#define BIT_OUT3_GRAYSCALE  0x0A
#define BIT_OUT4_GRAYSCALE  0x0C
#define BIT_OUT5_GRAYSCALE  0x0E

/* B[5:4] */
#define BIT_ENABLE_SLEEP_MODE   0x00
#define BIT_DISABLE_SLEEP_MODE  0x20

/**
 * BP5758D register out control address - Byte1
 */
/* B[4:0] */
#define BIT_OUT1_ENABLE 0x01
#define BIT_OUT2_ENABLE 0x02
#define BIT_OUT3_ENABLE 0x04
#define BIT_OUT4_ENABLE 0x08
#define BIT_OUT5_ENABLE 0x10

#define BIT_ALL_OUT_ENABLE (BIT_OUT1_ENABLE | BIT_OUT2_ENABLE | BIT_OUT3_ENABLE | BIT_OUT4_ENABLE | BIT_OUT5_ENABLE)
#define BIT_ALL_OUT_DISABLE 0x00

/**
 * BP5758D register current address - Byte2-6
 *
 *
 */
// Nothing

/**
 * BP5758D register grayscale address - Byte7-16
 *
 *
 */
#define BIT_GRAYSCALE_DATA_HEAD 0x00

typedef struct {
    uint8_t current[BP5758D_MAX_PIN];
    uint8_t mapping_addr[BP5758D_MAX_PIN];
    bool init_done;
} bp5758d_handle_t;

static bp5758d_handle_t *s_bp5758d = NULL;

static uint8_t get_mapping_addr(bp5758d_channel_t channel)
{
    uint8_t addr[] = { BIT_OUT1_GRAYSCALE, BIT_OUT2_GRAYSCALE, BIT_OUT3_GRAYSCALE, BIT_OUT4_GRAYSCALE, BIT_OUT5_GRAYSCALE };
    uint8_t result = addr[s_bp5758d->mapping_addr[channel]];

    return result;
}

static esp_err_t set_sleep_mode_and_current(bool enable_sleep, uint8_t *current)
{
    uint8_t addr = BASE_ADDR | BIT_OUT_SELECT;
    uint8_t value[6] = { 0 };

    if (enable_sleep) {
        addr |= BIT_ENABLE_SLEEP_MODE;
        value[0] = BIT_ALL_OUT_DISABLE;
        s_bp5758d->init_done = false;
        // The current parameters are not updated here.
    } else {
        addr |= BIT_DISABLE_SLEEP_MODE;
        value[0] = BIT_ALL_OUT_ENABLE;
        memcpy(&value[1], current, 5);
        s_bp5758d->init_done = true;
    }

    return iic_driver_write(addr, value, sizeof(value));
}

static void convert_current_value(uint8_t *output, uint8_t *input)
{
    for (int i = 0; i < 5; i++) {
        if (input[i] >= 64) {
            uint8_t temp = input[i];
            temp -= 62;
            input[i] = temp | 0x60;
        }
    }
    memcpy(output, input, 5);
    ESP_LOGD(TAG, "%d %d %d %d %d", output[0], output[1], output[2], output[3], output[4]);
}

esp_err_t bp5758d_set_standby_mode(bool enable_standby)
{
    BP5758D_CHECK(s_bp5758d, "not init", return ESP_ERR_INVALID_STATE);
    bp5758d_set_shutdown();

    return set_sleep_mode_and_current(enable_standby, s_bp5758d->current);
}

esp_err_t bp5758d_set_shutdown(void)
{
    BP5758D_CHECK(s_bp5758d, "not init", return ESP_ERR_INVALID_STATE);
    uint8_t _value[10] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_DISABLE_SLEEP_MODE | BIT_OUT1_GRAYSCALE;

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t bp5758d_regist_channel(bp5758d_channel_t channel, bp5758d_out_pin_t pin)
{
    BP5758D_CHECK(s_bp5758d, "not init", return ESP_ERR_INVALID_STATE);
    BP5758D_CHECK(channel < BP5758D_CHANNEL_MAX, "check channel fail", return ESP_ERR_INVALID_ARG);
    BP5758D_CHECK(pin < BP5758D_PIN_OUT_MAX, "check out pin fail", return ESP_ERR_INVALID_ARG);
    ESP_LOGD(TAG, "bp5758d_regist_channel:[%d]:%d", channel, pin);

    s_bp5758d->mapping_addr[channel] = pin;
    return ESP_OK;
}

esp_err_t bp5758d_set_channel(bp5758d_channel_t channel, uint16_t value)
{
    BP5758D_CHECK(s_bp5758d, "not init", return ESP_ERR_INVALID_STATE);
    BP5758D_CHECK(s_bp5758d->mapping_addr[channel] != INVALID_ADDR, "channel:%d not regist", return ESP_ERR_INVALID_STATE, channel);
    BP5758D_CHECK(value <= 1023, "value out of range", return ESP_ERR_INVALID_ARG);

    if (!s_bp5758d->init_done) {
        set_sleep_mode_and_current(false, s_bp5758d->current);
    }

    uint8_t _value[2] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_DISABLE_SLEEP_MODE;
    ESP_LOGD(TAG, "src: %d", value);

    _value[0] = (value & 0x1F) | BIT_GRAYSCALE_DATA_HEAD;
    _value[1] = (value >> 5) | BIT_GRAYSCALE_DATA_HEAD;
    addr |= (get_mapping_addr(channel));
    ESP_LOGD(TAG, "transfer: 0x%x 0x%x", _value[0], _value[1]);
    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t bp5758d_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b)
{
    BP5758D_CHECK(s_bp5758d, "not init", return ESP_ERR_INVALID_STATE);
    BP5758D_CHECK(s_bp5758d->mapping_addr[0] != INVALID_ADDR || s_bp5758d->mapping_addr[1] != INVALID_ADDR || s_bp5758d->mapping_addr[2] != INVALID_ADDR, "color channel not regist", return ESP_ERR_INVALID_ARG);

    if (!s_bp5758d->init_done) {
        set_sleep_mode_and_current(false, s_bp5758d->current);
    }

    uint8_t _value[6] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_OUT1_GRAYSCALE | BIT_DISABLE_SLEEP_MODE;

    _value[s_bp5758d->mapping_addr[0] * 2 + 0] = (value_r & 0x1F) | BIT_GRAYSCALE_DATA_HEAD;
    _value[s_bp5758d->mapping_addr[0] * 2 + 1] = (value_r >> 5) | BIT_GRAYSCALE_DATA_HEAD;

    _value[s_bp5758d->mapping_addr[1] * 2 + 0] = (value_g & 0x1F) | BIT_GRAYSCALE_DATA_HEAD;
    _value[s_bp5758d->mapping_addr[1] * 2 + 1] = (value_g >> 5) | BIT_GRAYSCALE_DATA_HEAD;

    _value[s_bp5758d->mapping_addr[2] * 2 + 0] = (value_b & 0x1F) | BIT_GRAYSCALE_DATA_HEAD;
    _value[s_bp5758d->mapping_addr[2] * 2 + 1] = (value_b >> 5) | BIT_GRAYSCALE_DATA_HEAD;

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t bp5758d_set_cw_channel(uint16_t value_c, uint16_t value_w)
{
    BP5758D_CHECK(s_bp5758d, "not init", return ESP_ERR_INVALID_STATE);
    BP5758D_CHECK(s_bp5758d->mapping_addr[3] != INVALID_ADDR || s_bp5758d->mapping_addr[4] != INVALID_ADDR, "white channel not regist", return ESP_ERR_INVALID_ARG);

    if (!s_bp5758d->init_done) {
        set_sleep_mode_and_current(false, s_bp5758d->current);
    }

    uint8_t _value[4] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_OUT4_GRAYSCALE | BIT_DISABLE_SLEEP_MODE;

    _value[(s_bp5758d->mapping_addr[3] - 3) * 2 + 0] = (value_c & 0x1F) | BIT_GRAYSCALE_DATA_HEAD;
    _value[(s_bp5758d->mapping_addr[3] - 3) * 2 + 1] = (value_c >> 5) | BIT_GRAYSCALE_DATA_HEAD;

    _value[(s_bp5758d->mapping_addr[4] - 3) * 2 + 0] = (value_w & 0x1F) | BIT_GRAYSCALE_DATA_HEAD;
    _value[(s_bp5758d->mapping_addr[4] - 3) * 2 + 1] = (value_w >> 5) | BIT_GRAYSCALE_DATA_HEAD;

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t bp5758d_set_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_c, uint16_t value_w)
{
    BP5758D_CHECK(s_bp5758d, "not init", return ESP_ERR_INVALID_STATE);
    BP5758D_CHECK(s_bp5758d->mapping_addr[3] != INVALID_ADDR || s_bp5758d->mapping_addr[4] != INVALID_ADDR, "white channel not regist", return ESP_ERR_INVALID_ARG);
    BP5758D_CHECK(s_bp5758d->mapping_addr[0] != INVALID_ADDR || s_bp5758d->mapping_addr[1] != INVALID_ADDR || s_bp5758d->mapping_addr[2] != INVALID_ADDR, "color channel not regist", return ESP_ERR_INVALID_ARG);

    if (!s_bp5758d->init_done) {
        set_sleep_mode_and_current(false, s_bp5758d->current);
    }

    uint8_t _value[10] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_OUT1_GRAYSCALE | BIT_DISABLE_SLEEP_MODE;

    _value[s_bp5758d->mapping_addr[0] * 2 + 0] = (value_r & 0x1F) | BIT_GRAYSCALE_DATA_HEAD;
    _value[s_bp5758d->mapping_addr[0] * 2 + 1] = (value_r >> 5) | BIT_GRAYSCALE_DATA_HEAD;

    _value[s_bp5758d->mapping_addr[1] * 2 + 0] = (value_g & 0x1F) | BIT_GRAYSCALE_DATA_HEAD;
    _value[s_bp5758d->mapping_addr[1] * 2 + 1] = (value_g >> 5) | BIT_GRAYSCALE_DATA_HEAD;

    _value[s_bp5758d->mapping_addr[2] * 2 + 0] = (value_b & 0x1F) | BIT_GRAYSCALE_DATA_HEAD;
    _value[s_bp5758d->mapping_addr[2] * 2 + 1] = (value_b >> 5) | BIT_GRAYSCALE_DATA_HEAD;

    _value[s_bp5758d->mapping_addr[3] * 2 + 0] = (value_c & 0x1F) | BIT_GRAYSCALE_DATA_HEAD;
    _value[s_bp5758d->mapping_addr[3] * 2 + 1] = (value_c >> 5) | BIT_GRAYSCALE_DATA_HEAD;

    _value[s_bp5758d->mapping_addr[4] * 2 + 0] = (value_w & 0x1F) | BIT_GRAYSCALE_DATA_HEAD;
    _value[s_bp5758d->mapping_addr[4] * 2 + 1] = (value_w >> 5) | BIT_GRAYSCALE_DATA_HEAD;

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t bp5758d_init(driver_bp5758d_t *config, void(*hook_func)(void *))
{
    esp_err_t err = ESP_OK;
    BP5758D_CHECK(config, "config is null", return ESP_ERR_INVALID_ARG);
    BP5758D_CHECK(config->current, "current is null", return ESP_ERR_INVALID_ARG);
    BP5758D_CHECK(!s_bp5758d, "already init done", return ESP_ERR_INVALID_ARG);

    s_bp5758d = calloc(1, sizeof(bp5758d_handle_t));
    BP5758D_CHECK(s_bp5758d, "alloc fail", return ESP_ERR_NO_MEM);
    memset(s_bp5758d->mapping_addr, INVALID_ADDR, BP5758D_MAX_PIN);
    convert_current_value(s_bp5758d->current, config->current);

    if (config->freq_khz > 300) {
        config->freq_khz = 300;
        ESP_LOGW(TAG, "The frequency is too high, adjust it to 300khz");
    }

    err |= iic_driver_init(I2C_NUM_0, config->iic_sda, config->iic_clk, config->freq_khz * IIC_BASE_UNIT_HZ);
    BP5758D_CHECK(err == ESP_OK, "i2c master init fail", goto EXIT);

    if (config->enable_iic_queue) {
        err |= iic_driver_send_task_create();
        BP5758D_CHECK(err == ESP_OK, "task create fail", goto EXIT);
    }

    return err;
EXIT:

    if (s_bp5758d) {
        free(s_bp5758d);
        s_bp5758d = NULL;
    }
    return err;
}

esp_err_t bp5758d_deinit(void)
{
    BP5758D_CHECK(s_bp5758d, "not init", return ESP_ERR_INVALID_STATE);

    bp5758d_set_shutdown();
    iic_driver_deinit();
    iic_driver_task_destroy();
    free(s_bp5758d);
    s_bp5758d = NULL;
    return ESP_OK;
}
