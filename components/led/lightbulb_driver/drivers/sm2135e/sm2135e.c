/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include <esp_log.h>

#include "iic.h"
#include "sm2135e.h"

static const char *TAG = "driver_sm2135e";

#define SM2135E_CHECK(a, str, action, ...)                                  \
    if (unlikely(!(a))) {                                                   \
        ESP_LOGE(TAG, str, ##__VA_ARGS__);                                  \
        action;                                                             \
    }

#if CONFIG_ENABLE_DRIVER_DEBUG_LOG_OUTPUT
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

#define INVALID_ADDR        0xFF
#define IIC_BASE_UNIT_HZ    1000
#define SM2135E_MAX_PIN     5
/**
 * sm2135e register start address - Byte0
 */
/* B[7:6] */
#define BASE_ADDR       0xC0

/* B[2:0] */
#define BIT_MAX_CURRENT 0x00
#define BIT_MODE_SELECT 0x01
#define BIT_R_OUT1      0x02
#define BIT_G_OUT2      0x03
#define BIT_B_OUT3      0x04
#define BIT_W_OUT4      0x05
#define BIT_Y_OUT5      0x06

/**
 * sm2135e register current address - Byte1
 *
 */
// Nothing

/**
 * sm2135e register mode address - Byte2
 *
 */
/* B[7] */
#define BIT_RGB_MODE 0x00
#define BIT_WY_MODE  0x80

/**
 * sm2135e register grayscale address - Byte3-7
 *
 */
// Nothing

typedef struct {
    sm2135e_rgb_current_t rgb_current;
    sm2135e_wy_current_t wy_current;
    uint8_t mapping_addr[SM2135E_MAX_PIN];
    bool running_wy_mode;
} sm2135e_handle_t;

static sm2135e_handle_t *s_sm2135e = NULL;

static uint8_t get_mapping_addr(sm2135e_channel_t channel)
{
    return s_sm2135e->mapping_addr[channel];
}

static uint8_t get_max_current(void)
{
    uint8_t value = (s_sm2135e->rgb_current << 4) | (s_sm2135e->wy_current);
    return value;
}

static esp_err_t set_mode_and_current(bool set_wy_mode, sm2135e_rgb_current_t rgb, sm2135e_wy_current_t wy)
{
    SM2135E_CHECK(s_sm2135e, "not init", return ESP_ERR_INVALID_STATE);

    uint8_t addr = BASE_ADDR | BIT_MAX_CURRENT;
    uint8_t mode = BIT_RGB_MODE;
    uint8_t _value[7] = { 0 };
    if (set_wy_mode) {
        mode = BIT_WY_MODE;
        s_sm2135e->running_wy_mode = true;
    } else {
        s_sm2135e->running_wy_mode = false;
    }

    _value[0] = (rgb << 4) | (wy);
    _value[1] = mode;

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t sm2135e_set_max_current(sm2135e_rgb_current_t rgb, sm2135e_wy_current_t wy)
{
    SM2135E_CHECK(s_sm2135e, "not init", return ESP_ERR_INVALID_STATE);

    uint8_t value = 0;
    uint8_t addr = BASE_ADDR | BIT_MAX_CURRENT;
    value = (rgb << 4) | (wy);
    return iic_driver_write(addr, &value, 1);
}

esp_err_t sm2135e_set_output_mode(bool set_wy_mode)
{
    SM2135E_CHECK(s_sm2135e, "not init", return ESP_ERR_INVALID_STATE);

    uint8_t addr = BASE_ADDR | BIT_MODE_SELECT;
    uint8_t mode = BIT_RGB_MODE;
    uint8_t _value[1] = { 0 };
    s_sm2135e->running_wy_mode = false;
    if (set_wy_mode) {
        mode = BIT_WY_MODE;
        s_sm2135e->running_wy_mode = true;
    }
    ESP_LOGD(TAG, "sm2135e_set_output_mode:%d", set_wy_mode);
    _value[0] = mode;

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t sm2135e_set_shutdown(void)
{
    SM2135E_CHECK(s_sm2135e, "not init", return ESP_ERR_INVALID_STATE);
    uint8_t _value[5] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_R_OUT1;
    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t sm2135e_regist_channel(sm2135e_channel_t channel, sm2135e_out_pin_t pin)
{
    SM2135E_CHECK(s_sm2135e, "not init", return ESP_ERR_INVALID_STATE);
    SM2135E_CHECK(channel < SM2135E_CHANNEL_MAX, "check channel fail", return ESP_ERR_INVALID_ARG);
    SM2135E_CHECK(pin < SM2135E_PIN_OUT_MAX, "check out pin fail", return ESP_ERR_INVALID_ARG);

    s_sm2135e->mapping_addr[channel] = pin;
    return ESP_OK;
}

esp_err_t sm2135e_set_channel(sm2135e_channel_t channel, uint8_t value)
{
    SM2135E_CHECK(s_sm2135e, "not init", return ESP_ERR_INVALID_STATE);
    SM2135E_CHECK(s_sm2135e->mapping_addr[channel] != INVALID_ADDR, "channel:%d not regist", return ESP_ERR_INVALID_STATE, channel);

    uint8_t _value = value;
    uint8_t addr = BASE_ADDR;
    esp_err_t err = ESP_OK;

    if (channel >= SM2135E_CHANNEL_W) {
        if (!s_sm2135e->running_wy_mode) {
            ESP_LOGD(TAG, "switch channel");
            err |= set_mode_and_current(true, s_sm2135e->rgb_current, s_sm2135e->wy_current);
        }
        addr |= (BIT_R_OUT1 + get_mapping_addr(channel));
        err |= iic_driver_write(addr, &_value, 1);
        return err;
    }

    if (s_sm2135e->running_wy_mode) {
        ESP_LOGD(TAG, "switch channel");
        err |= set_mode_and_current(false, s_sm2135e->rgb_current, s_sm2135e->wy_current);
    }
    addr |= (BIT_R_OUT1 + get_mapping_addr(channel));
    ESP_LOGD(TAG, "addr:%x value:%x", addr, _value);

    return iic_driver_write(addr, &_value, 1);
}

esp_err_t sm2135e_set_rgb_channel(uint8_t value_r, uint8_t value_g, uint8_t value_b)
{
    SM2135E_CHECK(s_sm2135e, "not init", return ESP_ERR_INVALID_STATE);
    SM2135E_CHECK(s_sm2135e->mapping_addr[0] != INVALID_ADDR || s_sm2135e->mapping_addr[1] != INVALID_ADDR || s_sm2135e->mapping_addr[2] != INVALID_ADDR, "color channel not regist", return ESP_ERR_INVALID_STATE);

    uint8_t _value[5] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_MAX_CURRENT;
    uint8_t mode = BIT_RGB_MODE;
    s_sm2135e->running_wy_mode = false;

    _value[0] = get_max_current();
    _value[1] = mode;
    _value[s_sm2135e->mapping_addr[SM2135E_CHANNEL_R] + 2] = value_r;
    _value[s_sm2135e->mapping_addr[SM2135E_CHANNEL_G] + 2] = value_g;
    _value[s_sm2135e->mapping_addr[SM2135E_CHANNEL_B] + 2] = value_b;

    ESP_LOGD(TAG, "addr:%x current:%x mode:%x [out1:[%x] out2:[%x] out3:[%x]]", addr, _value[0], _value[1], _value[2], _value[3], _value[4]);
    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t sm2135e_set_wy_channel(uint8_t value_w, uint8_t value_y)
{
    SM2135E_CHECK(s_sm2135e, "not init", return ESP_ERR_INVALID_STATE);
    SM2135E_CHECK(s_sm2135e->mapping_addr[3] != INVALID_ADDR || s_sm2135e->mapping_addr[4] != INVALID_ADDR, "white channel not regist", return ESP_ERR_INVALID_STATE);

    uint8_t _value[2] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_W_OUT4;

    _value[s_sm2135e->mapping_addr[SM2135E_CHANNEL_W] - 3] = value_w;
    _value[s_sm2135e->mapping_addr[SM2135E_CHANNEL_Y] - 3] = value_y;

    if (!s_sm2135e->running_wy_mode) {
        ESP_LOGD(TAG, "switch channel");
        set_mode_and_current(true, s_sm2135e->rgb_current, s_sm2135e->wy_current);
    }

    ESP_LOGD(TAG, "addr:%x out1:[%x] out2:[%x]", addr, _value[0], _value[1]);
    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t sm2135e_init(driver_sm2135e_t *config, void(*hook_func)(void *))
{
    esp_err_t err = ESP_OK;

    SM2135E_CHECK(config, "config is null", return ESP_ERR_INVALID_ARG);
    SM2135E_CHECK(!s_sm2135e, "already init done", return ESP_ERR_INVALID_ARG);

    s_sm2135e = calloc(1, sizeof(sm2135e_handle_t));
    SM2135E_CHECK(s_sm2135e, "alloc fail", return ESP_ERR_NO_MEM);
    memset(s_sm2135e->mapping_addr, INVALID_ADDR, SM2135E_MAX_PIN);

    s_sm2135e->rgb_current = config->rgb_current;
    s_sm2135e->wy_current = config->wy_current;

    if (config->freq_khz > 400) {
        config->freq_khz = 400;
        ESP_LOGW(TAG, "The frequency is too high, adjust it to 400khz");
    }

    err |= iic_driver_init(I2C_NUM_0, config->iic_sda, config->iic_clk, config->freq_khz * IIC_BASE_UNIT_HZ);
    SM2135E_CHECK(err == ESP_OK, "i2c master init fail", goto EXIT);

    if (config->enable_iic_queue) {
        err |= iic_driver_send_task_create();
        SM2135E_CHECK(err == ESP_OK, "task create fail", goto EXIT);
    }

    return err;
EXIT:
    if (s_sm2135e) {
        free(s_sm2135e);
        s_sm2135e = NULL;
    }
    return err;
}

esp_err_t sm2135e_deinit(void)
{
    SM2135E_CHECK(s_sm2135e, "not init", return ESP_ERR_INVALID_STATE);

    sm2135e_set_shutdown();
    iic_driver_deinit();
    iic_driver_task_destroy();
    free(s_sm2135e);
    s_sm2135e = NULL;
    return ESP_OK;
}
