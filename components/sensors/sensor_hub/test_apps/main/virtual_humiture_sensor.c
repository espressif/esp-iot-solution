/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include "iot_sensor_hub.h"
#include "esp_err.h"
#include "esp_random.h"

#ifdef CONFIG_SENSOR_INCLUDED_HUMITURE

/* per-instance context */
typedef struct {
    uint8_t addr;
} virtual_humiture_ctx_t;

esp_err_t virtual_humiture_init(void **sensor_ctx, i2c_bus_handle_t i2c_bus, uint8_t addr)
{
    virtual_humiture_ctx_t *ctx = calloc(1, sizeof(virtual_humiture_ctx_t));
    if (!ctx) {
        return ESP_ERR_NO_MEM;
    }
    ctx->addr = addr;
    *sensor_ctx = ctx;
    return ESP_OK;
}

esp_err_t virtual_humiture_deinit(void *sensor_ctx)
{
    free(sensor_ctx);
    return ESP_OK;
}

esp_err_t virtual_humiture_test(void *sensor_ctx)
{
    return ESP_OK;
}

esp_err_t virtual_humiture_acquire_humidity(void *sensor_ctx, float *h)
{
    *h = ((float)esp_random() / UINT32_MAX) * 100.0f;
    return ESP_OK;
}

esp_err_t virtual_humiture_acquire_temperature(void *sensor_ctx, float *t)
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
