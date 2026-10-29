/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "aht20.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"

#include "aht20_reg.h"

const static char *TAG = "AHT20";

static esp_err_t aht20_write_reg(aht20_dev_handle_t handle, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    i2c_bus_device_handle_t bus_inst = (i2c_bus_device_handle_t)handle;
    return i2c_bus_write_bytes(bus_inst, reg_addr, len, data);
}

static esp_err_t aht20_read_reg(aht20_dev_handle_t handle, uint8_t *data, size_t len)
{
    i2c_bus_device_handle_t bus_inst = (i2c_bus_device_handle_t)handle;
    return i2c_bus_read_bytes(bus_inst, NULL_I2C_MEM_ADDR, len, data);
}

static uint8_t aht20_calc_crc(uint8_t *data, uint8_t len)
{
    uint8_t i;
    uint8_t byte;
    uint8_t crc = 0xFF;

    for (byte = 0; byte < len; byte++) {
        crc ^= data[byte];
        for (i = 8; i > 0; --i) {
            if ((crc & 0x80) != 0) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc;
}

esp_err_t aht20_read_temperature_humidity(aht20_dev_handle_t handle,
                                          uint32_t *temperature_raw, float *temperature,
                                          uint32_t *humidity_raw, float *humidity)
{
    uint8_t status;
    uint8_t buf[7];
    uint32_t raw_data;

    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid device handle pointer");

    buf[0] = 0x33;
    buf[1] = 0x00;
    ESP_RETURN_ON_ERROR(aht20_write_reg(handle, AHT20_START_MEASURMENT_CMD, buf, 2), TAG, "I2C write error");

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_RETURN_ON_ERROR(aht20_read_reg(handle, &status, 1), TAG, "I2C read error");

    if ((status & BIT(AT581X_STATUS_Calibration_Enable)) &&
            (status & BIT(AT581X_STATUS_CRC_FLAG)) &&
            ((status & BIT(AT581X_STATUS_BUSY_INDICATION)) == 0)) {
        ESP_RETURN_ON_ERROR(aht20_read_reg(handle, buf, 7), TAG, "I2C read error");
        ESP_RETURN_ON_ERROR((aht20_calc_crc(buf, 6) != buf[6]), TAG, "crc is error");

        raw_data = buf[1];
        raw_data = raw_data << 8;
        raw_data += buf[2];
        raw_data = raw_data << 8;
        raw_data += buf[3];
        raw_data = raw_data >> 4;
        *humidity_raw = raw_data;
        *humidity = (float)raw_data * 100 / 1048576;

        raw_data = buf[3] & 0x0F;
        raw_data = raw_data << 8;
        raw_data += buf[4];
        raw_data = raw_data << 8;
        raw_data += buf[5];
        *temperature_raw = raw_data;
        *temperature = (float)raw_data * 200 / 1048576 - 50;
        return ESP_OK;
    } else {
        ESP_LOGI(TAG, "data is not ready");
        return ESP_ERR_NOT_FINISHED;
    }
}

esp_err_t aht20_new_sensor(const aht20_i2c_config_t *i2c_conf, aht20_dev_handle_t *handle_out)
{
    ESP_LOGI(TAG, "%-15s: %d.%d.%d", CHIP_NAME, AHT20_VER_MAJOR, AHT20_VER_MINOR, AHT20_VER_PATCH);
    ESP_LOGI(TAG, "%-15s: %1.1f - %1.1fV", "SUPPLY_VOLTAGE", SUPPLY_VOLTAGE_MIN, SUPPLY_VOLTAGE_MAX);
    ESP_LOGI(TAG, "%-15s: %.2f - %.2f℃", "TEMPERATURE", TEMPERATURE_MIN, TEMPERATURE_MAX);

    ESP_RETURN_ON_FALSE(i2c_conf, ESP_ERR_INVALID_ARG, TAG, "invalid device config pointer");
    ESP_RETURN_ON_FALSE(handle_out, ESP_ERR_INVALID_ARG, TAG, "invalid device handle pointer");

    i2c_bus_device_handle_t i2c_device_handle = i2c_bus_device_create(i2c_conf->bus_inst, i2c_conf->i2c_addr, 0);
    if (NULL == i2c_device_handle) {
        ESP_LOGE(TAG, "i2c_bus_device_create failed");
        return ESP_FAIL;
    }

    *handle_out = i2c_device_handle;
    return ESP_OK;
}

esp_err_t aht20_del_sensor(aht20_dev_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid device handle pointer");
    i2c_bus_device_handle_t bus_inst = (i2c_bus_device_handle_t)handle;
    esp_err_t ret = i2c_bus_device_delete(&bus_inst);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_bus_device_delete failed");
        return ret;
    }

    return ESP_OK;
}
