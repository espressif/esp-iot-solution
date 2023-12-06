/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include <esp_log.h>

#include "iic.h"
#include "bp1658cj.h"

static const char *TAG = "driver_bp1658cj";

#define BP1658CJ_CHECK(a, str, action, ...)                                 \
    if (unlikely(!(a))) {                                                   \
        ESP_LOGE(TAG, str, ##__VA_ARGS__);                                  \
        action;                                                             \
    }

#if CONFIG_ENABLE_DRIVER_DEBUG_LOG_OUTPUT
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

#define INVALID_ADDR        0xFF
#define IIC_BASE_UNIT_HZ    1000
#define BP1658CJ_MAX_PIN    5

/**
 * BP1658CJ register start address - Byte0
 */
#define BASE_ADDR 0x80

/* B[3:0] */
#define BIT_MAX_CURRENT             0x00
#define BIT_R_OUT1                  0x01
#define BIT_G_OUT2                  0x03
#define BIT_B_OUT3                  0x05
#define BIT_C_OUT4                  0x07
#define BIT_W_OUT5                  0x09

/* B[5:4] */
#define BIT_SLEEP_MODE_ENABLE       0x00
#define BIT_RGB_OUT_ENABLE          0x10
#define BIT_CW_OUT_ENABLE           0x20
#define BIT_ALL_OUT_ENABLE          0x30

/**
 * BP1658CJ register current address - Byte1
 */
// Nothing

/**
 * BP1658CJ register grayscale address - Byte 2-11
 *
 *
 */
// Nothing

typedef struct {
    bp1658cj_rgb_current_t rgb_current;
    bp1658cj_cw_current_t cw_current;
    uint8_t mapping_addr[BP1658CJ_MAX_PIN];
    bool init_done;
} bp1658cj_handle_t;

static bp1658cj_handle_t *s_bp1658cj = NULL;

static uint8_t get_mapping_addr(bp1658cj_channel_t channel)
{
    uint8_t addr[] = { BIT_R_OUT1, BIT_G_OUT2, BIT_B_OUT3, BIT_C_OUT4, BIT_W_OUT5 };
    uint8_t result = addr[s_bp1658cj->mapping_addr[channel]];

    return result;
}

static esp_err_t set_mode_and_current(bool enable_sleep_mode, bp1658cj_rgb_current_t rgb, bp1658cj_cw_current_t wy)
{
    uint8_t value[1] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_MAX_CURRENT | BIT_SLEEP_MODE_ENABLE;
    value[0] = (rgb << 4) | (wy);
    if (enable_sleep_mode) {
        addr |= BIT_SLEEP_MODE_ENABLE;
        s_bp1658cj->init_done = false;
    } else {
        addr |= BIT_ALL_OUT_ENABLE;
        s_bp1658cj->init_done = true;
    }

    return iic_driver_write(addr, value, sizeof(value));
}

esp_err_t bp1658cj_set_max_current(bp1658cj_rgb_current_t rgb, bp1658cj_cw_current_t wy)
{
    BP1658CJ_CHECK(s_bp1658cj, "not init", return ESP_ERR_INVALID_STATE);

    uint8_t value = 0;
    uint8_t addr = BASE_ADDR | BIT_MAX_CURRENT | BIT_ALL_OUT_ENABLE;
    value = (rgb << 4) | (wy);

    return iic_driver_write(addr, &value, 1);
}

esp_err_t bp1658cj_set_sleep_mode(bool enable_sleep)
{
    BP1658CJ_CHECK(s_bp1658cj, "not init", return ESP_ERR_INVALID_STATE);

    uint8_t addr = BASE_ADDR | BIT_SLEEP_MODE_ENABLE;
    if (enable_sleep) {
        addr |= BIT_SLEEP_MODE_ENABLE;
        s_bp1658cj->init_done = false;
    } else {
        addr |= BIT_ALL_OUT_ENABLE;
        s_bp1658cj->init_done = true;
    }

    return iic_driver_write(addr, NULL, 0);
}

esp_err_t bp1658cj_set_shutdown(void)
{
    BP1658CJ_CHECK(s_bp1658cj, "not init", return ESP_ERR_INVALID_STATE);

    uint8_t _value[10] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_R_OUT1 | BIT_ALL_OUT_ENABLE;

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t bp1658cj_regist_channel(bp1658cj_channel_t channel, bp1658cj_out_pin_t pin)
{
    BP1658CJ_CHECK(s_bp1658cj, "not init", return ESP_ERR_INVALID_STATE);
    BP1658CJ_CHECK(channel < BP1658CJ_CHANNEL_MAX, "check channel fail", return ESP_ERR_INVALID_ARG);
    BP1658CJ_CHECK(pin < BP1658CJ_PIN_OUT_MAX, "check out pin fail", return ESP_ERR_INVALID_ARG);

    s_bp1658cj->mapping_addr[channel] = pin;
    return ESP_OK;
}

esp_err_t bp1658cj_set_channel(bp1658cj_channel_t channel, uint16_t value)
{
    BP1658CJ_CHECK(s_bp1658cj, "not init", return ESP_ERR_INVALID_STATE);
    BP1658CJ_CHECK(s_bp1658cj->mapping_addr[channel] != INVALID_ADDR, "channel:%d not regist", return ESP_ERR_INVALID_STATE, channel);
    BP1658CJ_CHECK(value <= 1023, "value out of range", return ESP_ERR_INVALID_ARG);

    if (!s_bp1658cj->init_done) {
        set_mode_and_current(false, s_bp1658cj->rgb_current, s_bp1658cj->cw_current);
    }

    uint8_t _value[2] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_ALL_OUT_ENABLE | get_mapping_addr(channel);

    _value[0] = (value & 0x1F);
    _value[1] = (value >> 5);

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t bp1658cj_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b)
{
    BP1658CJ_CHECK(s_bp1658cj, "not init", return ESP_ERR_INVALID_STATE);
    BP1658CJ_CHECK(s_bp1658cj->mapping_addr[BP1658CJ_CHANNEL_R] != INVALID_ADDR || s_bp1658cj->mapping_addr[BP1658CJ_CHANNEL_G] != INVALID_ADDR || s_bp1658cj->mapping_addr[BP1658CJ_CHANNEL_B] != INVALID_ADDR, "color channel not regist", return ESP_ERR_INVALID_STATE);

    if (!s_bp1658cj->init_done) {
        set_mode_and_current(false, s_bp1658cj->rgb_current, s_bp1658cj->cw_current);
    }

    uint8_t _value[6] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_R_OUT1 | BIT_ALL_OUT_ENABLE;

    _value[s_bp1658cj->mapping_addr[0] * 2 + 0] = (value_r & 0x1F);
    _value[s_bp1658cj->mapping_addr[0] * 2 + 1] = (value_r >> 5);

    _value[s_bp1658cj->mapping_addr[1] * 2 + 0] = (value_g & 0x1F);
    _value[s_bp1658cj->mapping_addr[1] * 2 + 1] = (value_g >> 5);

    _value[s_bp1658cj->mapping_addr[2] * 2 + 0] = (value_b & 0x1F);
    _value[s_bp1658cj->mapping_addr[2] * 2 + 1] = (value_b >> 5);

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t bp1658cj_set_cw_channel(uint16_t value_c, uint16_t value_w)
{
    BP1658CJ_CHECK(s_bp1658cj, "not init", return ESP_ERR_INVALID_STATE);
    BP1658CJ_CHECK(s_bp1658cj->mapping_addr[BP1658CJ_CHANNEL_C] != INVALID_ADDR || s_bp1658cj->mapping_addr[BP1658CJ_CHANNEL_W] != INVALID_ADDR, "white channel not regist", return ESP_ERR_INVALID_STATE);

    if (!s_bp1658cj->init_done) {
        set_mode_and_current(false, s_bp1658cj->rgb_current, s_bp1658cj->cw_current);
    }
    uint8_t _value[4] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_C_OUT4 | BIT_ALL_OUT_ENABLE;

    _value[(s_bp1658cj->mapping_addr[3] - 3) * 2 + 0] = (value_c & 0x1F);
    _value[(s_bp1658cj->mapping_addr[3] - 3) * 2 + 1] = (value_c >> 5);

    _value[(s_bp1658cj->mapping_addr[4] - 3) * 2 + 0] = (value_w & 0x1F);
    _value[(s_bp1658cj->mapping_addr[4] - 3) * 2 + 1] = (value_w >> 5);

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t bp1658cj_set_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_c, uint16_t value_w)
{
    BP1658CJ_CHECK(s_bp1658cj, "not init", return ESP_ERR_INVALID_STATE);
    BP1658CJ_CHECK(s_bp1658cj->mapping_addr[BP1658CJ_CHANNEL_R] != INVALID_ADDR || s_bp1658cj->mapping_addr[BP1658CJ_CHANNEL_G] != INVALID_ADDR || s_bp1658cj->mapping_addr[BP1658CJ_CHANNEL_B] != INVALID_ADDR, "color channel not regist", return ESP_ERR_INVALID_STATE);
    BP1658CJ_CHECK(s_bp1658cj->mapping_addr[BP1658CJ_CHANNEL_C] != INVALID_ADDR || s_bp1658cj->mapping_addr[BP1658CJ_CHANNEL_W] != INVALID_ADDR, "white channel not regist", return ESP_ERR_INVALID_STATE);

    if (!s_bp1658cj->init_done) {
        set_mode_and_current(false, s_bp1658cj->rgb_current, s_bp1658cj->cw_current);
    }
    uint8_t _value[10] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_R_OUT1 | BIT_ALL_OUT_ENABLE;

    _value[s_bp1658cj->mapping_addr[0] * 2 + 0] = (value_r & 0x1F);
    _value[s_bp1658cj->mapping_addr[0] * 2 + 1] = (value_r >> 5);

    _value[s_bp1658cj->mapping_addr[1] * 2 + 0] = (value_g & 0x1F);
    _value[s_bp1658cj->mapping_addr[1] * 2 + 1] = (value_g >> 5);

    _value[s_bp1658cj->mapping_addr[2] * 2 + 0] = (value_b & 0x1F);
    _value[s_bp1658cj->mapping_addr[2] * 2 + 1] = (value_b >> 5);

    _value[s_bp1658cj->mapping_addr[3] * 2 + 0] = (value_c & 0x1F);
    _value[s_bp1658cj->mapping_addr[3] * 2 + 1] = (value_c >> 5);

    _value[s_bp1658cj->mapping_addr[4] * 2 + 0] = (value_w & 0x1F);
    _value[s_bp1658cj->mapping_addr[4] * 2 + 1] = (value_w >> 5);

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t bp1658cj_init(driver_bp1658cj_t *config, void(*hook_func)(void *))
{
    esp_err_t err = ESP_OK;

    BP1658CJ_CHECK(config, "config is null", return ESP_ERR_INVALID_ARG);
    BP1658CJ_CHECK(!s_bp1658cj, "already init done", return ESP_ERR_INVALID_ARG);

    s_bp1658cj = calloc(1, sizeof(bp1658cj_handle_t));
    BP1658CJ_CHECK(s_bp1658cj, "alloc fail", return ESP_ERR_NO_MEM);
    memset(s_bp1658cj->mapping_addr, INVALID_ADDR, BP1658CJ_MAX_PIN);

    s_bp1658cj->rgb_current = config->rgb_current;
    s_bp1658cj->cw_current = config->cw_current;

    if (config->freq_khz > 300) {
        config->freq_khz = 300;
        ESP_LOGW(TAG, "The frequency is too high, adjust it to 400khz");
    }

    err |= iic_driver_init(I2C_NUM_0, config->iic_sda, config->iic_clk, config->freq_khz * IIC_BASE_UNIT_HZ);
    BP1658CJ_CHECK(err == ESP_OK, "i2c master init fail", goto EXIT);

    if (config->enable_iic_queue) {
        err |= iic_driver_send_task_create();
        BP1658CJ_CHECK(err == ESP_OK, "task create fail", goto EXIT);
    }

    return err;
EXIT:

    if (s_bp1658cj) {
        free(s_bp1658cj);
        s_bp1658cj = NULL;
    }
    return err;
}

esp_err_t bp1658cj_deinit(void)
{
    BP1658CJ_CHECK(s_bp1658cj, "not init", return ESP_ERR_INVALID_STATE);

    bp1658cj_set_shutdown();
    iic_driver_deinit();
    iic_driver_task_destroy();
    free(s_bp1658cj);
    s_bp1658cj = NULL;
    return ESP_OK;
}
