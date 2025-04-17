/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include <esp_log.h>

#include "sm2182e.h"
#include "iic.h"

static const char *TAG = "driver_sm2182e";

#define INVALID_ADDR        0xFF
#define IIC_BASE_UNIT_HZ    1000
#define SM2182E_MAX_PIN     2

/**
 * SM2182E register start address - Byte0
 */
/* B[7:6] */
#define BASE_ADDR                   0xC0

/* B[4:3] */
#define BIT_ENABLE_ACTIVE           0x10
#define BIT_DISABLE_OUTPUT          0x00

/* B[2:0] */
#define BIT_CH_FIXED_VALUE          0x03

/**
 * SM2135EH register current address - Byte1
 */
// Nothing

/**
 * SM2135EH register grayscale address - Byte2-5
 *
 *
 */
// Nothing

typedef struct {
    sm2182e_cw_current_t cw_current;
    uint8_t mapping_addr[SM2182E_MAX_PIN];
    bool init_done;
} sm2182e_handle_t;

static sm2182e_handle_t *s_sm2182e = NULL;

esp_err_t sm2182e_set_standby_mode(bool enable_standby)
{
    DRIVER_CHECK(s_sm2182e, "not init", return ESP_ERR_INVALID_STATE);

    uint8_t addr = 0;
    uint8_t value[5] = {0};
    if (enable_standby) {
        addr = BASE_ADDR | BIT_DISABLE_OUTPUT | BIT_CH_FIXED_VALUE;
    } else {
        addr = BASE_ADDR | BIT_ENABLE_ACTIVE | BIT_CH_FIXED_VALUE;
    }

    return iic_driver_write(addr, value, sizeof(value));
}

esp_err_t sm2182e_set_shutdown(void)
{
    DRIVER_CHECK(s_sm2182e, "not init", return ESP_ERR_INVALID_STATE);

    return sm2182e_set_standby_mode(true);
}

esp_err_t sm2182e_regist_channel(sm2182e_channel_t channel, sm2182e_out_pin_t pin)
{
    DRIVER_CHECK(s_sm2182e, "not init", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK(channel >= SM2182E_CHANNEL_C && channel < SM2182E_CHANNEL_MAX, "check channel fail", return ESP_ERR_INVALID_ARG);
    DRIVER_CHECK(pin < SM2182E_PIN_OUT_MAX, "check out pin fail", return ESP_ERR_INVALID_ARG);

    s_sm2182e->mapping_addr[channel - 3] = pin;
    return ESP_OK;
}

esp_err_t sm2182e_set_cw_channel(uint16_t value_c, uint16_t value_w)
{
    DRIVER_CHECK(s_sm2182e, "not init", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK(s_sm2182e->mapping_addr[SM2182E_CHANNEL_C - 3] != INVALID_ADDR || s_sm2182e->mapping_addr[SM2182E_CHANNEL_W - 3] != INVALID_ADDR, "white channel not regist", return ESP_ERR_INVALID_STATE);
    DRIVER_CHECK(value_c <= 1023 && value_w <= 1023, "value out of range", return ESP_ERR_INVALID_ARG);

    uint8_t _value[5] = { 0 };
    uint8_t addr = BASE_ADDR | BIT_ENABLE_ACTIVE | BIT_CH_FIXED_VALUE;

    _value[0] = s_sm2182e->cw_current;

    _value[(s_sm2182e->mapping_addr[0]) * 2 + 2] = value_c & 0xFF;
    _value[(s_sm2182e->mapping_addr[0]) * 2 + 1] = value_c >> 8;

    _value[(s_sm2182e->mapping_addr[1]) * 2 + 2] = value_w & 0xFF;
    _value[(s_sm2182e->mapping_addr[1]) * 2 + 1] = value_w >> 8;

    ESP_LOGD(TAG, "addr:%x [out1:[%x][%x] out2:[%x][%x]]", addr, _value[1], _value[2], _value[3], _value[4]);
    return iic_driver_write(addr, _value, sizeof(_value));
}

sm2182e_cw_current_t sm2182e_cw_current_mapping(int current_mA)
{
    DRIVER_CHECK((current_mA >= 5) && (current_mA <= 80), "The current value is incorrect and cannot be mapped.", return SM2182E_CW_CURRENT_MAX);

    const uint8_t limits[] = {5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80};
    for (int i = 0; i < sizeof(limits); i++) {
        if (current_mA == limits[i]) {
            return (sm2182e_cw_current_t)i;
        }
    }

    return SM2182E_CW_CURRENT_MAX;
}

esp_err_t sm2182e_init(driver_sm2182e_t *config, void(*hook_func)(void *))
{
    esp_err_t err = ESP_OK;

    DRIVER_CHECK(config, "config is null", return ESP_ERR_INVALID_ARG);
    DRIVER_CHECK(!s_sm2182e, "already init done", return ESP_ERR_INVALID_ARG);
    DRIVER_CHECK(config->cw_current >= SM2182E_CW_CURRENT_5MA && config->cw_current < SM2182E_CW_CURRENT_MAX, "cw channel current param error", return ESP_ERR_INVALID_ARG);

    s_sm2182e = calloc(1, sizeof(sm2182e_handle_t));
    DRIVER_CHECK(s_sm2182e, "alloc fail", return ESP_ERR_NO_MEM);
    memset(s_sm2182e->mapping_addr, INVALID_ADDR, SM2182E_MAX_PIN);

    s_sm2182e->cw_current = config->cw_current;

    if (config->freq_khz > 400) {
        config->freq_khz = 400;
        ESP_LOGW(TAG, "The frequency is too high, adjust it to 400khz");
    }

    err |= iic_driver_init(I2C_NUM_0, config->iic_sda, config->iic_clk, config->freq_khz * IIC_BASE_UNIT_HZ);
    DRIVER_CHECK(err == ESP_OK, "i2c master init fail", goto EXIT);

    if (config->enable_iic_queue) {
        err |= iic_driver_send_task_create();
        DRIVER_CHECK(err == ESP_OK, "task create fail", goto EXIT);
    }

    return err;
EXIT:

    if (s_sm2182e) {
        free(s_sm2182e);
        s_sm2182e = NULL;
    }
    return err;
}

esp_err_t sm2182e_deinit(void)
{
    DRIVER_CHECK(s_sm2182e, "not init", return ESP_ERR_INVALID_STATE);

    sm2182e_set_shutdown();
    iic_driver_deinit();
    iic_driver_task_destroy();
    free(s_sm2182e);
    s_sm2182e = NULL;
    return ESP_OK;
}
