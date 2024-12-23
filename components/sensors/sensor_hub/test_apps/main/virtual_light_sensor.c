/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "iot_sensor_hub.h"
#include "esp_err.h"
#include "esp_random.h"

#ifdef CONFIG_SENSOR_INCLUDED_LIGHT

esp_err_t virtual_light_init(i2c_bus_handle_t i2c_bus, uint8_t addr)
{
    return ESP_OK;
}

esp_err_t virtual_light_deinit(void)
{
    return ESP_OK;
}

esp_err_t virtual_light_test(void)
{
    return ESP_OK;
}

esp_err_t virtual_light_acquire_light(float* l)
{
    *l = ((float)esp_random() / UINT32_MAX) * 1000.0f;
    return ESP_OK;
}

esp_err_t virtual_light_acquire_rgbw(float* r, float* g, float* b, float* w)
{
    *r = ((float)esp_random() / UINT32_MAX) * 255.0f;
    *g = ((float)esp_random() / UINT32_MAX) * 255.0f;
    *b = ((float)esp_random() / UINT32_MAX) * 255.0f;
    *w = ((float)esp_random() / UINT32_MAX) * 255.0f;
    return ESP_OK;
}

esp_err_t virtual_light_acquire_uv(float* uv, float* uva, float* uvb)
{
    *uv = ((float)esp_random() / UINT32_MAX) * 1000.0f;
    *uva = ((float)esp_random() / UINT32_MAX) * 1000.0f;
    *uvb = ((float)esp_random() / UINT32_MAX) * 1000.0f;
    return ESP_OK;
}

static light_impl_t virtual_bh1750_impl = {
    .init = virtual_light_init,
    .deinit = virtual_light_deinit,
    .test = virtual_light_test,
    .acquire_light = virtual_light_acquire_light,
    .acquire_rgbw = virtual_light_acquire_rgbw,
    .acquire_uv = virtual_light_acquire_uv,
};

SENSOR_HUB_DETECT_FN(LIGHT_SENSOR_ID, virtual_bh1750, &virtual_bh1750_impl);

#endif
