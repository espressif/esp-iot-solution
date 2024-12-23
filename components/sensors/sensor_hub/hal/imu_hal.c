/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hal/imu_hal.h"
#include "iot_sensor_hub.h"

#ifdef  CONFIG_SENSOR_INCLUDED_IMU

static const char *TAG = "IMU";

#define SENSOR_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

typedef struct {
    bus_handle_t bus;
    bool is_init;
    const imu_impl_t *impl;
} sensor_imu_t;

/****************************public functions*************************************/

sensor_imu_handle_t imu_create(bus_handle_t bus, const char *sensor_name, uint8_t addr)
{
    SENSOR_CHECK(bus != NULL, "i2c bus has not initialized", NULL);
    if (sensor_name == NULL || addr == 0) {
        ESP_LOGE(TAG, "Incorrect Sensor Information");
        return NULL;
    }

    for (sensor_hub_detect_fn_t *p = &__sensor_hub_detect_fn_array_start; p < &__sensor_hub_detect_fn_array_end; ++p) {
        sensor_info_t info;
        sensor_device_impl_t sensor_device_impl = (*(p->fn))(&info);

        if (sensor_device_impl != NULL && strcmp(sensor_name, info.name) == 0) {
            sensor_imu_t *p_sensor = (sensor_imu_t *)malloc(sizeof(sensor_imu_t));
            SENSOR_CHECK(p_sensor != NULL, "imu sensor creat failed", NULL);
            p_sensor->bus = bus;
            p_sensor->impl = (imu_impl_t *)(sensor_device_impl);

            esp_err_t ret = p_sensor->impl->init(bus, addr);
            if (ret != ESP_OK) {
                free(p_sensor);
                ESP_LOGE(TAG, "imu sensor init failed");
                return NULL;
            }
            p_sensor->is_init = true;
            return (sensor_imu_handle_t)p_sensor;
        }
    }

    return NULL;
}

esp_err_t imu_delete(sensor_imu_handle_t *sensor)
{
    SENSOR_CHECK(sensor != NULL && *sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_imu_t *p_sensor = (sensor_imu_t *)(*sensor);

    if (!p_sensor->is_init) {
        free(p_sensor);
        return ESP_OK;
    }

    p_sensor->is_init = false;
    esp_err_t ret = p_sensor->impl->deinit();
    SENSOR_CHECK(ret == ESP_OK, "imu sensor de-init failed", ESP_FAIL);
    free(p_sensor);
    *sensor = NULL;
    return ESP_OK;
}

esp_err_t imu_test(sensor_imu_handle_t sensor)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_imu_t *p_sensor = (sensor_imu_t *)(sensor);

    if (!p_sensor->is_init) {
        return ESP_FAIL;
    }

    if (p_sensor->impl->test == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_err_t ret = p_sensor->impl->test();
    return ret;
}

esp_err_t imu_acquire_acce(sensor_imu_handle_t sensor, axis3_t* acce)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_imu_t *p_sensor = (sensor_imu_t*)(sensor);
    esp_err_t ret = p_sensor->impl->acquire_acce(&acce->x, &acce->y, &acce->z);
    return ret;
}

esp_err_t imu_acquire_gyro(sensor_imu_handle_t sensor, axis3_t* gyro)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_imu_t *p_sensor = (sensor_imu_t*)(sensor);
    esp_err_t ret = p_sensor->impl->acquire_gyro(&gyro->x, &gyro->y, &gyro->z);
    return ret;
}

esp_err_t imu_sleep(sensor_imu_handle_t sensor)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_imu_t *p_sensor = (sensor_imu_t *)(sensor);
    esp_err_t ret = p_sensor->impl->sleep();
    return ret;
}

esp_err_t imu_wakeup(sensor_imu_handle_t sensor)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_imu_t *p_sensor = (sensor_imu_t *)(sensor);
    esp_err_t ret = p_sensor->impl->wakeup();
    return ret;
}

static esp_err_t imu_set_power(sensor_imu_handle_t sensor, sensor_power_mode_t power_mode)
{
    SENSOR_CHECK(sensor != NULL, "pointer can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_imu_t *p_sensor = (sensor_imu_t *)(sensor);
    esp_err_t ret;
    switch (power_mode) {
    case POWER_MODE_WAKEUP:
        if (p_sensor->impl->wakeup == NULL) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        ret = p_sensor->impl->wakeup();
        break;
    case POWER_MODE_SLEEP:
        if (p_sensor->impl->sleep == NULL) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        ret = p_sensor->impl->sleep();
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    return ret;
}

esp_err_t imu_acquire(sensor_imu_handle_t sensor, sensor_data_group_t *data_group)
{
    SENSOR_CHECK(sensor != NULL && data_group != NULL, "pointer can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_imu_t *p_sensor = (sensor_imu_t *)(sensor);
    esp_err_t ret;
    int i = 0;
    ret = p_sensor->impl->acquire_gyro(&data_group->sensor_data[i].gyro.x, &data_group->sensor_data[i].gyro.y, &data_group->sensor_data[i].gyro.z);
    if (ESP_OK == ret) {
        data_group->sensor_data[i].event_id = SENSOR_GYRO_DATA_READY;
        i++;
    }
    ret = p_sensor->impl->acquire_acce(&data_group->sensor_data[i].acce.x, &data_group->sensor_data[i].acce.y, &data_group->sensor_data[i].acce.z);
    if (ESP_OK == ret) {
        data_group->sensor_data[i].event_id = SENSOR_ACCE_DATA_READY;
        i++;
    }
    data_group->number = i;
    return ESP_OK;
}

esp_err_t imu_set_work_mode(sensor_imu_handle_t sensor, sensor_mode_t work_mode)
{
    SENSOR_CHECK(sensor != NULL, "pointer can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_imu_t *p_sensor = (sensor_imu_t *)(sensor);

    if (p_sensor->impl->set_mode == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return p_sensor->impl->set_mode(work_mode);
}

esp_err_t imu_set_range(sensor_imu_handle_t sensor, sensor_range_t range)
{
    SENSOR_CHECK(sensor != NULL, "pointer can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_imu_t *p_sensor = (sensor_imu_t *)(sensor);

    if (p_sensor->impl->set_range == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return p_sensor->impl->set_range(range);
}

esp_err_t imu_control(sensor_imu_handle_t sensor, sensor_command_t cmd, void *args)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    esp_err_t ret;
    switch (cmd) {
    case COMMAND_SET_MODE:
        ret = imu_set_work_mode(sensor, (sensor_mode_t)args);
        break;
    case COMMAND_SET_RANGE:
        ret = imu_set_range(sensor, (sensor_range_t)args);
        break;
    case COMMAND_SET_ODR:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    case COMMAND_SET_POWER:
        ret = imu_set_power(sensor, (sensor_power_mode_t)args);
        break;
    case COMMAND_SELF_TEST:
        ret = imu_test(sensor);
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    return ret;
}
#endif
