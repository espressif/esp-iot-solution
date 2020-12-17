// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hal/humiture_hal.h"

#ifdef CONFIG_SENSOR_HUMITURE_INCLUDED_SHT3X
#include "sht3x.h"
#endif
#ifdef CONFIG_SENSOR_HUMITURE_INCLUDED_HTS221
#include "hts221.h"
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static esp_err_t null_function(void){return ESP_ERR_NOT_SUPPORTED;}
static esp_err_t null_acquire_humidity_function(float *h){return ESP_ERR_NOT_SUPPORTED;}
static esp_err_t null_acquire_temperature_function(float *t){return ESP_ERR_NOT_SUPPORTED;}
#pragma GCC diagnostic pop

static const char *TAG = "HUMITURE|TEMPERATURE";

#define SENSOR_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

typedef struct {
    humiture_id_t id;
    esp_err_t (*init)(bus_handle_t);
    esp_err_t (*deinit)(void);
    esp_err_t (*test)(void);
    esp_err_t (*acquire_humidity)(float *);
    esp_err_t (*acquire_temperature)(float *);
    esp_err_t (*sleep)(void);
    esp_err_t (*wakeup)(void);
} humiture_impl_t;

typedef struct {
    humiture_id_t id;
    bus_handle_t bus;
    bool is_init;
    const humiture_impl_t *impl;
} sensor_humiture_t;

static const humiture_impl_t humiture_implementations[] = {
#ifdef CONFIG_SENSOR_HUMITURE_INCLUDED_SHT3X
    {
        .id = SHT3X_ID,
        .init = humiture_sht3x_init,
        .deinit = humiture_sht3x_deinit,
        .test = humiture_sht3x_test,
        .acquire_humidity = humiture_sht3x_acquire_humidity,
        .acquire_temperature = humiture_sht3x_acquire_temperature,
        .sleep = null_function,
        .wakeup = null_function,
    },
#endif
#ifdef CONFIG_SENSOR_HUMITURE_INCLUDED_HTS221
    {
        .id = HTS221_ID,
        .init = humiture_hts221_init,
        .deinit = humiture_hts221_deinit,
        .test = humiture_hts221_test,
        .acquire_humidity = humiture_hts221_acquire_humidity,
        .acquire_temperature = humiture_hts221_acquire_temperature,
        .sleep = humiture_hts221_sleep,
        .wakeup = humiture_hts221_wakeup,
    },
#endif
};

/****************************private functions*************************************/

static const humiture_impl_t *find_implementation(int id)
{
    const humiture_impl_t *active_driver = NULL;
    int count = sizeof(humiture_implementations) / sizeof(humiture_impl_t);

    for (int i = 0; i < count; i++) {
        if (humiture_implementations[i].id == id) {
            active_driver = &humiture_implementations[i];
            break;
        }
    }

    return active_driver;
}

/****************************public functions*************************************/

sensor_humiture_handle_t humiture_create(bus_handle_t bus, int id)
{
    SENSOR_CHECK(bus != NULL, "i2c bus has not initialized", NULL);
    const humiture_impl_t *sensor_impl = find_implementation(id);

    if (sensor_impl == NULL) {
        ESP_LOGE(TAG, "no driver founded, HUMITURE ID = %d", id);
        return NULL;
    }

    sensor_humiture_t *p_sensor = (sensor_humiture_t *)malloc(sizeof(sensor_humiture_t));
    SENSOR_CHECK(p_sensor != NULL, "humiture sensor creat failed", NULL);
    p_sensor->id = id;
    p_sensor->bus = bus;
    p_sensor->impl = sensor_impl;
    esp_err_t ret = p_sensor->impl->init(bus);

    if (ret != ESP_OK) {
        free(p_sensor);
        ESP_LOGE(TAG, "humiture sensor init failed");
        return NULL;
    }

    p_sensor->is_init = true;
    return (sensor_humiture_handle_t)p_sensor;
}

