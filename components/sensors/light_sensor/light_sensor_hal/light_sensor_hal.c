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
#include "light_sensor_hal.h"

#ifdef CONFIG_SENSOR_LIGHT_INCLUDED_BH1750
#include "bh1750.h"
#endif
#ifdef CONFIG_SENSOR_LIGHT_INCLUDED_VEML6040
#include "veml6040.h"
#endif
#ifdef CONFIG_SENSOR_LIGHT_INCLUDED_VEML6075
#include "veml6075.h"
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static esp_err_t null_function(void) {return ESP_ERR_NOT_FOUND;}
static esp_err_t null_acquire_light_function(float* l) {return ESP_ERR_NOT_FOUND;}
static esp_err_t null_acquire_rgbw_function(float* r, float* g, float* b, float* w) {return ESP_ERR_NOT_FOUND;}
static esp_err_t null_acquire_uv_function(float* uv, float* uva, float* uvb) {return ESP_ERR_NOT_FOUND;}
#pragma GCC diagnostic pop

static const char* TAG = "LIGHT|RGBW|UV";

#define SENSOR_CHECK(a, str, ret) if(!(a)) { \
    ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
    return (ret); \
    }

typedef struct {
    light_sensor_id_t id;
    esp_err_t (*init)(bus_handle_t);
    esp_err_t (*deinit)(void);
    esp_err_t (*test)(void);
    esp_err_t (*acquire_light)(float* l);
    esp_err_t (*acquire_rgbw)(float* r, float* g, float* b, float* w);
    esp_err_t (*acquire_uv)(float* uv, float* uva, float* uvb);
    esp_err_t (*sleep)(void);
    esp_err_t (*wakeup)(void);
} light_sensor_impl_t;

typedef struct {
    light_sensor_id_t id;
    bus_handle_t bus;
    bool is_init;
    const light_sensor_impl_t* impl;
}sensor_light_t;

static const light_sensor_impl_t light_sensor_implementations[] = {
#ifdef CONFIG_SENSOR_LIGHT_INCLUDED_BH1750
  {
    .id = BH1750_ID,
    .init = light_sensor_bh1750_init,
    .deinit = light_sensor_bh1750_deinit,
    .test = light_sensor_bh1750_test,
    .acquire_light = light_sensor_bh1750_acquire_light,
    .acquire_rgbw = null_acquire_rgbw_function,
    .acquire_uv = null_acquire_uv_function,
    .sleep = null_function,
    .wakeup = null_function,
  },
#endif
#ifdef CONFIG_SENSOR_LIGHT_INCLUDED_VEML6040
  {
    .id = VEML6040_ID,
    .init = light_sensor_veml6040_init,
    .deinit = light_sensor_veml6040_deinit,
    .test = light_sensor_veml6040_test,
    .acquire_light = null_acquire_light_function,
    .acquire_rgbw = light_sensor_veml6040_acquire_rgbw,
    .acquire_uv = null_acquire_uv_function,
    .sleep = null_function,
    .wakeup = null_function,
  },
#endif
#ifdef CONFIG_SENSOR_LIGHT_INCLUDED_VEML6075
  {
    .id = VEML6075_ID,
    .init = light_sensor_veml6075_init,
    .deinit = light_sensor_veml6075_deinit,
    .test = light_sensor_veml6075_test,
    .acquire_light = null_acquire_light_function,
    .acquire_rgbw = null_acquire_rgbw_function,
    .acquire_uv = light_sensor_veml6075_acquire_uv,
    .sleep = null_function,
    .wakeup = null_function,
  },
#endif
};

/****************************private functions*************************************/

static const light_sensor_impl_t* find_implementation(int id)
{
    const light_sensor_impl_t* active_driver = NULL;
    int count = sizeof(light_sensor_implementations)/sizeof(light_sensor_impl_t);
    for (int i = 0; i < count; i++) {
        if (light_sensor_implementations[i].id == id) {
            active_driver = &light_sensor_implementations[i];
            break;
        }
    }
    return active_driver;
}

/****************************public functions*************************************/

sensor_light_handle_t light_sensor_create(bus_handle_t bus, int id)
{
    SENSOR_CHECK(bus != NULL, "i2c bus has not initialized", NULL);
    const light_sensor_impl_t *sensor_impl = find_implementation(id);

    if (sensor_impl == NULL) {
        ESP_LOGE(TAG, "no driver founded, LIGHT ID = %d", id);
        return NULL;
    }

    sensor_light_t* p_sensor = (sensor_light_t*)malloc(sizeof(sensor_light_t));
    SENSOR_CHECK(p_sensor != NULL, "light sensor creat failed", NULL);
    p_sensor->id = id;
    p_sensor->bus = bus;
    p_sensor->impl = sensor_impl;
    esp_err_t ret = p_sensor->impl->init(bus);
    if (ret != ESP_OK) {
        free(p_sensor);
        ESP_LOGE(TAG, "light sensor init failed");
        return NULL;
    }
    p_sensor->is_init = true;
    return (sensor_light_handle_t)p_sensor;
}

esp_err_t light_sensor_delete(sensor_light_handle_t *sensor)
{
    SENSOR_CHECK(sensor != NULL && *sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_STATE);
    sensor_light_t *p_sensor = (sensor_light_t* )(*sensor);

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
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_STATE);
    sensor_light_t *p_sensor = (sensor_light_t* )(sensor);
    if (!p_sensor->is_init) {
        return ESP_FAIL;
    }
    esp_err_t ret = p_sensor->impl->test();
    return ret;
}

esp_err_t light_sensor_acquire_light(sensor_light_handle_t sensor, float *lux)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_STATE);
    sensor_light_t *p_sensor = (sensor_light_t* )(sensor);
    esp_err_t ret = p_sensor->impl->acquire_light(lux);
    return ret;
}

esp_err_t light_sensor_acquire_rgbw(sensor_light_handle_t sensor, rgbw_t *rgbw)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_STATE);
    sensor_light_t *p_sensor = (sensor_light_t* )(sensor);
    esp_err_t ret = p_sensor->impl->acquire_rgbw(&rgbw->r, &rgbw->g, &rgbw->b, &rgbw->w);
    return ret;
}

esp_err_t light_sensor_acquire_uv(sensor_light_handle_t sensor, uv_t *uv)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_STATE);
    sensor_light_t *p_sensor = (sensor_light_t* )(sensor);
    esp_err_t ret = p_sensor->impl->acquire_uv(&uv->uv, &uv->uva, &uv->uvb);
    return ret;
}

esp_err_t light_sensor_sleep(sensor_light_handle_t sensor)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_STATE);
    sensor_light_t *p_sensor = (sensor_light_t* )(sensor);
    esp_err_t ret = p_sensor->impl->sleep();
    return ret; 
}

esp_err_t light_sensor_wakeup(sensor_light_handle_t sensor)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_STATE);
    sensor_light_t *p_sensor = (sensor_light_t* )(sensor);
    esp_err_t ret = p_sensor->impl->wakeup();
    return ret;
}