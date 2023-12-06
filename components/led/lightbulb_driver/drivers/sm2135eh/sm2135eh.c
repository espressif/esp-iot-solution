/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include <esp_log.h>

#include "sm2135eh.h"
#include "iic.h"

static const char *TAG = "driver_sm2135eh";

#define SM2135EH_CHECK(a, str, action, ...)                                 \
    if (unlikely(!(a))) {                                                   \
        ESP_LOGE(TAG, str, ##__VA_ARGS__);                                  \
        action;                                                             \
    }

#if CONFIG_ENABLE_DRIVER_DEBUG_LOG_OUTPUT
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

#define INVALID_ADDR        0xFF
#define IIC_BASE_UNIT_HZ    1000
#define SM2135EH_MAX_PIN    5

/**
 * SM2135EH register start address - Byte0
 */
#define BASE_ADDR 0xC0

/* B[2:0] */
#define BIT_MAX_CURRENT             0x00
#define BIT_STANDBY_MODE_SELECT     0x01
#define BIT_R_OUT1                  0x02
#define BIT_G_OUT2                  0x03
#define BIT_B_OUT3                  0x04
#define BIT_W_OUT4                  0x05
#define BIT_Y_OUT5                  0x06

/**
 * SM2135EH register current address - Byte1
 */
// Nothing

/**
 * SM2135EH register standby mode address - Byte2
 *
 */
/* B[5] */
#define BIT_ENABLE_STANDBY  0x20
#define BIT_DISABLE_STANDBY 0x00

/**
 * SM2135EH register grayscale address - Byte3-7
 *
 *
 */
// Nothing

typedef struct {
    sm2135eh_rgb_current_t rgb_current;
    sm2135eh_wy_current_t wy_current;
    uint8_t mapping_addr[SM2135EH_MAX_PIN];
    bool init_done;
} sm2135eh_handle_t;

static sm2135eh_handle_t *s_sm2135eh = NULL;

static uint8_t get_mapping_addr(sm2135eh_channel_t channel)
{
    uint8_t addr[] = { BIT_R_OUT1, BIT_G_OUT2, BIT_B_OUT3, BIT_W_OUT4, BIT_Y_OUT5 };
    uint8_t result = addr[s_sm2135eh->mapping_addr[channel]];

    return result;
}

static esp_err_t set_mode_and_current(bool enable_standby_mode, sm2135eh_rgb_current_t rgb, sm2135eh_wy_current_t wy)
{
    uint8_t value[2] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_MAX_CURRENT;
    value[0] = (rgb << 4) | (wy);
    value[1] = BIT_DISABLE_STANDBY;
    if (enable_standby_mode) {
        value[1] |= BIT_ENABLE_STANDBY;
        s_sm2135eh->init_done = false;
    } else {
        s_sm2135eh->init_done = true;
    }

    return iic_driver_write(addr, value, sizeof(value));
}

esp_err_t sm2135eh_set_max_current(sm2135eh_rgb_current_t rgb, sm2135eh_wy_current_t wy)
{
    SM2135EH_CHECK(s_sm2135eh, "not init", return ESP_ERR_INVALID_STATE);

    uint8_t value = 0;
    uint8_t addr = BASE_ADDR | BIT_MAX_CURRENT;
    value = (rgb << 4) | (wy);

    return iic_driver_write(addr, &value, 1);
}

esp_err_t sm2135eh_set_standby_mode(bool enable_standby)
{
    SM2135EH_CHECK(s_sm2135eh, "not init", return ESP_ERR_INVALID_STATE);

    uint8_t addr = BASE_ADDR | BIT_STANDBY_MODE_SELECT;
    uint8_t value = BIT_DISABLE_STANDBY;
    if (enable_standby) {
        value |= BIT_ENABLE_STANDBY;
        s_sm2135eh->init_done = false;
    } else {
        s_sm2135eh->init_done = true;
    }
    ESP_LOGD(TAG, "sm2135eh_set_standby_mode:%d", enable_standby);

    return iic_driver_write(addr, &value, 1);
}

esp_err_t sm2135eh_set_shutdown(void)
{
    SM2135EH_CHECK(s_sm2135eh, "not init", return ESP_ERR_INVALID_STATE);

    uint8_t _value[5] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_R_OUT1;
    iic_driver_write(addr, _value, sizeof(_value));

    return sm2135eh_set_standby_mode(true);
}

esp_err_t sm2135eh_regist_channel(sm2135eh_channel_t channel, sm2135eh_out_pin_t pin)
{
    SM2135EH_CHECK(s_sm2135eh, "not init", return ESP_ERR_INVALID_STATE);
    SM2135EH_CHECK(channel < SM2135EH_CHANNEL_MAX, "check channel fail", return ESP_ERR_INVALID_ARG);
    SM2135EH_CHECK(pin < SM2135EH_PIN_OUT_MAX, "check out pin fail", return ESP_ERR_INVALID_ARG);

    s_sm2135eh->mapping_addr[channel] = pin;
    return ESP_OK;
}

esp_err_t sm2135eh_set_channel(sm2135eh_channel_t channel, uint8_t value)
{
    SM2135EH_CHECK(s_sm2135eh, "not init", return ESP_ERR_INVALID_STATE);
    SM2135EH_CHECK(s_sm2135eh->mapping_addr[channel] != INVALID_ADDR, "channel:%d not regist", return ESP_ERR_INVALID_STATE, channel);

    if (!s_sm2135eh->init_done) {
        set_mode_and_current(false, s_sm2135eh->rgb_current, s_sm2135eh->wy_current);
    }
    uint8_t _value = value;
    uint8_t addr = BASE_ADDR | get_mapping_addr(channel);

    return iic_driver_write(addr, &_value, sizeof(_value));
}

esp_err_t sm2135eh_set_rgb_channel(uint8_t value_r, uint8_t value_g, uint8_t value_b)
{
    SM2135EH_CHECK(s_sm2135eh, "not init", return ESP_ERR_INVALID_STATE);
    SM2135EH_CHECK(s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_R] != INVALID_ADDR || s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_G] != INVALID_ADDR || s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_B] != INVALID_ADDR, "color channel not regist", return ESP_ERR_INVALID_STATE);

    if (!s_sm2135eh->init_done) {
        set_mode_and_current(false, s_sm2135eh->rgb_current, s_sm2135eh->wy_current);
    }
    uint8_t _value[3] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_R_OUT1;

    _value[s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_R]] = value_r;
    _value[s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_G]] = value_g;
    _value[s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_B]] = value_b;

    ESP_LOGD(TAG, "addr:%x [out1:[%x] out2:[%x] out3:[%x]]", addr, _value[0], _value[1], _value[2]);
    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t sm2135eh_set_wy_channel(uint8_t value_w, uint8_t value_y)
{
    SM2135EH_CHECK(s_sm2135eh, "not init", return ESP_ERR_INVALID_STATE);
    SM2135EH_CHECK(s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_W] != INVALID_ADDR || s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_Y] != INVALID_ADDR, "white channel not regist", return ESP_ERR_INVALID_STATE);

    if (!s_sm2135eh->init_done) {
        set_mode_and_current(false, s_sm2135eh->rgb_current, s_sm2135eh->wy_current);
    }
    uint8_t _value[2] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_W_OUT4;

    _value[s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_W] - 3] = value_w;
    _value[s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_Y] - 3] = value_y;

    ESP_LOGD(TAG, "addr:%x [out1:[%x] out2:[%x]]", addr, _value[0], _value[1]);
    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t sm2135eh_set_rgbwy_channel(uint8_t value_r, uint8_t value_g, uint8_t value_b, uint8_t value_w, uint8_t value_y)
{
    SM2135EH_CHECK(s_sm2135eh, "not init", return ESP_ERR_INVALID_STATE);
    SM2135EH_CHECK(s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_R] != INVALID_ADDR || s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_G] != INVALID_ADDR || s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_B] != INVALID_ADDR, "color channel not regist", return ESP_ERR_INVALID_STATE);
    SM2135EH_CHECK(s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_W] != INVALID_ADDR || s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_Y] != INVALID_ADDR, "white channel not regist", return ESP_ERR_INVALID_STATE);

    if (!s_sm2135eh->init_done) {
        sm2135eh_set_max_current(s_sm2135eh->rgb_current, s_sm2135eh->wy_current);
        sm2135eh_set_standby_mode(false);
        s_sm2135eh->init_done = true;
    }
    uint8_t _value[5] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_R_OUT1;

    _value[s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_R]] = value_r;
    _value[s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_G]] = value_g;
    _value[s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_B]] = value_b;
    _value[s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_W]] = value_w;
    _value[s_sm2135eh->mapping_addr[SM2135EH_CHANNEL_Y]] = value_y;

    ESP_LOGD(TAG, "addr:%x [out1:[%x] out2:[%x] out3:[%x] out4:[%x] out5:[%x]]", addr, _value[0], _value[1], _value[2], _value[3], _value[4]);
    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t sm2135eh_init(driver_sm2135eh_t *config, void(*hook_func)(void *))
{
    esp_err_t err = ESP_OK;

    SM2135EH_CHECK(config, "config is null", return ESP_ERR_INVALID_ARG);
    SM2135EH_CHECK(!s_sm2135eh, "already init done", return ESP_ERR_INVALID_ARG);

    s_sm2135eh = calloc(1, sizeof(sm2135eh_handle_t));
    SM2135EH_CHECK(s_sm2135eh, "alloc fail", return ESP_ERR_NO_MEM);
    memset(s_sm2135eh->mapping_addr, INVALID_ADDR, SM2135EH_MAX_PIN);

    s_sm2135eh->rgb_current = config->rgb_current;
    s_sm2135eh->wy_current = config->wy_current;

    if (config->freq_khz > 400) {
        config->freq_khz = 400;
        ESP_LOGW(TAG, "The frequency is too high, adjust it to 400khz");
    }

    err |= iic_driver_init(I2C_NUM_0, config->iic_sda, config->iic_clk, config->freq_khz * IIC_BASE_UNIT_HZ);
    SM2135EH_CHECK(err == ESP_OK, "i2c master init fail", goto EXIT);

    if (config->enable_iic_queue) {
        err |= iic_driver_send_task_create();
        SM2135EH_CHECK(err == ESP_OK, "task create fail", goto EXIT);
    }

    return err;
EXIT:

    if (s_sm2135eh) {
        free(s_sm2135eh);
        s_sm2135eh = NULL;
    }
    return err;
}

esp_err_t sm2135eh_deinit(void)
{
    SM2135EH_CHECK(s_sm2135eh, "not init", return ESP_ERR_INVALID_STATE);

    sm2135eh_set_shutdown();
    iic_driver_deinit();
    iic_driver_task_destroy();
    free(s_sm2135eh);
    s_sm2135eh = NULL;
    return ESP_OK;
}
