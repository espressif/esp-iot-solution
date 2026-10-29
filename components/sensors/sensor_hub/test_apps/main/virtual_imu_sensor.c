/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include "iot_sensor_hub.h"
#include "esp_err.h"
#include "esp_random.h"

#ifdef CONFIG_SENSOR_INCLUDED_IMU

/* per-instance context */
typedef struct {
    uint8_t addr;
} virtual_imu_ctx_t;

esp_err_t virtual_imu_init(void **sensor_ctx, i2c_bus_handle_t i2c_bus, uint8_t addr)
{
    virtual_imu_ctx_t *ctx = calloc(1, sizeof(virtual_imu_ctx_t));
    if (!ctx) {
        return ESP_ERR_NO_MEM;
    }
    ctx->addr = addr;
    *sensor_ctx = ctx;
    return ESP_OK;
}

esp_err_t virtual_imu_deinit(void *sensor_ctx)
{
    free(sensor_ctx);
    return ESP_OK;
}

esp_err_t virtual_imu_test(void *sensor_ctx)
{
    return ESP_OK;
}

esp_err_t virtual_imu_acquire_acce(void *sensor_ctx, float *acce_x, float *acce_y, float *acce_z)
{
    *acce_x = ((float)esp_random() / UINT32_MAX) * 100.0f - 50.0f;
    *acce_y = ((float)esp_random() / UINT32_MAX) * 100.0f - 50.0f;
    *acce_z = ((float)esp_random() / UINT32_MAX) * 100.0f - 50.0f;

    return ESP_OK;
}

esp_err_t virtual_imu_acquire_gyro(void *sensor_ctx, float *gyro_x, float *gyro_y, float *gyro_z)
{
    *gyro_x = ((float)esp_random() / UINT32_MAX) * 5000.0f - 2500.0f;
    *gyro_y = ((float)esp_random() / UINT32_MAX) * 5000.0f - 2500.0f;
    *gyro_z = ((float)esp_random() / UINT32_MAX) * 5000.0f - 2500.0f;
    return ESP_OK;
}

static imu_impl_t virtual_mpu6050_impl = {
    .init = virtual_imu_init,
    .deinit = virtual_imu_deinit,
    .test = virtual_imu_test,
    .acquire_acce = virtual_imu_acquire_acce,
    .acquire_gyro = virtual_imu_acquire_gyro,
};

SENSOR_HUB_DETECT_FN(IMU_ID, virtual_mpu6050, &virtual_mpu6050_impl);

#endif
