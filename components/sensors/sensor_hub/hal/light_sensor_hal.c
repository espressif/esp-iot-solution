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
#include "hal/light_sensor_hal.h"
#include "iot_sensor_hub.h"

#ifdef CONFIG_SENSOR_INCLUDED_LIGHT

static const char* TAG = "LIGHT|RGBW|UV";

#define SENSOR_CHECK(a, str, ret) if(!(a)) { \
    ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
    return (ret); \
    }

typedef struct {
    bus_handle_t bus;
    bool is_init;
    const light_impl_t* impl;
} sensor_light_t;

/****************************public functions*************************************/

sensor_light_handle_t light_sensor_create(bus_handle_t bus, const char *sensor_name, uint8_t addr)
{
    SENSOR_CHECK(bus != NULL, "i2c bus has not initialized", NULL);
    if (sensor_name == NULL || addr == 0) {
        ESP_LOGE(TAG, "Incorrect Sensor Information");
        return NULL;
    }

    // search the sensor driver from a specific segment
    for (sensor_hub_detect_fn_t *p = &__sensor_hub_detect_fn_array_start; p < &__sensor_hub_detect_fn_array_end; ++p) {
        sensor_info_t info;
        sensor_device_impl_t sensor_device_impl = (*(p->fn))(&info);

        if (sensor_device_impl != NULL && strcmp(sensor_name, info.name) == 0) {
            sensor_light_t* p_sensor = (sensor_light_t*)malloc(sizeof(sensor_light_t));
            SENSOR_CHECK(p_sensor != NULL, "light sensor creat failed", NULL);
            p_sensor->bus = bus;
            p_sensor->impl = (light_impl_t *)(sensor_device_impl);

            esp_err_t ret = p_sensor->impl->init(bus, addr);
            if (ret != ESP_OK) {
                free(p_sensor);
                ESP_LOGE(TAG, "light sensor init failed");
                return NULL;
            }
            p_sensor->is_init = true;
            return (sensor_light_handle_t)p_sensor;
        }
    }

    return NULL;
}

esp_err_t light_sensor_delete(sensor_light_handle_t *sensor)
{
    SENSOR_CHECK(sensor != NULL && *sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_light_t *p_sensor = (sensor_light_t*)(*sensor);

    if (!p_sensor->is_init) {
        free(p_sensor);
        return ESP_OK;
    }

    p_sensor->is_init = false;
    esp_err_t ret = p_sensor->impl->deinit();
    SENSOR_CHECK(ret == ESP_OK, "light sensor de-init failed", ESP_FAIL);
    free(p_sensor);
    *sensor = NULL;
    return ESP_OK;
}

esp_err_t light_sensor_test(sensor_light_handle_t sensor)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_light_t *p_sensor = (sensor_light_t*)(sensor);
    if (!p_sensor->is_init) {
        return ESP_FAIL;
    }

    if (p_sensor->impl->test == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_err_t ret = p_sensor->impl->test();
    return ret;
}

esp_err_t light_sensor_acquire_light(sensor_light_handle_t sensor, float *lux)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_light_t *p_sensor = (sensor_light_t*)(sensor);
    esp_err_t ret = p_sensor->impl->acquire_light(lux);
    return ret;
}

esp_err_t light_sensor_acquire_rgbw(sensor_light_handle_t sensor, rgbw_t *rgbw)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_light_t *p_sensor = (sensor_light_t*)(sensor);
    esp_err_t ret = p_sensor->impl->acquire_rgbw(&rgbw->r, &rgbw->g, &rgbw->b, &rgbw->w);
    return ret;
}

esp_err_t light_sensor_acquire_uv(sensor_light_handle_t sensor, uv_t *uv)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_light_t *p_sensor = (sensor_light_t*)(sensor);
    esp_err_t ret = p_sensor->impl->acquire_uv(&uv->uv, &uv->uva, &uv->uvb);
    return ret;
}

esp_err_t light_sensor_sleep(sensor_light_handle_t sensor)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_light_t *p_sensor = (sensor_light_t*)(sensor);
    esp_err_t ret = p_sensor->impl->sleep();
    return ret;
}

esp_err_t light_sensor_wakeup(sensor_light_handle_t sensor)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_light_t *p_sensor = (sensor_light_t*)(sensor);
    esp_err_t ret = p_sensor->impl->wakeup();
    return ret;
}

static esp_err_t light_sensor_set_power(sensor_light_handle_t sensor, sensor_power_mode_t power_mode)
{
    SENSOR_CHECK(sensor != NULL, "pointer can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_light_t *p_sensor = (sensor_light_t *)(sensor);
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

esp_err_t light_sensor_acquire(sensor_light_handle_t sensor, sensor_data_group_t *data_group)
{
    SENSOR_CHECK(sensor != NULL && data_group != NULL, "pointer can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_light_t *p_sensor = (sensor_light_t *)(sensor);
    esp_err_t ret;
    int i = 0;
    ret = p_sensor->impl->acquire_light(&data_group->sensor_data[i].light);
    if (ESP_OK == ret) {
        data_group->sensor_data[i].event_id = SENSOR_LIGHT_DATA_READY;
        i++;
    }
    ret = p_sensor->impl->acquire_rgbw(&data_group->sensor_data[i].rgbw.r, &data_group->sensor_data[i].rgbw.g,
                                       &data_group->sensor_data[i].rgbw.b, &data_group->sensor_data[i].rgbw.w);
    if (ESP_OK == ret) {
        data_group->sensor_data[i].event_id = SENSOR_RGBW_DATA_READY;
        i++;
    }
    ret = p_sensor->impl->acquire_uv(&data_group->sensor_data[i].uv.uv, &data_group->sensor_data[i].uv.uva,
                                     &data_group->sensor_data[i].uv.uvb);
    if (ESP_OK == ret) {
        data_group->sensor_data[i].event_id = SENSOR_UV_DATA_READY;
        i++;
    }
    data_group->number = i;
    return ESP_OK;
}

esp_err_t light_sensor_set_work_mode(sensor_light_handle_t sensor, sensor_mode_t work_mode)
{
    SENSOR_CHECK(sensor != NULL, "pointer can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_light_t *p_sensor = (sensor_light_t *)(sensor);

    if (p_sensor->impl->set_mode == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return p_sensor->impl->set_mode(work_mode);
}

esp_err_t light_sensor_set_range(sensor_light_handle_t sensor, sensor_range_t range)
{
    SENSOR_CHECK(sensor != NULL, "pointer can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_light_t *p_sensor = (sensor_light_t *)(sensor);

    if (p_sensor->impl->set_range == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return p_sensor->impl->set_range(range);
}

esp_err_t light_sensor_control(sensor_light_handle_t sensor, sensor_command_t cmd, void *args)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    esp_err_t ret;
    switch (cmd) {
    case COMMAND_SET_MODE:
        ret = light_sensor_set_work_mode(sensor, (sensor_mode_t)args);
        break;
    case COMMAND_SET_RANGE:
        ret = light_sensor_set_range(sensor, (sensor_range_t)args);
        break;
    case COMMAND_SET_ODR:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    case COMMAND_SET_POWER:
        ret = light_sensor_set_power(sensor, (sensor_power_mode_t)args);
        break;
    case COMMAND_SELF_TEST:
        ret = light_sensor_test(sensor);
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    return ret;
}
#endif
