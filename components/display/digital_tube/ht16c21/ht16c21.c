/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "ht16c21.h"
#include "i2c_bus.h"

static const char *TAG = "HT16C21";

#define HT_CHECK(a, str, ret) if (!(a)) {                                              \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);       \
        return (ret);                                                                   \
    }

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
} ht16c21_dev_t;

ht16c21_handle_t ht16c21_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    ht16c21_dev_t *seg = (ht16c21_dev_t *) calloc(1, sizeof(ht16c21_dev_t));
    HT_CHECK(NULL != seg, "Memory for ht16c21 is not enough", NULL);
    i2c_bus_device_handle_t i2c_dev = i2c_bus_device_create(bus, dev_addr, 0);
    if (NULL == i2c_dev) {
        free(seg);
        HT_CHECK(false, "Create i2c device failed", NULL);
    }
    seg->i2c_dev = i2c_dev;
    seg->dev_addr = dev_addr;
    return (ht16c21_handle_t) seg;
}

esp_err_t ht16c21_delete(ht16c21_handle_t dev)
{
    HT_CHECK(NULL != dev, "Handle is invalid", ESP_ERR_INVALID_ARG);
    ht16c21_dev_t *seg = (ht16c21_dev_t *) dev;
    i2c_bus_device_delete(&seg->i2c_dev);
    free(seg);
    return ESP_OK;
}

esp_err_t ht16c21_write_cmd(ht16c21_handle_t dev, ht16c21_cmd_t hd16c21_cmd, uint8_t val)
{
    HT_CHECK(NULL != dev, "Handle is invalid", ESP_ERR_INVALID_ARG);
    ht16c21_dev_t *seg = (ht16c21_dev_t *) dev;
    esp_err_t ret = i2c_bus_write_byte(seg->i2c_dev, (uint8_t) hd16c21_cmd, val);
    return ret;
}

esp_err_t ht16c21_ram_write_byte(ht16c21_handle_t dev, uint8_t address, uint8_t buf)
{
    HT_CHECK(NULL != dev, "Handle is invalid", ESP_ERR_INVALID_ARG);
    return ht16c21_ram_write(dev, address, &buf, 1);
}

esp_err_t ht16c21_ram_write(ht16c21_handle_t dev, uint8_t address, uint8_t *buf, uint8_t len)
{
    HT_CHECK(NULL != dev, "Handle is invalid", ESP_ERR_INVALID_ARG);
    ht16c21_dev_t *seg = (ht16c21_dev_t *) dev;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (seg->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, HT16C21_CMD_IOOUT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, address, ACK_CHECK_EN);
    i2c_master_write(cmd, buf, len, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = i2c_bus_cmd_begin(seg->i2c_dev, cmd);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t ht16c21_ram_read_byte(ht16c21_handle_t dev, uint8_t address, uint8_t *data)
{
    HT_CHECK(NULL != dev, "Handle is invalid", ESP_ERR_INVALID_ARG);
    return ht16c21_ram_read(dev, address, data, 1);
}

esp_err_t ht16c21_ram_read(ht16c21_handle_t dev, uint8_t address, uint8_t *buf, uint8_t len)
{
    HT_CHECK(NULL != dev, "Handle is invalid", ESP_ERR_INVALID_ARG);
    ht16c21_dev_t *seg = (ht16c21_dev_t *) dev;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (seg->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, HT16C21_CMD_IOOUT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, address, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_read(cmd, buf, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    int ret = i2c_bus_cmd_begin(seg->i2c_dev, cmd);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t ht16c21_param_config(ht16c21_handle_t dev, ht16c21_config_t *ht16c21_conf)
{
    esp_err_t ret = ESP_OK;
    HT_CHECK(NULL != dev, "Handle is invalid", ESP_ERR_INVALID_ARG);
    ret |= ht16c21_write_cmd(dev, HT16C21_CMD_DRIMO, ht16c21_conf->duty_bias);
    ret |= ht16c21_write_cmd(dev, HT16C21_CMD_SYSMO, ht16c21_conf->oscillator_display);
    ret |= ht16c21_write_cmd(dev, HT16C21_CMD_FRAME, ht16c21_conf->frame_frequency);
    ret |= ht16c21_write_cmd(dev, HT16C21_CMD_BLINK, ht16c21_conf->blinking_frequency);
    ret |= ht16c21_write_cmd(dev, HT16C21_CMD_IVA, ht16c21_conf->pin_and_voltage | ht16c21_conf->adjustment_voltage);
    HT_CHECK(ESP_OK == ret, "initialize failed", ESP_FAIL);
    return ESP_OK;
}
