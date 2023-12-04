/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include <esp_log.h>

#include "iic.h"
#include "sm2x35egh.h"

/* Driver available for sm2235egh sm2335egh */
static const char *TAG = "sm2x35egh";

#define SM2x35EGH_CHECK(a, str, action, ...)                                \
    if (unlikely(!(a))) {                                                   \
        ESP_LOGE(TAG, str, ##__VA_ARGS__);                                  \
        action;                                                             \
    }

#if CONFIG_ENABLE_DRIVER_DEBUG_LOG_OUTPUT
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

#define INVALID_ADDR        0xFF
#define IIC_BASE_UNIT_HZ    1000
#define SM2x35EGH_MAX_PIN   5

/**
 * SM2x35EGH register start address - Byte0
 */
#define BASE_ADDR           0xC0

/* B[2:0] */
#define BIT_R_OUT1          0x00
#define BIT_G_OUT2          0x01
#define BIT_B_OUT3          0x02
#define BIT_C_OUT4          0x03
#define BIT_W_OUT5          0x04

/* B[4:3] */
#define BIT_STANDBY         0x00
#define BIT_RGB_CHANNEL     0x08
#define BIT_WY_CHANNEL      0x10
#define BIT_ALL_CHANNEL     0x18

/**
 * SM2x35EGH register current address - Byte1
 */
/* B[3:0] */
// w/y current

/* B[7:4] */
// R/G/B current

/**
 * SM2x35EGH register grayscale address - Byte2-7
 *
 *
 */
// Nothing

typedef struct {
    sm2x35egh_rgb_current_t rgb_current;
    sm2x35egh_cw_current_t cw_current;
    uint8_t mapping_addr[SM2x35EGH_MAX_PIN];
    bool init_done;
} sm2x35eh_handle_t;

static sm2x35eh_handle_t *s_sm2x35egh = NULL;

static uint8_t get_mapping_addr(sm2x35egh_channel_t channel)
{
    uint8_t addr[] = { BIT_R_OUT1, BIT_G_OUT2, BIT_B_OUT3, BIT_C_OUT4, BIT_W_OUT5 };
    uint8_t result = addr[s_sm2x35egh->mapping_addr[channel]];

    return result;
}

static uint8_t get_max_current(void)
{
    uint8_t value = s_sm2x35egh->rgb_current << 4 | s_sm2x35egh->cw_current;
    return value;
}

esp_err_t sm2x35egh_set_standby_mode(bool enable_standby)
{
    SM2x35EGH_CHECK(s_sm2x35egh, "not init", return ESP_ERR_INVALID_STATE);

    uint8_t addr = BASE_ADDR | BIT_STANDBY | BIT_R_OUT1;
    uint8_t value[11] = { 0 };
    value[0] = get_max_current();
    if (!enable_standby) {
        addr = BASE_ADDR | BIT_ALL_CHANNEL | BIT_R_OUT1;
    }

    return iic_driver_write(addr, value, sizeof(value));
}

esp_err_t sm2x35egh_set_shutdown(void)
{
    SM2x35EGH_CHECK(s_sm2x35egh, "not init", return ESP_ERR_INVALID_STATE);

    uint8_t addr = BASE_ADDR | BIT_ALL_CHANNEL | BIT_R_OUT1;
    uint8_t value[11] = { 0 };
    value[0] = get_max_current();

    return iic_driver_write(addr, value, sizeof(value));
}

esp_err_t sm2x35egh_regist_channel(sm2x35egh_channel_t channel, sm2x35egh_out_pin_t pin)
{
    SM2x35EGH_CHECK(s_sm2x35egh, "not init", return ESP_ERR_INVALID_STATE);
    SM2x35EGH_CHECK(channel < SM2x35EGH_CHANNEL_MAX, "check channel fail", return ESP_ERR_INVALID_ARG);
    SM2x35EGH_CHECK(pin < SM2x35EGH_PIN_OUT_MAX, "check out pin fail", return ESP_ERR_INVALID_ARG);

    s_sm2x35egh->mapping_addr[channel] = pin;
    return ESP_OK;
}

esp_err_t sm2x35egh_set_channel(sm2x35egh_channel_t channel, uint16_t value)
{
    SM2x35EGH_CHECK(s_sm2x35egh, "not init", return ESP_ERR_INVALID_STATE);
    SM2x35EGH_CHECK(s_sm2x35egh->mapping_addr[channel] != INVALID_ADDR, "channel:%d not regist", return ESP_ERR_INVALID_STATE, channel);
    SM2x35EGH_CHECK(value <= 1023, "value out of range", return ESP_ERR_INVALID_ARG);

    uint8_t addr = BASE_ADDR | BIT_ALL_CHANNEL | get_mapping_addr(channel);
    uint8_t _value[3] = { 0 };

    _value[0] = get_max_current();
    _value[2] = value & 0xFF;
    _value[1] = value >> 8;

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t sm2x35egh_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b)
{
    SM2x35EGH_CHECK(s_sm2x35egh, "not init", return ESP_ERR_INVALID_STATE);
    SM2x35EGH_CHECK(s_sm2x35egh->mapping_addr[0] != INVALID_ADDR || s_sm2x35egh->mapping_addr[1] != INVALID_ADDR || s_sm2x35egh->mapping_addr[2] != INVALID_ADDR, "color channel not regist", return ESP_ERR_INVALID_STATE);

    uint8_t addr = BASE_ADDR | BIT_ALL_CHANNEL | BIT_R_OUT1;
    uint8_t _value[7] = { 0 };

    _value[0] = get_max_current();

    _value[s_sm2x35egh->mapping_addr[0] * 2 + 2] = value_r & 0xFF;
    _value[s_sm2x35egh->mapping_addr[0] * 2 + 1] = value_r >> 8;

    _value[s_sm2x35egh->mapping_addr[1] * 2 + 2] = value_g & 0xFF;
    _value[s_sm2x35egh->mapping_addr[1] * 2 + 1] = value_g >> 8;

    _value[s_sm2x35egh->mapping_addr[2] * 2 + 2] = value_b & 0xFF;
    _value[s_sm2x35egh->mapping_addr[2] * 2 + 1] = value_b >> 8;

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t sm2x35egh_set_cw_channel(uint16_t value_c, uint16_t value_w)
{
    SM2x35EGH_CHECK(s_sm2x35egh, "not init", return ESP_ERR_INVALID_STATE);
    SM2x35EGH_CHECK(s_sm2x35egh->mapping_addr[3] != INVALID_ADDR || s_sm2x35egh->mapping_addr[4] != INVALID_ADDR, "white channel not regist", return ESP_ERR_INVALID_STATE);

    uint8_t _value[5] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_ALL_CHANNEL | BIT_C_OUT4;
    _value[0] = get_max_current();

    _value[(s_sm2x35egh->mapping_addr[3] - 3) * 2 + 2] = value_c & 0xFF;
    _value[(s_sm2x35egh->mapping_addr[3] - 3) * 2 + 1] = value_c >> 8;

    _value[(s_sm2x35egh->mapping_addr[4] - 3) * 2 + 2] = value_w & 0xFF;
    _value[(s_sm2x35egh->mapping_addr[4] - 3) * 2 + 1] = value_w >> 8;

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t sm2x35egh_set_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_c, uint16_t value_w)
{
    SM2x35EGH_CHECK(s_sm2x35egh, "not init", return ESP_ERR_INVALID_STATE);
    SM2x35EGH_CHECK(s_sm2x35egh->mapping_addr[3] != INVALID_ADDR || s_sm2x35egh->mapping_addr[4] != INVALID_ADDR, "white channel not regist", return ESP_ERR_INVALID_STATE);

    uint8_t _value[11] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_ALL_CHANNEL | BIT_R_OUT1;
    _value[0] = get_max_current();

    _value[s_sm2x35egh->mapping_addr[0] * 2 + 2] = value_r & 0xFF;
    _value[s_sm2x35egh->mapping_addr[0] * 2 + 1] = value_r >> 8;

    _value[s_sm2x35egh->mapping_addr[1] * 2 + 2] = value_g & 0xFF;
    _value[s_sm2x35egh->mapping_addr[1] * 2 + 1] = value_g >> 8;

    _value[s_sm2x35egh->mapping_addr[2] * 2 + 2] = value_b & 0xFF;
    _value[s_sm2x35egh->mapping_addr[2] * 2 + 1] = value_b >> 8;

    _value[s_sm2x35egh->mapping_addr[3] * 2 + 2] = value_c & 0xFF;
    _value[s_sm2x35egh->mapping_addr[3] * 2 + 1] = value_c >> 8;

    _value[s_sm2x35egh->mapping_addr[4] * 2 + 2] = value_w & 0xFF;
    _value[s_sm2x35egh->mapping_addr[4] * 2 + 1] = value_w >> 8;

    return iic_driver_write(addr, _value, sizeof(_value));
}

esp_err_t sm2x35egh_init(driver_sm2x35egh_t *config, void(*hook_func)(void *))
{
    esp_err_t err = ESP_OK;

    SM2x35EGH_CHECK(config, "config is null", return ESP_ERR_INVALID_ARG);
    SM2x35EGH_CHECK(!s_sm2x35egh, "already init done", return ESP_ERR_INVALID_ARG);

    s_sm2x35egh = calloc(1, sizeof(sm2x35eh_handle_t));
    SM2x35EGH_CHECK(s_sm2x35egh, "alloc fail", return ESP_ERR_NO_MEM);
    memset(s_sm2x35egh->mapping_addr, INVALID_ADDR, SM2x35EGH_MAX_PIN);

    s_sm2x35egh->rgb_current = config->rgb_current;
    s_sm2x35egh->cw_current = config->cw_current;

    if (config->freq_khz > 400) {
        config->freq_khz = 400;
        ESP_LOGW(TAG, "The frequency is too high, adjust it to 400khz");
    }

    err |= iic_driver_init(I2C_NUM_0, config->iic_sda, config->iic_clk, config->freq_khz * IIC_BASE_UNIT_HZ);
    SM2x35EGH_CHECK(err == ESP_OK, "i2c master init fail", goto EXIT);

    if (config->enable_iic_queue) {
        err |= iic_driver_send_task_create();
        SM2x35EGH_CHECK(err == ESP_OK, "task create fail", goto EXIT);
    }

    return err;
EXIT:

    if (s_sm2x35egh) {
        free(s_sm2x35egh);
    }
    return err;
}

esp_err_t sm2x35egh_deinit(void)
{
    SM2x35EGH_CHECK(s_sm2x35egh, "not init", return ESP_ERR_INVALID_STATE);

    sm2x35egh_set_shutdown();
    iic_driver_deinit();
    iic_driver_task_destroy();
    free(s_sm2x35egh);
    s_sm2x35egh = NULL;
    return ESP_OK;
}
