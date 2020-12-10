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
#include "imu_hal.h"

#ifdef CONFIG_SENSOR_IMU_INCLUDED_MPU6050
#include "mpu6050.h"
#endif
#ifdef CONFIG_SENSOR_IMU_INCLUDED_LIS2DH12
#include "lis2dh12.h"
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static esp_err_t null_function(void) {return ESP_ERR_NOT_FOUND;}
static esp_err_t null_acquire_function(float* x, float* y, float* z) {return ESP_ERR_NOT_FOUND;}
#pragma GCC diagnostic pop

static const char* TAG = "IMU";

#define SENSOR_CHECK(a, str, ret) if(!(a)) { \
    ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
    return (ret); \
    }

typedef struct {
  imu_id_t id;
  esp_err_t (*init)(bus_handle_t);
  esp_err_t (*deinit)(void);
  esp_err_t (*test)(void);
  esp_err_t (*acquire_acce)(float* acce_x, float* acce_y, float* acce_z);
  esp_err_t (*acquire_gyro)(float* gyro_x, float* gyro_y, float* gyro_z);
  esp_err_t (*sleep)(void);
  esp_err_t (*wakeup)(void);
} imu_impl_t;

typedef struct {
    imu_id_t id;
    bus_handle_t bus;
    bool is_init;
    const imu_impl_t* impl;
}sensor_imu_t;

static const imu_impl_t imu_implementations[] = {
#ifdef CONFIG_SENSOR_IMU_INCLUDED_MPU6050
  {
    .id = MPU6050_ID,
    .init = imu_mpu6050_init,
    .deinit = imu_mpu6050_deinit,
    .test = imu_mpu6050_test,
    .acquire_acce = imu_mpu6050_acquire_acce,
    .acquire_gyro = imu_mpu6050_acquire_gyro,
    .sleep = imu_mpu6050_sleep,
    .wakeup = imu_mpu6050_wakeup,
  },
#endif
#ifdef CONFIG_SENSOR_IMU_INCLUDED_LIS2DH12
  {
    .id = LIS2DH12_ID,
    .init = imu_lis2dh12_init,
    .deinit = imu_lis2dh12_deinit,
    .test = imu_lis2dh12_test,
    .acquire_acce = imu_lis2dh12_acquire_acce,
    .acquire_gyro = null_acquire_function,
    .sleep = null_function,
    .wakeup = null_function,
  },
#endif
};

/****************************private functions*************************************/

static const imu_impl_t* find_implementation(imu_id_t id)
{
  const imu_impl_t* active_driver = NULL;
  int count = sizeof(imu_implementations)/sizeof(imu_impl_t);
  for (int i = 0; i < count; i++) {
    if (imu_implementations[i].id == id) {
      active_driver = &imu_implementations[i];
      break;
    }
  }
  return active_driver;
}

/****************************public functions*************************************/

sensor_imu_handle_t imu_create(bus_handle_t bus, int imu_id)
{
    SENSOR_CHECK(bus != NULL, "i2c bus has not initialized", NULL);
    const imu_impl_t *sensor_impl = find_implementation(imu_id);

    if (sensor_impl == NULL) {
        ESP_LOGE(TAG, "no driver founded, IMU ID = %d", imu_id);
        return NULL;
    }

    sensor_imu_t* p_sensor = (sensor_imu_t*)malloc(sizeof(sensor_imu_t));
    SENSOR_CHECK(p_sensor != NULL, "imu sensor creat failed", NULL);
    p_sensor->id = imu_id;
    p_sensor->bus = bus;
    p_sensor->impl = sensor_impl;
    esp_err_t ret = p_sensor->impl->init(bus);
    if (ret != ESP_OK) {
        free(p_sensor);
        ESP_LOGE(TAG, "imu sensor init failed");
        return NULL;
    }
    p_sensor->is_init = true;
    return (sensor_imu_handle_t)p_sensor;
}

esp_err_t imu_delete(sensor_imu_handle_t *sensor)
{
    SENSOR_CHECK(sensor != NULL && *sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_STATE);
    sensor_imu_t *p_sensor = (sensor_imu_t* )(*sensor);

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
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_STATE);
    sensor_imu_t *p_sensor = (sensor_imu_t* )(sensor);
    if (!p_sensor->is_init) {
        return ESP_FAIL;
    }
    esp_err_t ret = p_sensor->impl->test();
    return ret;
}

esp_err_t imu_acquire_acce(sensor_imu_handle_t sensor, axis3_t* acce)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_STATE);
    sensor_imu_t *p_sensor = (sensor_imu_t* )(sensor);
    esp_err_t ret = p_sensor->impl->acquire_acce(&acce->x, &acce->y, &acce->z);
    return ret;
}

esp_err_t imu_acquire_gyro(sensor_imu_handle_t sensor, axis3_t* gyro)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_STATE);
    sensor_imu_t *p_sensor = (sensor_imu_t* )(sensor);
    esp_err_t ret = p_sensor->impl->acquire_gyro(&gyro->x, &gyro->y, &gyro->z);
    return ret;
}

esp_err_t imu_sleep(sensor_imu_handle_t sensor)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_STATE);
    sensor_imu_t *p_sensor = (sensor_imu_t* )(sensor);
    esp_err_t ret = p_sensor->impl->sleep();
    return ret; 
}

esp_err_t imu_wakeup(sensor_imu_handle_t sensor)
{
    SENSOR_CHECK(sensor != NULL, "sensor handle can't be NULL ", ESP_ERR_INVALID_STATE);
    sensor_imu_t *p_sensor = (sensor_imu_t* )(sensor);
    esp_err_t ret = p_sensor->impl->wakeup();
    return ret;
}
