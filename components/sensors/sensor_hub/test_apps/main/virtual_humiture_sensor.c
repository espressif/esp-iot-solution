/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "iot_sensor_hub.h"
#include "esp_err.h"
#include "esp_random.h"

#ifdef CONFIG_SENSOR_INCLUDED_HUMITURE

esp_err_t virtual_humiture_init(i2c_bus_handle_t i2c_bus, uint8_t addr)
{
    return ESP_OK;
}

esp_err_t virtual_humiture_deinit(void)
{
    return ESP_OK;
}

esp_err_t virtual_humiture_test(void)
{
    return ESP_OK;
}

esp_err_t virtual_humiture_acquire_humidity(float *h)
{
    *h = ((float)esp_random() / UINT32_MAX) * 100.0f;
    return ESP_OK;
}

esp_err_t virtual_humiture_acquire_temperature(float *t)
{
    *t = ((float)esp_random() / UINT32_MAX) * 100.0f;
    return ESP_OK;
}

static humiture_impl_t virtual_sht3x_impl = {
    .init = virtual_humiture_init,
    .deinit = virtual_humiture_deinit,
    .test = virtual_humiture_test,
    .acquire_humidity = virtual_humiture_acquire_humidity,
    .acquire_temperature = virtual_humiture_acquire_temperature,
};

SENSOR_HUB_DETECT_FN(HUMITURE_ID, virtual_sht3x, &virtual_sht3x_impl);

#endif