esp_err_t humiture_delete(sensor_humiture_handle_t *sensor)
{
    SENSOR_CHECK(sensor != NULL && *sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_humiture_t *p_sensor = (sensor_humiture_t *)(*sensor);

    if (!p_sensor->is_init) {
        free(p_sensor);
        return ESP_OK;
    }

    p_sensor->is_init = false;
    esp_err_t ret = p_sensor->impl->deinit();
    SENSOR_CHECK(ret == ESP_OK, "humiture sensor de-init failed", ESP_FAIL);
    free(p_sensor);
    *sensor = NULL;
    return ESP_OK;
}

esp_err_t humiture_test(sensor_humiture_handle_t sensor)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_humiture_t *p_sensor = (sensor_humiture_t *)(sensor);

    if (!p_sensor->is_init) {
        return ESP_FAIL;
    }

    esp_err_t ret = p_sensor->impl->test();
    return ret;
}

esp_err_t humiture_acquire_humidity(sensor_humiture_handle_t sensor, float *humidity)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_humiture_t *p_sensor = (sensor_humiture_t *)(sensor);
    esp_err_t ret = p_sensor->impl->acquire_humidity(humidity);
    return ret;
}

esp_err_t humiture_acquire_temperature(sensor_humiture_handle_t sensor, float *temperature)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_humiture_t *p_sensor = (sensor_humiture_t *)(sensor);
    esp_err_t ret = p_sensor->impl->acquire_temperature(temperature);
    return ret;
}

esp_err_t humiture_sleep(sensor_humiture_handle_t sensor)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_humiture_t *p_sensor = (sensor_humiture_t *)(sensor);
    esp_err_t ret = p_sensor->impl->sleep();
    return ret;
}

esp_err_t humiture_wakeup(sensor_humiture_handle_t sensor)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_humiture_t *p_sensor = (sensor_humiture_t *)(sensor);
    esp_err_t ret = p_sensor->impl->wakeup();
    return ret;
}

static esp_err_t humiture_set_power(sensor_humiture_handle_t sensor, sensor_power_mode_t power_mode)
{
    SENSOR_CHECK(sensor != NULL, "pointer can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_humiture_t *p_sensor = (sensor_humiture_t *)(sensor);
    esp_err_t ret;
    switch (power_mode)
    {
    case POWER_MODE_WAKEUP:
        ret = p_sensor->impl->wakeup();
        break;
    case POWER_MODE_SLEEP:
        ret = p_sensor->impl->sleep();
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    return ret;
}

esp_err_t humiture_acquire(sensor_humiture_handle_t sensor, sensor_data_group_t *data_group)
{
    SENSOR_CHECK(sensor != NULL && data_group != NULL, "pointer can't be NULL ", ESP_ERR_INVALID_ARG);
    sensor_humiture_t *p_sensor = (sensor_humiture_t *)(sensor);
    esp_err_t ret;
    int i = 0;
    ret = p_sensor->impl->acquire_temperature(&data_group->sensor_data[i].temperature);
    if (ESP_OK == ret) {
        data_group->sensor_data[i].event_id = SENSOR_TEMP_DATA_READY;
        i++;
    }
    ret = p_sensor->impl->acquire_humidity(&data_group->sensor_data[i].humidity);
    if (ESP_OK == ret) {
        data_group->sensor_data[i].event_id = SENSOR_HUMI_DATA_READY;
        i++;
    }
    data_group->number = i;
    return ESP_OK;
}

esp_err_t humiture_control(sensor_humiture_handle_t sensor, sensor_command_t cmd, void *args)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_ARG);
    esp_err_t ret;
    switch (cmd)
    {
    case COMMAND_SET_MODE:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    case COMMAND_SET_RANGE:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    case COMMAND_SET_ODR:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    case COMMAND_SET_POWER:
        ret = humiture_set_power(sensor, (sensor_power_mode_t)args);
        break;
    case COMMAND_SELF_TEST:
        ret = humiture_test(sensor);
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    return ret;
}
