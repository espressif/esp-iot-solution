/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ch455g.h"

#define I2C_DEFAULT_SPEED   100000

static i2c_port_t s_i2c_port = -1;
static SemaphoreHandle_t s_cmd_sem = NULL;

/**
 * Digital tube address command lookup take
 */
const uint16_t index_lookup_table[CH455_MAX_TUBE_NUM] = {0x1400, 0x1500, 0x1600, 0x1700};
/**
 *  BCD code lookup table
 */
const uint8_t code_lookup_table[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x58, 0x5E, 0x79, 0x71};

esp_err_t ch455g_driver_install(i2c_port_t i2c_num, gpio_num_t sda_pin, gpio_num_t scl_pin)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl_pin,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_DEFAULT_SPEED,
    };
    esp_err_t ret = i2c_param_config(i2c_num, &conf);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = i2c_driver_install(i2c_num, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    s_i2c_port = i2c_num;
    s_cmd_sem = xSemaphoreCreateMutex();
    if (s_cmd_sem == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t ch455g_write_cmd(uint16_t ch455_cmd)
{
    xSemaphoreTake(s_cmd_sem, portMAX_DELAY);
    if (s_i2c_port == -1) {
        xSemaphoreGive(s_cmd_sem);
        return ESP_ERR_INVALID_STATE;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        xSemaphoreGive(s_cmd_sem);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = i2c_master_start(cmd);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    ret = i2c_master_write_byte(cmd, ((uint8_t)(ch455_cmd >> 7) & CH455_I2C_MASK) | CH455_I2C_ADDR, 1);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    ret = i2c_master_write_byte(cmd, (uint8_t)ch455_cmd, 1);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    ret = i2c_master_stop(cmd);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    ret = i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        goto cleanup;
    }
    i2c_cmd_link_delete(cmd);

    xSemaphoreGive(s_cmd_sem);
    return ESP_OK;

cleanup:
    xSemaphoreGive(s_cmd_sem);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t ch455_driver_uninstall(void)
{
    if (s_i2c_port == -1) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = i2c_driver_delete(s_i2c_port);
    if (ret != ESP_OK) {
        abort();
    }
    s_i2c_port = -1;
    vSemaphoreDelete(s_cmd_sem);
    return ESP_OK;
}
