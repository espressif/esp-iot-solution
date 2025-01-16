/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "mcp3201.h"

typedef struct {
    spi_bus_device_handle_t spi_dev;
} mcp3201_sensor_t;

mcp3201_handle_t mcp3201_create(spi_bus_handle_t spi_bus, spi_device_config_t *dev_cfg)
{
    mcp3201_sensor_t *sens = (mcp3201_sensor_t *) calloc(1, sizeof(mcp3201_sensor_t));
    sens->spi_dev = spi_bus_device_create(spi_bus, dev_cfg);
    if (sens->spi_dev == NULL) {
        free(sens);
        return NULL;
    }
    return (mcp3201_handle_t) sens;
}

esp_err_t mcp3201_delete(mcp3201_handle_t *sensor)
{
    if (*sensor == NULL) {
        return ESP_OK;
    }

    mcp3201_sensor_t *sens = (mcp3201_sensor_t *)(*sensor);
    spi_bus_device_delete(&sens->spi_dev);
    free(sens);
    *sensor = NULL;
    return ESP_OK;
}

esp_err_t mcp3201_get_data(mcp3201_handle_t sensor, int16_t *data)
{
    mcp3201_sensor_t *sens = (mcp3201_sensor_t *) sensor;
    uint8_t rbuf[3] = {0};
    esp_err_t ret = spi_bus_transfer_bytes(sens->spi_dev, rbuf, NULL, 3);

    if (ret != ESP_OK) {
        *data = -1;
        return ret;
    }
    *data = ((rbuf[0] & 0x1F) << 7) + (rbuf[1] >> 1);
    return ESP_OK;
}
