/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "esp_log.h"
#include "mvh3004d.h"
#include "iot_sensor_hub.h"

static const char *TAG = "mvh3004d";
typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
} mvh3004d_dev_t;

mvh3004d_handle_t mvh3004d_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    mvh3004d_dev_t *sensor = (mvh3004d_dev_t *) calloc(1, sizeof(mvh3004d_dev_t));
    sensor->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (sensor->i2c_dev == NULL) {
        free(sensor);
        return NULL;
    }
    sensor->dev_addr = dev_addr;
    return (mvh3004d_handle_t) sensor;
}

esp_err_t mvh3004d_delete(mvh3004d_handle_t *sensor)
{
    if (*sensor == NULL) {
        return ESP_OK;
    }
    mvh3004d_dev_t *sens = (mvh3004d_dev_t *)(*sensor);
    i2c_bus_device_delete(&sens->i2c_dev);
    free(sens);
    *sensor = NULL;
    return ESP_OK;
}

esp_err_t mvh3004d_get_data(mvh3004d_handle_t sensor, float *tp, float *rh)
{
    mvh3004d_dev_t *sens = (mvh3004d_dev_t *) sensor;
    uint8_t data[4] = {0};
    esp_err_t ret = i2c_bus_read_bytes(sens->i2c_dev, NULL_I2C_MEM_ADDR, 4, data);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SEND READ ERROR \n");
        return ESP_FAIL;
    }

    if (rh) {
        *rh = (float)(((data[0] << 8) | data[1]) & 0x3fff) * 100 / (16384 - 1);
    }

    if (tp) {
        *tp = ((float)((data[2] << 6) | data[3] >> 2)) * 165 / 16383 - 40;
    }

    return ESP_OK;
}

esp_err_t mvh3004d_get_huminity(mvh3004d_handle_t sensor, float *rh)
{
    return mvh3004d_get_data(sensor, NULL, rh);
}

esp_err_t mvh3004d_get_temperature(mvh3004d_handle_t sensor, float *tp)
{
    return mvh3004d_get_data(sensor, tp, NULL);
}

#ifdef CONFIG_SENSOR_INCLUDED_HUMITURE

static mvh3004d_handle_t mvh3004d = NULL;
static bool is_init = false;

esp_err_t humiture_mvh3004d_init(i2c_bus_handle_t i2c_bus, uint8_t addr)
{
    if (is_init || !i2c_bus) {
        return ESP_FAIL;
    }

    mvh3004d = mvh3004d_create(i2c_bus, addr);

    if (!mvh3004d) {
        return ESP_FAIL;
    }

    is_init = true;
    return ESP_OK;
}

esp_err_t humiture_mvh3004d_deinit(void)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    esp_err_t ret = mvh3004d_delete(&mvh3004d);

    if (ret != ESP_OK) {
        return ESP_FAIL;
    }

    is_init = false;
    return ESP_OK;
}

esp_err_t humiture_mvh3004d_test(void)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t humiture_mvh3004d_acquire_humidity(float *h)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    float humidity = 0;
    esp_err_t ret = mvh3004d_get_huminity(mvh3004d, &humidity);

    if (ret == ESP_OK) {
        *h = humidity;
        return ESP_OK;
    }

    *h = 0;
    return ESP_FAIL;
}

esp_err_t humiture_mvh3004d_acquire_temperature(float *t)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    float temperature = 0;
    esp_err_t ret = mvh3004d_get_temperature(mvh3004d, &temperature);

    if (ret == ESP_OK) {
        *t = temperature;
        return ESP_OK;
    }

    *t = 0;
    return ESP_FAIL;
}

static humiture_impl_t mvh3004d_impl = {
    .init = humiture_mvh3004d_init,
    .deinit = humiture_mvh3004d_deinit,
    .test = humiture_mvh3004d_test,
    .acquire_humidity = humiture_mvh3004d_acquire_humidity,
    .acquire_temperature = humiture_mvh3004d_acquire_temperature,
};

SENSOR_HUB_DETECT_FN(HUMITURE_ID, mvh3004d, &mvh3004d_impl);

#endif
